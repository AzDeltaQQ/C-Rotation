#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Enum for different resource types
enum class ResourceType {
    Mana,
    Rage,
    Energy,
    Focus,    // Add Focus resource
    None // For spells with no cost
};

// Enum for specifying the target of an aura check
enum class TargetUnit {
    Player,
    Target,
    Focus,
    Friendly,
    SelfOrFriendly // NEW: For Player or Friendly units
    // Could add Party1, Party2, etc. later if needed
};

// NEW: Enum for specifying the logic for multiple aura IDs in a condition
enum class AuraConditionLogic {
    ANY_OF, // Condition met if any of the specified auras meet the presence criteria
    ALL_OF  // Condition met if all of the specified auras meet the presence criteria
};

// Enum for specifying the valid target types for a spell
enum class SpellTargetType {
    Self,       // Self-cast spells like buffs, seals
    Friendly,   // Spells that can be cast on friendly targets (heals, buffs)
    Enemy,      // Spells that require enemy targets (damage, debuffs)
    SelfOrFriendly, // Added for self or friendly targets
    Any,        // Spells that can target anyone
    None        // Spells that don't require a target
};

// --- ConditionType, Condition struct, and their JSON handlers ---
// Enum for specifying the type of condition (used in the new generic Condition struct)
enum class ConditionType {
    HEALTH_PERCENT_BELOW,
    MANA_PERCENT_ABOVE,
    TARGET_IS_CASTING,
    PLAYER_HAS_AURA,
    TARGET_HAS_AURA,
    PLAYER_MISSING_AURA,
    TARGET_MISSING_AURA,
    SPELL_OFF_COOLDOWN,
    SPELL_NOT_ON_COOLDOWN,
    MELEE_UNITS_AROUND_PLAYER_GREATER_THAN,
    UNITS_IN_FRONTAL_CONE_GT,
    PLAYER_THREAT_ON_TARGET_BELOW_PERCENT,
    SPELL_HAS_CHARGES,
    PLAYER_IS_FACING_TARGET,
    COMBO_POINTS_GREATER_THAN_OR_EQUAL_TO,
    UNKNOWN
};

// JSON Serialization for ConditionType
NLOHMANN_JSON_SERIALIZE_ENUM(ConditionType, {
    {ConditionType::HEALTH_PERCENT_BELOW, "HEALTH_PERCENT_BELOW"},
    {ConditionType::MANA_PERCENT_ABOVE, "MANA_PERCENT_ABOVE"},
    {ConditionType::TARGET_IS_CASTING, "TARGET_IS_CASTING"},
    {ConditionType::PLAYER_HAS_AURA, "PLAYER_HAS_AURA"},
    {ConditionType::TARGET_HAS_AURA, "TARGET_HAS_AURA"},
    {ConditionType::PLAYER_MISSING_AURA, "PLAYER_MISSING_AURA"},
    {ConditionType::TARGET_MISSING_AURA, "TARGET_MISSING_AURA"},
    {ConditionType::SPELL_OFF_COOLDOWN, "SPELL_OFF_COOLDOWN"},
    {ConditionType::SPELL_NOT_ON_COOLDOWN, "SPELL_NOT_ON_COOLDOWN"},
    {ConditionType::MELEE_UNITS_AROUND_PLAYER_GREATER_THAN, "MELEE_UNITS_AROUND_PLAYER_GREATER_THAN"},
    {ConditionType::UNITS_IN_FRONTAL_CONE_GT, "UNITS_IN_FRONTAL_CONE_GT"},
    {ConditionType::PLAYER_THREAT_ON_TARGET_BELOW_PERCENT, "PLAYER_THREAT_ON_TARGET_BELOW_PERCENT"},
    {ConditionType::SPELL_HAS_CHARGES, "SPELL_HAS_CHARGES"},
    {ConditionType::PLAYER_IS_FACING_TARGET, "PLAYER_IS_FACING_TARGET"},
    {ConditionType::COMBO_POINTS_GREATER_THAN_OR_EQUAL_TO, "COMBO_POINTS_GREATER_THAN_OR_EQUAL_TO"},
    {ConditionType::UNKNOWN, "UNKNOWN"}
})

