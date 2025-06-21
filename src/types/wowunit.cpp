#include "wowunit.h"
#include "../utils/memory.h"   // Use relative path
#include "../logs/log.h"      // Use relative path
#include "types.h"            // Ensure types.h is included
#include "../objectManager/ObjectManager.h" // Added include for ObjectManager
#include <sstream>
#include <chrono>
#include <vector> // Add this at the top of the file with other includes
#include "types/wowunit.h"
#include "types/wowobject.h"
#include <string>
#include <cmath>

// Define PI if not already available globally or in a math constants header
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

const float PI_CONST = static_cast<float>(M_PI);
const float TWO_PI_CONST = 2.0f * PI_CONST;

// Placeholder for missing utility functions if they don\'t exist elsewhere
// These are simplified and might need to be replaced with proper implementations
// if they aren\'t already available through existing includes.
namespace { // Anonymous namespace for local helpers
    std::string GuidToStringLocal(uint64_t guid) {
        std::stringstream ss;
        ss << "0x" << std::hex << guid;
        return ss.str();
    }

    std::string HexToStringLocal(uintptr_t val) {
        std::stringstream ss;
        ss << "0x" << std::hex << val;
        return ss.str();
    }
}

// Constructor
WowUnit::WowUnit(uintptr_t baseAddress, WGUID guid) 
    : WowObject(baseAddress, guid, OBJECT_UNIT), // Explicitly set type
      m_cachedHealth(0),
      m_cachedMaxHealth(0),
      m_cachedLevel(0),
      m_cachedPowerType(PowerType::POWER_TYPE_MANA), // Default to Mana
      m_cachedUnitFlags(0),
      m_cachedUnitFlags2(0),
      m_cachedDynamicFlags(0),
      m_cachedCastingSpellId(0),
      m_cachedChannelSpellId(0),
      m_cachedTargetGUID(), // Initialize new member
      m_cachedFactionId(0),  // Initialize new member
      m_cachedCastingEndTimeMs(0),
      m_cachedChannelEndTimeMs(0),
      m_cachedMovementFlags(0), // RE-ADDED initializer
      m_cachedFacing(0.0f), // Renamed from m_cachedRotation
      m_cachedScale(1.0f),
      m_cachedIsInCombat(false),
      // Initialize Threat Data
      m_cachedHighestThreatTargetGUID(),
      m_cachedThreatManagerBasePtr(0),
      m_cachedTopThreatEntryPtr(0),
      // Initialize Player-Specific Global Data
      m_cachedComboPoints(0),
      m_cachedComboPointTargetGUID()
      // m_cachedThreatTableEntries is implicitly default-constructed (empty vector)
{
    // Initialize the power arrays to zeros
    for (int i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
        m_cachedPowers[i] = 0;
        m_cachedMaxPowers[i] = 0;
        m_hasPowerType[i] = false;
    }
    
    // Initial update can be deferred to ObjectManager::ProcessFoundObject
}

void WowUnit::ResetCache() {
    if (m_baseAddress == 0) {
        m_cachedHealth = 0;
        m_cachedMaxHealth = 0;
        // Initialize the power arrays to zeros
        for (int i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
            m_cachedPowers[i] = 0;
            m_cachedMaxPowers[i] = 0;
            m_hasPowerType[i] = false;
        }
        m_cachedPowerType = PowerType::POWER_TYPE_MANA; // Default
        m_cachedTargetGUID = WGUID();
        m_cachedLevel = 0;
        m_cachedUnitFlags = 0;
        m_cachedUnitFlags2 = 0;
        m_cachedDynamicFlags = 0;
        m_cachedCastingSpellId = 0;
        m_cachedChannelSpellId = 0;
        m_cachedFactionId = 0;
        m_cachedCastingEndTimeMs = 0;
        m_cachedChannelEndTimeMs = 0;
        m_cachedMovementFlags = 0; // RE-ADDED reset
        m_cachedFacing = 0.0f; // Renamed from m_cachedRotation
        m_cachedScale = 1.0f;
        m_cachedIsInCombat = false;

        // Reset Threat Data
        m_cachedHighestThreatTargetGUID = WGUID();
        m_cachedThreatManagerBasePtr = 0;
        m_cachedTopThreatEntryPtr = 0;
        m_cachedThreatTableEntries.clear();

        // Reset Player-Specific Global Data
        m_cachedComboPoints = 0;
        m_cachedComboPointTargetGUID = WGUID();
    }
}

