#include "wowobject.h"
#include "../utils/memory.h" // Use relative path
#include "../logs/log.h"      // Use relative path
#include <sstream>
#include <chrono>
#include "wowplayer.h"      
#include "wowunit.h"
#include "wowgameobject.h"

// Offsets moved to wowobject.h

// --- WowObject Implementation --- 

// Constructor directly setting type
WowObject::WowObject(uintptr_t baseAddress, WGUID guid, WowObjectType type)
    : m_baseAddress(baseAddress), m_guid(guid), m_type(type) {}

// Constructor that reads type from memory
WowObject::WowObject(uintptr_t baseAddress, WGUID guid)
    : m_baseAddress(baseAddress), m_guid(guid), m_type(OBJECT_NONE) 
{
    // Read the type from memory if base address is valid
    if (m_baseAddress != 0) {
        try {
             m_type = Memory::Read<WowObjectType>(m_baseAddress + Offsets::OBJECT_TYPE);
             // Basic validation
             if (m_type < OBJECT_NONE || m_type >= OBJECT_TOTAL) {
                 m_type = OBJECT_NONE;
             }
        } catch (const MemoryAccessError& e) {
            std::stringstream ss;
            ss << "[WowObject] Failed to read type for GUID 0x" << std::hex << guid.ToUint64() << " at Addr 0x" << baseAddress << ": " << e.what();
            Core::Log::Message(ss.str());
            m_type = OBJECT_NONE; // Set to none on error
            m_baseAddress = 0;    // Invalidate object if type read fails?
        }
    }
}

// --- WowObject Getters (Modify existing) ---
Vector3 WowObject::GetPosition() {
    // No memory read here, just return cached value
    return m_cachedPosition; 
}

std::string WowObject::GetName() {
    // No memory read here, just return cached value
    return m_cachedName; 
}

// Helper method to read name via VTable (WoWBot method)
std::string WowObject::ReadNameFromVTable() {
    if (!m_baseAddress) return "";
    
    // Define the function signature based on WoWBot structure
    typedef char* (__thiscall* GetNameFunc)(void* thisptr);

    try {
        // 1. Read VTable Pointer from Object Base
        uintptr_t vtableAddr = Memory::Read<uintptr_t>(m_baseAddress);
        if (!vtableAddr) { return "[Error VTable Null]"; }

        // 2. Read Function Pointer from VTable Address using the index
        uintptr_t funcAddr = Memory::Read<uintptr_t>(vtableAddr + (Offsets::VF_GetName * sizeof(void*)));
        if (!funcAddr) { return "[Error Func Addr Null]"; }

        // 3. Cast and Call the Function
        GetNameFunc func = reinterpret_cast<GetNameFunc>(funcAddr);
        char* namePtrFromGame = func(reinterpret_cast<void*>(m_baseAddress)); 
        if (!namePtrFromGame) { return ""; } // Empty name is valid

        // 4. Read the string content from the pointer returned by the game function
        // Use the existing Memory::ReadString for safety
        return Memory::ReadString(reinterpret_cast<uintptr_t>(namePtrFromGame), 100); // Limit length

    } catch (const MemoryAccessError& e) {
        (void)e; // Mark as unused
        // Log specific memory read errors if desired, otherwise return error string
        // Core::Log::Message("[WowObject::ReadNameFromVTable] Memory Read Error: " + std::string(e.what()));
        return "[Error VTable Name Read]";
    } catch (...) {
        return "[Error VTable Name Unknown]";
    }
}

