#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "../types/Rotation.h"
#include "../objectManager/ObjectManager.h"
#include "../types/types.h"
#include "../types/FactionInfo.h"
#include <atomic>

// Forward declarations
class WowUnit;
class WowPlayer;
struct Vector3;

namespace Rotation { // Forward declaration
    class RotationEngine;
}

// DEBUG: Define a specific GUID to get verbose logging for.
// extern constexpr uint64_t DEBUG_TARGET_GUID; // Old

namespace Spells {

// extern uint64_t g_debug_target_guid; // New: Moved into namespace Spells

// Add IntersectFlags enum based on user's information
enum IntersectFlags : uint32_t {
    None = 0x0,
    DoodadCollision = 0x1,
    WmoCollision = 0x2,
    WmoRender = 0x4,
    WmoNoCamCollision = 0x10,
    Terrain = 0x100,
    IgnoreWmoDoodad = 0x10000,
    LiquidWaterWalkable = 0x20000,
    LiquidAll = 0x20000,
    Cull = 0x80000,
    EntityCollision = 0x100000,
    EntityRender = 0x800000,

    // Combined flags for various use cases
    // "0x100111" used by projectiles, spell casting, etc.
    GameGenericLOS = 0x100111, 
    
    // "0x120171" Complex collision used by physics, camera, etc.
    GamePhysicsLOS = 0x120171,

    // Flags based on RE and direct game logs:
    GameObservedPlayerLOS = 0x1000124,  // Seen in game's 1st trace call. terrain | wmo_related_0x20 | unknown_0x4 | unknown_high_bit_0x10000000
    GameLiquidOrDoodadLOS = 0x20000,   // Seen in game's 4th trace call, potentially specific doodad/liquid interaction.

    // Original definitions, can be kept for reference or specific uses if needed
    Collision           = DoodadCollision | WmoCollision | Terrain | EntityCollision, // Original: 0x100111
    WorldGeometryLineOfSight = WmoCollision | DoodadCollision | Terrain, // Original: 0x111 - Current default for IsInLineOfSight

    // Redefine LineOfSight and LineOfSightDetailed based on common game usage for clarity if desired
    // For example, if GameGenericLOS is the most common for true spell LOS:
    LineOfSight = GameGenericLOS, // Defaulting to a more comprehensive one based on RE
    LineOfSightDetailed = GamePhysicsLOS // Example if physics one is 'detailed'
};

// Update to match World__Intersect function signature exactly as in user's code
// NOTE: This function returns TRUE if a collision was detected (line is BLOCKED),
// and FALSE if no collision was found (line is CLEAR)
// Based on LOSSTUFF.txt, processWorldFrameTrace (0x77F310) signature:
// int processWorldFrameTrace(CGWorldFrame* worldFramePtr, float *startPos, float *endPos, int flags, int *hitResultMaybeBool, int a6, int a7)
// Where hitResultMaybeBool is a float* for hitFraction. Return is int (0 for clear, non-zero for hit).
// CORRECTED ORDER based on disassembly analysis (LOSSTUFF.txt, handleWorldRaycast call to processWorldFrameTrace):
// Actual parameter order: worldframe, start, end, hitFraction*, flags, a6, a7
typedef int (*World_Intersect_t)(void* worldframe, Vector3* start, Vector3* end, float* hitFraction, IntersectFlags flags, int param_a6, int param_a7);

// We'll keep this for backward compatibility but mark as deprecated
// typedef bool (*TraceLineAndProcess_t)(float* start, float* end, float* hitPosition, float* hitDistance, IntersectFlags flags);

extern World_Intersect_t WorldIntersect;

// External GUID pointer for current target
extern uint64_t* CurrentTargetGUID_ptr;

// Target reaction cache entry
struct ReactionCacheEntry {
    uint64_t targetGuid;
    int reaction;
    std::chrono::steady_clock::time_point timestamp;
};

class TargetingManager {
public:
    TargetingManager(ObjectManager& objMgr);
    
    // Target reaction functions
    bool IsUnitAttackable(WowUnit* playerUnit, WowUnit* targetUnit);
    bool IsUnitFriendly(WowUnit* playerUnit, WowUnit* targetUnit);
    
    // New BG Mode methods
    void SetBGModeEnabled(bool enabled);
    bool IsBGModeEnabled() const;
    void UpdateLocalPlayerFaction(WowPlayer* player);
    FactionInfo::PlayerFaction GetLocalPlayerFaction() const;
    
    // Get cached reaction (thread-safe)
    int GetCachedReaction(WowUnit* player, WowUnit* target);
    
    // Find best target based on target type (older version, potentially for specific GUID returns)
    uint64_t FindBestTarget(Rotation::RotationEngine* engine_ptr,
                            Rotation::TargetType targetType, // Corrected scope
                            const std::string& nameFilter = "", 
                            bool useNameFilter = false,
                            bool isTankingMode = false,
                            bool isHealingSpellContext = false); // NEW: Context for healing spells
    
    // Validate target for spell conditions
    bool ShouldHealTarget(WowUnit* target, float healthThreshold);
    
    // Check if a target needs healing based on conditions
    bool FindHealingTargetForConditions(const std::vector<Rotation::Condition>& conditions, 
                                        uint64_t& outTargetGuid);
                                        
    // Clear reaction cache
    void ClearReactionCache();

    // New function to find the best target based on type (enemy, friendly) and other criteria
    std::shared_ptr<WowUnit> FindBestTarget(
        WowPlayer* player, 
        Rotation::TargetType targetType, // Corrected scope
        bool useLosCheck,
        bool useRangeCheck,
        float maxRange
        // Add other criteria parameters as needed (e.g., health thresholds, specific buffs/debuffs)
    );

    bool IsUnitBlacklisted(WowUnit* unit) const;

private:
    ObjectManager& objectManager;
    
    // Reaction cache
    std::vector<ReactionCacheEntry> reactionCache;
    const int MAX_CACHE_SIZE = 30;
    const std::chrono::seconds CACHE_TTL{10};
    std::mutex cacheMutex;

    // Unit name blacklist
    std::unordered_set<std::string> unitNameBlacklist;

    // New BG Mode members
    std::atomic<bool> m_bgModeEnabled{false};
    FactionInfo::PlayerFaction m_localPlayerFaction{FactionInfo::PlayerFaction::UNKNOWN};
    mutable std::mutex m_factionMutex; // To protect m_localPlayerFaction
};

// Check line of sight between two points
bool IsInLineOfSight(const Vector3& startPos, const Vector3& endPos, bool forceLog);

// Target a unit (set current target to GUID)
void TargetUnit(uint64_t targetGuid);

// Function to set the global debug target GUID
// void SetDebugTargetGUID(uint64_t new_guid);

// Function to convert IntersectFlags to a string for logging
std::string IntersectFlagsToString(IntersectFlags flags);

// Check line of sight (LOS) between two units
bool HasLineOfSight(WowUnit* unit1, WowUnit* unit2);

} 