// Override UpdateDynamicData for unit-specific fields
void WowUnit::UpdateDynamicData() {
    // { std::stringstream ss; ss << "[WowUnit::UpdateDynamicData] Processing GUID 0x" << std::hex << GetGUID64() << std::dec << " with baseAddr 0x" << std::hex << m_baseAddress << std::dec; Core::Log::Message(ss.str()); } // Added this log
    if (!m_baseAddress) {
        // Clear cache if object is invalid
        WowObject::UpdateDynamicData(); // Call base class to clear name etc.
        m_cachedPosition = Vector3();
        m_cachedHealth = 0;
        m_cachedMaxHealth = 0;
        for (int i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
            m_cachedPowers[i] = 0;
            m_cachedMaxPowers[i] = 0;
            m_hasPowerType[i] = false;
        }
        m_cachedPowerType = PowerType::POWER_TYPE_MANA; // Default
        m_cachedTargetGUID = WGUID();
        m_cachedLevel = 0;
        m_cachedUnitFlags = 0;
        m_cachedUnitFlags2 = 0;
        m_cachedDynamicFlags = 0;
        m_cachedCastingSpellId = 0;
        m_cachedChannelSpellId = 0;
        m_cachedFactionId = 0;
        m_cachedCastingEndTimeMs = 0;
        m_cachedChannelEndTimeMs = 0;
        m_cachedMovementFlags = 0; // RE-ADDED reset
        m_cachedFacing = 0.0f; // Renamed from m_cachedRotation
        m_cachedScale = 1.0f;
        m_cachedIsInCombat = false;
        // Reset other cached values specific to WowUnit if added later

        // Reset Player-Specific Global Data
        m_cachedComboPoints = 0;
        m_cachedComboPointTargetGUID = WGUID();

        // Reset Threat Data in case of invalid base address
        m_cachedHighestThreatTargetGUID = WGUID();
        m_cachedThreatManagerBasePtr = 0;
        m_cachedTopThreatEntryPtr = 0;
        m_cachedThreatTableEntries.clear();
        return;
    }

    // --- Base Class Update ---
    WowObject::UpdateDynamicData(); // Update name, etc.

    // Declare descriptorPtr outside the try blocks
    uintptr_t descriptorPtr = 0;

    // --- Unit-Specific Updates ---
    try {
        // Read Position (Units usually have position)
        float posX = Memory::Read<float>(m_baseAddress + Offsets::OBJECT_POS_X);
        float posY = Memory::Read<float>(m_baseAddress + Offsets::OBJECT_POS_Y);
        float posZ = Memory::Read<float>(m_baseAddress + Offsets::OBJECT_POS_Z);
        // { std::stringstream ss; ss << "[WowUnit::UpdateDynamicData] Read Pos for GUID 0x" << std::hex << GetGUID64() << std::dec << ": (" << posX << ", " << posY << ", " << posZ << ") from baseAddr 0x" << std::hex << m_baseAddress << std::dec; Core::Log::Message(ss.str()); } // Added this log
        m_cachedPosition.x = posY; // Game's X coordinate
        m_cachedPosition.y = posX; // Game's Y coordinate
        m_cachedPosition.z = posZ; // Game's Z coordinate

        // Read Target GUID (and get descriptor pointer)
        descriptorPtr = Memory::Read<uintptr_t>(m_baseAddress + Offsets::OBJECT_DESCRIPTOR_PTR); // Assign here
        if (descriptorPtr)
        {
            // Use offset 0x48 from UnitFields struct dump for Target GUID
            uint64_t targetGuid64 = Memory::Read<uint64_t>(descriptorPtr + 0x48); 
            m_cachedTargetGUID = WGUID(targetGuid64); // Construct WGUID from the uint64_t
        }
        else
        {
            m_cachedTargetGUID = WGUID(); // Clear if descriptor pointer is null
        }

    } catch (const MemoryAccessError& e) { // Correct exception type
        std::stringstream ss;
        ss << "[WowUnit::UpdateDynamicData] Position/Target Read Exception for GUID 0x" << std::hex << GetGUID64() << std::dec << ": " << e.what();
        Core::Log::Message(ss.str());
        // If position read fails, invalidate it. Keep potentially valid name.
        m_cachedPosition = Vector3(); 
        m_cachedTargetGUID = WGUID();
        m_cachedChannelSpellId = 0;
        m_cachedCastingEndTimeMs = 0;
        m_cachedChannelEndTimeMs = 0;
        m_cachedMovementFlags = 0; // RE-ADDED reset in catch
        m_cachedFacing = 0.0f; // Renamed from m_cachedRotation in catch
    } catch (...) { // Catch all other exceptions
         std::stringstream ss_unknown;
         ss_unknown << "[WowUnit::UpdateDynamicData] Unknown exception during Position/Target read for GUID 0x" << std::hex << GetGUID64() << std::dec;
         Core::Log::Message(ss_unknown.str()); 
         m_cachedPosition = Vector3();
         m_cachedTargetGUID = WGUID();
         // return; 
        m_cachedChannelSpellId = 0;
        m_cachedCastingEndTimeMs = 0;
        m_cachedChannelEndTimeMs = 0;
        m_cachedMovementFlags = 0; // RE-ADDED reset in catch
        m_cachedFacing = 0.0f; // Renamed from m_cachedRotation in catch
    }

    // --- Read Movement Flags (Using correct 0xD8 pointer offset) ---
    try {
        // Get the known local player GUID directly from the ObjectManager
        WGUID knownLocalPlayerGuid = ObjectManager::GetInstance()->GetLocalPlayerGuid();
        
        // Compare this unit's GUID directly with the known local player GUID
        bool isLocalPlayer = knownLocalPlayerGuid.IsValid() && (knownLocalPlayerGuid.ToUint64() == this->GetGUID64());

        if (isLocalPlayer) { 
            uintptr_t movementComponentPtr = 0;
            // 1. Try reading the POINTER to the CMovement component
            try {
                 movementComponentPtr = Memory::Read<uintptr_t>(m_baseAddress + Offsets::UNIT_MOVEMENT_COMPONENT_PTR);
                 // { std::stringstream ss; ss << "[MovementDebug] Read movementComponentPtr: 0x" << std::hex << movementComponentPtr; Core::Log::Message(ss.str()); } // Keep commented
            } catch (const MemoryAccessError& /*e_ptr*/) {
                 // Log error reading the pointer
                 // { std::stringstream ss_err; ss_err << "[MovementDebug] *** MemoryAccessError reading movement POINTER at 0xD8 for local player 0x" << std::hex << GetGUID64() << ": " << e_ptr.what(); Core::Log::Message(ss_err.str()); } // Commented out
                 m_cachedMovementFlags = 0;
                 movementComponentPtr = 0; // Ensure ptr is 0 on error
            } catch (...) {
                 // Log unknown error reading the pointer
                 // { std::stringstream ss_err; ss_err << "[MovementDebug] *** Unknown EXCEPTION reading movement POINTER at 0xD8 for local player 0x" << std::hex << GetGUID64(); Core::Log::Message(ss_err.str()); } // Commented out
                 m_cachedMovementFlags = 0;
                 movementComponentPtr = 0; // Ensure ptr is 0 on error
            }
            
            if (movementComponentPtr != 0) { // Simplified check: Proceed only if pointer read succeeded AND is not null
                // 2. Pointer read OK and is not null, try reading the flags from [movementComponentPtr + 0x44]
                try {
                    m_cachedMovementFlags = Memory::Read<uint32_t>(movementComponentPtr + Offsets::MOVEMENT_FLAGS);
                    // Log the flags read successfully
                    // { std::stringstream ss; ss << "[MovementDebug] Read m_cachedMovementFlags for local player: 0x" << std::hex << m_cachedMovementFlags; Core::Log::Message(ss.str()); }  // Commented out
                } catch (const MemoryAccessError& /*e_flags*/) {
                    // Log error reading flags
                    // { std::stringstream ss_err; ss_err << "[MovementDebug] *** MemoryAccessError reading movement FLAGS at ptr+0x44 for local player 0x" << std::hex << GetGUID64() << ": " << e_flags.what(); Core::Log::Message(ss_err.str()); } // Commented out
                    m_cachedMovementFlags = 0;
                } catch (...) {
                    // Log unknown error reading flags
                    // { std::stringstream ss_err; ss_err << "[MovementDebug] *** Unknown EXCEPTION reading movement FLAGS at ptr+0x44 for local player 0x" << std::hex << GetGUID64(); Core::Log::Message(ss_err.str()); } // Commented out
                    m_cachedMovementFlags = 0;
                }
            } else {
                // Pointer was null or read failed (error already logged above)
                m_cachedMovementFlags = 0; 
            }
        } else {
            // Not the local player, ensure flags are zero
            m_cachedMovementFlags = 0; 
        }
    } catch (...) { // Outer catch for safety, log any unexpected errors
        // Log unexpected outer exception
        // { std::stringstream ss_err; ss_err << "[MovementDebug] *** UNHANDLED Outer EXCEPTION *** in movement block for GUID 0x" << std::hex << GetGUID64() << ". Flags set to 0."; Core::Log::Message(ss_err.str()); } // Commented out
        m_cachedMovementFlags = 0;
    }
    // --- End Movement Flags ---

    // --- Read Descriptor Fields (Potentially less critical, try separately) ---
    try {
         if (descriptorPtr) {
            m_cachedHealth = Memory::Read<int>(descriptorPtr + Offsets::UNIT_FIELD_HEALTH);
            m_cachedMaxHealth = Memory::Read<int>(descriptorPtr + Offsets::UNIT_FIELD_MAXHEALTH);
            m_cachedLevel = Memory::Read<int>(descriptorPtr + Offsets::UNIT_FIELD_LEVEL);
            
            // Power Type is a single byte, not a full field (WoWBot method)
            m_cachedPowerType = Memory::Read<uint8_t>(descriptorPtr + Offsets::DESCRIPTOR_FIELD_POWTYPE); 
            
            // --- Restore Power Reading Loop from Backup --- 
            // Initialize arrays to ensure clean state
            for (int i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
                m_hasPowerType[i] = false;
                m_cachedPowers[i] = 0;
                m_cachedMaxPowers[i] = 0;
            }

            // Read values for ALL power types, not just the primary one (Matches Backup)
            for (uint8_t powerType = 0; powerType < PowerType::POWER_TYPE_COUNT; powerType++) {
                // Skip unsupported power types
                if (powerType == 5) continue; // Index 5 is unused in WoW 3.3.5

                uintptr_t powerOffset = Offsets::UNIT_FIELD_POWER_BASE + (powerType * 4);
                uintptr_t maxPowerOffset = Offsets::UNIT_FIELD_MAXPOWER_BASE + (powerType * 4);
                
                int power = Memory::Read<int>(descriptorPtr + powerOffset);
                int maxPower = Memory::Read<int>(descriptorPtr + maxPowerOffset);
                
                m_cachedPowers[powerType] = power;
                m_cachedMaxPowers[powerType] = maxPower;
                
                // Mark this power type as active if it has a max value
                if (maxPower > 0) {
                    m_hasPowerType[powerType] = true;
                }
            }
            // --- End Restore Power Reading Loop --- 

            m_cachedUnitFlags = Memory::Read<uint32_t>(descriptorPtr + Offsets::UNIT_FIELD_FLAGS);
            // Keep new flags commented out
            // m_cachedUnitFlags2 = Memory::Read<uint32_t>(descriptorPtr + UNIT_FIELD_FLAGS_2);
            // m_cachedDynamicFlags = Memory::Read<uint32_t>(descriptorPtr + UNIT_DYNAMIC_FLAGS);
            // Read Faction using the added offset
            m_cachedFactionId = Memory::Read<uint32_t>(descriptorPtr + Offsets::UNIT_FIELD_FACTION_TEMPLATE);

             // --- Restore Casting/Channeling Spell ID Reads from Backup --- 
             // (Reading from object base, like backup)
             m_cachedCastingSpellId = Memory::Read<uint32_t>(m_baseAddress + Offsets::OBJECT_CASTING_ID); // Assumes OBJECT_CASTING_ID is defined globally
             m_cachedChannelSpellId = Memory::Read<uint32_t>(m_baseAddress + Offsets::OBJECT_CHANNEL_ID); // Assumes OBJECT_CHANNEL_ID is defined globally
             // Uncomment EndTimeMs reads
             m_cachedCastingEndTimeMs = Memory::Read<uint32_t>(m_baseAddress + Offsets::OBJECT_CASTING_END_TIME);
             m_cachedChannelEndTimeMs = Memory::Read<uint32_t>(m_baseAddress + Offsets::OBJECT_CHANNEL_END_TIME);
             // --- End Restore Casting/Channeling Reads --- 

        } else {
            // Handle case where descriptor pointer is null (e.g., clear cached values)
            m_cachedHealth = 0;
            m_cachedMaxHealth = 0;
            m_cachedLevel = 0;
            m_cachedPowerType = PowerType::POWER_TYPE_MANA;
            for (int i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
                m_cachedPowers[i] = 0;
                m_cachedMaxPowers[i] = 0;
                m_hasPowerType[i] = false;
            }
            m_cachedUnitFlags = 0;
            // Reset commented-out flags as well
            m_cachedUnitFlags2 = 0;
            m_cachedDynamicFlags = 0;
            m_cachedFactionId = 0;
            m_cachedCastingSpellId = 0;
            m_cachedChannelSpellId = 0;
            m_cachedCastingEndTimeMs = 0;
            m_cachedChannelEndTimeMs = 0;
            m_cachedMovementFlags = 0; // RE-ADDED reset
            m_cachedFacing = 0.0f; // Renamed from m_cachedRotation
        }

    } catch (const MemoryAccessError& /*e*/) {
         std::stringstream ss;
         // ss << "[WowUnit::UpdateDynamicData] Descriptor Read Exception for GUID 0x" << std::hex << GetGUID64() << ": " << e.what(); // Commented out
         // Core::Log::Message(ss.str()); // Commented out
         // Invalidate descriptor fields on error
         m_cachedHealth = 0;
         m_cachedMaxHealth = 0;
         for (int i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
             m_cachedPowers[i] = 0;
             m_cachedMaxPowers[i] = 0;
             m_hasPowerType[i] = false;
         }
         m_cachedPowerType = PowerType::POWER_TYPE_MANA;
         m_cachedLevel = 0;
         m_cachedFactionId = 0;
         m_cachedUnitFlags = 0;
         // Reset commented-out flags as well
         m_cachedUnitFlags2 = 0;
         m_cachedDynamicFlags = 0;
         m_cachedCastingSpellId = 0;
         m_cachedChannelSpellId = 0;
         m_cachedCastingEndTimeMs = 0;
         m_cachedChannelEndTimeMs = 0;
         m_cachedMovementFlags = 0; // RE-ADDED reset in catch
         m_cachedFacing = 0.0f; // Renamed from m_cachedRotation in catch
    } catch (...) {
         // Core::Log::Message("[WowUnit::UpdateDynamicData] Unknown exception during descriptor read for GUID 0x" + std::to_string(GetGUID64())); // Commented out
         // Invalidate descriptor fields on error
         m_cachedHealth = 0;
         m_cachedMaxHealth = 0;
         for (int i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
             m_cachedPowers[i] = 0;
             m_cachedMaxPowers[i] = 0;
             m_hasPowerType[i] = false;
         }
         m_cachedPowerType = PowerType::POWER_TYPE_MANA;
         m_cachedLevel = 0;
         m_cachedFactionId = 0;
         m_cachedUnitFlags = 0;
         // Reset commented-out flags as well
         m_cachedUnitFlags2 = 0;
         m_cachedDynamicFlags = 0;
         m_cachedCastingSpellId = 0;
         m_cachedChannelSpellId = 0;
         m_cachedCastingEndTimeMs = 0;
         m_cachedChannelEndTimeMs = 0;
         m_cachedMovementFlags = 0; // RE-ADDED reset in catch
         m_cachedFacing = 0.0f; // Renamed from m_cachedRotation in catch
    }

    // --- Read Facing using direct offset ---
    if (m_baseAddress != 0) { // Ensure base address is valid
        try {
            m_cachedFacing = Memory::Read<float>(m_baseAddress + Offsets::OBJECT_FACING_OFFSET); // Renamed from m_cachedRotation
        } catch (const std::exception& /*e*/) { // Marked 'e' as unused
            // Log or handle memory read error for facing
            // Core::Log::Error("Failed to read facing for GUID " + m_guid.ToString() + ": " + e.what());
            m_cachedFacing = 0.0f; // Default to 0 on error. Renamed from m_cachedRotation
        }
    } else {
        m_cachedFacing = 0.0f; // Default to 0 if base address is invalid. Renamed from m_cachedRotation
    }
    // --- End Read Facing ---

    // --- Read Threat Data (This unit's threat on its top target) ---
    m_cachedThreatTableEntries.clear(); // Clear previous entries
    if (m_baseAddress != 0) {
        try {
            // Offsets (Ideally from types.h or a global Offsets struct)
            const uintptr_t UNIT_HIGHEST_THREAT_AGAINST_GUID_OFFSET = 0xFD8;
            const uintptr_t UNIT_THREAT_MANAGER_BASE_OFFSET = 0xFE0;
            const uintptr_t UNIT_OWN_TOP_THREAT_ENTRY_PTR_OFFSET = 0xFEC;

            const uintptr_t THREAT_ENTRY_TARGET_GUID_OFFSET = 0x20;
            const uintptr_t THREAT_ENTRY_STATUS_OFFSET = 0x28;
            const uintptr_t THREAT_ENTRY_PERCENTAGE_OFFSET = 0x29;
            const uintptr_t THREAT_ENTRY_RAW_VALUE_OFFSET = 0x2C;

            uint64_t highestThreatTargetGuid64 = Memory::Read<uint64_t>(m_baseAddress + UNIT_HIGHEST_THREAT_AGAINST_GUID_OFFSET);
            m_cachedHighestThreatTargetGUID = WGUID(highestThreatTargetGuid64);

            m_cachedThreatManagerBasePtr = Memory::Read<uintptr_t>(m_baseAddress + UNIT_THREAT_MANAGER_BASE_OFFSET);
            m_cachedTopThreatEntryPtr = Memory::Read<uintptr_t>(m_baseAddress + UNIT_OWN_TOP_THREAT_ENTRY_PTR_OFFSET);

            if (m_cachedTopThreatEntryPtr != 0) {
                ThreatEntry topEntry;
                uint64_t entryTargetGuid64 = Memory::Read<uint64_t>(m_cachedTopThreatEntryPtr + THREAT_ENTRY_TARGET_GUID_OFFSET);
                topEntry.targetGUID = WGUID(entryTargetGuid64);
                topEntry.status = Memory::Read<uint8_t>(m_cachedTopThreatEntryPtr + THREAT_ENTRY_STATUS_OFFSET);
                topEntry.percentage = Memory::Read<uint8_t>(m_cachedTopThreatEntryPtr + THREAT_ENTRY_PERCENTAGE_OFFSET);
                topEntry.rawValue = Memory::Read<uint32_t>(m_cachedTopThreatEntryPtr + THREAT_ENTRY_RAW_VALUE_OFFSET);
                
                // Attempt to resolve the name of the target in the threat entry
                ObjectManager* objMgr = ObjectManager::GetInstance();
                if (objMgr) {
                    auto targetObj = objMgr->GetObjectByGUID(topEntry.targetGUID);
                    if (targetObj) {
                        topEntry.targetName = targetObj->GetName();
                    } else {
                        topEntry.targetName = "Unknown Target";
                    }
                } else {
                    topEntry.targetName = "ObjMgr N/A";
                }

                m_cachedThreatTableEntries.push_back(topEntry);
            }
        } catch (const MemoryAccessError& e) {
            // Log sparingly or handle error, e.g., clear threat data
            // std::stringstream ss; ss << "[WowUnit::UpdateDynamicData] MemoryAccessError reading threat data for GUID 0x" << std::hex << GetGUID64() << ": " << e.what(); Core::Log::Message(ss.str());
            m_cachedHighestThreatTargetGUID = WGUID();
            m_cachedThreatManagerBasePtr = 0;
            m_cachedTopThreatEntryPtr = 0;
            m_cachedThreatTableEntries.clear();
        } catch (...) {
            // std::stringstream ss; ss << "[WowUnit::UpdateDynamicData] Unknown exception reading threat data for GUID 0x" << std::hex << GetGUID64(); Core::Log::Message(ss.str());
            m_cachedHighestThreatTargetGUID = WGUID();
            m_cachedThreatManagerBasePtr = 0;
            m_cachedTopThreatEntryPtr = 0;
            m_cachedThreatTableEntries.clear();
        }
    } else {
        // Base address is null, ensure threat data is cleared
        m_cachedHighestThreatTargetGUID = WGUID();
        m_cachedThreatManagerBasePtr = 0;
        m_cachedTopThreatEntryPtr = 0;
        m_cachedThreatTableEntries.clear();
    }
    // --- End Read Threat Data ---

    // Update other unit-specific dynamic data
    // For example, movement flags, channel info (already done above for some)

    // Update timestamp only if at least basic reads succeeded
    m_lastCacheUpdateTime = std::chrono::steady_clock::now();

    // --- Read Player-Specific Global Data (if this is the local player) ---
    WGUID localPlayerGuid = ObjectManager::GetInstance()->GetLocalPlayerGuid();
    if (localPlayerGuid.IsValid() && localPlayerGuid.ToUint64() == GetGUID64()) {
        try {
            m_cachedComboPoints = Memory::Read<uint8_t>(Offsets::COMBO_POINTS_ADDR); // Read from address 0x00BD084D
            uint64_t comboTargetGuid64 = Memory::Read<uint64_t>(Offsets::COMBO_POINTS_TARGET_GUID_ADDR); // Read from address 0xBD08A8
            m_cachedComboPointTargetGUID = WGUID(comboTargetGuid64);
            
            // DEBUG - Log combo points data, but only for significant changes
            static uint8_t lastComboPoints = 0;
            static uint64_t lastTargetGuid = 0;
            if (lastComboPoints != m_cachedComboPoints || lastTargetGuid != comboTargetGuid64) {
                std::stringstream ss_cp;
                ss_cp << "[ComboPointsDebug] Reading combo points data:"
                      << " Points=" << static_cast<int>(m_cachedComboPoints)
                      << ", Target GUID=0x" << std::hex << comboTargetGuid64 << std::dec;
                Core::Log::Message(ss_cp.str());
                lastComboPoints = m_cachedComboPoints;
                lastTargetGuid = comboTargetGuid64;
            }
        } catch (const MemoryAccessError& /*e*/) {
            // Silently handle memory read errors
            m_cachedComboPoints = 0;
            m_cachedComboPointTargetGUID = WGUID();
        } catch (...) {
            // Silently handle unknown exceptions
            m_cachedComboPoints = 0;
            m_cachedComboPointTargetGUID = WGUID();
        }
    }
}

