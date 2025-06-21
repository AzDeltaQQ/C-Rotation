#pragma once

#include "types.h"
#include <string>
#include <cstdint>
#include <chrono> // For throttling timestamp

// --- Offsets (Moved from wowobject.cpp) ---
// Based on WoWBot 3.3.5a source
namespace Offsets {
    // Object Base Relative
    constexpr uintptr_t OBJECT_TYPE = 0x14; // From WoWBot
    constexpr uintptr_t OBJECT_POS_X = 0x79C; 
    constexpr uintptr_t OBJECT_POS_Y = 0x798;
    constexpr uintptr_t OBJECT_POS_Z = 0x7A0;
    constexpr uintptr_t OBJECT_FACING_OFFSET = 0x7A8; // Offset for unit facing/rotation
    constexpr uintptr_t OBJECT_UNIT_FIELDS_PTR = 0x8; // Pointer to UnitFields/Descriptor
    constexpr uintptr_t OBJECT_DESCRIPTOR_PTR = 0x8;  // From WoWBot
    constexpr uintptr_t UNIT_NAME_PTR = 0xB30; // Common offset for name pointer (Unit/Player)
    constexpr uintptr_t GAMEOBJECT_NAME_PTR = 0x214; // Common offset for GO name pointer
    constexpr uintptr_t OBJECT_CASTING_ID = 0xA6C; 
    constexpr uintptr_t OBJECT_CASTING_END_TIME = 0xA7C;
    constexpr uintptr_t OBJECT_CHANNEL_ID = 0xA80; 
    constexpr uintptr_t OBJECT_CHANNEL_END_TIME = 0xA88;
   // constexpr uintptr_t OBJECT_CASTING_ID        = 0xC90;

    // --- Unit Movement Offsets (Based on Analysis) RE-ADDED ---
    
    // Offset relative to CGUnit_C base address that holds the POINTER to the CMovement component
    constexpr uintptr_t UNIT_MOVEMENT_COMPONENT_PTR = 0xD8;
    // Offset relative to the CMovement component's base address (the pointer read from 0xD8)
    constexpr uintptr_t MOVEMENT_FLAGS = 0x44;     
    
    // --------------------------------------------------------

 // game object specific offsets
    constexpr uintptr_t GO_RAW_POS_X = 0xE8;
    constexpr uintptr_t GO_RAW_POS_Y = 0xEC; // Corrected: X + 0x4
    constexpr uintptr_t GO_RAW_POS_Z = 0xF0; // Corrected: X + 0x8

    // UnitFields/Descriptor Relative (Offsets are multiplied by 4 in WoW memory layout, but raw offset is given)
    constexpr uintptr_t UNIT_FIELD_HEALTH = 0x18 * 4;      // From WoWBot
    constexpr uintptr_t UNIT_FIELD_MAXHEALTH = 0x20 * 4;   // From WoWBot
    constexpr uintptr_t UNIT_FIELD_LEVEL = 0x36 * 4;       // From WoWBot
    constexpr uintptr_t UNIT_FIELD_POWER_BASE = 0x19 * 4;  // From WoWBot (UNIT_FIELD_POWER1)
    constexpr uintptr_t UNIT_FIELD_MAXPOWER_BASE = 0x21 * 4;// From WoWBot (UNIT_FIELD_MAXPOWER1)
    // Power Type: WoWBot uses fallback: descriptorPtr + 0x47. Let's define that.
    constexpr uintptr_t DESCRIPTOR_FIELD_POWTYPE = 0x47;   // Byte offset relative to Descriptor base
    constexpr uintptr_t UNIT_FIELD_FLAGS = 0x3B * 4;       // From WoWBot
    // ADDED: Faction offset (placeholder, check common 3.3.5 layouts if needed)
    constexpr uintptr_t UNIT_FIELD_FACTION_TEMPLATE = 0x30 * 4;

