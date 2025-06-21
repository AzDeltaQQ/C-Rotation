#pragma once

#include <cstdint>

// Define function pointer types matching Lua C API signatures (adjust as needed)
// Note: These signatures are guesses based on common Lua C API patterns.
// You might need to adjust them based on the actual function prototypes if known.

// Represents the Lua state
using lua_State = struct lua_State_dummy*; // Opaque struct pointer

// Function pointer types
using tLuaLoadBuffer = int(*)(lua_State *L, const char *buff, size_t sz, const char *name);
using tLuaPCall = int(*)(lua_State *L, int nargs, int nresults, int errfunc);
using tLuaGetTop = int(*)(lua_State *L);
using tLuaSetTop = void(*)(lua_State *L, int idx);
using tLuaType = int(*)(lua_State *L, int idx);
using tLuaToNumber = double(*)(lua_State *L, int idx); // Lua numbers are doubles
using tLuaToLString = const char*(*)(lua_State *L, int idx, size_t *len);
using tLuaToBoolean = int(*)(lua_State *L, int idx); // Lua booleans map to integers (0 or 1)
using tLuaDoString = int(*)(lua_State* L, const char* s); // Common signature, might vary
using tLuaGetLocalizedText = const char* (*)(lua_State* L, const char* key, int gender); // Guessed signature
using tLuaGetGlobal = void(*)(lua_State* L, const char* name); // Signature for LuaGetGlobalOrPushNil (assumes it pushes value/nil)
using tLuaRemove = void(*)(lua_State* L, int index); // Signature for removeFrameComponent
using tLuaPushCClosure = void(*)(lua_State* L, void* func, int n); // Signature for createAndCopyFrameScriptClosure (func type might need adjustment, n=upvalues)
using tLuaPushString = void(*)(lua_State* L, const char* s); // Signature for pushStringOrDefault
using tLuaPushNumber = void(*)(lua_State* L, double n);      // Signature for pushFrameScriptNumber (likely lua_pushnumber)
using tLuaPushBoolean = void(*)(lua_State* L, int b);         // Signature for pushBooleanFrameScriptValue (likely lua_pushboolean)
using tLuaPushNil = void(*)(lua_State* L);                    // Signature for pushNilValue (likely lua_pushnil)
using tLuaNext = int(*)(lua_State* L, int idx);               // Signature for lua_next
using tLuaRawGet = void(*)(lua_State* L, int idx);            // Signature for luaRawGetHelper (likely lua_rawget)
using tLuaRawSet = void(*)(lua_State* L, int idx);            // Signature for lua_rawset
using tLuaGetMetaTable = int(*)(lua_State* L, int idx);       // Signature for lua_getmetatable
using tLuaSetMetaTable = int(*)(lua_State* L, int idx);       // Signature for lua_setmetatable
using tLuaCreateTable = void(*)(lua_State* L, int narr, int nrec); // Signature for AppendFrameScriptTableEntry (likely lua_createtable)
using tLuaGetTable = void(*)(lua_State* L, int idx);           // Signature for fetchAndAssignTableEntry (likely lua_gettable)
using tLuaSetField = void(*)(lua_State* L, int idx, const char* k); // Signature for addScriptObjectToTable (likely lua_setfield)

// Lua Function Addresses (WoW 3.3.5a 12340 - Absolute)
// constexpr uintptr_t BASE_ADDRESS = 0x0; // No longer needed for in-process injection

// Global Lua State Pointer Address
// This is the absolute address where the lua_State* is stored.
constexpr uintptr_t LuaStateAddr = 0x00D3F78C;

// Function Addresses (Absolute)
constexpr uintptr_t LuaLoadBufferAddr     = 0x0084F860;
constexpr uintptr_t LuaPCallAddr          = 0x0084EC50;
constexpr uintptr_t LuaGetTopAddr         = 0x0084DBD0;
constexpr uintptr_t LuaSetTopAddr         = 0x0084DBF0;
constexpr uintptr_t LuaTypeAddr           = 0x0084DEB0;
constexpr uintptr_t LuaToNumberAddr       = 0x0084E030;
constexpr uintptr_t LuaToLStringAddr      = 0x0084E0E0;
constexpr uintptr_t LuaToBooleanAddr      = 0x0084E0B0;
constexpr uintptr_t LuaDoStringAddr       = 0x00819210;
constexpr uintptr_t LuaGetLocalizedTextAddr = 0x007225E0;
constexpr uintptr_t LuaGetGlobalAddr        = 0x00816910; // Address for LuaGetGlobalOrPushNil
constexpr uintptr_t LuaRemoveAddr           = 0x0084DC50; // Address for removeFrameComponent
constexpr uintptr_t LuaPushCClosureAddr     = 0x0084E400; // Address for createAndCopyFrameScriptClosure
constexpr uintptr_t LuaPushStringAddr       = 0x0084E350; // Address for pushStringOrDefault
constexpr uintptr_t LuaPushNumberAddr       = 0x0084E2A0; // Address for pushFrameScriptNumber (likely lua_pushnumber)
constexpr uintptr_t LuaPushBooleanAddr      = 0x0084E4D0; // Address for pushBooleanFrameScriptValue (likely lua_pushboolean)
constexpr uintptr_t LuaPushNilAddr          = 0x0084E280; // Address for pushNilValue (likely lua_pushnil)
constexpr uintptr_t LuaNextAddr             = 0x00854690; // Address for lua_next
constexpr uintptr_t LuaRawGetAddr           = 0x00854510; // Address for luaRawGetHelper (likely lua_rawget)
constexpr uintptr_t LuaRawSetAddr           = 0x00854550; // Address for lua_rawset
constexpr uintptr_t LuaGetMetaTableAddr     = 0x00854270; // Address for lua_getmetatable
constexpr uintptr_t LuaSetMetaTableAddr     = 0x008542C0; // Address for lua_setmetatable
constexpr uintptr_t LuaCreateTableAddr      = 0x0084E6E0; // Address for AppendFrameScriptTableEntry (likely lua_createtable)
constexpr uintptr_t LuaGetTableAddr         = 0x0084E560; // Address for fetchAndAssignTableEntry (likely lua_gettable)
constexpr uintptr_t LuaSetFieldAddr         = 0x0084E590; // Address for addScriptObjectToTable (likely lua_setfield)


