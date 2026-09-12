// This file is part of Telegram Desktop.
#include "plugins/lua_core.h"

#include <QLibrary>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

namespace Plugins {

LuaCore::LuaCore() {
	initialize();
}

LuaCore::~LuaCore() {
	if (_libraryHandle) {
		auto lib = static_cast<QLibrary*>(_libraryHandle);
		if (lib->isLoaded()) {
			lib->unload();
		}
		delete lib;
		_libraryHandle = nullptr;
	}
}

LuaCore &LuaCore::Instance() {
	static LuaCore instance;
	return instance;
}

bool LuaCore::initialize() {
	if (_loaded) {
		return true;
	}
	return loadLibrary();
}

bool LuaCore::isAvailable() const {
	return _loaded;
}

QString LuaCore::engineStatus() const {
	return _statusMessage;
}

bool LuaCore::loadLibrary() {
	const auto appDir = QCoreApplication::applicationDirPath();
	const QStringList candidates = {
		appDir + "/lua54.dll",
		appDir + "/lua5.4.dll",
		appDir + "/lua.dll",
		"lua54",
		"lua5.4",
		"lua"
	};

	QLibrary *lib = nullptr;
	for (const auto &path : candidates) {
		auto testLib = new QLibrary(path);
		if (testLib->load()) {
			lib = testLib;
			break;
		}
		delete testLib;
	}

	if (!lib) {
		_statusMessage = "Lua library (lua54.dll) not found in application directory.";
		return false;
	}

	_libraryHandle = lib;

	// Resolve functions
	#define RESOLVE(name) \
		_##name = reinterpret_cast<decltype(_##name)>(lib->resolve(#name)); \
		if (!_##name) { \
			_statusMessage = QString("Failed to resolve Lua symbol: %1").arg(#name); \
			lib->unload(); \
			return false; \
		}

	RESOLVE(luaL_newstate)
	RESOLVE(lua_close)
	RESOLVE(luaL_openlibs)
	RESOLVE(luaL_loadfilex)
	RESOLVE(luaL_loadstring)
	RESOLVE(lua_pcallk)
	RESOLVE(lua_getglobal)
	RESOLVE(lua_setglobal)
	RESOLVE(lua_getfield)
	RESOLVE(lua_setfield)
	RESOLVE(lua_pushcclosure)
	RESOLVE(lua_pushstring)
	RESOLVE(lua_pushlstring)
	RESOLVE(lua_pushnumber)
	RESOLVE(lua_pushinteger)
	RESOLVE(lua_pushboolean)
	RESOLVE(lua_pushnil)
	RESOLVE(lua_pushvalue)
	RESOLVE(lua_tolstring)
	RESOLVE(lua_tointegerx)
	RESOLVE(lua_tonumberx)
	RESOLVE(lua_toboolean)
	RESOLVE(lua_type)
	RESOLVE(lua_createtable)
	RESOLVE(lua_settop)
	RESOLVE(lua_gettop)
	RESOLVE(luaL_ref)
	RESOLVE(luaL_unref)
	RESOLVE(lua_rawgeti)
	RESOLVE(lua_rawseti)
	RESOLVE(lua_next)

	#undef RESOLVE

	_loaded = true;
	_statusMessage = QString("Lua 5.4 Engine loaded successfully (%1)").arg(lib->fileName());
	return true;
}

lua_State *LuaCore::newState() {
	if (!_loaded || !_luaL_newstate) {
		return nullptr;
	}
	lua_State *L = _luaL_newstate();
	if (L && _luaL_openlibs) {
		_luaL_openlibs(L);
	}
	return L;
}

void LuaCore::closeState(lua_State *L) {
	if (_loaded && _lua_close && L) {
		_lua_close(L);
	}
}

bool LuaCore::runFile(lua_State *L, const QString &filePath, QString &error) {
	if (!_loaded || !L) {
		error = "Lua engine not loaded";
		return false;
	}

	const QByteArray pathUtf8 = filePath.toUtf8();
	if (_luaL_loadfilex(L, pathUtf8.constData(), nullptr) != 0) {
		error = toString(L, -1);
		pop(L, 1);
		return false;
	}

	return pcall(L, 0, 0, error);
}

bool LuaCore::runString(lua_State *L, const QString &code, QString &error) {
	if (!_loaded || !L) {
		error = "Lua engine not loaded";
		return false;
	}

	const QByteArray codeUtf8 = code.toUtf8();
	if (_luaL_loadstring(L, codeUtf8.constData()) != 0) {
		error = toString(L, -1);
		pop(L, 1);
		return false;
	}

	return pcall(L, 0, 0, error);
}

void LuaCore::registerGlobalFunction(lua_State *L, const char *name, LuaCFunction fn) {
	if (!_loaded || !L) return;
	_lua_pushcclosure(L, fn, 0);
	_lua_setglobal(L, name);
}

void LuaCore::createTable(lua_State *L, int narr, int nrec) {
	if (_loaded && L) {
		_lua_createtable(L, narr, nrec);
	}
}

void LuaCore::setFieldFunction(lua_State *L, const char *key, LuaCFunction fn) {
	if (!_loaded || !L) return;
	_lua_pushcclosure(L, fn, 0);
	_lua_setfield(L, -2, key);
}

void LuaCore::setFieldString(lua_State *L, const char *key, const QString &val) {
	if (!_loaded || !L) return;
	const QByteArray utf8 = val.toUtf8();
	_lua_pushstring(L, utf8.constData());
	_lua_setfield(L, -2, key);
}

void LuaCore::setFieldNumber(lua_State *L, const char *key, double val) {
	if (!_loaded || !L) return;
	_lua_pushnumber(L, val);
	_lua_setfield(L, -2, key);
}

void LuaCore::setFieldInteger(lua_State *L, const char *key, int64_t val) {
	if (!_loaded || !L) return;
	_lua_pushinteger(L, val);
	_lua_setfield(L, -2, key);
}

void LuaCore::setFieldBoolean(lua_State *L, const char *key, bool val) {
	if (!_loaded || !L) return;
	_lua_pushboolean(L, val ? 1 : 0);
	_lua_setfield(L, -2, key);
}

void LuaCore::setGlobal(lua_State *L, const char *name) {
	if (_loaded && L) {
		_lua_setglobal(L, name);
	}
}

bool LuaCore::getGlobal(lua_State *L, const char *name) {
	if (!_loaded || !L) return false;
	return _lua_getglobal(L, name) != 0;
}

bool LuaCore::getField(lua_State *L, int idx, const char *key) {
	if (!_loaded || !L) return false;
	return _lua_getfield(L, idx, key) != 0;
}

bool LuaCore::isFunction(lua_State *L, int idx) {
	if (!_loaded || !L) return false;
	return _lua_type(L, idx) == 6; // LUA_TFUNCTION
}

bool LuaCore::isTable(lua_State *L, int idx) {
	if (!_loaded || !L) return false;
	return _lua_type(L, idx) == 5; // LUA_TTABLE
}

void LuaCore::setField(lua_State *L, int idx, const char *key) {
	if (_loaded && _lua_setfield && L) {
		_lua_setfield(L, idx, key);
	}
}

int LuaCore::type(lua_State *L, int idx) {
	if (!_loaded || !L || !_lua_type) return 0;
	return _lua_type(L, idx);
}

bool LuaCore::next(lua_State *L, int idx) {
	if (!_loaded || !L || !_lua_next) return false;
	return _lua_next(L, idx) != 0;
}

int LuaCore::ref(lua_State *L) {
	if (_loaded && _luaL_ref && L) {
		return _luaL_ref(L, -1001000); // LUA_REGISTRYINDEX
	}
	return -1;
}

void LuaCore::unref(lua_State *L, int r) {
	if (_loaded && _luaL_unref && L && r >= 0) {
		_luaL_unref(L, -1001000, r);
	}
}

void LuaCore::pushRef(lua_State *L, int r) {
	if (_loaded && _lua_rawgeti && L && r >= 0) {
		_lua_rawgeti(L, -1001000, r);
	}
}

void LuaCore::pushJsonValue(lua_State *L, const QJsonValue &val) {
	switch (val.type()) {
	case QJsonValue::Null:
	case QJsonValue::Undefined:
		pushNil(L);
		break;
	case QJsonValue::Bool:
		pushBoolean(L, val.toBool());
		break;
	case QJsonValue::Double:
		pushNumber(L, val.toDouble());
		break;
	case QJsonValue::String:
		pushString(L, val.toString());
		break;
	case QJsonValue::Array: {
		const auto arr = val.toArray();
		createTable(L, arr.size(), 0);
		for (int i = 0; i < arr.size(); ++i) {
			pushJsonValue(L, arr[i]);
			if (_loaded && _lua_rawseti) {
				_lua_rawseti(L, -2, i + 1);
			} else {
				pop(L, 1);
			}
		}
		break;
	}
	case QJsonValue::Object: {
		const auto obj = val.toObject();
		createTable(L, 0, obj.size());
		for (auto it = obj.begin(); it != obj.end(); ++it) {
			pushJsonValue(L, it.value());
			setField(L, -2, it.key().toUtf8().constData());
		}
		break;
	}
	}
}

QJsonValue LuaCore::toJsonValue(lua_State *L, int idx) {
	const int t = type(L, idx);
	switch (t) {
	case 0: // LUA_TNIL
		return QJsonValue(QJsonValue::Null);
	case 1: // LUA_TBOOLEAN
		return QJsonValue(toBoolean(L, idx));
	case 3: // LUA_TNUMBER
		return QJsonValue(toNumber(L, idx));
	case 4: // LUA_TSTRING
		return QJsonValue(toString(L, idx));
	case 5: { // LUA_TTABLE
		QJsonObject obj;
		pushNil(L);
		const int actualIdx = (idx < 0) ? (idx - 1) : idx;
		while (next(L, actualIdx)) {
			const QString key = toString(L, -2);
			const QJsonValue v = toJsonValue(L, -1);
			obj[key] = v;
			pop(L, 1);
		}
		return obj;
	}
	default:
		return QJsonValue(toString(L, idx));
	}
}

void LuaCore::pushNil(lua_State *L) {
	if (_loaded && L) _lua_pushnil(L);
}

void LuaCore::pushString(lua_State *L, const QString &str) {
	if (!_loaded || !L) return;
	const QByteArray utf8 = str.toUtf8();
	_lua_pushlstring(L, utf8.constData(), utf8.size());
}

void LuaCore::pushNumber(lua_State *L, double val) {
	if (_loaded && L) _lua_pushnumber(L, val);
}

void LuaCore::pushInteger(lua_State *L, int64_t val) {
	if (_loaded && L) _lua_pushinteger(L, val);
}

void LuaCore::pushBoolean(lua_State *L, bool val) {
	if (_loaded && L) _lua_pushboolean(L, val ? 1 : 0);
}

void LuaCore::pushValue(lua_State *L, int idx) {
	if (_loaded && L) _lua_pushvalue(L, idx);
}

QString LuaCore::toString(lua_State *L, int idx) {
	if (!_loaded || !L) return QString();
	size_t len = 0;
	const char *str = _lua_tolstring(L, idx, &len);
	return str ? QString::fromUtf8(str, static_cast<int>(len)) : QString();
}

int64_t LuaCore::toInteger(lua_State *L, int idx) {
	if (!_loaded || !L) return 0;
	int isnum = 0;
	return _lua_tointegerx(L, idx, &isnum);
}

double LuaCore::toNumber(lua_State *L, int idx) {
	if (!_loaded || !L) return 0.0;
	int isnum = 0;
	return _lua_tonumberx(L, idx, &isnum);
}

bool LuaCore::toBoolean(lua_State *L, int idx) {
	if (!_loaded || !L) return false;
	return _lua_toboolean(L, idx) != 0;
}

bool LuaCore::pcall(lua_State *L, int nargs, int nresults, QString &error) {
	if (!_loaded || !L) return false;
	if (_lua_pcallk(L, nargs, nresults, 0, 0, nullptr) != 0) {
		error = toString(L, -1);
		pop(L, 1);
		return false;
	}
	return true;
}

void LuaCore::pop(lua_State *L, int n) {
	if (_loaded && L) {
		_lua_settop(L, -n - 1);
	}
}

int LuaCore::getTop(lua_State *L) {
	if (!_loaded || !L) return 0;
	return _lua_gettop(L);
}

} // namespace Plugins
