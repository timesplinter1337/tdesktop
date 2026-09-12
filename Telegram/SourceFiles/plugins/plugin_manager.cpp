// This file is part of Telegram Desktop.
#include "plugins/plugin_manager.h"
#include "plugins/lua_core.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QSet>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <crl/crl_on_main.h>

#include "main/main_session.h"
#include "main/main_account.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_thread.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_widget.h"
#include "window/window_session_controller.h"
#include "api/api_editing.h"
#include "data/data_histories.h"
#include "apiwrap.h"
#include "ui/toast/toast.h"
#include "logs.h"

namespace Plugins {

namespace {

// C-callbacks for Lua: Core
int Lua_Log(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto text = core.toString(L, 1);
	LOG(("Lua Plugin: %1").arg(text));
	return 0;
}

int Lua_SendMessage(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto peerId = static_cast<uint64>(core.toInteger(L, 1));
	const auto text = core.toString(L, 2);
	crl::on_main([=] {
		PluginManager::Instance().sendMessage(peerId, text, 0);
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

// C-callbacks for Lua: telegram.ui
int Lua_UI_SetStyleSheet(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto css = core.toString(L, 1);
	PluginManager::Instance().setAppStyleSheet(css);
	return 0;
}

int Lua_UI_AddStyleSheet(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto css = core.toString(L, 1);
	PluginManager::Instance().addAppStyleSheet(css);
	return 0;
}

int Lua_UI_GetStyleSheet(lua_State *L) {
	auto &core = LuaCore::Instance();
	core.pushString(L, PluginManager::Instance().getAppStyleSheet());
	return 1;
}

int Lua_UI_ShowToast(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto text = core.toString(L, 1);
	PluginManager::Instance().showToast(text);
	return 0;
}

int Lua_UI_Copy(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto text = core.toString(L, 1);
	PluginManager::Instance().copyToClipboard(text);
	return 0;
}

int Lua_UI_OpenUrl(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto url = core.toString(L, 1);
	PluginManager::Instance().openUrl(url);
	return 0;
}

// C-callbacks for Lua: telegram.chat
int Lua_Chat_GetActive(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto info = PluginManager::Instance().getActivePeerInfo();
	core.createTable(L, 0, 4);
	core.setFieldInteger(L, "id", static_cast<int64_t>(info.id));
	core.setFieldString(L, "type", info.type);
	core.setFieldString(L, "title", info.title);
	core.setFieldString(L, "username", info.username);
	return 1;
}

int Lua_Chat_GetChat(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto peerId = static_cast<uint64>(core.toInteger(L, 1));
	const auto info = PluginManager::Instance().getPeerInfo(peerId);
	core.createTable(L, 0, 4);
	core.setFieldInteger(L, "id", static_cast<int64_t>(info.id));
	core.setFieldString(L, "type", info.type);
	core.setFieldString(L, "title", info.title);
	core.setFieldString(L, "username", info.username);
	return 1;
}

int Lua_Chat_GetMe(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto info = PluginManager::Instance().getMeInfo();
	core.createTable(L, 0, 5);
	core.setFieldInteger(L, "id", static_cast<int64_t>(info.id));
	core.setFieldString(L, "first_name", info.firstName);
	core.setFieldString(L, "last_name", info.lastName);
	core.setFieldString(L, "username", info.username);
	core.setFieldString(L, "phone", info.phone);
	return 1;
}

int Lua_Chat_SendMessage(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto peerId = static_cast<uint64>(core.toInteger(L, 1));
	const auto text = core.toString(L, 2);
	const auto replyTo = core.getTop(L) >= 3 ? static_cast<uint64>(core.toInteger(L, 3)) : 0;
	crl::on_main([=] {
		PluginManager::Instance().sendMessage(peerId, text, replyTo);
	});
	return 0;
}

int Lua_Chat_EditMessage(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto peerId = static_cast<uint64>(core.toInteger(L, 1));
	const auto msgId = static_cast<int>(core.toInteger(L, 2));
	const auto text = core.toString(L, 3);
	crl::on_main([=] {
		PluginManager::Instance().editMessage(peerId, msgId, text);
	});
	return 0;
}

int Lua_Chat_DeleteMessage(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto peerId = static_cast<uint64>(core.toInteger(L, 1));
	const auto msgId = static_cast<int>(core.toInteger(L, 2));
	crl::on_main([=] {
		PluginManager::Instance().deleteMessage(peerId, msgId);
	});
	return 0;
}

// C-callbacks for Lua: telegram.input
int Lua_Input_GetText(lua_State *L) {
	auto &core = LuaCore::Instance();
	core.pushString(L, PluginManager::Instance().getInputText());
	return 1;
}

int Lua_Input_SetText(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto text = core.toString(L, 1);
	PluginManager::Instance().setInputText(text);
	return 0;
}

int Lua_Input_InsertText(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto text = core.toString(L, 1);
	PluginManager::Instance().insertInputText(text);
	return 0;
}

// C-callbacks for Lua: telegram.http
int Lua_Http_Get(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto url = core.toString(L, 1);
	QMap<QString, QString> headers;
	int cbIdx = 2;
	if (core.isTable(L, 2)) {
		core.pushNil(L);
		while (core.next(L, 2)) {
			headers[core.toString(L, -2)] = core.toString(L, -1);
			core.pop(L, 1);
		}
		cbIdx = 3;
	}
	if (core.isFunction(L, cbIdx)) {
		core.pushValue(L, cbIdx);
		const int cbRef = core.ref(L);
		PluginManager::Instance().httpRequest(L, "GET", url, headers, QByteArray(), cbRef);
	}
	return 0;
}

int Lua_Http_Post(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto url = core.toString(L, 1);
	const auto body = core.toString(L, 2).toUtf8();
	QMap<QString, QString> headers;
	int cbIdx = 3;
	if (core.isTable(L, 3)) {
		core.pushNil(L);
		while (core.next(L, 3)) {
			headers[core.toString(L, -2)] = core.toString(L, -1);
			core.pop(L, 1);
		}
		cbIdx = 4;
	}
	if (core.isFunction(L, cbIdx)) {
		core.pushValue(L, cbIdx);
		const int cbRef = core.ref(L);
		PluginManager::Instance().httpRequest(L, "POST", url, headers, body, cbRef);
	}
	return 0;
}

int Lua_Http_Request(lua_State *L) {
	auto &core = LuaCore::Instance();
	if (!core.isTable(L, 1) || !core.isFunction(L, 2)) {
		return 0;
	}
	QString url;
	QString method = "GET";
	QByteArray body;
	QMap<QString, QString> headers;

	if (core.getField(L, 1, "url")) {
		url = core.toString(L, -1);
		core.pop(L, 1);
	}
	if (core.getField(L, 1, "method")) {
		method = core.toString(L, -1);
		core.pop(L, 1);
	}
	if (core.getField(L, 1, "body")) {
		body = core.toString(L, -1).toUtf8();
		core.pop(L, 1);
	}
	if (core.getField(L, 1, "headers") && core.isTable(L, -1)) {
		core.pushNil(L);
		while (core.next(L, -2)) {
			headers[core.toString(L, -2)] = core.toString(L, -1);
			core.pop(L, 1);
		}
		core.pop(L, 1);
	} else {
		core.pop(L, 1);
	}

	core.pushValue(L, 2);
	const int cbRef = core.ref(L);
	PluginManager::Instance().httpRequest(L, method, url, headers, body, cbRef);
	return 0;
}

// C-callbacks for Lua: telegram.timer
int Lua_Timer_SetTimeout(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto delayMs = static_cast<int>(core.toInteger(L, 1));
	if (core.isFunction(L, 2)) {
		core.pushValue(L, 2);
		const int cbRef = core.ref(L);
		const int timerId = PluginManager::Instance().setTimeout(L, delayMs, cbRef);
		core.pushInteger(L, timerId);
		return 1;
	}
	return 0;
}

int Lua_Timer_SetInterval(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto intervalMs = static_cast<int>(core.toInteger(L, 1));
	if (core.isFunction(L, 2)) {
		core.pushValue(L, 2);
		const int cbRef = core.ref(L);
		const int timerId = PluginManager::Instance().setInterval(L, intervalMs, cbRef);
		core.pushInteger(L, timerId);
		return 1;
	}
	return 0;
}

int Lua_Timer_Clear(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto timerId = static_cast<int>(core.toInteger(L, 1));
	PluginManager::Instance().clearTimer(timerId);
	return 0;
}

// C-callbacks for Lua: telegram.storage
int Lua_Storage_Get(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto plugin = PluginManager::Instance().findPluginByState(L);
	if (!plugin) return 0;
	const auto key = core.toString(L, 1);
	const auto defVal = (core.getTop(L) >= 2) ? core.toJsonValue(L, 2) : QJsonValue();
	const auto res = PluginManager::Instance().storageGet(plugin->id, key, defVal);
	core.pushJsonValue(L, res);
	return 1;
}

int Lua_Storage_Set(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto plugin = PluginManager::Instance().findPluginByState(L);
	if (!plugin) return 0;
	const auto key = core.toString(L, 1);
	const auto val = core.toJsonValue(L, 2);
	PluginManager::Instance().storageSet(plugin->id, key, val);
	return 0;
}

int Lua_Storage_Remove(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto plugin = PluginManager::Instance().findPluginByState(L);
	if (!plugin) return 0;
	const auto key = core.toString(L, 1);
	PluginManager::Instance().storageRemove(plugin->id, key);
	return 0;
}

int Lua_Storage_All(lua_State *L) {
	auto &core = LuaCore::Instance();
	const auto plugin = PluginManager::Instance().findPluginByState(L);
	if (!plugin) return 0;
	const auto all = PluginManager::Instance().storageAll(plugin->id);
	core.pushJsonValue(L, all);
	return 1;
}

} // namespace

PluginManager::PluginManager() {
	ensurePluginsDirectoryExists();
	LuaCore::Instance().initialize();
	reloadPlugins();
}

PluginManager::~PluginManager() {
	auto &core = LuaCore::Instance();
	for (auto &plugin : _plugins) {
		if (plugin.L) {
			clearTimersForState(plugin.L);
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

void PluginManager::setSessionController(Window::SessionController *controller) {
	_sessionController = controller;
	if (controller) {
		_session = &controller->session();
	}
}

Window::SessionController *PluginManager::sessionController() const {
	return _sessionController;
}

void PluginManager::setActiveHistoryWidget(HistoryWidget *widget) {
	_activeHistoryWidget = widget;
}

void PluginManager::clearActiveHistoryWidget(HistoryWidget *widget) {
	if (_activeHistoryWidget == widget) {
		_activeHistoryWidget = nullptr;
	}
}

HistoryWidget *PluginManager::activeHistoryWidget() const {
	return _activeHistoryWidget.data();
}

QString PluginManager::pluginsDirectory() const {
	const auto exeDir = QDir(QCoreApplication::applicationDirPath()).filePath("plugins");
	if (QDir(exeDir).exists()) {
		return exeDir;
	}
	const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	return QDir(base).filePath("plugins");
}

void PluginManager::ensurePluginsDirectoryExists() {
	const auto exeDir = QDir(QCoreApplication::applicationDirPath()).filePath("plugins");
	QDir(exeDir).mkpath(".");

	const auto path = pluginsDirectory();
	QDir dir(path);
	if (!dir.exists()) {
		dir.mkpath(".");
	}

	// Create sample plugin if directory is empty
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
				"function Plugin:on_disable()\n"
				"    telegram.log(\"Plugin \" .. self.name .. \" disabled!\")\n"
				"end\n\n"
				"function Plugin:on_pre_send(text, peer_id)\n"
				"    if text == \".shrug\" or text == \"/shrug\" then\n"
				"        return \"¯\\\\_(ツ)_/¯\"\n"
				"    end\n"
				"    return text\n"
				"end\n\n"
				"function Plugin:on_message(msg)\n"
				"    if not msg.out and string.find(string.lower(msg.text), \"привет\") then\n"
				"        telegram.ui.show_toast(\"Привет от пользователя: \" .. tostring(msg.from_id))\n"
				"    end\n"
				"end\n";
			file.write(sampleContent);
		}
	}
}

void PluginManager::saveConfig() {
	QJsonObject root;
	for (const auto &p : _plugins) {
		root[p.id] = p.enabled;
	}
	const auto dir = pluginsDirectory();
	QDir().mkpath(dir);
	const auto path = QDir(dir).filePath("plugins_config.json");
	QFile file(path);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	}
}

QMap<QString, bool> PluginManager::readSavedStates() {
	QMap<QString, bool> states;
	const auto path = QDir(pluginsDirectory()).filePath("plugins_config.json");
	QFile file(path);
	if (file.open(QIODevice::ReadOnly)) {
		const auto doc = QJsonDocument::fromJson(file.readAll());
		if (doc.isObject()) {
			const auto obj = doc.object();
			for (auto it = obj.begin(); it != obj.end(); ++it) {
				states.insert(it.key(), it.value().toBool(true));
			}
		}
	}
	return states;
}

void PluginManager::registerTelegramAPI(lua_State *L) {
	auto &core = LuaCore::Instance();

	// Root telegram table
	core.createTable(L, 0, 10);

	core.setFieldFunction(L, "log", Lua_Log);
	core.setFieldFunction(L, "send_message", Lua_SendMessage);
	core.setFieldFunction(L, "show_toast", Lua_ShowToast);

	// telegram.ui
	core.createTable(L, 0, 8);
	core.setFieldFunction(L, "set_style_sheet", Lua_UI_SetStyleSheet);
	core.setFieldFunction(L, "add_style_sheet", Lua_UI_AddStyleSheet);
	core.setFieldFunction(L, "get_style_sheet", Lua_UI_GetStyleSheet);
	core.setFieldFunction(L, "show_toast", Lua_UI_ShowToast);
	core.setFieldFunction(L, "copy", Lua_UI_Copy);
	core.setFieldFunction(L, "open_url", Lua_UI_OpenUrl);
	core.setField(L, -2, "ui");

	// telegram.chat
	core.createTable(L, 0, 8);
	core.setFieldFunction(L, "get_active", Lua_Chat_GetActive);
	core.setFieldFunction(L, "get_me", Lua_Chat_GetMe);
	core.setFieldFunction(L, "get_chat", Lua_Chat_GetChat);
	core.setFieldFunction(L, "send_message", Lua_Chat_SendMessage);
	core.setFieldFunction(L, "edit_message", Lua_Chat_EditMessage);
	core.setFieldFunction(L, "delete_message", Lua_Chat_DeleteMessage);
	core.setField(L, -2, "chat");

	// telegram.input
	core.createTable(L, 0, 4);
	core.setFieldFunction(L, "get_text", Lua_Input_GetText);
	core.setFieldFunction(L, "set_text", Lua_Input_SetText);
	core.setFieldFunction(L, "insert_text", Lua_Input_InsertText);
	core.setField(L, -2, "input");

	// telegram.http
	core.createTable(L, 0, 4);
	core.setFieldFunction(L, "get", Lua_Http_Get);
	core.setFieldFunction(L, "post", Lua_Http_Post);
	core.setFieldFunction(L, "request", Lua_Http_Request);
	core.setField(L, -2, "http");

	// telegram.timer
	core.createTable(L, 0, 4);
	core.setFieldFunction(L, "set_timeout", Lua_Timer_SetTimeout);
	core.setFieldFunction(L, "set_interval", Lua_Timer_SetInterval);
	core.setFieldFunction(L, "clear", Lua_Timer_Clear);
	core.setField(L, -2, "timer");

	// telegram.storage
	core.createTable(L, 0, 4);
	core.setFieldFunction(L, "get", Lua_Storage_Get);
	core.setFieldFunction(L, "set", Lua_Storage_Set);
	core.setFieldFunction(L, "remove", Lua_Storage_Remove);
	core.setFieldFunction(L, "all", Lua_Storage_All);
	core.setField(L, -2, "storage");

	core.setGlobal(L, "telegram");
}

void PluginManager::loadPluginFile(const QString &filePath, bool isEnabled) {
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

	PluginInfo info;
	info.id = QFileInfo(filePath).fileName();
	info.filePath = filePath;
	info.L = L;
	info.enabled = isEnabled;

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

		core.pop(L, 1);
	} else {
		core.pop(L, 1);
	}

	if (info.name.isEmpty()) {
		info.name = info.id;
	}

	LOG(("PluginManager: Loaded '%1' (%2), enabled=%3")
		.arg(info.name)
		.arg(info.id)
		.arg(info.enabled));

	_plugins.push_back(std::move(info));
	auto &plugin = _plugins.back();

	if (plugin.enabled) {
		if (core.getGlobal(plugin.L, "Plugin") && core.isTable(plugin.L, -1)) {
			if (core.getField(plugin.L, -1, "on_enable") && core.isFunction(plugin.L, -1)) {
				core.pushValue(plugin.L, -2); // self
				QString err;
				if (!core.pcall(plugin.L, 1, 0, err)) {
					plugin.lastError = err;
					LOG(("PluginManager: Error in on_enable (%1): %2").arg(plugin.name).arg(err));
				}
			} else {
				core.pop(plugin.L, 1);
			}
			core.pop(plugin.L, 1);
		}
	}
}

void PluginManager::reloadPlugins() {
	auto &core = LuaCore::Instance();
	if (!core.isAvailable()) {
		core.initialize();
	}

	QMap<QString, bool> states = readSavedStates();
	for (const auto &p : _plugins) {
		states[p.id] = p.enabled;
	}

	for (auto &p : _plugins) {
		if (p.L) {
			clearTimersForState(p.L);
			core.closeState(p.L);
			p.L = nullptr;
		}
	}
	_plugins.clear();

	ensurePluginsDirectoryExists();

	QStringList candidateDirs;
	candidateDirs.push_back(QDir(QCoreApplication::applicationDirPath()).filePath("plugins"));
	candidateDirs.push_back(pluginsDirectory());
	const auto roaming = QDir::homePath() + "/AppData/Roaming";
	candidateDirs.push_back(roaming + "/TelegramDesktop/plugins");
	candidateDirs.push_back(roaming + "/Telegram Desktop/plugins");

	QSet<QString> seenFiles;
	for (const auto &dirPath : candidateDirs) {
		QDir dir(dirPath);
		if (!dir.exists()) {
			continue;
		}
		const auto entries = dir.entryInfoList({ "*.lua" }, QDir::Files, QDir::Name);
		for (const auto &fileInfo : entries) {
			const auto name = fileInfo.fileName();
			if (!seenFiles.contains(name)) {
				seenFiles.insert(name);
				const bool isEnabled = states.value(name, true);
				loadPluginFile(fileInfo.absoluteFilePath(), isEnabled);
			}
		}
	}
	LOG(("PluginManager: Loaded %1 plugin(s)").arg(_plugins.size()));
}

const std::vector<PluginInfo> &PluginManager::plugins() const {
	return _plugins;
}

void PluginManager::setPluginEnabled(const QString &id, bool enabled) {
	for (auto &p : _plugins) {
		if (p.id == id) {
			if (p.enabled == enabled) {
				return;
			}
			p.enabled = enabled;
			LOG(("PluginManager: Plugin '%1' enabled set to %2").arg(id).arg(enabled));

			auto &core = LuaCore::Instance();
			if (p.L && core.isAvailable()) {
				if (!enabled) {
					clearTimersForState(p.L);
				}

				if (core.getGlobal(p.L, "Plugin") && core.isTable(p.L, -1)) {
					const char *hook = enabled ? "on_enable" : "on_disable";
					if (core.getField(p.L, -1, hook) && core.isFunction(p.L, -1)) {
						core.pushValue(p.L, -2); // self
						QString err;
						if (!core.pcall(p.L, 1, 0, err)) {
							LOG(("PluginManager: Error in %1 on %2: %3")
								.arg(hook)
								.arg(p.name)
								.arg(err));
						}
					} else {
						core.pop(p.L, 1);
					}
					core.pop(p.L, 1);
				} else {
					core.pop(p.L, 1);
				}
			}

			saveConfig();
			break;
		}
	}
}

QString PluginManager::dispatchPreSend(const QString &text, uint64 peerId) {
	if (_plugins.empty()) {
		reloadPlugins();
	}

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
					LOG(("PluginManager: Error in on_pre_send (%1): %2").arg(p.name).arg(err));
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
	if (_plugins.empty()) {
		reloadPlugins();
	}

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

				core.createTable(p.L, 0, 5);
				core.setFieldString(p.L, "text", text);
				core.setFieldInteger(p.L, "from_id", static_cast<int64_t>(fromId));
				core.setFieldInteger(p.L, "peer_id", static_cast<int64_t>(peerId));
				core.setFieldInteger(p.L, "date", date);
				core.setFieldBoolean(p.L, "out", out);

				QString err;
				if (!core.pcall(p.L, 2, 0, err)) {
					p.lastError = err;
					LOG(("PluginManager: Error in on_message (%1): %2").arg(p.name).arg(err));
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

bool PluginManager::dispatchCommand(const QString &text, uint64 peerId) {
	const auto trimmed = text.trimmed();
	if (!trimmed.startsWith('/') && !trimmed.startsWith('.')) {
		return false;
	}

	const auto spaceIdx = trimmed.indexOf(' ');
	const auto cmd = (spaceIdx == -1)
		? trimmed.mid(1)
		: trimmed.mid(1, spaceIdx - 1);
	const auto args = (spaceIdx == -1)
		? QString()
		: trimmed.mid(spaceIdx + 1).trimmed();

	auto &core = LuaCore::Instance();
	if (!core.isAvailable()) {
		return false;
	}

	bool handled = false;
	for (auto &p : _plugins) {
		if (!p.enabled || !p.L) {
			continue;
		}

		if (core.getGlobal(p.L, "Plugin") && core.isTable(p.L, -1)) {
			if (core.getField(p.L, -1, "on_command") && core.isFunction(p.L, -1)) {
				core.pushValue(p.L, -2); // self
				core.pushString(p.L, cmd);
				core.pushString(p.L, args);
				core.pushInteger(p.L, static_cast<int64_t>(peerId));

				QString err;
				if (core.pcall(p.L, 4, 1, err)) {
					if (core.toBoolean(p.L, -1)) {
						handled = true;
					}
					core.pop(p.L, 1);
				} else {
					p.lastError = err;
					LOG(("PluginManager: Error in on_command (%1): %2").arg(p.name).arg(err));
				}
			} else {
				core.pop(p.L, 1);
			}
			core.pop(p.L, 1);
		} else {
			core.pop(p.L, 1);
		}

		if (handled) {
			break;
		}
	}

	return handled;
}

void PluginManager::dispatchChatChanged(uint64 peerId) {
	auto &core = LuaCore::Instance();
	if (!core.isAvailable()) {
		return;
	}

	for (auto &p : _plugins) {
		if (!p.enabled || !p.L) {
			continue;
		}

		if (core.getGlobal(p.L, "Plugin") && core.isTable(p.L, -1)) {
			if (core.getField(p.L, -1, "on_chat_changed") && core.isFunction(p.L, -1)) {
				core.pushValue(p.L, -2); // self
				core.pushInteger(p.L, static_cast<int64_t>(peerId));
				QString err;
				if (!core.pcall(p.L, 2, 0, err)) {
					p.lastError = err;
					LOG(("PluginManager: Error in on_chat_changed (%1): %2").arg(p.name).arg(err));
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

void PluginManager::setAppStyleSheet(const QString &css) {
	crl::on_main([=] {
		_customStyleSheet = css;
		if (auto app = qApp) {
			app->setStyleSheet(css);
		}
	});
}

void PluginManager::addAppStyleSheet(const QString &css) {
	crl::on_main([=] {
		_customStyleSheet += "\n" + css;
		if (auto app = qApp) {
			app->setStyleSheet(_customStyleSheet);
		}
	});
}

QString PluginManager::getAppStyleSheet() const {
	return _customStyleSheet;
}

void PluginManager::showToast(const QString &text) {
	crl::on_main([=] {
		if (QApplication::activeWindow() || !QApplication::topLevelWidgets().isEmpty()) {
			Ui::Toast::Show(text);
		} else {
			LOG(("PluginManager: Toast (before window): %1").arg(text));
		}
	});
}

void PluginManager::copyToClipboard(const QString &text) {
	crl::on_main([=] {
		if (const auto cb = QGuiApplication::clipboard()) {
			cb->setText(text);
		}
	});
}

void PluginManager::openUrl(const QString &url) {
	crl::on_main([=] {
		QDesktopServices::openUrl(QUrl(url));
	});
}

PeerInfo PluginManager::getActivePeerInfo() const {
	PeerInfo info;
	if (_sessionController) {
		const auto key = _sessionController->activeChatCurrent();
		if (const auto history = key.history()) {
			const auto peer = history->peer;
			info.id = peer->id.value;
			info.title = peer->name();
			info.username = peer->username();
			if (peer->isUser()) {
				info.type = "user";
			} else if (peer->isChat()) {
				info.type = "chat";
			} else if (peer->isChannel()) {
				info.type = "channel";
			} else {
				info.type = "unknown";
			}
		}
	}
	return info;
}

PeerInfo PluginManager::getPeerInfo(uint64 peerId) const {
	PeerInfo info;
	if (_session) {
		if (const auto peer = _session->data().peer(PeerId(peerId))) {
			info.id = peer->id.value;
			info.title = peer->name();
			info.username = peer->username();
			if (peer->isUser()) {
				info.type = "user";
			} else if (peer->isChat()) {
				info.type = "chat";
			} else if (peer->isChannel()) {
				info.type = "channel";
			} else {
				info.type = "unknown";
			}
		}
	}
	return info;
}

UserInfo PluginManager::getMeInfo() const {
	UserInfo info;
	if (_session) {
		if (const auto user = _session->user()) {
			info.id = user->id.value;
			info.firstName = user->firstName;
			info.lastName = user->lastName;
			info.username = user->username();
			info.phone = user->phone();
		}
	}
	return info;
}

void PluginManager::sendMessage(uint64 peerId, const QString &text, uint64 replyTo) {
	if (!_session || text.isEmpty()) {
		return;
	}

	const auto peer = _session->data().peer(PeerId(peerId));
	if (!peer) {
		return;
	}

	const auto history = _session->data().history(peer);
	auto action = Api::SendAction(static_cast<Data::Thread*>(history.get()));
	if (replyTo > 0) {
		action.replyTo.messageId = FullMsgId(peer->id, static_cast<MsgId>(replyTo));
	}
	auto message = Api::MessageToSend(action);
	message.textWithTags = { text, {} };
	_session->api().sendMessage(std::move(message));
}

void PluginManager::editMessage(uint64 peerId, int msgId, const QString &text) {
	if (!_session || text.isEmpty() || msgId <= 0) {
		return;
	}
	const auto peer = _session->data().peer(PeerId(peerId));
	if (!peer) {
		return;
	}
	const auto item = _session->data().message(peer, static_cast<MsgId>(msgId));
	if (!item) {
		return;
	}
	Api::EditTextMessage(item, { text, {} }, {}, {}, nullptr, nullptr, false);
}

void PluginManager::deleteMessage(uint64 peerId, int msgId) {
	if (!_session || msgId <= 0) {
		return;
	}
	const auto peer = _session->data().peer(PeerId(peerId));
	if (!peer) {
		return;
	}
	_session->data().histories().deleteMessages(
		MessageIdsList{ FullMsgId(peer->id, static_cast<MsgId>(msgId)) },
		true);
}

QString PluginManager::getInputText() const {
	if (_activeHistoryWidget) {
		return _activeHistoryWidget->getFieldTextWithTags().text;
	}
	return QString();
}

void PluginManager::setInputText(const QString &text) {
	crl::on_main([=] {
		if (_activeHistoryWidget) {
			_activeHistoryWidget->setFieldText({ text, {} });
		}
	});
}

void PluginManager::insertInputText(const QString &text) {
	crl::on_main([=] {
		if (_activeHistoryWidget) {
			_activeHistoryWidget->insertFieldText(text);
		}
	});
}

void PluginManager::httpRequest(
		lua_State *L,
		const QString &method,
		const QString &url,
		const QMap<QString, QString> &headers,
		const QByteArray &body,
		int cbRef) {
	if (!_network) {
		_network = std::make_unique<QNetworkAccessManager>();
	}

	QNetworkRequest req;
	req.setUrl(QUrl(url));
	for (auto it = headers.begin(); it != headers.end(); ++it) {
		req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
	}

	QNetworkReply *reply = nullptr;
	const auto upperMethod = method.toUpper();
	if (upperMethod == "POST") {
		reply = _network->post(req, body);
	} else if (upperMethod == "PUT") {
		reply = _network->put(req, body);
	} else if (upperMethod == "DELETE") {
		reply = _network->deleteResource(req);
	} else {
		reply = _network->get(req);
	}

	if (!reply) {
		auto &core = LuaCore::Instance();
		core.unref(L, cbRef);
		return;
	}

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		auto &core = LuaCore::Instance();
		const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const auto responseBody = QString::fromUtf8(reply->readAll());
		const auto errorStr = reply->error() != QNetworkReply::NoError
			? reply->errorString()
			: QString();

		core.pushRef(L, cbRef);
		if (core.isFunction(L, -1)) {
			core.createTable(L, 0, 4);
			core.setFieldInteger(L, "status", statusCode);
			core.setFieldString(L, "body", responseBody);
			core.setFieldBoolean(L, "ok", (statusCode >= 200 && statusCode < 300));
			if (!errorStr.isEmpty()) {
				core.setFieldString(L, "error", errorStr);
			} else {
				core.pushNil(L);
				core.setField(L, -2, "error");
			}

			QString err;
			core.pcall(L, 1, 0, err);
		} else {
			core.pop(L, 1);
		}

		core.unref(L, cbRef);
		reply->deleteLater();
	});
}

int PluginManager::setTimeout(lua_State *L, int delayMs, int cbRef) {
	const int id = _nextTimerId++;
	auto timer = new QTimer();
	timer->setSingleShot(true);

	TimerInfo info;
	info.id = id;
	info.timer = timer;
	info.L = L;
	info.cbRef = cbRef;
	info.isInterval = false;
	_timers.insert(id, info);

	QObject::connect(timer, &QTimer::timeout, [=] {
		auto &core = LuaCore::Instance();
		core.pushRef(L, cbRef);
		if (core.isFunction(L, -1)) {
			QString err;
			core.pcall(L, 0, 0, err);
		} else {
			core.pop(L, 1);
		}
		core.unref(L, cbRef);
		_timers.remove(id);
		timer->deleteLater();
	});

	timer->start(qMax(0, delayMs));
	return id;
}

int PluginManager::setInterval(lua_State *L, int intervalMs, int cbRef) {
	const int id = _nextTimerId++;
	auto timer = new QTimer();
	timer->setSingleShot(false);

	TimerInfo info;
	info.id = id;
	info.timer = timer;
	info.L = L;
	info.cbRef = cbRef;
	info.isInterval = true;
	_timers.insert(id, info);

	QObject::connect(timer, &QTimer::timeout, [=] {
		auto &core = LuaCore::Instance();
		core.pushRef(L, cbRef);
		if (core.isFunction(L, -1)) {
			QString err;
			core.pcall(L, 0, 0, err);
		} else {
			core.pop(L, 1);
		}
	});

	timer->start(qMax(10, intervalMs));
	return id;
}

void PluginManager::clearTimer(int timerId) {
	if (auto it = _timers.find(timerId); it != _timers.end()) {
		auto &core = LuaCore::Instance();
		core.unref(it->L, it->cbRef);
		it->timer->stop();
		it->timer->deleteLater();
		_timers.erase(it);
	}
}

void PluginManager::clearTimersForState(lua_State *L) {
	auto &core = LuaCore::Instance();
	for (auto it = _timers.begin(); it != _timers.end();) {
		if (it->L == L) {
			core.unref(it->L, it->cbRef);
			it->timer->stop();
			it->timer->deleteLater();
			it = _timers.erase(it);
		} else {
			++it;
		}
	}
}

QJsonValue PluginManager::storageGet(const QString &pluginId, const QString &key, const QJsonValue &defaultVal) {
	if (!_pluginStorageCache.contains(pluginId)) {
		const auto dir = QDir(pluginsDirectory()).filePath("storage");
		QDir().mkpath(dir);
		const auto filePath = QDir(dir).filePath(pluginId + ".json");
		QFile file(filePath);
		if (file.open(QIODevice::ReadOnly)) {
			const auto doc = QJsonDocument::fromJson(file.readAll());
			if (doc.isObject()) {
				_pluginStorageCache[pluginId] = doc.object();
			}
		}
	}
	const auto &obj = _pluginStorageCache[pluginId];
	return obj.contains(key) ? obj.value(key) : defaultVal;
}

void PluginManager::storageSet(const QString &pluginId, const QString &key, const QJsonValue &val) {
	auto &obj = _pluginStorageCache[pluginId];
	obj[key] = val;

	const auto dir = QDir(pluginsDirectory()).filePath("storage");
	QDir().mkpath(dir);
	const auto filePath = QDir(dir).filePath(pluginId + ".json");
	QFile file(filePath);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	}
}

void PluginManager::storageRemove(const QString &pluginId, const QString &key) {
	auto &obj = _pluginStorageCache[pluginId];
	obj.remove(key);

	const auto dir = QDir(pluginsDirectory()).filePath("storage");
	QDir().mkpath(dir);
	const auto filePath = QDir(dir).filePath(pluginId + ".json");
	QFile file(filePath);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	}
}

QJsonObject PluginManager::storageAll(const QString &pluginId) {
	(void)storageGet(pluginId, QString(), QJsonValue());
	return _pluginStorageCache.value(pluginId);
}

PluginInfo *PluginManager::findPluginByState(lua_State *L) {
	for (auto &p : _plugins) {
		if (p.L == L) {
			return &p;
		}
	}
	return nullptr;
}

} // namespace Plugins