// GetPower with special handling for Rage (stored * 10 in 3.3.5)
/* REMOVING REDEFINITION - Defined in header
int WowUnit::GetPower() const {
    return GetPowerByType(m_cachedPowerType);
}
*/

// Helper to get power type as string
std::string WowUnit::GetPowerTypeString() const {
    switch (static_cast<PowerType>(m_cachedPowerType)) {
        case PowerType::POWER_TYPE_MANA:       return "Mana";
        case PowerType::POWER_TYPE_RAGE:       return "Rage";
        case PowerType::POWER_TYPE_FOCUS:      return "Focus";
        case PowerType::POWER_TYPE_ENERGY:     return "Energy";
        case PowerType::POWER_TYPE_HAPPINESS:  return "Happiness";
        case PowerType::POWER_TYPE_RUNE:       return "Rune"; // DK specific? Need enum update
        case PowerType::POWER_TYPE_RUNIC_POWER: return "Runic Power"; // DK specific?
        default:                    return "Unknown (" + std::to_string(m_cachedPowerType) + ")";
    }
}

// Get Position (Overridden from WowObject)
Vector3 WowUnit::GetPosition() {
    if (!m_baseAddress) return Vector3(); // Return zero vector if invalid
    
    // Use cached value updated by UpdateDynamicData
    return m_cachedPosition;
}