// Generic Condition struct for RotationStep
struct Condition {
    ConditionType type = ConditionType::HEALTH_PERCENT_BELOW;
    
    // Common fields, interpretation depends on 'type'
    float value = 0.0f;             // For HEALTH/MANA checks (percentage), or other thresholds like unit counts or threat %
    float meleeRangeValue = 5.0f;   // For MELEE_UNITS_AROUND_PLAYER_GREATER_THAN (range component) or general range for UNITS_IN_FRONTAL_CONE_GT
    float coneAngleDegrees = 90.0f; // For UNITS_IN_FRONTAL_CONE_GT (angle component)
    float facingConeAngle = 60.0f;  // For PLAYER_IS_FACING_TARGET (angle component)
    uint32_t spellId = 0;           // For SPELL_OFF_COOLDOWN, or single aura checks (if multiAuraIds is empty), or TARGET_IS_CASTING (specific spell), or SPELL_HAS_CHARGES

    // Fields for Health/Mana/Resource type conditions
    bool targetIsPlayer = false;    // If true, 'value' applies to player, else to target/focus
    bool targetIsFriendly = false;  // If targetIsPlayer is false, this specifies if the target unit should be friendly

    // Fields for Aura type conditions
    std::vector<uint32_t> multiAuraIds;
    AuraConditionLogic multiAuraLogic = AuraConditionLogic::ANY_OF;
    uint64_t casterGuid = 0;
    int minStacks = 0;
    TargetUnit auraTarget = TargetUnit::Player; // Specifies who must have/be missing the aura (Player, Target, Focus, Friendly)
                                                // Note: PLAYER_HAS_AURA vs TARGET_HAS_AURA type implies the primary target.
                                                // This 'auraTarget' field could refine it further if needed, or be used if
                                                // the ConditionType itself becomes more generic like AURA_CHECK.
                                                // For now, keep it simple: PLAYER_HAS_AURA refers to player, TARGET_HAS_AURA to current step target.
};

// JSON Deserialization for Condition
inline void from_json(const nlohmann::json& j, Condition& c) {
    j.at("type").get_to(c.type);
    c.value = j.value("value", 0.0f);
    c.targetIsPlayer = j.value("targetIsPlayer", false);
    c.targetIsFriendly = j.value("targetIsFriendly", false);
    
    c.spellId = j.value("spellId", 0u);
    if (j.contains("multiAuraIds")) {
        j.at("multiAuraIds").get_to(c.multiAuraIds);
    }
    if (j.contains("multiAuraLogic")) {
        j.at("multiAuraLogic").get_to(c.multiAuraLogic);
    }
    c.casterGuid = j.value("casterGuid", 0ULL);
    c.minStacks = j.value("minStacks", 0);
    c.auraTarget = j.value("auraTarget", TargetUnit::Player); // Deserialize auraTarget

    c.meleeRangeValue = j.value("meleeRangeValue", 5.0f);
    c.coneAngleDegrees = j.value("coneAngleDegrees", 90.0f);
    c.facingConeAngle = j.value("facingConeAngle", 60.0f);
}

// JSON Serialization for Condition
inline void to_json(nlohmann::json& j, const Condition& c) {
    j = nlohmann::json{
        {"type", c.type},
        {"value", c.value},
        {"targetIsPlayer", c.targetIsPlayer},
        {"targetIsFriendly", c.targetIsFriendly},
        
        {"spellId", c.spellId},
        {"multiAuraIds", c.multiAuraIds},
        {"multiAuraLogic", c.multiAuraLogic},
        {"casterGuid", c.casterGuid},
        {"minStacks", c.minStacks},
        {"auraTarget", c.auraTarget},

        {"meleeRangeValue", c.meleeRangeValue},
        {"coneAngleDegrees", c.coneAngleDegrees},
        {"facingConeAngle", c.facingConeAngle}
    };
}
// --- End of ConditionType, Condition struct, and their JSON handlers ---


