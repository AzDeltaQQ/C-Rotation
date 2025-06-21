#pragma once

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <cstdint>
#include <chrono>
#include <atomic>

#include "../types/types.h"     // Use types defined in our project
#include "../types/wowobject.h" // Use objects defined in our project
#include "../types/WowPlayer.h" // ADDED: Full definition for WowPlayer needed for std::shared_ptr<WowPlayer> members and methods

// Forward declare GameStateManager to use its GetInstance() method in IsInitialized()
// class GameStateManager; // <<< REMOVED FORWARD DECLARATION

// --- Constants (Based on WoWBot 3.3.5a - VERIFY THESE FOR YOUR VERSION) --- 
namespace GameOffsets {
    constexpr uintptr_t STATIC_CLIENT_CONNECTION = 0x00C79CE0;
    constexpr uintptr_t OBJECT_MANAGER_OFFSET    = 0x2ED0;
    constexpr uintptr_t OBJECT_TYPE_OFFSET       = 0x14;
    constexpr uintptr_t CURRENT_TARGET_GUID_ADDR = 0x00BD07B0;
    constexpr uintptr_t LOCAL_GUID_OFFSET        = 0xC0; // Added offset for direct read
    constexpr uintptr_t IS_IN_WORLD_ADDR         = 0x00B6AA38; // Added game state check
    constexpr uintptr_t ENUM_VISIBLE_OBJECTS_ADDR = 0x004D4B30; // From disassembly
    constexpr uintptr_t GET_OBJECT_BY_GUID_INNER_ADDR = 0x004D4BB0; // From disassembly (findObjectByIdAndData)
    constexpr uintptr_t GET_LOCAL_PLAYER_GUID_ADDR = 0x0; // Set to 0, we will attempt direct read first
    constexpr uintptr_t WORLD_LOADED_FLAG_ADDR = 0x00BEBA40; // Seems to be 1 when world is loaded/loading textures
    constexpr uintptr_t PLAYER_IS_LOOTING_OFFSET = 0x18E8; // Offset from player base for looting status (Byte: 1 if looting, 0 if not)
}

// Callback for EnumVisibleObjects
typedef int(__cdecl* EnumVisibleObjectsCallback)(uint32_t guid_low, uint32_t guid_high, int callback_arg);

// EnumVisibleObjects function itself
typedef int(__cdecl* EnumVisibleObjectsFn)(EnumVisibleObjectsCallback callback, int callback_arg);

// GetObjectPtrByGuid (Inner function, often __thiscall)
typedef void* (__thiscall* GetObjectPtrByGuidInnerFn)(void* thisptr, uint32_t guid_low, WGUID* pGuidStruct);

// GetLocalPlayerGuid function (Assuming global function pointer)
typedef uint64_t(__cdecl* GetLocalPlayerGuidFn)();

class ObjectManager {
private:
    // Singleton instance
    static ObjectManager* s_instance;
    static std::mutex s_instanceMutex; // Mutex to protect instance creation

    // Pointers to WoW memory/functions
    EnumVisibleObjectsFn m_enumVisibleObjects;
    GetObjectPtrByGuidInnerFn m_getObjectPtrByGuidInner;
    GetLocalPlayerGuidFn m_getLocalPlayerGuidFn; // Can be null if reading address directly

    // Cache of objects (Using WGUID as key, matches WoWBot)
    std::map<WGUID, std::shared_ptr<WowObject>> m_objectCache;
    mutable std::mutex m_cacheMutex;
    std::shared_ptr<WowPlayer> m_cachedLocalPlayer;
    WGUID m_localPlayerGuid; // Added missing member
    
    // Callback for enumeration
    static int __cdecl EnumObjectsCallback(uint32_t guid_low, uint32_t guid_high, int callback_arg);
    
    // Actual pointer to the game's object manager structure
    ObjectManagerActual* m_objectManagerPtr; 
    
    // Initialization state
    std::atomic<bool> m_isFullyInitialized; // Flag indicating if OM pointer is valid
    bool m_funcPtrsInitialized; // Flag indicating if function pointers are set
    std::atomic<bool> m_isActive; // NEW: Flag indicating if the OM is active and safe to use
    
    // --- Throttling --- 
    std::chrono::steady_clock::time_point m_lastUpdateTime; // RESTORE: Timestamp of the last successful update

    // --- Background Threading (REMOVED) ---
    // std::thread m_updateThread; 
    // std::atomic<bool> m_stopThread; 
    // std::atomic<bool> m_threadRunning; 
    // void BackgroundUpdateLoop(); 
    // ---------------------------------------

    // Private constructor for singleton
    ObjectManager();

    // Helper to process found objects
    void ProcessFoundObject(WGUID guid, void* objectPtr);
    
