#include "objectManager.h"
#include "../logs/log.h"         // Use actual logger
#include "../utils/memory.h"   // Use correct memory utility
#include "../types/wowobject.h" 
#include "../types/WowPlayer.h"    // Explicit include
#include "../types/wowunit.h"      // Explicit include
#include "../types/wowgameobject.h"// Explicit include
#include "../types/types.h"     // Use actual type definitions
#include <cstdint> // Ensure standard integer types are included

#include <algorithm> 
#include <sstream>
#include <memory> // Added explicitly
#include <chrono> // Include chrono header
#include <cmath>  // For std::sqrt, std::atan2, std::fabs
#include <vector>
#include "../game_state/GameStateManager.h" // <<< ADDED for game state checks

// Define PI if not using C++20 <numbers>
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_F
    #define M_PI_F 3.1415926535f
#endif

// Initialize static singleton instance and mutex
ObjectManager* ObjectManager::s_instance = nullptr;
std::mutex ObjectManager::s_instanceMutex;

namespace { // Anonymous namespace for constants like the update interval
    const std::chrono::milliseconds UPDATE_INTERVAL(500); // Throttling interval for synchronous update

    // Define Offsets (These should ideally be in a dedicated GameOffsets header)
    // Typical 1.12.1 offsets - VERIFY THESE FOR YOUR TARGET CLIENT
    constexpr uintptr_t OM_GUID_OFFSET = 0x30;
    constexpr uintptr_t OM_TYPE_OFFSET = 0x14;
    constexpr uintptr_t OM_BASE_ADDRESS_OFFSET = 0x8;
    constexpr uintptr_t DESCRIPTOR_OFFSET = 0x8;
}

// --- Helper Functions --- 

// Combine low/high 32-bit parts into a 64-bit GUID (if needed)
// uint64_t CombineGuids(uint32_t low, uint32_t high) {
//     return (static_cast<uint64_t>(high) << 32) | low;
// }

// Helper function to read GUID safely
WGUID ObjectManager::ReadGUID(uintptr_t baseAddress, uintptr_t offset) {
    uintptr_t guidAddress = baseAddress + offset;
    uint64_t guidValue = 0;
    try {
        // Use Memory namespace
        guidValue = Memory::Read<uint64_t>(guidAddress);
    } catch (const MemoryAccessError& e) {
        // Log and return zeroed GUID
        Core::Log::Message(std::string("ReadGUID error: ") + e.what());
        return {}; // Default construct WGUID
    }
    return WGUID(guidValue); // Use WGUID constructor instead of GuidFromUint64
}

// Helper function to read object type safely
WowObjectType ObjectManager::ReadObjectType(uintptr_t baseAddress, uintptr_t offset) {
    uintptr_t typeAddress = baseAddress + offset;
    uint32_t typeValue = 0; // Type is stored as a uint32 (confirmed by OM structure)
    try {
        // Use Memory namespace
        typeValue = Memory::Read<uint32_t>(typeAddress);
    } catch (const MemoryAccessError& e) {
        // Log and return default type
        Core::Log::Message(std::string("ReadObjectType error: ") + e.what());
        return WowObjectType::OBJECT_NONE; // Use OBJECT_NONE instead of OBJECT_UNKNOWN
    }
    // Mask out high bits if necessary (though type is usually just lower bits)
    // typeValue &= 0xFFFF; // Or whatever mask is appropriate if > 7 is used for flags
    if (typeValue >= WowObjectType::OBJECT_TOTAL) { // Basic sanity check
         Core::Log::Message("ReadObjectType warning: Read invalid type value " + std::to_string(typeValue));
         return WowObjectType::OBJECT_NONE;
    }
    return static_cast<WowObjectType>(typeValue);
}

// Helper function to read object base address safely
uintptr_t ObjectManager::ReadObjectBaseAddress(uintptr_t entryAddress) {
    uintptr_t baseAddress = 0;
    try {
        // Use Memory namespace
        // Assuming entryAddress is the pointer to the object structure itself (like in the OM linked list/hash table)
        // And the base address of the actual object data is at a specific offset within that structure.
        baseAddress = Memory::Read<uintptr_t>(entryAddress + DESCRIPTOR_OFFSET); // Read the descriptor pointer
    } catch (const MemoryAccessError& e) {
        // Log and return 0
        Core::Log::Message(std::string("ReadObjectBaseAddress error: ") + e.what());
        return 0;
    }
    return baseAddress;
}

// Helper to read descriptor fields
template <typename T>
T ObjectManager::ReadDescriptorField(uintptr_t baseAddress, uint32_t fieldOffset) {
    // This assumes baseAddress is the START of the descriptor fields array
    // If baseAddress is the object base, you first need to read the descriptor pointer:
    // uintptr_t descriptorPtr = Memory::Read<uintptr_t>(baseAddress + DESCRIPTOR_OFFSET);
    // if (!descriptorPtr) return T{};
    // return Memory::Read<T>(descriptorPtr + fieldOffset);

    // Assuming the function is called with descriptorPtr already resolved:
    try {
        if (!baseAddress) return T{}; // Check if the base (descriptor ptr) is null
        return Memory::Read<T>(baseAddress + fieldOffset);
    } catch (const MemoryAccessError& e) {
        Core::Log::Message(std::string("ReadDescriptorField error: ") + e.what());
        return T{}; // Return default value on error
    }
}

// Constructor (private for singleton)
ObjectManager::ObjectManager() 
    : m_enumVisibleObjects(nullptr),
      m_getObjectPtrByGuidInner(nullptr),
      m_getLocalPlayerGuidFn(nullptr),
      m_objectManagerPtr(nullptr),
      m_isFullyInitialized(false), // atomic
      m_funcPtrsInitialized(false),
      m_isActive(false),           // atomic, NEW
      m_cachedLocalPlayer(nullptr),
      m_localPlayerGuid({}), // Initialize WGUID
      m_lastUpdateTime(std::chrono::steady_clock::now()) // RESTORE Initialize timestamp
{
    // Core::Log::Message("[ObjectManager] Instance created.");
}

// Destructor
ObjectManager::~ObjectManager() {
    // Core::Log::Message("[ObjectManager] Instance destruction requested.");
    // Core::Log::Message("[ObjectManager] Instance destroyed.");
    // Cache is cleared automatically by shared_ptr/map destructor
}

// Get singleton instance (thread-safe)
ObjectManager* ObjectManager::GetInstance() {
    if (!s_instance) {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        if (!s_instance) {
            s_instance = new ObjectManager();
        }
    }
    return s_instance;
}