    // VFTable indices (based on WoWBot)
    constexpr int VF_GetName = 54;

    // Descriptor field offsets (relative to descriptor base)
    constexpr uintptr_t OBJECT_FIELD_GUID = 0x00;     // Low/High GUID at 0x00/0x04
    constexpr uintptr_t OBJECT_FIELD_TYPE = 0x0C * 4; // 4 bytes per field, index * 4 = actual offset
    constexpr uintptr_t OBJECT_FIELD_ENTRY = 0x01 * 4;// From WoWBot
    constexpr uintptr_t OBJECT_FIELD_SCALE_X = 0x04 * 4; // Scale field offset (0x10)

} // namespace Offsets
// -----------------------------------------

// Forward declarations for derived types
class WowUnit;
class WowPlayer;
class WowGameObject;
// Add others (WowItem, WowContainer, WowCorpse) if needed later

// Base class for all World of Warcraft objects
class WowObject {
protected:
    uintptr_t m_baseAddress; // Base memory address of the object
    WGUID m_guid;          // Object's globally unique identifier
    WowObjectType m_type;    // Cached object type

    // --- Cached Data (NEW) ---
    std::string m_cachedName; 
    Vector3 m_cachedPosition;
    // Add more cached members as needed (e.g., rotation, scale)
    std::chrono::steady_clock::time_point m_lastCacheUpdateTime; // For potential future throttling

    // Helper to read name via VTable (based on WoWBot)
    std::string ReadNameFromVTable(); 

public:
    // Constructor taking pointer, guid, and type (used by default case in OM)
    WowObject(uintptr_t baseAddress, WGUID guid, WowObjectType type);
    // Constructor taking pointer and guid (type read internally)
    WowObject(uintptr_t baseAddress, WGUID guid);

    virtual ~WowObject() = default;

    // --- Common Accessors --- 

    WGUID GetGUID() const { return m_guid; }
    uint64_t GetGUID64() const { return m_guid.ToUint64(); }
    uintptr_t GetBaseAddress() const { return m_baseAddress; }
    WowObjectType GetType() const { return m_type; }

    // Virtual methods to be implemented by derived classes or read from memory
    virtual Vector3 GetPosition(); // Read position from memory
    virtual std::string GetName(); // Read name from memory (complex)
    
    // Add method to update dynamic data if needed (like position)
    // Make it virtual so derived classes can add their specific logic
    virtual void UpdateDynamicData();

    // --- Add IsValid Check ---
    virtual bool IsValid() const { return m_baseAddress != 0; }

    // --- Add Interaction Method (VTable based) ---
    virtual void Interact(); // VTable index 44

    // --- Add Getters for Cached Data ---
    Vector3 GetCachedPosition() const { return m_cachedPosition; }
    std::string GetCachedName() const { return m_cachedName; }

    // --- Type Casting Helpers --- 
    // These allow safe casting to derived types if needed

    // Check type without needing RTTI (dynamic_cast)
    bool IsPlayer() const { return m_type == OBJECT_PLAYER; }
    bool IsUnit() const { return m_type == OBJECT_UNIT || m_type == OBJECT_PLAYER; } // Units include players
    bool IsGameObject() const { return m_type == OBJECT_GAMEOBJECT; }
    // Add IsItem(), IsContainer(), IsCorpse() etc.

    // Check if unit is friendly to player
    virtual bool IsFriendly() const;

    // Declare casting helper methods (definitions will be in wowobject.cpp)
    WowPlayer* ToPlayer();
    WowUnit* ToUnit();
    WowGameObject* ToGameObject();
    const WowPlayer* ToPlayer() const;
    const WowUnit* ToUnit() const;
    const WowGameObject* ToGameObject() const;
    
    // If using shared_ptrs, provide static_pointer_cast helpers too (optional)
    // static std::shared_ptr<WowPlayer> AsPlayer(std::shared_ptr<WowObject> obj) { 
// ... rest of class definition
};