// Check if unit is friendly to player based on faction
bool WowUnit::IsFriendly() const {
    // This is a simplified implementation
    // In WoW, faction relationships are complex and depend on reputation, etc.
    
    // Some known friendly faction IDs (Alliance/Horde/Neutral)
    static const std::vector<uint32_t> friendlyFactions = {
        2, 5, 6, 69, 72, 469, 67, 471, 1604, 1610, 1629, 1630, 1666, 1791, 
        54, 68, 76, 81, 530, 911, 1595, 1671, 1894, 
        11, 12, 35, 55, 68, 72, 76, 79, 80, 123, 
        271, 469, 474, 495, 530, 577, 1638, 1639, 1640
        // Common town NPC factions (vendors, guards, etc.)
    };
    
    // Check if current faction is in our friendly list
    for (const auto& faction : friendlyFactions) {
        if (m_cachedFactionId == faction) {
            return true;
        }
    }
    
    // Alternative: Could check if unit is not attackable
    if ((m_cachedUnitFlags & 0x08) != 0) { // UnitFlag: Not Attackable
        return true;
    }
    
    // Check if unit is a player of the same faction 
    // (would need to compare to player's faction)
    
    // Default return - neutral/hostile
    return false;
}

// Gets reaction from this unit to another unit using WoW's native reaction function
// Returns: 1=Hostile, 2=Unfriendly, 3=Neutral, 4=Friendly/Self, 5-8=Higher friendship levels
int WowUnit::GetReaction(WowUnit* otherUnit) const {
    // Define the function type for the WoW reaction function
    // int __thiscall determineUnitInteraction(WoWUnit* this_unit, WoWUnit* other_unit);
    typedef int (__thiscall *DetermineUnitInteractionFn)(void* thisUnit, void* otherUnit);
    
    // Address from the disassembly provided
    static const DetermineUnitInteractionFn determineUnitInteraction = 
        reinterpret_cast<DetermineUnitInteractionFn>(0x7251C0);
    
    // Call the native function
    try {
        if (!otherUnit) return 0; // Invalid target
        
        // Skip logging to avoid const issues
        // Use const_cast to access the non-const BaseAddress method
        void* thisWowUnitPtr = reinterpret_cast<void*>(const_cast<WowUnit*>(this)->GetBaseAddress());
        void* otherWowUnitPtr = reinterpret_cast<void*>(otherUnit->GetBaseAddress());
        
        if (!thisWowUnitPtr || !otherWowUnitPtr) return 0;
        
        // Call the game's function
        int reaction = determineUnitInteraction(thisWowUnitPtr, otherWowUnitPtr);
        
        return reaction;
    }
    catch (const std::exception& /*e*/) { // Marked 'e' as unused
        return 0;
    }
    catch (...) {
        return 0;
    }
}