// Reset internal state
void ObjectManager::ResetState() {
    std::lock_guard<std::mutex> lock(m_cacheMutex); // Use the cache mutex for safety
    // Core::Log::Message("[ObjectManager] Resetting state (clearing cache and player info)...");
    m_objectCache.clear();      // Clear the main object map
    m_localPlayerGuid = WGUID(); // Reset local player GUID
    m_cachedLocalPlayer = nullptr; // Reset cached local player pointer
    m_objectManagerPtr = nullptr; // Force re-acquisition on next TryFinishInitialization
    m_isFullyInitialized.store(false, std::memory_order_release);
    m_isActive.store(false, std::memory_order_release); // Also mark as inactive
    // m_funcPtrsInitialized = false; // Keep function pointers if they were found once

    // Clear any other potentially stale data if needed
    // e.g., if you cache target GUIDs, etc.

    // Core::Log::Message("[ObjectManager] State reset complete.");
}

// Initialize function pointers - RENAMED from Initialize
bool ObjectManager::InitializeFunctions(uintptr_t enumVisibleObjectsAddr, uintptr_t getObjectPtrByGuidInnerAddr, uintptr_t getLocalPlayerGuidAddr) {
    // Core::Log::Message("[ObjectManager] Initializing function pointers...");
    if (m_funcPtrsInitialized) {
        // Core::Log::Message("[ObjectManager] Function pointers already initialized.");
        return true;
    }

    m_enumVisibleObjects = reinterpret_cast<EnumVisibleObjectsFn>(enumVisibleObjectsAddr);
    m_getObjectPtrByGuidInner = reinterpret_cast<GetObjectPtrByGuidInnerFn>(getObjectPtrByGuidInnerAddr);
    m_getLocalPlayerGuidFn = reinterpret_cast<GetLocalPlayerGuidFn>(getLocalPlayerGuidAddr);

    m_funcPtrsInitialized = (m_enumVisibleObjects != nullptr && m_getObjectPtrByGuidInner != nullptr);

    if (m_funcPtrsInitialized) {
        // Core::Log::Message("[ObjectManager] Required function pointers acquired.");
        if (!m_getLocalPlayerGuidFn) {
             // Core::Log::Message("[ObjectManager] Warning: GetLocalPlayerGuid function pointer is null (will attempt direct read if address available).");
        }
    } else {
         // Core::Log::Message("[ObjectManager] ERROR: Failed to acquire required function pointers (EnumVisibleObjects or GetObjectPtrByGuidInner).");
    }
    return m_funcPtrsInitialized;
}

// Static Shutdown method implementation
void ObjectManager::Shutdown() {
    // Core::Log::Message("[ObjectManager] Shutdown requested.");
    std::lock_guard<std::mutex> lock(s_instanceMutex); // Lock instance creation mutex
    if (s_instance) {
        // Clear cache and reset members
        {
             std::lock_guard<std::mutex> cacheLock(s_instance->m_cacheMutex);
             s_instance->m_objectCache.clear(); 
             s_instance->m_cachedLocalPlayer = nullptr;
             s_instance->m_localPlayerGuid = {}; // Reset WGUID
        }
        
        s_instance->m_objectManagerPtr = nullptr;
        s_instance->m_enumVisibleObjects = nullptr;
        s_instance->m_getObjectPtrByGuidInner = nullptr;
        s_instance->m_getLocalPlayerGuidFn = nullptr;
        s_instance->m_isFullyInitialized.store(false, std::memory_order_release);
        s_instance->m_funcPtrsInitialized = false;
        s_instance->m_isActive.store(false, std::memory_order_release); // Ensure inactive on shutdown
                
        delete s_instance;
        s_instance = nullptr;
        // Core::Log::Message("[ObjectManager] Singleton instance deleted.");
    } else {
         // Core::Log::Message("[ObjectManager] Shutdown requested, but no instance exists.");
    }
}

// TryFinishInitialization attempts to read the game pointers
bool ObjectManager::TryFinishInitialization() {
    // --- ADDED: Game State Check --- 
    if (!GameStateManager::GetInstance().IsFullyInWorld()) {
        // Core::Log::Message("[ObjectManager] TryFinishInitialization: Deferred. Not fully in world.");
        m_isActive.store(false, std::memory_order_release); // Ensure inactive if not in world
        return false;
    }
    // --- END Game State Check ---

    if (m_isFullyInitialized.load(std::memory_order_acquire)) {
        // If already fully initialized, ensure isActive is also true (since we passed the IsFullyInWorld check).
        m_isActive.store(true, std::memory_order_release); 
        return true;
    }

    // --- Attempt to read pointers FIRST ---
    ObjectManagerActual* tempObjMgrPtr = nullptr;
    uintptr_t clientConnection = 0;
    bool pointersValid = false;
    try {
        clientConnection = Memory::Read<uintptr_t>(GameOffsets::STATIC_CLIENT_CONNECTION);
        if (!clientConnection) { 
             // Core::Log::Message("[ObjectManager] TryFinishInitialization Attempt Failed: Could not read ClientConnection (Address: 0x" + std::to_string(GameOffsets::STATIC_CLIENT_CONNECTION) + "). Will check world state.");
             // Keep pointersValid as false
        } else {
            // Log ClientConnection value found
            // {
            //     std::stringstream ss; ss << "[ObjectManager] TryFinishInitialization: Found ClientConnection = 0x" << std::hex << clientConnection;
            //     Core::Log::Message(ss.str());
            // }
            
            uintptr_t objMgrAddress = clientConnection + GameOffsets::OBJECT_MANAGER_OFFSET;
            tempObjMgrPtr = Memory::Read<ObjectManagerActual*>(objMgrAddress);
            if (!tempObjMgrPtr) { 
                 // std::stringstream ss; ss << "[ObjectManager] TryFinishInitialization Attempt Failed: Could not read ObjectManager pointer from 0x" << std::hex << objMgrAddress << " (ClientConnection + 0x" << GameOffsets::OBJECT_MANAGER_OFFSET << "). Will check world state.";
                 // Core::Log::Message(ss.str());
                 // Keep pointersValid as false
            } else {
                 // Log ObjectManager Ptr value found
                 // {
                 //     std::stringstream ss; ss << "[ObjectManager] TryFinishInitialization: Found ObjectManager Ptr = 0x" << std::hex << reinterpret_cast<uintptr_t>(tempObjMgrPtr);
                 //     Core::Log::Message(ss.str());
                 // }
                 pointersValid = true; // Both pointers read successfully!
            }
        }
    } catch (const MemoryAccessError& e) {
        Core::Log::Message("[ObjectManager::TryFinishInitialization] MemoryAccessError during initial pointer read attempt: " + std::string(e.what()) + ". Will check world state.");
        pointersValid = false; 
    } catch (const std::exception& e) {
        Core::Log::Message("[ObjectManager::TryFinishInitialization] EXCEPTION during initial pointer read attempt: " + std::string(e.what()) + ". Will check world state.");
        pointersValid = false;
    } catch (...) {
        // Core::Log::Message("[ObjectManager::TryFinishInitialization] Unknown exception during initial pointer read attempt. Will check world state.");
        pointersValid = false;
    }
    // --- End pointer reading attempt ---

    if (pointersValid) {
        m_objectManagerPtr = tempObjMgrPtr;
        m_isFullyInitialized.store(true, std::memory_order_release); 
        m_isActive.store(true, std::memory_order_release); // Set active when fully initialized
        // Core::Log::Message("[ObjectManager] TryFinishInitialization Succeeded! Pointers acquired.");
        return true;
    } 

    // --- If pointers are NOT valid, NOW check the world state to see if we *should* expect them ---
    // If initialization failed, ensure we are marked as inactive.
    m_isActive.store(false, std::memory_order_release);
    try {
        DWORD gameState = Memory::Read<DWORD>(GameOffsets::IS_IN_WORLD_ADDR);
        if (gameState == 0xA) { // 0xA seems to indicate "In World"
            // Pointers were invalid, but we ARE in the world. This is the problematic late-injection state.
            // Core::Log::Message("[ObjectManager] TryFinishInitialization Failed: In world (State 0xA), but OM pointers still invalid. Retrying next frame...");
        } else {
            // Not in world yet, pointers being invalid is expected.
            // Core::Log::Message("[ObjectManager] TryFinishInitialization: Not in world yet (GameState: " + std::to_string(gameState) + "), OM pointers not ready (expected).");
        }
    } catch (const MemoryAccessError& e) {
        Core::Log::Message("[ObjectManager::TryFinishInitialization] MemoryAccessError checking game state *after* pointer failure: " + std::string(e.what()));
    } catch (...) {
        Core::Log::Message("[ObjectManager::TryFinishInitialization] Unknown exception checking game state *after* pointer failure.");
    }
    
    // If we reached here, either pointers were invalid OR world state check failed/showed not ready.
    return false; 
}

