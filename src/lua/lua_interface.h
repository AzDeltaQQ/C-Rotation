#pragma once

#include <string>
#include <vector>
#include <optional>

namespace LuaInterface {

    // Executes a Lua string.
    // Returns true on success, false on error.
    bool DoString(const std::string& luaCode);

    // Calls a Lua function by name with arguments and retrieves results.
    // functionName: The global name of the Lua function to call.
    // args: A vector of strings to be pushed as arguments.
    // expectedResults: The number of results expected on the Lua stack.
    // Returns a vector of strings representing the results, or std::nullopt on error.
    std::optional<std::vector<std::string>> CallFunction(const std::string& functionName, const std::vector<std::string>& args, int expectedResults = 0);

    // Gets a localized string from the game's localization system via Lua.
    // key: The localization key.
    // gender: An optional gender specifier (depends on game implementation).
    // Returns the localized string, or std::nullopt if not found or on error.
    std::optional<std::string> GetLocalizedText(const std::string& key, int gender = 0); // Default gender might vary

    // --- Add more wrapper functions as needed for other Lua API calls ---
    // Example:
    // std::optional<double> GetNumber(const std::string& globalVarName);
    // std::optional<std::string> GetString(const std::string& globalVarName);
    // bool GetBoolean(const std::string& globalVarName);

} 