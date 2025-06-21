#pragma once

#include "wowobject.h"
#include <vector>
#include <string>

// Represents a single entry in a unit's threat table,
// detailing its threat against a specific target.
struct ThreatEntry {
    WGUID targetGUID;        // GUID of the unit this threat is against
    uint8_t status;          // Threat status (e.g., tanking, high, low)
    uint8_t percentage;      // Threat percentage
    uint32_t rawValue;       // Raw numerical threat value
    std::string targetName;  // Cached name of the target, resolved by ObjectManager

    ThreatEntry() : status(0), percentage(0), rawValue(0) {}
};

// Represents Unit objects (Players, NPCs)
class WowUnit : public WowObject {
protected:
    // --- Cached Unit-Specific Data ---
    int m_cachedHealth = 0;
    int m_cachedMaxHealth = 0;
    int m_cachedLevel = 0;
    uint8_t m_cachedPowerType = 0; // Primary power type
    int m_cachedPowers[PowerType::POWER_TYPE_COUNT] = {0}; // Power values for all types
    int m_cachedMaxPowers[PowerType::POWER_TYPE_COUNT] = {0}; // Max power values for all types
    bool m_hasPowerType[PowerType::POWER_TYPE_COUNT] = {false}; // Tracks which power types are active
    uint32_t m_cachedUnitFlags = 0;
    uint32_t m_cachedUnitFlags2 = 0; // Added for UNIT_FIELD_FLAGS_2
    uint32_t m_cachedDynamicFlags = 0; // Added for UNIT_DYNAMIC_FLAGS
    uint32_t m_cachedCastingSpellId = 0;
    uint32_t m_cachedChannelSpellId = 0;
    uint32_t m_cachedCastingEndTimeMs = 0;
    uint32_t m_cachedChannelEndTimeMs = 0;
    WGUID m_cachedTargetGUID;
    uint32_t m_cachedFactionId; // Renamed/Corrected from m_cachedFaction
    uint32_t m_cachedMovementFlags = 0; // RE-ADDED for movement check
    float m_cachedScale;
    float m_cachedFacing; // Renamed from m_cachedRotation

    bool m_cachedIsInCombat = false; // Added missing declaration

    // --- Cached Threat Data (This unit's threat ON others) ---
    WGUID m_cachedHighestThreatTargetGUID;     // GUID of the unit this unit has highest threat against (is tanking)
    uintptr_t m_cachedThreatManagerBasePtr;    // Pointer to this unit's ThreatManager structure in game memory
    uintptr_t m_cachedTopThreatEntryPtr;       // Pointer to the top ThreatEntry in this unit's ThreatManager
    std::vector<ThreatEntry> m_cachedThreatTableEntries; // Stores entries from this unit's threat table (currently only top one)

    // For pathing
    Vector3 m_targetPosition;

    // --- Cached Player-Specific Global Data (only populated if this unit is the local player) ---
    uint8_t m_cachedComboPoints = 0;
    WGUID m_cachedComboPointTargetGUID;

public:
    // Define constants for unit flags
    static const uint32_t UNIT_FLAG_IN_COMBAT = 0x00080000; // Corrected to standard AffectingCombat flag
    static const uint32_t UNIT_FLAG_FLEEING = 0x00800000; // Corrected to the specific fleeing bit

    // Constructor matching ObjectManager usage
    WowUnit(uintptr_t baseAddress, WGUID guid);

    virtual ~WowUnit() = default;

    // Override UpdateDynamicData for unit-specific fields
    virtual void UpdateDynamicData() override;
    
    // Reset cached data
    void ResetCache();

    // --- Accessors for Cached Unit Data ---
    int GetHealth() const { return m_cachedHealth; }
    int GetMaxHealth() const { return m_cachedMaxHealth; }
    int GetLevel() const { return m_cachedLevel; }
    float GetHealthPercent() const;
    int GetPower() const { return m_cachedPowers[m_cachedPowerType]; } // For primary power type
    int GetMaxPower() const { return m_cachedMaxPowers[m_cachedPowerType]; }
    uint8_t GetPowerType() const { return m_cachedPowerType; }
    uint32_t GetUnitFlags() const { return m_cachedUnitFlags; } // Inline getter
    uint32_t GetUnitFlags2() const { return m_cachedUnitFlags2; }
    uint32_t GetDynamicFlags() const { return m_cachedDynamicFlags; }
    uint32_t GetCastingSpellId() const { return m_cachedCastingSpellId; } // Inline getter
    uint32_t GetChannelSpellId() const { return m_cachedChannelSpellId; } // Inline getter
    // m_cachedCastingEndTimeMs and m_cachedChannelEndTimeMs are used by IsCasting/IsChanneling, no direct public getters usually needed
    WGUID GetTargetGUID() const { return m_cachedTargetGUID; }
    uint32_t GetFactionId() const { return m_cachedFactionId; }
    uint32_t GetMovementFlags() const { return m_cachedMovementFlags; } // RE-ADDED getter for raw flags
    