// Static Callback for object enumeration 
int __cdecl ObjectManager::EnumObjectsCallback(uint32_t guid_low, uint32_t guid_high, int callback_arg) {
    ObjectManager* instance = reinterpret_cast<ObjectManager*>(callback_arg);
    // Log entry with GUID
    WGUID guid = { guid_low, guid_high };
    // REMOVED verbose logging
    // { std::stringstream ss_cb_entry; ss_cb_entry << "[EnumCallback] Processing GUID 0x" << std::hex << guid.ToUint64(); Core::Log::Message(ss_cb_entry.str()); }

    if (!instance || !instance->m_isActive.load(std::memory_order_acquire) || !instance->m_isFullyInitialized.load(std::memory_order_acquire) || !instance->m_objectManagerPtr || !instance->m_getObjectPtrByGuidInner) { 
        // Core::Log::Message("[EnumCallback] Instance invalid or OM not ready. Skipping GUID 0x%llX", guid.ToUint64());
        // Log if skipping due to invalid instance state
        { std::stringstream ss_cb_skip; ss_cb_skip << "[EnumCallback] Skipping GUID 0x" << std::hex << guid.ToUint64() << " - Instance/OM not active/ready."; Core::Log::Message(ss_cb_skip.str()); } // Reduced noise
        return 0; // Stop enumeration if instance is bad
    } 
    
    void* objPtr = nullptr;
    try {
         WGUID guidCopy = guid; 
         // Core::Log::Message("[EnumCallback] Calling GetObjectPtrByGuidInner for GUID 0x%llX", guid.ToUint64());
         objPtr = instance->m_getObjectPtrByGuidInner(instance->m_objectManagerPtr, guid.low, &guidCopy);
         // Core::Log::Message("[EnumCallback] GetObjectPtrByGuidInner returned 0x%p for GUID 0x%llX", objPtr, guid.ToUint64());
        if (objPtr) { 
             instance->ProcessFoundObject(guid, objPtr);
        } else {
             // Core::Log::Message("[EnumCallback] GetObjectPtrByGuidInner returned NULL for GUID 0x%llX", guid.ToUint64());
        }
    } catch (const MemoryAccessError& e) {
        // Keep error logging
        std::stringstream ss; ss << "[EnumObjectsCallback] MemoryAccessError for GUID 0x" << std::hex << guid.ToUint64() << ": " << e.what();
        Core::Log::Message(ss.str());
        return 1; 
    } catch (const std::exception& e) {
        // Keep error logging
        std::stringstream ss; ss << "[EnumObjectsCallback] EXCEPTION for GUID 0x" << std::hex << guid.ToUint64() << ": " << e.what();
        Core::Log::Message(ss.str());
        return 1; 
    } catch (...) {
        // Keep error logging
        std::stringstream ss; ss << "[EnumObjectsCallback] UNKNOWN EXCEPTION for GUID 0x" << std::hex << guid.ToUint64();
        Core::Log::Message(ss.str());
        return 1; 
    }

    // --- Add Local Player Check --- 
    // If this object's GUID matches the known local player GUID, update the cached player pointer immediately.
    if (instance->m_localPlayerGuid.IsValid() && guid == instance->m_localPlayerGuid) {
        // Log that we found the matching GUID in the callback
        // { std::stringstream ss_cb_found; ss_cb_found << "[EnumCallback] Found matching local player GUID: 0x" << std::hex << guid.ToUint64(); Core::Log::Message(ss_cb_found.str()); } // Commented out

        // Use find instead of direct access for safety
        std::shared_ptr<WowObject> objFromCache = nullptr;
        { // Scope for lock
            std::lock_guard<std::mutex> lock(instance->m_cacheMutex); // Lock before accessing cache
             auto it = instance->m_objectCache.find(guid);
             if (it != instance->m_objectCache.end()) {
                 objFromCache = it->second;
             }
        } // Lock released

        if (objFromCache) {
            if (auto player = std::dynamic_pointer_cast<WowPlayer>(objFromCache)) { 
                // Log successful update
                // { std::stringstream ss_cb_update; ss_cb_update << "[EnumCallback] Updating cached local player pointer for GUID 0x" << std::hex << guid.ToUint64() << " (Ptr: 0x" << player.get() << ")"; Core::Log::Message(ss_cb_update.str()); } // Commented out
                // Lock again to update the cached pointer
                std::lock_guard<std::mutex> lock(instance->m_cacheMutex);
                instance->m_cachedLocalPlayer = player;
            } else {
                // Log cast failure
                // { std::stringstream ss_cb_cast_fail; ss_cb_cast_fail << "[EnumCallback] Warning: Found local player GUID 0x" << std::hex << guid.ToUint64() << " in cache but failed to cast to WowPlayer."; Core::Log::Message(ss_cb_cast_fail.str()); } // Commented out
                 // Potentially clear cache if this happens unexpectedly?
                 std::lock_guard<std::mutex> lock(instance->m_cacheMutex);
                 instance->m_cachedLocalPlayer = nullptr; 
            }
        } else {
             // Log if the object wasn't found in the cache even though ProcessFoundObject should have added it
             // { std::stringstream ss_cb_not_found; ss_cb_not_found << "[EnumCallback] Warning: Found matching local player GUID 0x" << std::hex << guid.ToUint64() << " but object not found in cache after ProcessFoundObject."; Core::Log::Message(ss_cb_not_found.str()); } // Commented out
             // Clear cache just in case?
             std::lock_guard<std::mutex> lock(instance->m_cacheMutex);
             instance->m_cachedLocalPlayer = nullptr;
        }
    }
    // --- End Local Player Check ---

    return 1; // Continue enumeration
}

