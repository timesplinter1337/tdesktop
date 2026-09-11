// This file is part of Telegram Desktop.
#include "plugins/plugin_manager.h"
#include "plugins/lua_core.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <crl/crl_on_main.h>

#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "apiwrap.h"
#include "ui/toast/toast.h"
#include "logs.h"

namespace Plugins {

namespace {

// C-callbacks for Lua
int Lua_SendMessage(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto peerId = static_cast<uint64>(core.toInteger(L, 1));
	const auto text = core.toString(L, 2);

	crl::on_main([=] {
		PluginManager::Instance().sendMessage(peerId, text);
	});
	return 0;
}

int Lua_ShowToast(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto text = core.toString(L, 1);

	crl::on_main([=] {
		PluginManager::Instance().showToast(text);
	});
	return 0;
}

int Lua_Log(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto text = core.toString(L, 1);
	LOG(("Lua Plugin: %1").arg(text));
	return 0;
}

} // namespace

PluginManager::PluginManager() {
	ensurePluginsDirectoryExists();
	LuaCore::Instance().initialize();
}

PluginManager::~PluginManager() {
	auto &core = LuaCore::Instance();
	for (auto &plugin : _plugins) {
		if (plugin.L) {
			core.closeState(plugin.L);
			plugin.L = nullptr;
		}
	}
}

PluginManager &PluginManager::Instance() {
	static PluginManager instance;
	return instance;
}

void PluginManager::setSession(Main::Session *session) {
	_session = session;
}

Main::Session *PluginManager::session() const {
	return _session;
}

QString PluginManager::pluginsDirectory() const {
	const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	return QDir(base).filePath("plugins");
}

void PluginManager::ensurePluginsDirectoryExists() {
	const auto path = pluginsDirectory();
	QDir dir(path);
	if (!dir.exists()) {
		dir.mkpath(".");
	}

	// Create a sample plugin if directory is empty
	const auto samplePath = dir.filePath("shrug_and_greeter.lua");
	if (!QFile::exists(samplePath)) {
		QFile file(samplePath);
		if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			const char sampleContent[] =
				"-- Sample Telegram Desktop Lua Plugin\n"
				"Plugin = {\n"
				"    name = \"Auto Shrug & Greeter\",\n"
				"    author = \"Community\",\n"
				"    version = \"1.0\",\n"
				"    description = \"Replaces .shrug with emoji and responds to greetings\"\n"
				"}\n\n"
				"function Plugin:on_enable()\n"
				"    telegram.log(\"Plugin \" .. self.name .. \" enabled!\")\n"
				"end\n\n"
				"-- Hook for outgoing messages (pre-send)\n"
				"function Plugin:on_pre_send(text, peer_id)\n"
				"    if text == \".shrug\" then\n"
				"        return \"¯\\\\_(ツ)_/¯\"\n"
				"    end\n"
				"    if text == \".ping\" then\n"
				"        return \"Pong from Lua Plugin!\"\n"
				"    end\n"
				"    return text\n"
				"end\n\n"
				"-- Hook for incoming messages\n"
				"function Plugin:on_message(msg)\n"
				"    if not msg.out and string.find(string.lower(msg.text), \"привет\") then\n"
				"        telegram.show_toast(\"Привет от пользователя: \" .. tostring(msg.from_id))\n"
				"    end\n"
				"end\n";
			file.write(sampleContent);
		}
	}
}

void PluginManager::registerTelegramAPI(lua_State *L) {
	auto &core = LuaCore::Instance();
	core.createTable(L, 0, 4);
	core.setFieldFunction(L, "send_message", Lua_SendMessage);
	core.setFieldFunction(L, "show_toast", Lua_ShowToast);
	core.setFieldFunction(L, "log", Lua_Log);
	core.setGlobal(L, "telegram");
}

