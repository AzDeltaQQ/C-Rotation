#include "lua_interface.h"
#include "lua_types.h"
#include <iostream> // For error logging (replace with your logging solution)


// Basic error handler function (can be registered with lua_pcall)
// Pops the error message from the stack and prints it.
static int BasicErrorHandler(lua_State *L) {
    const char *msg = LuaToLString(L, -1, NULL);
    if (msg == nullptr) {
        msg = "(error object is not a string)";
    }
    std::cerr << "Lua Error: " << msg << std::endl;
    // Potentially add stack traceback here if needed
    LuaSetTop(L, -2); // Remove error message from stack
    return 1; // Indicate error handled (though pcall will still return error code)
}

namespace LuaInterface {

    bool DoString(const std::string& luaCode) {
        lua_State* L = GetLuaState();
        if (!L) {
            std::cerr << "DoString Error: Failed to get Lua state." << std::endl;
            return false;
        }

        // Load the string
        if (LuaLoadBuffer(L, luaCode.c_str(), luaCode.length(), "DoStringChunk") != 0) {
            // Error loading chunk (e.g., syntax error)
            size_t len;
            const char* errorMsg = LuaToLString(L, -1, &len);
            std::cerr << "LuaLoadBuffer Error: " << (errorMsg ? errorMsg : "Unknown") << std::endl;
            LuaSetTop(L, -2); // Pop error message
            return false;
        }

        // Execute the loaded chunk
        // Push the error handler onto the stack
        // lua_pushcfunction(L, BasicErrorHandler); // Need Lua_PushCFunction address
        // int errFuncIndex = LuaGetTop(L); // Get index of error handler

        // Note: Using 0 for errfunc index as we don't have pushcfunction yet
        if (LuaPCall(L, 0, 0, 0 /* errFuncIndex */) != 0) {
            // Error during execution caught by pcall (or our handler if pushed)
            // Error message is already on the stack if using default pcall error handling
            // If using our handler, it might have already printed it.
             size_t len;
            const char* errorMsg = LuaToLString(L, -1, &len);
            std::cerr << "LuaPCall Error: " << (errorMsg ? errorMsg : "Unknown") << std::endl;
            LuaSetTop(L, -2); // Pop error message
            // lua_remove(L, errFuncIndex); // Remove error handler if pushed
            return false;
        }

        // lua_remove(L, errFuncIndex); // Remove error handler if pushed
        return true;
    }

    // Partially implemented - Requires addresses for lua_getglobal, lua_pushstring,
    // lua_isstring, lua_pushcfunction, and lua_remove for full functionality.
    std::optional<std::vector<std::string>> CallFunction(const std::string& functionName, const std::vector<std::string>& args, int expectedResults) {
         lua_State* L = GetLuaState();
         if (!L) {
             std::cerr << "CallFunction Error: Failed to get Lua state." << std::endl;
             return std::nullopt;
         }

        int initialTop = LuaGetTop(L);

        // Push the error handler
        // Using LuaPushCClosure (createAndCopyFrameScriptClosure) with 0 upvalues
        // Note: Ensure BasicErrorHandler signature matches what LuaPushCClosure expects.
        // It might need to be cast if LuaPushCClosure expects a specific generic pointer type.
        LuaPushCClosure(L, reinterpret_cast<void*>(BasicErrorHandler), 0);
        int errFuncIndex = LuaGetTop(L); // Get index of error handler

        // Get Global Function using the found address
        LuaGetGlobal(L, functionName.c_str());
        // if (LuaType(L, -1) != LUA_TFUNCTION) {
        //     std::cerr << "CallFunction Error: '" << functionName << "' is not a function." << std::endl;
        //     LuaSetTop(L, initialTop); // Clean up stack
        //     return std::nullopt;
        // }

        // Push Arguments
        for (const auto& arg : args) {
            LuaPushString(L, arg.c_str());
        }

        // Call the function using LuaPCall
        // Note: Since we couldn't push the function or args, this call is incorrect
        //       and will likely fail or operate on whatever is on the stack.
        //       Passing args.size() even though args weren't pushed.
        int status = LuaPCall(L, static_cast<int>(args.size()), expectedResults, errFuncIndex);

        if (status != 0) {
            // Error during execution
            size_t len;
            const char* errorMsg = LuaToLString(L, -1, &len);
            std::cerr << "LuaPCall Error (Status " << status << "): " << (errorMsg ? errorMsg : "Unknown") << std::endl;
            // Error message is already popped by pcall if errFuncIndex is 0,
            // or handled by BasicErrorHandler if it were pushed.
            LuaSetTop(L, initialTop); // Ensure stack is restored
            return std::nullopt;
        }

        // Process results
        std::vector<std::string> results;
        int currentTop = LuaGetTop(L);
        // Calculate actual results count, considering the error function was pushed before the target function
        int actualResults = currentTop - (errFuncIndex -1); // errFuncIndex points to the error func, results are above it.

        // We only expect results if expectedResults isn't 0 (or LUA_MULTRET)
        if (expectedResults > 0 && actualResults > 0) {
             results.reserve(std::min(actualResults, expectedResults));
             for (int i = 0; i < std::min(actualResults, expectedResults); ++i) {
                int stackIndex = initialTop + 1 + i;
                // Check type before converting (use constants from lua_types.h)
                if (LuaType(L, stackIndex) == LUA_TSTRING) {
                    size_t len;
                    const char* resStr = LuaToLString(L, stackIndex, &len);
                    if (resStr) {
                        results.emplace_back(resStr, len);
                    }
                } else if (LuaType(L, stackIndex) == LUA_TNUMBER) {
                     double num = LuaToNumber(L, stackIndex);
                     results.emplace_back(std::to_string(num));
                } else if (LuaType(L, stackIndex) == LUA_TBOOLEAN) {
                    results.emplace_back(LuaToBoolean(L, stackIndex) ? "true" : "false");
                } else if (LuaType(L, stackIndex) == LUA_TNIL) {
                    results.emplace_back("nil");
                } else {
                     // Handle other types (table, function, userdata) or add placeholder
                     results.emplace_back("[non-convertible result type]");
                }
             }
        }

        // Remove error handler function from stack if it was pushed and call succeeded
        if (errFuncIndex != 0 && status == 0) {
            LuaRemove(L, errFuncIndex);
        }

        // Restore the stack to its original state if results weren't expected to be left
        // LuaPCall should leave only results, but we restore manually for safety/clarity
        int expectedTop = initialTop + (status == 0 ? results.size() : 0); // Only leave results on success
        LuaSetTop(L, expectedTop); // Restore stack, leaving results if successful
        // A more robust approach might be LuaSetTop(L, initialTop); after copying results.

        return results;
     }

    // Placeholder - needs actual implementation
    std::optional<std::string> GetLocalizedText(const std::string& key, int gender) {
        lua_State* L = GetLuaState();
        if (!L) {
            std::cerr << "GetLocalizedText Error: Failed to get Lua state." << std::endl;
            return std::nullopt;
        }
        const char* result = LuaGetLocalizedText(L, key.c_str(), gender);
        if(result) {
            return std::string(result);
        }
        return std::nullopt; // Not found or error
    }

} 