// Helper to process a found object pointer
void ObjectManager::ProcessFoundObject(WGUID guid, void* objectPtr) {
    // --- Input validation (same as before) ---
    if (!objectPtr || !guid.IsValid()) {
        // Log invalid input
        // std::stringstream ss_invalid; ss_invalid << "[ProcessFoundObject] Skipped: Invalid GUID (0x" << std::hex << guid.ToUint64() << ") or null Ptr (0x" << (void*)objectPtr << ")"; Core::Log::Message(ss_invalid.str());
        return;
    }
    // Check if OM is active before proceeding
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Core::Log::Message("[ProcessFoundObject] Skipped GUID 0x" + guid.ToString() + ": OM became inactive."); // Optional: log sparingly
        return;
    }
    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(objectPtr);
    // -------------------------------------------

    // Log entry and pointer (maybe reduce verbosity)
    // std::stringstream ss_entry; ss_entry << "[ProcessFoundObject] GUID 0x" << std::hex << guid.ToUint64() << " Ptr: 0x" << baseAddr; Core::Log::Message(ss_entry.str());

    // *** ADDED: Log if this is the local player GUID being processed ***
    if (guid == m_localPlayerGuid) {
        // { std::stringstream ss_pfo_local; ss_pfo_local << "[ProcessFoundObject] Processing Local Player GUID: 0x" << std::hex << guid.ToUint64(); Core::Log::Message(ss_pfo_local.str()); } // Commented out
    }
    // ********************************************************************

    try {
        // Log before reading type
        // Core::Log::Message("[ProcessFoundObject] Reading object type...");
        WowObjectType type = Memory::Read<WowObjectType>(baseAddr + GameOffsets::OBJECT_TYPE_OFFSET);
        // Core::Log::Message("[ProcessFoundObject] Read Type: " + std::to_string(type));

        // Validate type
        if (type <= OBJECT_NONE || type >= OBJECT_TOTAL) {
             // Core::Log::Message("[ProcessFoundObject] Invalid type read: " + std::to_string(type));
             type = OBJECT_NONE;
        }

        if (type != OBJECT_NONE) { 
            std::shared_ptr<WowObject> obj;
            // Use enum identifiers directly
            switch (type) {
                case OBJECT_PLAYER:     obj = std::make_shared<WowPlayer>(baseAddr, guid); break;
                case OBJECT_UNIT:       obj = std::make_shared<WowUnit>(baseAddr, guid); break;
                case OBJECT_GAMEOBJECT: obj = std::make_shared<WowGameObject>(baseAddr, guid); break;
                // Add cases for OBJECT_ITEM, OBJECT_CONTAINER etc. if specific classes exist
                default:                obj = std::make_shared<WowObject>(baseAddr, guid, type); break;
            }
            
            if (obj) { 
                 // Update dynamic data FIRST, before locking
                 obj->UpdateDynamicData(); // Read name, pos, etc. (virtual call)

                 // Now lock ONLY to insert into the cache
                 std::lock_guard<std::mutex> lock(m_cacheMutex);
                 m_objectCache[guid] = obj; 
            } else {
                 // Log only if creation failed, this is important
                 std::stringstream ss; ss << "[ProcessFoundObject] FAILED make_shared for GUID 0x" << std::hex << guid.ToUint64();
                 Core::Log::Message(ss.str());
            }
        }
    } catch (const MemoryAccessError& e) {
        // Keep error logging
        std::stringstream ss; ss << "[ProcessFoundObject] MemoryAccessError for GUID 0x" << std::hex << guid.ToUint64() << ": " << e.what();
        Core::Log::Message(ss.str());
    } catch (const std::exception& e) {
         // Keep error logging
        std::stringstream ss; ss << "[ProcessFoundObject] EXCEPTION for GUID 0x" << std::hex << guid.ToUint64() << ": " << e.what();
        Core::Log::Message(ss.str());
    } catch (...) {
         // Keep error logging
        std::stringstream ss; ss << "[ProcessFoundObject] UNKNOWN EXCEPTION for GUID 0x" << std::hex << guid.ToUint64();
        Core::Log::Message(ss.str());
    }
}