// Check if unit is hostile based on reaction
bool WowUnit::IsHostile() const {
    // NOTE: This is a simplified implementation
    // In actual WoW, this would use GetReaction with the player unit
    // For now, just using the opposite of IsFriendly
    return !IsFriendly();
}

// Check if unit is attackable (can be targeted by harmful spells)
bool WowUnit::IsAttackable() const {
    // Unit not attackable if it has the "not attackable" flag
    // Common UnitFlags: 0x08 = Not Attackable
    if ((m_cachedUnitFlags & 0x08) != 0) {
        return false;
    }
    
    // Unit is dead - can't attack
    if (IsDead()) {
        return false;
    }
    
    // Unit is in a non-combat state (should check more flags here)
    if ((m_cachedUnitFlags & 0x02) != 0) { // UNIT_FLAG_NON_ATTACKABLE
        return false;
    }
    
    // This would ideally use GetReaction with player unit and check reaction <= 2
    // Reactions 1 (Hostile) and 2 (Unfriendly) are attackable, 3+ are not
    
    return IsHostile();
}

// Helper for displaying a specific power type as string
std::string WowUnit::GetPowerTypeString(uint8_t powerType) const {
    switch (static_cast<PowerType>(powerType)) {
        case PowerType::POWER_TYPE_MANA:       return "Mana";
        case PowerType::POWER_TYPE_RAGE:       return "Rage";
        case PowerType::POWER_TYPE_FOCUS:      return "Focus";
        case PowerType::POWER_TYPE_ENERGY:     return "Energy";
        case PowerType::POWER_TYPE_HAPPINESS:  return "Happiness";
        case PowerType::POWER_TYPE_RUNE:       return "Rune";
        case PowerType::POWER_TYPE_RUNIC_POWER: return "Runic Power";
        default:                    return "Unknown (" + std::to_string(powerType) + ")";
    }
}