// Structure for an aura check condition (Legacy, for backward compatibility with old JSON files if needed)
struct AuraCondition {
    std::vector<int> auraIds; 
    AuraConditionLogic logic = AuraConditionLogic::ANY_OF; 
    TargetUnit target = TargetUnit::Player;
    bool presence = true; 
    int minStacks = 0; 
};

// Structure for a health check condition (Legacy, for backward compatibility)
struct HealthCondition {
    TargetUnit target = TargetUnit::Target; 
    float percent = 50.0f; 
};

// Structure for priority condition that increases spell priority when met
struct PriorityCondition {
    enum class Type {
        PLAYER_HAS_AURA,
        TARGET_HAS_AURA,
        TARGET_HEALTH_PERCENT_BELOW,
        PLAYER_HEALTH_PERCENT_BELOW,
        PLAYER_RESOURCE_PERCENT_ABOVE,
        PLAYER_RESOURCE_PERCENT_BELOW, 
        TARGET_DISTANCE_BELOW 
    };
    
    Type type = Type::PLAYER_HAS_AURA;
    int auraId = 0;          
    float thresholdValue = 0; 
    int priorityBoost = 50;   
    ResourceType resourceType = ResourceType::Mana; 
    float distanceThreshold = 0.0f; 
    int minStacks = 0;        
};

// Structure for a single step in a rotation, containing all spell details
struct RotationStep {
    // Spell Properties embedded directly
    int id = 0;
    std::string name = "New Step Spell";
    float minRange = 0.0f; 
    float maxRange = 0.0f; 
    ResourceType resourceType = ResourceType::None;
    int resourceCost = 0;
    float castTime = 0.0f;
    bool isChanneled = false;
    SpellTargetType targetType = SpellTargetType::Enemy; 
    bool castableWhileMoving = false; 
    int baseDamage = 0; 
    bool requiresTarget = true; 
    int maxCharges = 1;         // NEW: Maximum number of charges for the spell
    float rechargeTime = 0.0f;  // NEW: Time in seconds for one charge to recharge
    bool isHeal = false;        // NEW: Is this a healing spell?

    // Priority settings
    int basePriority = 10;                          
    std::vector<PriorityCondition> priorityBoosts;  

    // Conditions for this step
    std::vector<AuraCondition> auraConditions; // Kept for loading old formats
    std::vector<HealthCondition> healthConditions; // Kept for loading old formats
    std::vector<Condition> conditions; // NEW: Generic conditions list, UI will primarily use this

};

// Structure to hold rotation data
struct Rotation {
    std::string name = "New Rotation";
    std::string className = "Any"; 
    std::vector<RotationStep> steps;
};