// Update() function - Now performs the core logic, called synchronously (e.g., from EndScene)
void ObjectManager::Update()
{
    // --- ADDED: Game State Check and m_isActive Update ---
    if (!GameStateManager::GetInstance().IsFullyInWorld()) {
        if (m_isActive.load(std::memory_order_acquire)) { // Only log/clear if it was previously active
            // Core::Log::Message("[ObjectManager::Update] Now Not in world. Setting OM inactive and clearing cache.");
            m_objectCache.clear();      // Clear the main object map
            m_cachedLocalPlayer = nullptr; // Reset cached local player pointer
            // Consider if m_localPlayerGuid should also be reset or if it's okay to persist
        }
        m_isActive.store(false, std::memory_order_release);
        m_isFullyInitialized.store(false, std::memory_order_release); // If not in world, we are not 'fully initialized' in a usable sense
        return;
    }
    // If we are here, we are in the world. Mark as active before trying to initialize.
    m_isActive.store(true, std::memory_order_release);
    // --- END Game State Check ---

    // Core::Log::Message("[ObjectManager::Update] Update called."); 
    // First, check if the OM is even supposed to be active.
    // This is a broader check than m_isFullyInitialized, as m_isActive can be false even if pointers were once read (e.g. during shutdown sequence)
    // if (!m_isActive.load(std::memory_order_acquire)) { // This check is now effectively handled by the IsFullyInWorld check above
    //     static int inactive_log_counter = 0;
    //     if (++inactive_log_counter % 100 == 0) { // Log very sparingly
    //         Core::Log::Message("[ObjectManager::Update] Aborted: ObjectManager is not active.");
    //         inactive_log_counter = 0;
    //     }
    //     return;
    // }

    // --- Add Throttling Check --- 
    auto now = std::chrono::steady_clock::now();
    if (now - m_lastUpdateTime < UPDATE_INTERVAL) {
        return; // Not time to update yet
    }
    // ------------------------

    // Ensure we are initialized (m_isFullyInitialized checks if pointers are good, m_isActive confirms overall state)
    if (!m_isFullyInitialized.load(std::memory_order_acquire)) { // m_isActive is true if we got here
        // Core::Log::Message("[ObjectManager::Update] OM not fully initialized pointer-wise, calling TryFinishInitialization...");
        if (!TryFinishInitialization()) {
            // TryFinishInitialization will set m_isActive to false if it fails (e.g. not in world, or bad pointers in world)
            // Core::Log::Message("[ObjectManager::Update] TryFinishInitialization failed. Aborting Update."); 
            // No need to set m_isActive false here, TryFinishInitialization or the top check handles it.
            return; // Still couldn't initialize
        }
        // Core::Log::Message("[ObjectManager::Update] TryFinishInitialization succeeded during update check.");
    }

    // If TryFinishInitialization succeeded, m_isFullyInitialized is true, and m_isActive is true.
    // If we are here, means: IsFullyInWorld() is true, m_isActive is true, m_isFullyInitialized (pointers) is true.

    // Core::Log::Message("[ObjectManager::Update] Performing synchronous update cycle...");

    // --- Clear the cache BEFORE enumeration --- 
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_objectCache.clear(); 
        m_cachedLocalPlayer = nullptr;
        // Core::Log::Message("[ObjectManager::Update] Cache cleared before enumeration.");
    }
    // ------------------------------------------

    // Call the game's EnumVisibleObjects function
    if (m_enumVisibleObjects) {
        // Log call... 
        // Core::Log::Message("[ObjectManager::Update] Calling EnumVisibleObjects...");
        try {
            m_enumVisibleObjects(EnumObjectsCallback, (intptr_t)this);
        } catch (const std::exception& e) {
            Core::Log::Message(std::string("[ObjectManager::Update] Exception during EnumVisibleObjects/Callbacks: ") + e.what());
            { std::lock_guard<std::mutex> lock(m_cacheMutex); m_objectCache.clear(); m_cachedLocalPlayer = nullptr; } // Clear cache on error
        } catch (...) {
            Core::Log::Message("[ObjectManager::Update] Unknown/SEH exception during EnumVisibleObjects/Callbacks.");
            { std::lock_guard<std::mutex> lock(m_cacheMutex); m_objectCache.clear(); m_cachedLocalPlayer = nullptr; } // Clear cache on error
        }
        // Core::Log::Message("[ObjectManager::Update] Enumeration finished."); // Comment out
    } else {
        Core::Log::Message("[ObjectManager::Update] EnumVisibleObjects function pointer is null. Update aborted."); // Keep error log
        return; 
    }

    // --- Update timestamp AFTER successful execution (or attempt) --- 
    m_lastUpdateTime = now;

    // NOTE: RefreshLocalPlayerCache should be called separately *after* Update()
}

// Refresh cached player pointer - NOW ONLY UPDATES m_localPlayerGuid
void ObjectManager::RefreshLocalPlayerCache() {
    // Core::Log::Message("[RefreshLocalPlayerCache] Determining local player GUID..."); // Optional: Log entry
    if (!m_isFullyInitialized) {
        // Core::Log::Message("[RefreshLocalPlayerCache] Aborted: OM not fully initialized.");
        return;
    }

    WGUID currentLocalPlayerGuid;
    bool directReadSucceeded = false;

    // Method 1: Read from ObjectManager pointer (based on provided C# code)
    if (m_objectManagerPtr) {
        try {
            uintptr_t guidAddress = reinterpret_cast<uintptr_t>(m_objectManagerPtr) + 0xC0; // Offset from C# code
            uint64_t guidValue = Memory::Read<uint64_t>(guidAddress);
            currentLocalPlayerGuid = WGUID(guidValue);
            if (currentLocalPlayerGuid.IsValid()) {
                directReadSucceeded = true;
                // { std::stringstream ss_om_read; ss_om_read << "[RefreshLocalPlayerCache] Read via OM Ptr + 0xC0 succeeded. GUID: 0x" << std::hex << guidValue; Core::Log::Message(ss_om_read.str()); } // Commented out
            } else {
                // Core::Log::Message("[RefreshLocalPlayerCache] Read via OM Ptr + 0xC0 returned invalid GUID (0)."); // Commented out
            }
        } catch (const MemoryAccessError& /*e*/) {
            // Core::Log::Message(std::string("[RefreshLocalPlayerCache] Read via OM Ptr + 0xC0 failed: MemoryAccessError: ") + e.what()); // Commented out
        } catch (...) {
            // Core::Log::Message("[RefreshLocalPlayerCache] Read via OM Ptr + 0xC0 failed: Unknown exception."); // Commented out
        }
    } else {
        // Core::Log::Message("[RefreshLocalPlayerCache] Cannot read via OM Ptr: ObjectManager pointer is null."); // Commented out
    }

    // Method 2: Fallback to function pointer if OM Ptr read failed
    bool fallbackAttempted = false;
    if (!directReadSucceeded && m_getLocalPlayerGuidFn) {
        fallbackAttempted = true;
        // Core::Log::Message("[RefreshLocalPlayerCache] Attempting fallback using GetLocalPlayerGuid function..."); // Commented out
        try {
            uint64_t guid64 = m_getLocalPlayerGuidFn();
            currentLocalPlayerGuid = WGUID(guid64);
            // Log success/failure of fallback
            if (currentLocalPlayerGuid.IsValid()) {
                // { std::stringstream ss_fallback; ss_fallback << "[RefreshLocalPlayerCache] Fallback function succeeded. GUID: 0x" << std::hex << guid64; Core::Log::Message(ss_fallback.str()); } // Commented out
            } else {
                // Core::Log::Message("[RefreshLocalPlayerCache] Fallback function returned invalid GUID (0)."); // Commented out
            }
        } catch (const MemoryAccessError& /*e*/) {
            // Core::Log::Message(std::string("[RefreshLocalPlayerCache] Fallback function failed: MemoryAccessError: ") + e.what()); // Commented out
        } catch (...) {
            // Core::Log::Message("[RefreshLocalPlayerCache] Fallback function failed: Unknown exception."); // Commented out
        }
    } else if (!directReadSucceeded) {
        // Core::Log::Message("[RefreshLocalPlayerCache] Cannot determine GUID: Direct read failed AND fallback function pointer is null."); // Commented out
    }

    // --- Log the determined GUID ---
    // std::stringstream ss_refresh;
    // ss_refresh << "[RefreshLocalPlayerCache] CurrentLocalGUID determined to be: 0x" << std::hex << currentLocalPlayerGuid.ToUint64();
    // Core::Log::Message(ss_refresh.str()); // Commented out
    // -----------------------------

    // Update the stored local player GUID
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        if (m_localPlayerGuid != currentLocalPlayerGuid) { // Only log if it changes
            // std::stringstream ss_guid_change; ss_guid_change << "[RefreshLocalPlayerCache] Updating m_localPlayerGuid from 0x" << std::hex << m_localPlayerGuid.ToUint64() << " to 0x" << currentLocalPlayerGuid.ToUint64(); Core::Log::Message(ss_guid_change.str()); // Commented out
            m_localPlayerGuid = currentLocalPlayerGuid;
            // Clear the cached player pointer if the GUID changed, the callback will refresh it.
            // Log clearing cache due to GUID change/invalidity
            if (!currentLocalPlayerGuid.IsValid() && m_cachedLocalPlayer) {
                 // Core::Log::Message("[RefreshLocalPlayerCache] Clearing cached local player due to invalid new GUID."); // Commented out
                m_cachedLocalPlayer = nullptr;
            } else if (currentLocalPlayerGuid.IsValid()) {
                 // Core::Log::Message("[RefreshLocalPlayerCache] Local GUID changed, cache update pending callback."); // Commented out
            }
        }
        // If GUID is invalid, ensure cached pointer is null
        if (!m_localPlayerGuid.IsValid() && m_cachedLocalPlayer) {
            // Core::Log::Message("[RefreshLocalPlayerCache] Clearing cached local player because stored GUID is invalid."); // Commented out
            m_cachedLocalPlayer = nullptr;
        }

        // *** ADDED: Attempt immediate cache update after GUID is set ***
        if (m_localPlayerGuid.IsValid()) {
            auto it = m_objectCache.find(m_localPlayerGuid);
            if (it != m_objectCache.end() && it->second) {
                if (auto player = std::dynamic_pointer_cast<WowPlayer>(it->second)) {
                    // Found the player in the cache, update the pointer now
                    if (m_cachedLocalPlayer != player) { // Avoid redundant updates/logging
                        // Core::Log::Message("[RefreshLocalPlayerCache] Updating cached local player pointer immediately from cache."); // Optional log
                        m_cachedLocalPlayer = player;
                    }
                } else {
                    // Found GUID in cache but it wasn't a WowPlayer? Should not happen for player GUID.
                    // Core::Log::Message("[RefreshLocalPlayerCache] Warning: Found local player GUID in cache but failed cast to WowPlayer."); // Optional log
                    if (m_cachedLocalPlayer) { // Clear if cast failed
                         m_cachedLocalPlayer = nullptr;
                    }
                }
            } else {
                 // Player object not yet in cache, EnumCallback will handle it later.
                 // Core::Log::Message("[RefreshLocalPlayerCache] Local player object not yet in cache."); // Optional log
                 // No need to clear m_cachedLocalPlayer here if it's already set from a previous frame
            }
        }
        // *** END ADDED CODE ***
    }

    // REMOVED: Cache lookup and m_cachedLocalPlayer update logic.
    // The EnumObjectsCallback now handles updating m_cachedLocalPlayer directly.
}