// --- WowObject UpdateDynamicData (New Implementation) ---
void WowObject::UpdateDynamicData() {
    if (!m_baseAddress) {
        // Clear cache if object is invalid
        m_cachedName = "";
        m_cachedPosition = Vector3();
        return;
    }

    // Optional throttling:
    // auto now = std::chrono::steady_clock::now();
    // if (now - m_lastCacheUpdateTime < std::chrono::milliseconds(100)) { return; }

    // Core::Log::Message("[WowObject::UpdateDynamicData] Updating base data for GUID 0x" + std::to_string(m_guid.ToUint64())); // REMOVED VERBOSE LOG

    try {
        // --- Update Name Cache using VTable method --- 
        m_cachedName = ReadNameFromVTable();
        // ---------------------------------------------

        // --- DO NOT READ POSITION IN BASE CLASS --- 
        // Position reading should be handled by derived classes like WowUnit, WowGameObject
        // m_cachedPosition.x = Memory::Read<float>(m_baseAddress + Offsets::OBJECT_POS_X);
        // m_cachedPosition.y = Memory::Read<float>(m_baseAddress + Offsets::OBJECT_POS_Y);
        // m_cachedPosition.z = Memory::Read<float>(m_baseAddress + Offsets::OBJECT_POS_Z);
        // ------------------------------------------
        
        // Update timestamp (only after successful name read, if applicable)
        m_lastCacheUpdateTime = std::chrono::steady_clock::now();

    } catch (const MemoryAccessError& e) {
        std::stringstream ss;
        ss << "[WowObject::UpdateDynamicData] Exception for GUID 0x" << std::hex << m_guid.ToUint64() << ": " << e.what();
        Core::Log::Message(ss.str());
        // Don't overwrite potentially valid name on position read error
        // m_cachedName = "[Read Error]"; 
        m_cachedPosition = Vector3();
    } catch (...) {
        Core::Log::Message("[WowObject::UpdateDynamicData] Unknown exception for GUID 0x" + std::to_string(m_guid.ToUint64()));
        // m_cachedName = "[Read Error]";
        m_cachedPosition = Vector3();
    }
}


WowPlayer* WowObject::ToPlayer() { 
    return IsPlayer() ? static_cast<WowPlayer*>(this) : nullptr; 
}

WowUnit* WowObject::ToUnit() { 
    return IsUnit() ? static_cast<WowUnit*>(this) : nullptr; 
}

WowGameObject* WowObject::ToGameObject() { 
    return IsGameObject() ? static_cast<WowGameObject*>(this) : nullptr; 
}

const WowPlayer* WowObject::ToPlayer() const { 
    return IsPlayer() ? static_cast<const WowPlayer*>(this) : nullptr; 
}

const WowUnit* WowObject::ToUnit() const { 
    return IsUnit() ? static_cast<const WowUnit*>(this) : nullptr; 
}

const WowGameObject* WowObject::ToGameObject() const { 
    return IsGameObject() ? static_cast<const WowGameObject*>(this) : nullptr; 
}

// Optional shared_ptr helpers could be defined here too
// std::shared_ptr<WowPlayer> WowObject::AsPlayer(std::shared_ptr<WowObject> obj) { ... } 
// ... etc ... 

// --- End of Class Implementations --- 

// Add this method implementation

bool WowObject::IsFriendly() const {
    // Default implementation for base class
    // Only units and players have faction data
    // Default to false if not overridden
    return false;
}

void WowObject::Interact() {
    if (!m_baseAddress) {
        Core::Log::Message("[WowObject::Interact] Attempted to interact with null base address.");
        return;
    }

    // Define the function signature for the VTable Interact method
    // This is a common signature for a __thiscall member function
    typedef void (__thiscall* InteractFn)(void* thisObject);

    try {
        // 1. Get the VTable pointer (array of function pointers)
        // The VTable pointer is usually the first thing at the object's base address.
        uintptr_t* vtable = *(uintptr_t**)m_baseAddress;
        if (!vtable) {
            Core::Log::Message("[WowObject::Interact] VTable is null.");
            return;
        }

        // 2. Get the function pointer from the VTable at index 44
        InteractFn interactFunc = reinterpret_cast<InteractFn>(vtable[44]);
        if (!interactFunc) {
            Core::Log::Message("[WowObject::Interact] Interact function pointer at VTable index 44 is null.");
            return;
        }

        // 3. Call the function
        // The 'this' pointer for the game's object instance is m_baseAddress
        Core::Log::Message("[WowObject::Interact] Calling VTable Interact for GUID 0x" + std::to_string(GetGUID64()));
        interactFunc(reinterpret_cast<void*>(m_baseAddress));

    } catch (const MemoryAccessError& e) {
        std::stringstream ss;
        ss << "[WowObject::Interact] MemoryAccessError for GUID 0x" << std::hex << GetGUID64() << ": " << e.what();
        Core::Log::Message(ss.str());
    } catch (...) {
        std::stringstream ss;
        ss << "[WowObject::Interact] Unknown exception during VTable call for GUID 0x" << std::hex << GetGUID64();
        Core::Log::Message(ss.str());
    }
}