void PluginManager::loadPluginFile(const QString &filePath) {
	auto &core = LuaCore::Instance();
	if (!core.isAvailable()) {
		return;
	}

	lua_State *L = core.newState();
	if (!L) {
		return;
	}

	registerTelegramAPI(L);

	QString error;
	if (!core.runFile(L, filePath, error)) {
		PluginInfo info;
		info.id = QFileInfo(filePath).fileName();
		info.name = info.id;
		info.filePath = filePath;
		info.enabled = false;
		info.lastError = error;
		core.closeState(L);
		_plugins.push_back(std::move(info));
		return;
	}

	// Read Plugin table
	PluginInfo info;
	info.id = QFileInfo(filePath).fileName();
	info.filePath = filePath;
	info.L = L;
	info.enabled = true;

	if (core.getGlobal(L, "Plugin") && core.isTable(L, -1)) {
		if (core.getField(L, -1, "name")) {
			info.name = core.toString(L, -1);
			core.pop(L, 1);
		}
		if (core.getField(L, -1, "author")) {
			info.author = core.toString(L, -1);
			core.pop(L, 1);
		}
		if (core.getField(L, -1, "version")) {
			info.version = core.toString(L, -1);
			core.pop(L, 1);
		}
		if (core.getField(L, -1, "description")) {
			info.description = core.toString(L, -1);
			core.pop(L, 1);
		}

		// Call on_enable if exists
		if (core.getField(L, -1, "on_enable") && core.isFunction(L, -1)) {
			core.pushValue(L, -2); // self
			QString err;
			core.pcall(L, 1, 0, err);
		} else {
			core.pop(L, 1);
		}
		core.pop(L, 1); // pop Plugin table
	} else {
		core.pop(L, 1);
	}

	if (info.name.isEmpty()) {
		info.name = info.id;
	}

	_plugins.push_back(std::move(info));
}

void PluginManager::reloadPlugins() {
	auto &core = LuaCore::Instance();

	// Close existing states
	for (auto &p : _plugins) {
		if (p.L) {
			core.closeState(p.L);
			p.L = nullptr;
		}
	}
	_plugins.clear();

	ensurePluginsDirectoryExists();
	QDir dir(pluginsDirectory());
	const auto entries = dir.entryInfoList({ "*.lua" }, QDir::Files, QDir::Name);
	for (const auto &fileInfo : entries) {
		loadPluginFile(fileInfo.absoluteFilePath());
	}
}

const std::vector<PluginInfo> &PluginManager::plugins() const {
	return _plugins;
}

void PluginManager::setPluginEnabled(const QString &id, bool enabled) {
	for (auto &p : _plugins) {
		if (p.id == id) {
			p.enabled = enabled;
			break;
		}
	}
}

QString PluginManager::dispatchPreSend(const QString &text, uint64 peerId) {
	auto &core = LuaCore::Instance();
	if (!core.isAvailable()) {
		return text;
	}

	QString currentText = text;
	for (auto &p : _plugins) {
		if (!p.enabled || !p.L) {
			continue;
		}

		if (core.getGlobal(p.L, "Plugin") && core.isTable(p.L, -1)) {
			if (core.getField(p.L, -1, "on_pre_send") && core.isFunction(p.L, -1)) {
				core.pushValue(p.L, -2); // self
				core.pushString(p.L, currentText);
				core.pushInteger(p.L, static_cast<int64_t>(peerId));

				QString err;
				if (core.pcall(p.L, 3, 1, err)) {
					const auto res = core.toString(p.L, -1);
					if (!res.isEmpty()) {
						currentText = res;
					}
					core.pop(p.L, 1);
				} else {
					p.lastError = err;
				}
			} else {
				core.pop(p.L, 1);
			}
			core.pop(p.L, 1);
		} else {
			core.pop(p.L, 1);
		}
	}

	return currentText;
}

void PluginManager::dispatchMessageReceived(
		const QString &text,
		uint64 fromId,
		uint64 peerId,
		int32 date,
		bool out) {
	auto &core = LuaCore::Instance();
	if (!core.isAvailable()) {
		return;
	}

	for (auto &p : _plugins) {
		if (!p.enabled || !p.L) {
			continue;
		}

		if (core.getGlobal(p.L, "Plugin") && core.isTable(p.L, -1)) {
			if (core.getField(p.L, -1, "on_message") && core.isFunction(p.L, -1)) {
				core.pushValue(p.L, -2); // self

				// Create msg table
				core.createTable(p.L, 0, 5);
				core.setFieldString(p.L, "text", text);
				core.setFieldInteger(p.L, "from_id", static_cast<int64_t>(fromId));
				core.setFieldInteger(p.L, "peer_id", static_cast<int64_t>(peerId));
				core.setFieldInteger(p.L, "date", date);
				core.setFieldBoolean(p.L, "out", out);

				QString err;
				if (!core.pcall(p.L, 2, 0, err)) {
					p.lastError = err;
				}
			} else {
				core.pop(p.L, 1);
			}
			core.pop(p.L, 1);
		} else {
			core.pop(p.L, 1);
		}
	}
}

void PluginManager::sendMessage(uint64 peerId, const QString &text) {
	if (!_session || text.isEmpty()) {
		return;
	}

	const auto peer = _session->data().peer(PeerId(peerId));
	if (!peer) {
		return;
	}

	auto action = Api::SendAction(peer);
	auto message = Api::MessageToSend(action);
	message.textWithTags = { text, {} };
	_session->api().sendMessage(std::move(message));
}

void PluginManager::showToast(const QString &text) {
	Ui::Toast::Show(text);
}

} // namespace Plugins