    // --- Memory Reading Helpers (Private) ---
    // These wrap Memory::Read with basic checks and logging, using member offsets if needed
    WGUID ReadGUID(uintptr_t baseAddress, uintptr_t offset);
    WowObjectType ReadObjectType(uintptr_t baseAddress, uintptr_t offset);
    uintptr_t ReadObjectBaseAddress(uintptr_t entryAddress); // Reads descriptor pointer
    template <typename T>
    T ReadDescriptorField(uintptr_t baseAddress, uint32_t fieldOffset); // Assumes baseAddress is descriptor ptr
    // -----------------------------------------

    // Helper for GetLocalPlayer to avoid re-locking mutex (if needed)
    std::shared_ptr<WowObject> GetObjectByGUID_locked(WGUID guid);

    // Helper to convert uint64 to WGUID
    static WGUID Guid64ToWGUID(uint64_t guid64) { return WGUID(guid64); }
    static uint64_t WGUIDToGuid64(WGUID wguid) { return wguid.ToUint64(); }

public:
    ~ObjectManager();

    // Prevent copying
    ObjectManager(const ObjectManager&) = delete;
    ObjectManager& operator=(const ObjectManager&) = delete;
    
    // Get singleton instance (thread-safe)
    static ObjectManager* GetInstance();
    
    // Static shutdown method (cleans up singleton)
    static void Shutdown(); 

    // Reset internal state (e.g., for relog)
    void ResetState();

    // Initialize function pointers (can be called early)
    // Returns true if addresses seem valid (non-null)
    bool InitializeFunctions(uintptr_t enumVisibleObjectsAddr, uintptr_t getObjectPtrByGuidInnerAddr, uintptr_t getLocalPlayerGuidAddr);
    
    // Try to finish initialization by reading the OM pointer (call periodically until it returns true)
    // REMOVED: This now also starts the background thread if initialization is successful.
    bool TryFinishInitialization(); 
    
    // Check if fully initialized (OM pointer valid AND game in suitable state)
    bool IsInitialized() const; // DECLARATION ONLY

    // Check if the background thread is running (REMOVED)
    // bool IsUpdateThreadRunning() const { return m_threadRunning; }
    
    // Update object cache (enumerates objects)
    void Update();
    
    // Refresh cached player pointer (needs to be called after Update)
    void RefreshLocalPlayerCache();
    
    // --- Game State Checks ---
    bool IsPlayerInWorld() const; // New method

    // --- Object Accessors (Using WGUID like WoWBot) --- 
    std::shared_ptr<WowObject> GetObjectByGUID(WGUID guid);
    std::shared_ptr<WowObject> GetObjectByGUID(uint64_t guid64); // Convenience overload
    std::vector<std::shared_ptr<WowObject>> GetObjectsByType(WowObjectType type);
    std::shared_ptr<WowPlayer> GetLocalPlayer();
    
    // Get all objects (returns a copy for thread safety)
    std::map<WGUID, std::shared_ptr<WowObject>> GetAllObjects(); 
    // Const version (if needed)
    // std::map<WGUID, std::shared_ptr<WowObject>> GetAllObjects() const;
    
    // Find objects by name (case-insensitive search)
    std::vector<std::shared_ptr<WowObject>> FindObjectsByName(const std::string& name);
    
    // Get nearest object of a specific type
    std::shared_ptr<WowObject> GetNearestObject(WowObjectType type, float maxDistance = 100.0f);

    // Get objects within a certain distance from a point
    std::vector<std::shared_ptr<WowObject>> GetObjectsWithinDistance(const Vector3& center, float distance);
    
    // Get the GUID of the currently targeted unit (reads from global variable)
    uint64_t GetCurrentTargetGUID() const;

    // Returns the underlying pointer to the actual game object manager
    ObjectManagerActual* GetInternalObjectManagerPtr() const; 

    // Getters for cached information
    std::vector<std::shared_ptr<WowObject>> GetAllObjects() const;
    std::vector<std::shared_ptr<WowUnit>> GetAllUnits() const;
    std::vector<std::shared_ptr<WowPlayer>> GetAllPlayers() const;
    std::vector<std::shared_ptr<WowGameObject>> GetAllGameObjects() const;
    std::shared_ptr<WowObject> GetObjectByGuid(const WGUID& guid) const;
    std::shared_ptr<WowUnit> GetUnitByGuid(const WGUID& guid) const;
    std::shared_ptr<WowPlayer> GetPlayerByGuid(const WGUID& guid) const;
    std::shared_ptr<WowGameObject> GetGameObjectByGuid(const WGUID& guid) const;
    std::shared_ptr<WowPlayer> GetLocalPlayer() const;
    WGUID GetLocalPlayerGuid() const; // ADDED Getter

    // New method to count units in melee range
    int CountUnitsInMeleeRange(std::shared_ptr<WowUnit> centerUnit, float range = 5.0f, bool includeHostile = true, bool includeFriendly = false, bool includeNeutral = false);
    int CountUnitsInFrontalCone(std::shared_ptr<WowUnit> caster, float range, float coneAngleDegrees, bool includeHostile = true, bool includeFriendly = false, bool includeNeutral = false);
}; 