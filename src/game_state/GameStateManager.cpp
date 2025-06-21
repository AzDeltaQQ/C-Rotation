#include "GameStateManager.h"
#include "../logs/log.h"     // For Core::Log (needed for implementation)
#include "../utils/memory.h" // For Memory::Read (needed for implementation)

GameStateManager* GameStateManager::s_instance = nullptr;
std::mutex GameStateManager::s_instanceMutex;

GameStateManager& GameStateManager::GetInstance() {
    if (!s_instance) {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        if (!s_instance) {
            s_instance = new GameStateManager();
        }
    }
    return *s_instance;
}

GameStateManager::GameStateManager()
    : m_rawWorldLoadedDwordValue(0), m_isLoadingValue(0), m_gameStateString("Uninitialized"),
      m_lastLoggedRawWorldLoadedDword(~0U), m_lastLoggedIsLoading(~0U), m_lastLoggedGameStateString("") {}

GameStateManager::~GameStateManager() {
    // Core::Log::Message("[GameStateManager] Destroyed.");
}

void GameStateManager::Update() {
    uint32_t currentRawWorldLoadedDword = 0;
    uint32_t currentIsLoading = 0;
    std::string currentGameStateString = "Error";
    char gameStateBuffer[64] = {0};

    try {
        currentRawWorldLoadedDword = Memory::Read<uint32_t>(GameStateOffsets::WorldLoadedAddr);
    } catch (const MemoryAccessError& e) {
        static int errorCount = 0;
        if (errorCount++ % 100 == 0) {
             Core::Log::Message("[GameStateManager] Error reading WorldLoadedAddr: " + std::string(e.what()));
        }
        currentRawWorldLoadedDword = 0;
    }

    try {
        currentIsLoading = Memory::Read<uint32_t>(GameStateOffsets::IsLoadingAddr);
    } catch (const MemoryAccessError& e) {
        static int errorCount = 0;
        if (errorCount++ % 100 == 0) {
            Core::Log::Message("[GameStateManager] Error reading IsLoadingAddr: " + std::string(e.what()));
        }
        currentIsLoading = 0;
    }

    try {
        bool readSuccess = true;
        for (size_t i = 0; i < sizeof(gameStateBuffer) - 1; ++i) {
            gameStateBuffer[i] = Memory::Read<char>(GameStateOffsets::GameStateArrayAddr + i);
            if (gameStateBuffer[i] == '\0') { break; }
        }
        gameStateBuffer[sizeof(gameStateBuffer) - 1] = '\0';
        currentGameStateString = std::string(gameStateBuffer);
        if (strlen(gameStateBuffer) == 0 && readSuccess) { /* Empty is fine */ }
    } catch (const MemoryAccessError& e) {
        static int errorCount = 0;
        if (errorCount++ % 100 == 0) {
            Core::Log::Message("[GameStateManager] Error reading GameStateArrayAddr: " + std::string(e.what()));
        }
        currentGameStateString = "Exception reading GameState";
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_rawWorldLoadedDwordValue = currentRawWorldLoadedDword;
    m_isLoadingValue = currentIsLoading;
    m_gameStateString = currentGameStateString;
}

// Raw Value Getters
bool GameStateManager::GetRawWorldLoadedFlag() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rawWorldLoadedDwordValue != 0;
}

uint32_t GameStateManager::GetRawIsLoadingValue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isLoadingValue;
}

std::string GameStateManager::GetRawGameStateString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gameStateString;
}

uint32_t GameStateManager::GetRawWorldLoadedDword() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rawWorldLoadedDwordValue;
}

// Interpreted State Getters
bool GameStateManager::IsFullyInWorld() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // As per user request, only the WorldLoaded flag matters.
    // 0x1 means loaded, 0x0 means not loaded.
    return m_rawWorldLoadedDwordValue != 0;
}

bool GameStateManager::IsAtLoginScreen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // GameState string comparison might be case-sensitive.
    return m_gameStateString == "login"; // Assuming "login" is the exact string
}

bool GameStateManager::IsAtCharSelectScreen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Based on your screenshot, WorldLoaded can be true at charselect
    return m_rawWorldLoadedDwordValue != 0 && m_isLoadingValue == 0 && m_gameStateString == "charselect";
}

bool GameStateManager::IsLoadingScreen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Covers states: 2 (loading to char select), 3 (logging out), 0xA (loading into world)
    return m_isLoadingValue == 2 || m_isLoadingValue == 3 || m_isLoadingValue == 0xA || m_isLoadingValue == 0x10;
}

bool GameStateManager::IsLoggingOut() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isLoadingValue == 3;
}

bool GameStateManager::IsLoadingToCharSelect() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isLoadingValue == 2;
}

bool GameStateManager::IsLoadingIntoWorld() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // You mentioned IsLoading changes to 0x10 (16 decimal) when logging into world
    // So, I'll use that here. If it's 0xA (10 decimal), we can change this back.
    return m_isLoadingValue == 0xA || m_isLoadingValue == 0x10; 
} 