// Function Pointers (to be initialized)
inline tLuaLoadBuffer LuaLoadBuffer = reinterpret_cast<tLuaLoadBuffer>(LuaLoadBufferAddr);
inline tLuaPCall LuaPCall = reinterpret_cast<tLuaPCall>(LuaPCallAddr);
inline tLuaGetTop LuaGetTop = reinterpret_cast<tLuaGetTop>(LuaGetTopAddr);
inline tLuaSetTop LuaSetTop = reinterpret_cast<tLuaSetTop>(LuaSetTopAddr);
inline tLuaType LuaType = reinterpret_cast<tLuaType>(LuaTypeAddr);
inline tLuaToNumber LuaToNumber = reinterpret_cast<tLuaToNumber>(LuaToNumberAddr);
inline tLuaToLString LuaToLString = reinterpret_cast<tLuaToLString>(LuaToLStringAddr);
inline tLuaToBoolean LuaToBoolean = reinterpret_cast<tLuaToBoolean>(LuaToBooleanAddr);
inline tLuaDoString LuaDoString = reinterpret_cast<tLuaDoString>(LuaDoStringAddr);
inline tLuaGetLocalizedText LuaGetLocalizedText = reinterpret_cast<tLuaGetLocalizedText>(LuaGetLocalizedTextAddr);
inline tLuaGetGlobal LuaGetGlobal = reinterpret_cast<tLuaGetGlobal>(LuaGetGlobalAddr);
inline tLuaRemove LuaRemove = reinterpret_cast<tLuaRemove>(LuaRemoveAddr);
inline tLuaPushCClosure LuaPushCClosure = reinterpret_cast<tLuaPushCClosure>(LuaPushCClosureAddr);
inline tLuaPushString LuaPushString = reinterpret_cast<tLuaPushString>(LuaPushStringAddr);
inline tLuaPushNumber LuaPushNumber = reinterpret_cast<tLuaPushNumber>(LuaPushNumberAddr);
inline tLuaPushBoolean LuaPushBoolean = reinterpret_cast<tLuaPushBoolean>(LuaPushBooleanAddr);
inline tLuaPushNil LuaPushNil = reinterpret_cast<tLuaPushNil>(LuaPushNilAddr);
inline tLuaNext LuaNext = reinterpret_cast<tLuaNext>(LuaNextAddr);
inline tLuaRawGet LuaRawGet = reinterpret_cast<tLuaRawGet>(LuaRawGetAddr);
inline tLuaRawSet LuaRawSet = reinterpret_cast<tLuaRawSet>(LuaRawSetAddr);
inline tLuaGetMetaTable LuaGetMetaTable = reinterpret_cast<tLuaGetMetaTable>(LuaGetMetaTableAddr);
inline tLuaSetMetaTable LuaSetMetaTable = reinterpret_cast<tLuaSetMetaTable>(LuaSetMetaTableAddr);
inline tLuaCreateTable LuaCreateTable = reinterpret_cast<tLuaCreateTable>(LuaCreateTableAddr);
inline tLuaGetTable LuaGetTable = reinterpret_cast<tLuaGetTable>(LuaGetTableAddr);
inline tLuaSetField LuaSetField = reinterpret_cast<tLuaSetField>(LuaSetFieldAddr);

// Helper function to get the current Lua state
// Reads the pointer directly from the absolute address LuaStateAddr.
inline lua_State* GetLuaState() {
    // Treat LuaStateAddr as the memory location holding the lua_State pointer
    // Cast it to a pointer-to-pointer-to-lua_State and dereference it once.
    return *reinterpret_cast<lua_State**>(LuaStateAddr);
}

// Lua Type Constants (standard Lua)
#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8 