// ADDED DEFINITION FROM HEADER
bool ObjectManager::IsInitialized() const {
    return m_isFullyInitialized.load(std::memory_order_acquire) &&
           m_isActive.load(std::memory_order_acquire) &&
           GameStateManager::GetInstance().IsFullyInWorld(); // Explicitly check current game state too
}

// GetObjectByGUID - Remains thread-safe due to lock
std::shared_ptr<WowObject> ObjectManager::GetObjectByGUID(WGUID guid) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return GetObjectByGUID_locked(guid);
}

// GetObjectByGUID (uint64_t overload) - Remains thread-safe
std::shared_ptr<WowObject> ObjectManager::GetObjectByGUID(uint64_t guid64) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return nullptr;
    }
    return GetObjectByGUID(WGUID(guid64)); 
}

// ADDED: Implementation for the const version that takes const WGUID&
std::shared_ptr<WowObject> ObjectManager::GetObjectByGuid(const WGUID& guid) const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex); // Ensure m_cacheMutex is mutable or use a different lock strategy for const methods if necessary
    // Assuming GetObjectByGUID_locked can be called. If GetObjectByGUID_locked is not const,
    // this might require casting away const-ness of 'this' or making GetObjectByGUID_locked const 
    // if it doesn't modify any members, or providing a const version of GetObjectByGUID_locked.
    // For now, let's assume GetObjectByGUID_locked is suitable or that the mutex handling is the primary concern for const-correctness here.
    auto it = m_objectCache.find(guid); // m_objectCache.find is a const operation
    return (it != m_objectCache.end()) ? it->second : nullptr;
}

// Helper for locked access - Signature uses WGUID
std::shared_ptr<WowObject> ObjectManager::GetObjectByGUID_locked(WGUID guid) {
    auto it = m_objectCache.find(guid);
    return (it != m_objectCache.end()) ? it->second : nullptr;
}

// GetObjectsByType - Remains thread-safe due to lock
std::vector<std::shared_ptr<WowObject>> ObjectManager::GetObjectsByType(WowObjectType type) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return {}; // Return empty vector
    }
    std::vector<std::shared_ptr<WowObject>> results;
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    results.reserve(m_objectCache.size() / 4 + 1); // Avoid zero reserve 
    for (const auto& pair : m_objectCache) {
        if (pair.second && pair.second->GetType() == type) {
            results.push_back(pair.second);
        }
    }
    return results;
}

// GetLocalPlayer - Remains thread-safe due to lock
std::shared_ptr<WowPlayer> ObjectManager::GetLocalPlayer() {
    // No need to check m_isFullyInitialized, background thread handles this
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cachedLocalPlayer;
}

// GetAllObjects - Returns a *copy* of the cache, thread-safe due to lock
std::map<WGUID, std::shared_ptr<WowObject>> ObjectManager::GetAllObjects() {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return {}; // Return empty map
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_objectCache; // Return a copy
}

// FindObjectsByName - Remains thread-safe due to lock
std::vector<std::shared_ptr<WowObject>> ObjectManager::FindObjectsByName(const std::string& name) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return {}; // Return empty vector
    }
    std::vector<std::shared_ptr<WowObject>> results;
    if (name.empty()) return results;

    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    results.reserve(m_objectCache.size() / 10 + 1); 
    std::string lowerObjName; 
    for (const auto& pair : m_objectCache) {
        if (!pair.second) continue; 
        
        std::string objName = pair.second->GetName(); // Uses cached name
        if (!objName.empty() && objName.find('[') == std::string::npos) { // Avoid comparing partially read/error names
             lowerObjName = objName;
             std::transform(lowerObjName.begin(), lowerObjName.end(), lowerObjName.begin(), ::tolower);
            if (lowerObjName.find(lowerName) != std::string::npos) {
                results.push_back(pair.second);
            }
        }
    }
    return results;
}

