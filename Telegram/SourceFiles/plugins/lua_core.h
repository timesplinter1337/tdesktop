// This file is part of Telegram Desktop.
// Lua Core Engine Interface for Telegram Desktop Plugins
#pragma once

#include <QString>
#include <QByteArray>
#include <functional>
#include <memory>

namespace Plugins {

// Forward declaration of internal Lua types
struct lua_State;
using LuaCFunction = int (*)(lua_State *L);

class LuaCore final {
public:
	static LuaCore &Instance();

	bool initialize();
	bool isAvailable() const;
	QString engineStatus() const;

	lua_State *newState();
	void closeState(lua_State *L);

	bool runFile(lua_State *L, const QString &filePath, QString &error);
	bool runString(lua_State *L, const QString &code, QString &error);

	// Stack & Global operations
	void registerGlobalFunction(lua_State *L, const char *name, LuaCFunction fn);
	void createTable(lua_State *L, int narr = 0, int nrec = 0);
	void setFieldFunction(lua_State *L, const char *key, LuaCFunction fn);
	void setFieldString(lua_State *L, const char *key, const QString &val);
	void setFieldNumber(lua_State *L, const char *key, double val);
	void setFieldInteger(lua_State *L, const char *key, int64_t val);
	void setFieldBoolean(lua_State *L, const char *key, bool val);
	void setField(lua_State *L, int idx, const char *key);
	void setGlobal(lua_State *L, const char *name);

	bool getGlobal(lua_State *L, const char *name);
	bool getField(lua_State *L, int idx, const char *key);
	bool isFunction(lua_State *L, int idx);
	bool isTable(lua_State *L, int idx);
	int type(lua_State *L, int idx);
	bool next(lua_State *L, int idx);

	// Registry references for async callbacks
	int ref(lua_State *L);
	void unref(lua_State *L, int r);
	void pushRef(lua_State *L, int r);

	void pushNil(lua_State *L);
	void pushString(lua_State *L, const QString &str);
	void pushNumber(lua_State *L, double val);
	void pushInteger(lua_State *L, int64_t val);
	void pushBoolean(lua_State *L, bool val);
	void pushValue(lua_State *L, int idx);

	void pushJsonValue(lua_State *L, const class QJsonValue &val);
	class QJsonValue toJsonValue(lua_State *L, int idx);

	QString toString(lua_State *L, int idx);
	int64_t toInteger(lua_State *L, int idx);
	double toNumber(lua_State *L, int idx);
	bool toBoolean(lua_State *L, int idx);

	bool pcall(lua_State *L, int nargs, int nresults, QString &error);
	void pop(lua_State *L, int n = 1);
	int getTop(lua_State *L);

private:
	LuaCore();
	~LuaCore();

	bool loadLibrary();

	void *_libraryHandle = nullptr;
	bool _loaded = false;
	QString _statusMessage;

	// Function pointers to Lua 5.4 C API
	lua_State *(*_luaL_newstate)() = nullptr;
	void (*_lua_close)(lua_State *) = nullptr;
	void (*_luaL_openlibs)(lua_State *) = nullptr;
	int (*_luaL_loadfilex)(lua_State *, const char *, const char *) = nullptr;
	int (*_luaL_loadstring)(lua_State *, const char *) = nullptr;
	int (*_lua_pcallk)(lua_State *, int, int, int, intptr_t, void *) = nullptr;
	int (*_lua_getglobal)(lua_State *, const char *) = nullptr;
	void (*_lua_setglobal)(lua_State *, const char *) = nullptr;
	int (*_lua_getfield)(lua_State *, int, const char *) = nullptr;
	void (*_lua_setfield)(lua_State *, int, const char *) = nullptr;
	void (*_lua_pushcclosure)(lua_State *, LuaCFunction, int) = nullptr;
	const char *(*_lua_pushstring)(lua_State *, const char *) = nullptr;
	const char *(*_lua_pushlstring)(lua_State *, const char *, size_t) = nullptr;
	void (*_lua_pushnumber)(lua_State *, double) = nullptr;
	void (*_lua_pushinteger)(lua_State *, int64_t) = nullptr;
	void (*_lua_pushboolean)(lua_State *, int) = nullptr;
	void (*_lua_pushnil)(lua_State *) = nullptr;
	void (*_lua_pushvalue)(lua_State *, int) = nullptr;
	const char *(*_lua_tolstring)(lua_State *, int, size_t *) = nullptr;
	int64_t (*_lua_tointegerx)(lua_State *, int, int *) = nullptr;
	double (*_lua_tonumberx)(lua_State *, int, int *) = nullptr;
	int (*_lua_toboolean)(lua_State *, int) = nullptr;
	int (*_lua_type)(lua_State *, int) = nullptr;
	void (*_lua_createtable)(lua_State *, int, int) = nullptr;
	void (*_lua_settop)(lua_State *, int) = nullptr;
	int (*_lua_gettop)(lua_State *) = nullptr;
	int (*_luaL_ref)(lua_State *, int) = nullptr;
	void (*_luaL_unref)(lua_State *, int, int) = nullptr;
	void (*_lua_rawgeti)(lua_State *, int, int64_t) = nullptr;
	void (*_lua_rawseti)(lua_State *, int, int64_t) = nullptr;
	int (*_lua_next)(lua_State *, int) = nullptr;
};

} // namespace Plugins
