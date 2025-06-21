#pragma once

#include <cstdint>
#include <string>
#include <mutex>
// Removed: No direct dependency on log.h or memory.h in the header itself
// #include "../logs/log.h"     // For Core::Log
// #include "../utils/memory.h" // For Memory::Read

namespace GameStateOffsets {
    constexpr uintptr_t WorldLoadedAddr = 0x00BEBA40; // dd (DWORD) - bool or uint32_t
    constexpr uintptr_t GameStateArrayAddr = 0x00B6A9E0; // db 40h dup(?) (char[64])
    constexpr uintptr_t IsLoadingAddr = 0x00B6AA38;     // dd (DWORD) - uint32_t
}

class GameStateManager {
public:
    static GameStateManager& GetInstance();

    GameStateManager(const GameStateManager&) = delete;
    GameStateManager& operator=(const GameStateManager&) = delete;

    void Update();

    // Raw Value Getters (primarily for display/debug)
    bool GetRawWorldLoadedFlag() const; // True if raw value is non-zero
    uint32_t GetRawIsLoadingValue() const;
    std::string GetRawGameStateString() const;
    uint32_t GetRawWorldLoadedDword() const; // Actual DWORD value

    // Interpreted State Getters
    bool IsFullyInWorld() const;       // WorldLoaded = TRUE AND IsLoading = 0
    bool IsAtLoginScreen() const;
    bool IsAtCharSelectScreen() const;
    bool IsLoadingScreen() const;      // Covers various loading states (2, 3, 0xA)
    bool IsLoggingOut() const;         // IsLoading = 3
    bool IsLoadingToCharSelect() const; // IsLoading = 2
    bool IsLoadingIntoWorld() const;   // IsLoading = 0xA (or 0x10 if confirmed)

private:
    GameStateManager();
    ~GameStateManager();

    uint32_t m_rawWorldLoadedDwordValue; // Stores the actual DWORD from memory
    // bool m_worldLoaded_Interpreted; // Replaced by GetRawWorldLoadedFlag & IsFullyInWorld
    uint32_t m_isLoadingValue;
    std::string m_gameStateString; // Max 63 chars + null terminator

    // For logging changes to avoid spam
    uint32_t m_lastLoggedRawWorldLoadedDword;
    uint32_t m_lastLoggedIsLoading;
    std::string m_lastLoggedGameStateString;

    mutable std::mutex m_mutex;

    static GameStateManager* s_instance;
    static std::mutex s_instanceMutex;
}; 