// GetNearestObject - Remains thread-safe due to locking
std::shared_ptr<WowObject> ObjectManager::GetNearestObject(WowObjectType type, float maxDistance) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return nullptr;
    }
    auto player = GetLocalPlayer(); // This locks internally
    if (!player) return nullptr;
    
    Vector3 playerPos = player->GetPosition(); // Uses cached position 
    if (playerPos.IsZero()) {
        return nullptr; 
    }

    std::shared_ptr<WowObject> nearest = nullptr;
    float nearestDistSq = maxDistance * maxDistance;
    WGUID playerGuid = player->GetGUID(); 
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    for (const auto& pair : m_objectCache) {
        // Compare WGUID directly
        if (pair.second && pair.second->GetType() == type && pair.first != playerGuid) {
            Vector3 objPos = pair.second->GetPosition(); // Uses cached position
            float distSq = playerPos.DistanceSq(objPos);
            
            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearest = pair.second;
            }
        }
    }
    
    return nearest;
}

// GetObjectsWithinDistance - Remains thread-safe due to lock
std::vector<std::shared_ptr<WowObject>> ObjectManager::GetObjectsWithinDistance(const Vector3& center, float distance) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all for getters
        return {}; // Return empty vector
    }
    std::vector<std::shared_ptr<WowObject>> results;
    float distSqThreshold = distance * distance;
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    results.reserve(m_objectCache.size() / 5 + 1); 
    for (const auto& pair : m_objectCache) {
        if (pair.second) { 
            Vector3 objPos = pair.second->GetPosition(); // Uses cached position
            float distSq = center.DistanceSq(objPos);
            if (distSq <= distSqThreshold) {
                results.push_back(pair.second);
            }
        }
    }
    return results;
}

// ADDED Getter for Local Player GUID
WGUID ObjectManager::GetLocalPlayerGuid() const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return {}; // Return default WGUID
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_localPlayerGuid;
}

// --- Implementations for specific type getters (ADDED) ---

std::shared_ptr<WowUnit> ObjectManager::GetUnitByGuid(const WGUID& guid) const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return nullptr;
    }
    std::shared_ptr<WowObject> obj = GetObjectByGuid(guid); // Call the existing const GetObjectByGuid
    if (obj && (obj->GetType() == OBJECT_UNIT || obj->GetType() == OBJECT_PLAYER)) {
        // Use static_pointer_cast for efficiency if type is known to be correct
        // Or dynamic_pointer_cast for safety if type might mismatch (though GetType check helps)
        return std::static_pointer_cast<WowUnit>(obj);
    }
    return nullptr;
}

std::shared_ptr<WowPlayer> ObjectManager::GetPlayerByGuid(const WGUID& guid) const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return nullptr;
    }
    std::shared_ptr<WowObject> obj = GetObjectByGuid(guid); // Call the existing const GetObjectByGuid
    if (obj && obj->GetType() == OBJECT_PLAYER) {
        // Use static_pointer_cast for efficiency
        return std::static_pointer_cast<WowPlayer>(obj);
    }
    return nullptr;
}

std::shared_ptr<WowGameObject> ObjectManager::GetGameObjectByGuid(const WGUID& guid) const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return nullptr;
    }
    std::shared_ptr<WowObject> obj = GetObjectByGuid(guid); // Call the existing const GetObjectByGuid
    if (obj && obj->GetType() == OBJECT_GAMEOBJECT) {
        // Use static_pointer_cast for efficiency
        return std::static_pointer_cast<WowGameObject>(obj);
    }
    return nullptr;
}

// Implementation for GetLocalPlayer const (returns cached shared_ptr)
std::shared_ptr<WowPlayer> ObjectManager::GetLocalPlayer() const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cachedLocalPlayer;
}

// Implementations for GetAllX const versions
std::vector<std::shared_ptr<WowObject>> ObjectManager::GetAllObjects() const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return {}; // Return empty vector
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    std::vector<std::shared_ptr<WowObject>> result;
    result.reserve(m_objectCache.size());
    for(const auto& pair : m_objectCache) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<std::shared_ptr<WowUnit>> ObjectManager::GetAllUnits() const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return {}; // Return empty vector
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    std::vector<std::shared_ptr<WowUnit>> result;
    result.reserve(m_objectCache.size());
    for(const auto& pair : m_objectCache) {
        if(pair.second && (pair.second->GetType() == OBJECT_UNIT || pair.second->GetType() == OBJECT_PLAYER)) {
            result.push_back(std::static_pointer_cast<WowUnit>(pair.second));
        }
    }
    return result;
}

std::vector<std::shared_ptr<WowPlayer>> ObjectManager::GetAllPlayers() const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return {}; // Return empty vector
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    std::vector<std::shared_ptr<WowPlayer>> result;
    result.reserve(m_objectCache.size() / 10 + 1); // Players are usually fewer
    for(const auto& pair : m_objectCache) {
        if(pair.second && pair.second->GetType() == OBJECT_PLAYER) {
            result.push_back(std::static_pointer_cast<WowPlayer>(pair.second));
        }
    }
    return result;
}

std::vector<std::shared_ptr<WowGameObject>> ObjectManager::GetAllGameObjects() const {
    if (!m_isActive.load(std::memory_order_acquire)) {
        return {}; // Return empty vector
    }
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    std::vector<std::shared_ptr<WowGameObject>> result;
    result.reserve(m_objectCache.size() / 2 + 1); // Guess for GO count
    for(const auto& pair : m_objectCache) {
        if(pair.second && pair.second->GetType() == OBJECT_GAMEOBJECT) {
            result.push_back(std::static_pointer_cast<WowGameObject>(pair.second));
        }
    }
    return result;
}

// --- NEW: Game State Check Implementation ---
bool ObjectManager::IsPlayerInWorld() const {
    try {
        // Read the DWORD value at the specified game state address
        uint32_t gameState = *reinterpret_cast<volatile uint32_t*>(GameOffsets::IS_IN_WORLD_ADDR);
        
        // Log the game state value found
        static uint32_t lastLoggedGameState = 0xFFFFFFFF; // Initialize to a value that gameState is unlikely to be
        if (gameState != lastLoggedGameState) {
            std::stringstream ss_log;
            ss_log << "[ObjectManager::IsPlayerInWorld] Current game state flag (0x" 
                   << std::hex << GameOffsets::IS_IN_WORLD_ADDR 
                   << ") reads as: 0x" << std::hex << gameState 
                   << " (Decimal: " << std::dec << gameState 
                   << "). Expected 0x0 for in-world.";
            Core::Log::Message(ss_log.str());
            lastLoggedGameState = gameState;
        }

        // Value 0x0A (10) seems to indicate fully in-world and playable
        return gameState == 0x0; // NEW logic: 0x0 is in-world, 0xA is loading
    } catch (...) {
        // Handle potential read access violation if the address becomes invalid,
        // although this is less likely for static addresses.
        Core::Log::Message("[ObjectManager] Exception reading game state flag at address: " + std::to_string(GameOffsets::IS_IN_WORLD_ADDR));
        return false; // Assume not in world if we can't read the state
    }
}

// --- End NEW Game State Check ---

// Helper function to get current timestamp
// ... existing code ...

