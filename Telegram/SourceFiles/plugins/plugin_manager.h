#pragma once

#include <QString>
#include <QMap>
#include <vector>
#include <memory>
#include "plugins/lua_core.h"

namespace Main {
class Session;
} // namespace Main

namespace Plugins {

struct PluginInfo {
	QString id;
	QString name;
	QString author;
	QString version;
	QString description;
	bool enabled = true;
	QString filePath;
	QString lastError;
	lua_State *L = nullptr;
};

class PluginManager final {
public:
	static PluginManager &Instance();

	void setSession(Main::Session *session);
	[[nodiscard]] Main::Session *session() const;

	[[nodiscard]] QString pluginsDirectory() const;
	void reloadPlugins();

	[[nodiscard]] const std::vector<PluginInfo> &plugins() const;
	void setPluginEnabled(const QString &id, bool enabled);

	// Event hooks
	QString dispatchPreSend(const QString &text, uint64 peerId);
	void dispatchMessageReceived(
		const QString &text,
		uint64 fromId,
		uint64 peerId,
		int32 date,
		bool out);

	// Exposed to Lua
	void sendMessage(uint64 peerId, const QString &text);
	void showToast(const QString &text);

private:
	PluginManager();
	~PluginManager();

	void registerTelegramAPI(lua_State *L);
	void loadPluginFile(const QString &filePath, bool isEnabled = true);
	void ensurePluginsDirectoryExists();
	void saveConfig();
	[[nodiscard]] QMap<QString, bool> readSavedStates();

	Main::Session *_session = nullptr;
	std::vector<PluginInfo> _plugins;
};

} // namespace Plugins