// This code is now redundant since it's defined inline in the header
/*
int WowUnit::GetPowerByType(uint8_t powerType) const {
    if (powerType < PowerType::POWER_TYPE_COUNT) {
        return m_cachedPowers[powerType];
    }
    return 0;
}

int WowUnit::GetMaxPowerByType(uint8_t powerType) const {
    if (powerType < PowerType::POWER_TYPE_COUNT) {
        return m_cachedMaxPowers[powerType];
    }
    return 0;
}
*/ 

// --- Implementation of IsCasting / IsChanneling ---

// Helper function to get current time in milliseconds using std::chrono
// Needed for comparing against cast/channel end times
uint64_t GetCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool WowUnit::IsCasting() const {
    // Check based on analysis: SpellID != 0 and CurrentTime < EndTime
    if (m_cachedCastingSpellId == 0) {
        return false;
    }
    uint64_t currentTimeMs = GetCurrentTimeMs(); 
    // Note: WoW uses 32-bit DWORDs for time, potential wrap-around ignored for now
    return currentTimeMs < m_cachedCastingEndTimeMs; 
}

bool WowUnit::IsChanneling() const {
    // Check based on analysis: SpellID != 0 and CurrentTime < EndTime
    if (m_cachedChannelSpellId == 0) {
        return false;
    }
    uint64_t currentTimeMs = GetCurrentTimeMs();
    // Note: WoW uses 32-bit DWORDs for time, potential wrap-around ignored for now
    return currentTimeMs < m_cachedChannelEndTimeMs;
}