// New method implementation
// Removed potential 'namespace Core {' that might have been here
int ObjectManager::CountUnitsInMeleeRange(std::shared_ptr<WowUnit> centerUnit, float range, bool includeHostile, bool includeFriendly, bool includeNeutral) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all
        return 0;
    }
    if (!centerUnit || !IsInitialized()) { // IsInitialized now checks m_isActive
        return 0; // Cannot count without a center point or if OM is not ready
    }

    int count = 0;
    Vector3 centerPos = centerUnit->GetPosition();
    uint64_t centerGuid = centerUnit->GetGUID64();

    // Ensure objectMutex is a member of ObjectManager or accessible in this scope
    // If objectMutex is m_cacheMutex, or another member, ensure it's used correctly.
    // For example, if it's m_cacheMutex, it should be: std::lock_guard<std::mutex> lock(m_cacheMutex);
    // Assuming 'objects' is also a member like 'm_objectCache'.
    std::lock_guard<std::mutex> lock(m_cacheMutex); // CORRECTED to use m_cacheMutex, assuming 'objects' is m_objectCache
    for (const auto& pair : m_objectCache) { // CORRECTED to use m_objectCache
        if (!pair.second) { // Simplified check, GetType() check comes later if needed for WowUnit cast
            continue;
        }
        
        // Check type before casting to WowUnit
        WowObjectType objType = pair.second->GetType();
        if (objType != OBJECT_UNIT && objType != OBJECT_PLAYER) { // CORRECTED ENUM NAMES
            continue;
        }

        std::shared_ptr<WowUnit> currentUnit = std::static_pointer_cast<WowUnit>(pair.second);
        if (!currentUnit || currentUnit->GetGUID64() == centerGuid || currentUnit->IsDead()) {
            continue;
        }

        // Faction/Reaction Check
        bool shouldCount = false;
        // Ensure centerUnit.get() is valid if centerUnit is a shared_ptr.
        // WowUnit::GetReaction might expect a raw pointer.
        int reaction = currentUnit->GetReaction(centerUnit.get()); 

        if (includeHostile && reaction <= 2) { // Hostile (Reaction 1) or Unfriendly (Reaction 2)
            shouldCount = true;
        } else if (includeFriendly && reaction >= 4) { // Friendly (Reaction 4) or Honored/Revered/Exalted (5,6,7,8)
            shouldCount = true;
        } else if (includeNeutral && reaction == 3) { // Neutral (Reaction 3)
            shouldCount = true;
        }

        if (!shouldCount) {
            continue;
        }

        float distance = centerPos.Distance(currentUnit->GetPosition());
        if (distance <= range) {
            count++;
        }
    }
    return count;
}
// Removed potential closing '}' for 'namespace Core' that might have been here

// End of file maybe 

int ObjectManager::CountUnitsInFrontalCone(std::shared_ptr<WowUnit> caster, float range, float coneAngleDegrees, bool includeHostile, bool includeFriendly, bool includeNeutral) {
    if (!m_isActive.load(std::memory_order_acquire)) {
        // Log sparingly or not at all
        return 0;
    }
    if (!caster) return 0;

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    int count = 0;
    float coneAngleRadians = coneAngleDegrees * (M_PI_F / 180.0f); // Use M_PI_F and 180.0f
    float halfConeAngle = coneAngleRadians / 2.0f;

    Vector3 casterPos = caster->GetPosition();
    float casterFacing = caster->GetFacing(); // Radians

    for (const auto& pair : m_objectCache) {
        if (!pair.second || pair.second->GetGUID64() == caster->GetGUID64()) {
            continue; // Skip self or invalid objects
        }

        WowUnit* unitRawPtr = dynamic_cast<WowUnit*>(pair.second.get());
        if (!unitRawPtr) {
            continue; // Not a WowUnit
        }
        std::shared_ptr<WowUnit> currentUnit = std::static_pointer_cast<WowUnit>(pair.second);
        if (!currentUnit || currentUnit->IsDead()) {
            continue; // Skip dead units
        }

        Vector3 unitPos = currentUnit->GetPosition();
        float distanceSq = casterPos.DistanceSq(unitPos);

        if (distanceSq > (range * range)) {
            continue; // Out of range
        }

        // Check faction
        int reaction = currentUnit->GetReaction(caster.get());
        bool isHostile = reaction <= 2; // Hostile or Unfriendly
        bool isFriendly = reaction >= 4; // Friendly or higher
        bool isNeutral = reaction == 3;

        bool includeThisUnit = false;
        if (includeHostile && isHostile) includeThisUnit = true;
        if (includeFriendly && isFriendly) includeThisUnit = true;
        if (includeNeutral && isNeutral) includeThisUnit = true;

        if (!includeThisUnit) {
            continue;
        }

        // Calculate angle to unit relative to caster's positive X-axis
        float angleToUnit = std::atan2(unitPos.y - casterPos.y, unitPos.x - casterPos.x);

        // Adjust casterFacing to be in the range -PI to PI, matching atan2 output
        // casterFacing is assumed to be 0 for +X, PI/2 for +Y, PI or -PI for -X, -PI/2 for -Y
        // Convert game facing (0 to 2PI, 0 east) to atan2 system (0 east, PI/2 north)
        // This step might need adjustment based on how GetFacing() is defined (radians, 0 point, direction)
        // Assuming GetFacing() returns radians where 0 is along the positive X-axis (East)
        // and positive angle is counter-clockwise.
        
        float deltaAngle = angleToUnit - casterFacing;

        // Normalize deltaAngle to be between -PI and PI
        while (deltaAngle > M_PI_F) deltaAngle -= 2.0f * M_PI_F; // Use M_PI_F
        while (deltaAngle < -M_PI_F) deltaAngle += 2.0f * M_PI_F; // Use M_PI_F

        if (std::fabs(deltaAngle) <= halfConeAngle) {
            count++;
        }
    }
    return count;
}

// Method to get the local player's GUID
// ... existing code ... 

// GetInternalObjectManagerPtr - Returns the raw pointer, caller must be aware of OM state.
// No explicit m_isActive check here as it's a low-level getter.
ObjectManagerActual* ObjectManager::GetInternalObjectManagerPtr() const {
    return m_objectManagerPtr;
}

// GetCurrentTargetGUID - Read the current target GUID from the game with proper error handling
uint64_t ObjectManager::GetCurrentTargetGUID() const {
    if (!m_isActive.load(std::memory_order_acquire) || !m_isFullyInitialized.load(std::memory_order_acquire)) {
        return 0;
    }
    
    uint64_t targetGuid = 0;
    
    try {
        targetGuid = Memory::Read<uint64_t>(GameOffsets::CURRENT_TARGET_GUID_ADDR);
        return targetGuid;
    } catch (const MemoryAccessError& e) {
        // Error logging removed
        return 0; 
    } catch (const std::exception& e) {
        // Error logging removed
        return 0;
    } catch (...) {
        // Error logging removed
        return 0;
    }
}
