#pragma once

#include <QString>
#include <QMap>
#include <QPointer>
#include <QJsonValue>
#include <QJsonObject>
#include <vector>
#include <memory>
#include "plugins/lua_core.h"

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

class HistoryWidget;
class HistoryItem;
class PeerData;
class QNetworkAccessManager;
class QTimer;

namespace Ui {
class PopupMenu;
} // namespace Ui

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

struct PeerInfo {
	uint64 id = 0;
	QString type;
	QString title;
	QString username;
};

struct UserInfo {
	uint64 id = 0;
	QString firstName;
	QString lastName;
	QString username;
	QString phone;
};

struct TimerInfo {
	int id = 0;
	QTimer *timer = nullptr;
	lua_State *L = nullptr;
	int cbRef = -1;
	bool isInterval = false;
};

struct MessageAction {
	QString title;
	lua_State *L = nullptr;
	int cbRef = -1;
};

struct ChatAction {
	QString title;
	lua_State *L = nullptr;
	int cbRef = -1;
};

class PluginManager final {
public:
	static PluginManager &Instance();

	void setSession(Main::Session *session);
	[[nodiscard]] Main::Session *session() const;

	void setSessionController(Window::SessionController *controller);
	[[nodiscard]] Window::SessionController *sessionController() const;

	void setActiveHistoryWidget(HistoryWidget *widget);
	void clearActiveHistoryWidget(HistoryWidget *widget);
	[[nodiscard]] HistoryWidget *activeHistoryWidget() const;

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
	bool dispatchCommand(const QString &text, uint64 peerId);
	void dispatchChatChanged(uint64 peerId);

	// Context menu extensions
	void fillMessageContextMenu(not_null<Ui::PopupMenu*> menu, not_null<HistoryItem*> item);
	void fillChatContextMenu(not_null<Ui::PopupMenu*> menu, not_null<PeerData*> peer);
	void addMessageAction(lua_State *L, const QString &title, int cbRef);
	void addChatAction(lua_State *L, const QString &title, int cbRef);
	void clearActionsForState(lua_State *L);

	// Exposed to Lua: UI & Styling
	void setAppStyleSheet(const QString &css);
	void addAppStyleSheet(const QString &css);
	[[nodiscard]] QString getAppStyleSheet() const;
	void showToast(const QString &text);
	void copyToClipboard(const QString &text);
	void openUrl(const QString &url);

	// Native Dialogs
	void showAlert(const QString &title, const QString &text);
	void showConfirm(lua_State *L, const QString &title, const QString &text, int cbRef);
	void showPrompt(lua_State *L, const QString &title, const QString &placeholder, int cbRef);

	// Privacy & Ghost Mode
	[[nodiscard]] bool isGhostMode() const;
	void setGhostMode(bool enabled);
	[[nodiscard]] bool isTypingBlocked() const;
	void setTypingBlocked(bool blocked);
	[[nodiscard]] bool isReadBlocked() const;
	void setReadBlocked(bool blocked);

	// Exposed to Lua: Chat & Messages
	[[nodiscard]] PeerInfo getActivePeerInfo() const;
	[[nodiscard]] PeerInfo getPeerInfo(uint64 peerId) const;
	[[nodiscard]] UserInfo getMeInfo() const;
	void sendMessage(uint64 peerId, const QString &text, uint64 replyTo = 0);
	void editMessage(uint64 peerId, int msgId, const QString &text);
	void deleteMessage(uint64 peerId, int msgId);

	// Exposed to Lua: Input Field
	[[nodiscard]] QString getInputText() const;
	void setInputText(const QString &text);
	void insertInputText(const QString &text);

	// Exposed to Lua: HTTP Networking
	void httpGet(lua_State *L, const QString &url, int headersRef, int cbRef);
	void httpPost(lua_State *L, const QString &url, const QByteArray &body, int headersRef, int cbRef);
	void httpRequest(lua_State *L, const QString &method, const QString &url, const QMap<QString, QString> &headers, const QByteArray &body, int cbRef);

	// Exposed to Lua: Timers
	int setTimeout(lua_State *L, int delayMs, int cbRef);
	int setInterval(lua_State *L, int intervalMs, int cbRef);
	void clearTimer(int timerId);
	void clearTimersForState(lua_State *L);

	// Exposed to Lua: Storage
	[[nodiscard]] QJsonValue storageGet(const QString &pluginId, const QString &key, const QJsonValue &defaultVal);
	void storageSet(const QString &pluginId, const QString &key, const QJsonValue &val);
	void storageRemove(const QString &pluginId, const QString &key);
	[[nodiscard]] QJsonObject storageAll(const QString &pluginId);

	[[nodiscard]] PluginInfo *findPluginByState(lua_State *L);

private:
	PluginManager();
	~PluginManager();

	void registerTelegramAPI(lua_State *L);
	void loadPluginFile(const QString &filePath, bool isEnabled = true);
	void ensurePluginsDirectoryExists();
	void saveConfig();
	[[nodiscard]] QMap<QString, bool> readSavedStates();

	void dispatchMessageAction(
		const MessageAction &action,
		int itemId,
		uint64 peerId,
		const QString &text,
		const QString &senderName,
		int date,
		bool out);
	void dispatchChatAction(
		const ChatAction &action,
		uint64 peerId,
		const QString &title,
		const QString &username,
		bool isUser,
		bool isGroup,
		bool isChannel);

	Main::Session *_session = nullptr;
	Window::SessionController *_sessionController = nullptr;
	QPointer<HistoryWidget> _activeHistoryWidget;

	std::vector<PluginInfo> _plugins;
	std::vector<MessageAction> _messageActions;
	std::vector<ChatAction> _chatActions;

	std::unique_ptr<QNetworkAccessManager> _network;

	QMap<int, TimerInfo> _timers;
	int _nextTimerId = 1;

	QMap<QString, QJsonObject> _pluginStorageCache;
	QString _customStyleSheet;

	bool _ghostMode = false;
	bool _typingBlocked = false;
	bool _readBlocked = false;

	std::vector<QString> _pendingToasts;
};

} // namespace Plugins