// --- JSON Serialization for Enums (ResourceType, TargetUnit, AuraConditionLogic, SpellTargetType, PriorityCondition::Type) ---
NLOHMANN_JSON_SERIALIZE_ENUM(ResourceType, {
    {ResourceType::Mana, "Mana"}, {ResourceType::Rage, "Rage"}, {ResourceType::Energy, "Energy"},
    {ResourceType::Focus, "Focus"}, {ResourceType::None, "None"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(TargetUnit, {
    {TargetUnit::Player, "Player"}, {TargetUnit::Target, "Target"}, {TargetUnit::Focus, "Focus"},
    {TargetUnit::Friendly, "Friendly"}, {TargetUnit::SelfOrFriendly, "SelfOrFriendly"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(AuraConditionLogic, {
    {AuraConditionLogic::ANY_OF, "ANY_OF"}, {AuraConditionLogic::ALL_OF, "ALL_OF"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(SpellTargetType, {
    {SpellTargetType::Self, "Self"}, {SpellTargetType::Friendly, "Friendly"}, {SpellTargetType::Enemy, "Enemy"},
    {SpellTargetType::SelfOrFriendly, "SelfOrFriendly"}, {SpellTargetType::Any, "Any"}, {SpellTargetType::None, "None"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(PriorityCondition::Type, {
    {PriorityCondition::Type::PLAYER_HAS_AURA, "PLAYER_HAS_AURA"},
    {PriorityCondition::Type::TARGET_HAS_AURA, "TARGET_HAS_AURA"},
    {PriorityCondition::Type::TARGET_HEALTH_PERCENT_BELOW, "TARGET_HEALTH_PERCENT_BELOW"},
    {PriorityCondition::Type::PLAYER_HEALTH_PERCENT_BELOW, "PLAYER_HEALTH_PERCENT_BELOW"},
    {PriorityCondition::Type::PLAYER_RESOURCE_PERCENT_ABOVE, "PLAYER_RESOURCE_PERCENT_ABOVE"},
    {PriorityCondition::Type::PLAYER_RESOURCE_PERCENT_BELOW, "PLAYER_RESOURCE_PERCENT_BELOW"},
    {PriorityCondition::Type::TARGET_DISTANCE_BELOW, "TARGET_DISTANCE_BELOW"}
})

// --- JSON Serialization for LEGACY Conditions (AuraCondition, HealthCondition) ---
inline void to_json(nlohmann::json& j, const AuraCondition& ac) {
    j = nlohmann::json{
        {"auraIds", ac.auraIds}, {"logic", ac.logic}, {"target", ac.target},
        {"presence", ac.presence}, {"minStacks", ac.minStacks}
    };
}

inline void from_json(const nlohmann::json& j, AuraCondition& ac) {
    if (j.contains("auraId") && j.at("auraId").is_number_integer()) {
        ac.auraIds.push_back(j.at("auraId").get<int>());
    } else if (j.contains("auraIds")) {
        j.at("auraIds").get_to(ac.auraIds);
    }
    ac.logic = j.value("logic", AuraConditionLogic::ANY_OF);
    if (j.contains("target")) j.at("target").get_to(ac.target); // Check if key exists
    ac.presence = j.value("presence", true); 
    ac.minStacks = j.value("minStacks", 0);
}

inline void to_json(nlohmann::json& j, const HealthCondition& hc) {
    j = nlohmann::json{ {"target", hc.target}, {"percent", hc.percent} };
}

inline void from_json(const nlohmann::json& j, HealthCondition& hc) {
    if (j.contains("target")) j.at("target").get_to(hc.target); // Check if key exists
    if (j.contains("percent")) j.at("percent").get_to(hc.percent); // Check if key exists
}

// --- JSON Serialization for PriorityCondition ---
inline void to_json(nlohmann::json& j, const PriorityCondition& pc) {
    j = nlohmann::json{
        {"type", pc.type}, {"auraId", pc.auraId}, {"thresholdValue", pc.thresholdValue},
        {"priorityBoost", pc.priorityBoost}, {"resourceType", pc.resourceType},
        {"distanceThreshold", pc.distanceThreshold}, {"minStacks", pc.minStacks}
    };
}

inline void from_json(const nlohmann::json& j, PriorityCondition& pc) {
    j.at("type").get_to(pc.type);
    pc.auraId = j.value("auraId", 0);
    pc.thresholdValue = j.value("thresholdValue", 0.0f);
    pc.priorityBoost = j.value("priorityBoost", 50);
    pc.resourceType = j.value("resourceType", ResourceType::Mana);
    pc.distanceThreshold = j.value("distanceThreshold", 0.0f);
    pc.minStacks = j.value("minStacks", 0);
}

// --- JSON Serialization for RotationStep and Rotation ---

inline void to_json(nlohmann::json& j, const RotationStep& rs) {
    nlohmann::json rangeJson;
    if (rs.minRange > 0.0f || rs.maxRange > 0.0f) { // Save object if either is non-zero
        rangeJson = { {"min", rs.minRange}, {"max", rs.maxRange} };
    } else { // If both are zero, maybe save null or an empty object, or just maxRange as 0 for old format.
             // For consistency with new format, an object is better if we expect ranges often.
        rangeJson = { {"min", 0.0f}, {"max", 0.0f} };
    }

    j = nlohmann::json{
        {"id", rs.id}, {"name", rs.name}, {"range", rangeJson},
        {"resourceType", rs.resourceType}, {"resourceCost", rs.resourceCost},
        {"castTime", rs.castTime}, {"isChanneled", rs.isChanneled},
        {"targetType", rs.targetType}, {"requiresTarget", rs.requiresTarget},
        {"basePriority", rs.basePriority}, {"priorityBoosts", rs.priorityBoosts},
        {"auraConditions", rs.auraConditions}, // Legacy
        {"healthConditions", rs.healthConditions}, // Legacy
        {"conditions", rs.conditions}, // New generic conditions
        {"castableWhileMoving", rs.castableWhileMoving}, {"baseDamage", rs.baseDamage},
        {"maxCharges", rs.maxCharges},             // ADDED
        {"rechargeTime", rs.rechargeTime},          // ADDED
        {"isHeal", rs.isHeal}                      // ADDED
    };
}

inline void from_json(const nlohmann::json& j, RotationStep& rs) {
    j.at("id").get_to(rs.id);
    j.at("name").get_to(rs.name);
    
    if (j.contains("range")) {
        if (j["range"].is_number()) {
            rs.maxRange = j["range"].get<float>();
            rs.minRange = 0.0f;
        } else if (j["range"].is_object()) {
            rs.minRange = j["range"].value("min", 0.0f);
            rs.maxRange = j["range"].value("max", 0.0f);
        }
    } else {
        rs.minRange = 0.0f;
        rs.maxRange = 0.0f;
    }
    
    if (j.contains("resourceType")) j.at("resourceType").get_to(rs.resourceType);
    if (j.contains("resourceCost")) j.at("resourceCost").get_to(rs.resourceCost);
    if (j.contains("castTime")) j.at("castTime").get_to(rs.castTime);
    if (j.contains("isChanneled")) j.at("isChanneled").get_to(rs.isChanneled);
    
    rs.targetType = j.value("targetType", SpellTargetType::Enemy);
    rs.requiresTarget = j.value("requiresTarget", true); 
    rs.basePriority = j.value("basePriority", 10);
    rs.priorityBoosts = j.value("priorityBoosts", std::vector<PriorityCondition>{});
    
    // Load legacy first, then new generic.
    // A migration step could convert legacy to new generic format upon loading.
    rs.auraConditions = j.value("auraConditions", std::vector<AuraCondition>{});
    rs.healthConditions = j.value("healthConditions", std::vector<HealthCondition>{});
    rs.conditions = j.value("conditions", std::vector<Condition>{});
    
    rs.castableWhileMoving = j.value("castableWhileMoving", false);
    rs.baseDamage = j.value("baseDamage", 0);
    rs.maxCharges = j.value("maxCharges", 1);         // ADDED, default to 1
    rs.rechargeTime = j.value("rechargeTime", 0.0f);  // ADDED, default to 0.0
    rs.isHeal = j.value("isHeal", false);           // ADDED, default to false
}

inline void to_json(nlohmann::json& j, const Rotation& r) {
    j = nlohmann::json{
        {"name", r.name}, {"className", r.className}, {"steps", r.steps}
    };
}

inline void from_json(const nlohmann::json& j, Rotation& r) {
    j.at("name").get_to(r.name);
    j.at("className").get_to(r.className);
    j.at("steps").get_to(r.steps);
}