//--- End of IsCasting / IsChanneling ---

// --- Implementation of IsMoving RE-ADDED ---
bool WowUnit::IsMoving() const {
    // Check if any common movement flags are set based on server-side enum analysis.
    const uint32_t ACTIVE_LOCOMOTION_MASK =
        0x00000001 | // MOVEMENTFLAG_FORWARD
        0x00000002 | // MOVEMENTFLAG_BACKWARD
        0x00000004 | // MOVEMENTFLAG_STRAFE_LEFT
        0x00000008 | // MOVEMENTFLAG_STRAFE_RIGHT
        0x00001000 | // MOVEMENTFLAG_FALLING
        0x00002000 | // MOVEMENTFLAG_FALLING_FAR (Maybe redundant?)
        0x00200000 | // MOVEMENTFLAG_SWIMMING
        0x00400000 | // MOVEMENTFLAG_ASCENDING
        0x00800000 | // MOVEMENTFLAG_DESCENDING
        0x02000000 | // MOVEMENTFLAG_FLYING
        0x04000000 | // MOVEMENTFLAG_SPLINE_ELEVATION
        0x08000000 | // MOVEMENTFLAG_SPLINE_ENABLED
        0x40000000;  // MOVEMENTFLAG_HOVER
        // Note: Excludes TURNING (0x10, 0x20), PITCHING (0x40, 0x80), WALKING (0x100), ROOT (0x800), etc.
                                              
    return (m_cachedMovementFlags & ACTIVE_LOCOMOTION_MASK) != 0;
}
// --- End of IsMoving RE-ADDED ---

