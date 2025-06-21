#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint> // Include for uint32_t
#include "../objectManager/ObjectManager.h"
#include "nlohmann/json.hpp"

namespace Rotation {

// Forward declaration
class RotationEngine;

// NEW: Enum for specifying the logic for multiple aura IDs in a condition
// (Mirrors AuraConditionLogic in SpellData.h)
enum class AuraConditionLogic {
    ANY_OF, // Condition met if any of the specified auras meet the presence criteria
    ALL_OF  // Condition met if all of the specified auras meet the presence criteria
};

// JSON Serialization for Rotation::AuraConditionLogic
NLOHMANN_JSON_SERIALIZE_ENUM(AuraConditionLogic, {
    {AuraConditionLogic::ANY_OF, "ANY_OF"},
    {AuraConditionLogic::ALL_OF, "ALL_OF"}
})

// Condition structure for rotation steps
// Defined *before* RotationStep
struct Condition {
    enum class Type {
        HEALTH_PERCENT_BELOW,
        MANA_PERCENT_ABOVE,
        TARGET_IS_CASTING,
        PLAYER_HAS_AURA,    // Check if player has aura
        TARGET_HAS_AURA,    // Check if target has aura
        PLAYER_MISSING_AURA,// Check if player does NOT have aura
        TARGET_MISSING_AURA,// Check if target does NOT have aura
        SPELL_OFF_COOLDOWN,
        SPELL_NOT_ON_COOLDOWN,             // Spell is not on cooldown (alternative to SPELL_OFF_COOLDOWN with clearer intent)
        MELEE_UNITS_AROUND_PLAYER_GREATER_THAN, // Number of hostile units in melee range of player is greater than value
        UNITS_IN_FRONTAL_CONE_GT,          // Number of units in a frontal cone is greater than value
        PLAYER_THREAT_ON_TARGET_BELOW_PERCENT, // NEW: Player's threat on current target is below X%
        SPELL_HAS_CHARGES,                 // NEW: Check if a spell has a minimum number of charges
        PLAYER_IS_FACING_TARGET,           // NEW: Check if the player is facing the current target unit
        COMBO_POINTS_GREATER_THAN_OR_EQUAL_TO, // NEW: Player has at least X combo points on current target
        UNKNOWN                            // Should not be used, signifies an error or uninitialized condition
        // ... add many more
    };

    Type type = Type::HEALTH_PERCENT_BELOW;

    // --- Fields relevant to specific condition types --- 

    // For AURA checks:
    uint32_t spellId = 0; // Used if multiAuraIds is empty, or for single-aura checks
    std::vector<uint32_t> multiAuraIds; // NEW: List of aura IDs for ANY_OF/ALL_OF logic
    AuraConditionLogic multiAuraLogic = AuraConditionLogic::ANY_OF; // NEW: Logic for multiAuraIds
    uint64_t casterGuid = 0; // Optional: Check for aura applied by specific caster
    int minStacks = 0;      // Min stacks required (applies to single spellId or multiAuraIds based on logic)
    // Note: Presence/Absence is determined by Condition::Type (e.g., PLAYER_HAS_AURA vs PLAYER_MISSING_AURA)

    // For HEALTH/RESOURCE checks:
    float value = 0.0f;             // For HEALTH/MANA checks (percentage), or other thresholds
    float range = 0.0f;             // For MELEE_UNITS_AROUND_PLAYER or UNITS_IN_FRONTAL_CONE range
    float coneAngle = 0.0f;         // For UNITS_IN_FRONTAL_CONE angle in degrees
    float facingConeAngle = 60.0f;  // NEW: For PLAYER_IS_FACING_TARGET, angle in degrees (default 60)
    bool targetIsPlayer = false;    // If true, certain conditions (like health/aura) apply to player
    bool targetIsFriendly = false;  // If targetIsPlayer is false, specifies if target unit should be friendly (for some conditions)

    // For SPELL_OFF_COOLDOWN checks:
    // uint32_t spellId is used here as well.

    // For TARGET_IS_CASTING checks:
    // uint32_t spellId can optionally hold the spell ID being cast.

    // --- Deprecated/Potentially Redundant --- 
    std::string auraName = ""; // Checking by name is less reliable, prefer IDs.
    // bool auraPresence = true; // Replaced by using HAS_AURA vs MISSING_AURA types
    // int minAuraStacks = 0; // Merged into general minStacks

    // --- Evaluation Function (optional, can be replaced by logic in RotationConditions.cpp) ---
    std::function<bool(const ObjectManager&, uint64_t currentTargetGuid)> check = nullptr;
};


// Priority boost condition structure (assuming it's okay here, could also move before RotationStep)
struct PriorityCondition {
    enum class Type {
        PLAYER_HAS_AURA,
        TARGET_HAS_AURA,
        TARGET_HEALTH_PERCENT_BELOW,
        PLAYER_HEALTH_PERCENT_BELOW,
        PLAYER_RESOURCE_PERCENT_ABOVE,
        PLAYER_RESOURCE_PERCENT_BELOW, 
        TARGET_DISTANCE_BELOW,
        UNKNOWN // Added to handle unrecognized types
    };
    
    Type type = Type::PLAYER_HAS_AURA;
    uint32_t spellId = 0;      
    float thresholdValue = 0;  
    int priorityBoost = 50;    
    uint8_t resourceType = 0;  
    float distanceThreshold = 0.0f; 
};

// Enum for different target types a spell can have
// Defined *before* RotationStep uses it
enum class TargetType {
    SELF,       // Target is the player
    ENEMY,      // Target is a hostile unit
    FRIENDLY,   // Target is a friendly unit (includes player)
    FRIENDLY_NO_SELF, // Target is a friendly unit (excludes player)
    MOUSEOVER,  // Target is the unit under the mouse cursor
    ENEMY_ASSIST, // Target is the target of your current friendly target
    FOCUS,      // Target is the player's focus target (if implemented)
    PET,        // Target is the player's pet (if implemented)
    SELF_OR_FRIENDLY, // ADDED BACK
    ANY,              // ADDED BACK
    NONE        // No specific target needed for the spell logic (e.g. AoE around self)
};

// Now define RotationStep, which uses Condition and TargetType
struct RotationStep {
    std::string name;
    uint32_t spellId = 0;
    std::vector<Condition> conditions;
    TargetType targetType = TargetType::ENEMY; 
    bool requiresTarget = true; 
    float minRange = 0.0f;
    float maxRange = 0.0f;
    int manaCost = 0;
    std::string resourceType = "Mana"; // Default to Mana, can be "Rage", "Energy", etc.
    int basePriority = 0;
    bool isChannel = false;
    float castTime = 0.0f; 
    int maxCharges = 1;      // Default to 1 for non-charge spells
    float rechargeTime = 0.0f; // Default to 0 for non-charge spells
    bool isHeal = false;      // NEW: Flag to identify healing spells
    int baseDamage = 0;       // RE-ADDED: For damage calculations

    // Priority system fields
    std::vector<PriorityCondition> priorityBoosts; 
    
    // Cache for calculated priority during a rotation cycle
    mutable int calculatedPriority = 0;

    float castableWhileMoving = false; 
};


struct RotationProfile {
    std::string name;
    std::string filePath; 
    std::vector<RotationStep> steps;
    std::time_t last_modified = 0; 
    // Add settings like AoE toggle, cooldown usage toggles, etc.
};


} // End namespace Rotation 