    // Get the scale of the unit (default to 1.0 if not available)
    float GetScale() const { return m_cachedScale; }
    float GetFacing() const { return m_cachedFacing; } // Updated to return m_cachedFacing

    // --- Accessors for Player-Specific Global Data ---
    uint8_t GetComboPoints() const { return m_cachedComboPoints; }
    WGUID GetComboPointTargetGUID() const { return m_cachedComboPointTargetGUID; }

    // --- Accessors for Cached Threat Data ---
    WGUID GetHighestThreatTargetGUID() const { return m_cachedHighestThreatTargetGUID; }
    uintptr_t GetThreatManagerBasePtr() const { return m_cachedThreatManagerBasePtr; }
    uintptr_t GetTopThreatEntryPtr() const { return m_cachedTopThreatEntryPtr; }
    const std::vector<ThreatEntry>& GetThreatTableEntries() const { return m_cachedThreatTableEntries; }

    // Helper methods
    bool HasFlag(uint32_t flag) const { return (m_cachedUnitFlags & flag) != 0; }
    bool IsCasting() const; // Definition in .cpp, uses m_cachedCastingSpellId and m_cachedCastingEndTimeMs
    bool IsChanneling() const; // Definition in .cpp, uses m_cachedChannelSpellId and m_cachedChannelEndTimeMs
    bool IsMoving() const; // RE-ADDED for movement check
    bool HasTarget() const; // NEW: Check if the unit has a target
    bool IsDead() const { return m_cachedHealth <= 0; } // Simple check based on cached health
    bool IsInCombat() const; // Definition in .cpp, uses m_cachedUnitFlags & UNIT_FLAG_IN_COMBAT
    bool IsFleeing() const { return HasFlag(UNIT_FLAG_FLEEING); } // Check if unit is fleeing
    bool IsFacingUnit(const WowUnit* targetUnit, float coneAngleDegrees) const;
    std::string GetPowerTypeString() const; // Helper to convert type byte to string
    std::string GetPowerTypeString(uint8_t powerType) const; // New overload for specific power type
    virtual Vector3 GetPosition() override;
    
    // Override the IsFriendly method to check faction correctly
    virtual bool IsFriendly() const override;
    
    // Unit relationship methods
    bool IsHostile() const; // Check if unit is hostile (should be attacked)
    bool IsAttackable() const; // Check if unit can be attacked (no "Not Attackable" flag)

    // Gets reaction from this unit to another unit using WoW's native reaction function
    // Returns: 1=Hostile, 2=Unfriendly, 3=Neutral, 4=Friendly/Self, 5-8=Higher friendship levels
    int GetReaction(WowUnit* otherUnit) const;

    // Get power value for a specific power type
    int GetPowerByType(uint8_t powerType) const {
        if (powerType >= PowerType::POWER_TYPE_COUNT) return 0;
        
        // Special handling for Rage (stored * 10 in WoW 3.3.5)
        if (powerType == PowerType::POWER_TYPE_RAGE) {
            return m_cachedPowers[powerType] / 10;
        }
        return m_cachedPowers[powerType];
    }

    // Get max power value for a specific power type
    int GetMaxPowerByType(uint8_t powerType) const {
        if (powerType >= PowerType::POWER_TYPE_COUNT) return 0;
        return m_cachedMaxPowers[powerType];
    }

    // Check if a power type is active (has non-zero max value)
    bool HasPowerType(uint8_t powerType) const {
        if (powerType >= PowerType::POWER_TYPE_COUNT) return false;
        return m_hasPowerType[powerType];
    }

    // Get all active power types
    std::vector<uint8_t> GetActivePowerTypes() const {
        std::vector<uint8_t> activePowerTypes;
        for (uint8_t i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
            if (m_hasPowerType[i] && m_cachedMaxPowers[i] > 0) {
                activePowerTypes.push_back(i);
            }
        }
        return activePowerTypes;
    }

    // Ensure no stray virtual declarations here for simple cached data accessors.
    // IsCasting(), IsChanneling(), IsInCombat() have their definitions in WowUnit.cpp.
}; 