// Add the missing definition for IsInCombat
bool WowUnit::IsInCombat() const {
    return (m_cachedUnitFlags & UNIT_FLAG_IN_COMBAT) != 0;
}

// NEW: Implementation for HasTarget
bool WowUnit::HasTarget() const {
    return m_cachedTargetGUID.IsValid(); // Check if the cached target GUID is valid
}

// Helper function to normalize an angle to the range [-PI, PI]
// Placed here as a static helper or in an anonymous namespace if preferred
static float normalizeAngleHelper_WowUnit(float angle) {
    angle = std::fmod(angle + PI_CONST, TWO_PI_CONST);
    if (angle < 0.0f) {
        angle += TWO_PI_CONST;
    }
    return angle - PI_CONST; // Result is in [-PI, PI]
}

// Implementation for IsFacingUnit
bool WowUnit::IsFacingUnit(const WowUnit* targetUnit, float coneAngleDegrees) const {
    if (!targetUnit) {
        return false; // Cannot check facing against a null target
    }

    Vector3 currentUnitPos = this->GetCachedPosition();
    float currentUnitFacingRad = this->GetFacing(); // Assumes GetFacing() returns radians [0, 2*PI)
    Vector3 targetUnitPos = targetUnit->GetCachedPosition();

    float deltaX = targetUnitPos.x - currentUnitPos.x;
    float deltaY = targetUnitPos.y - currentUnitPos.y;

    if (std::fabs(deltaX) < 0.001f && std::fabs(deltaY) < 0.001f) {
        return true; // Same position, considered facing
    }

    float angleToTargetRad = std::atan2(deltaY, deltaX); // Returns angle in [-PI, PI]

    // Normalize currentUnitFacingRad to [-PI, PI] for consistent comparison with atan2 output
    // OR normalize angleToTargetRad to [0, 2*PI) to match currentUnitFacingRad's typical range.
    // Let's normalize currentUnitFacingRad to [-PI, PI] to make the angleDifference math cleaner with atan2's output.
    // float normalizedCurrentUnitFacingRad = normalizeAngleHelper_WowUnit(currentUnitFacingRad);
    // float angleDifference = angleToTargetRad - normalizedCurrentUnitFacingRad;

    // Alternative: Keep currentUnitFacingRad as [0, 2*PI) and adjust angleToTargetRad to [0, 2*PI)
    if (angleToTargetRad < 0.0f) {
        angleToTargetRad += TWO_PI_CONST;
    }
    // Now both angleToTargetRad and currentUnitFacingRad are conceptually in [0, 2*PI)
    // (currentUnitFacingRad is already, angleToTargetRad is now)

    float angleDifference = angleToTargetRad - currentUnitFacingRad;

    // Normalize the final difference to be the smallest angle (between -PI and PI)
    angleDifference = normalizeAngleHelper_WowUnit(angleDifference);

    float coneAngleRad = coneAngleDegrees * (PI_CONST / 180.0f);

    return std::fabs(angleDifference) <= (coneAngleRad / 2.0f);
}

/* REMOVING REDEFINITION - Defined in header
bool WowUnit::IsDead() const {
    return (m_cachedUnitFlags & UNIT_FLAG_DEAD) != 0;
}
*/

float WowUnit::GetHealthPercent() const {
    if (m_cachedMaxHealth == 0) {
        return 0.0f;
    }
    return (static_cast<float>(m_cachedHealth) / m_cachedMaxHealth) * 100.0f;
}
