#include "targeting.h"
#include "../logs/log.h"
#include "../utils/memory.h" // For direct memory access
#include "../types/wowunit.h"
#include "../types/WowPlayer.h" // Include WowPlayer header
#include "../hook.h" // Include for SubmitToEndScene
#include "../rotations/RotationEngine.h" // <<< ADDED for RotationEngine pointer
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <mutex> // For mutex and lock_guard
#include <cctype> // <<< ADDED for std::tolower

#include "../gui/gui.h" // For GUI functions

#include "../types/FactionInfo.h"
#include "../spells/auras.h" // For Spells::UnitHasAura

// Forward declarations for functions from Rotation namespace
// These are defined in RotationExecution.cpp
// Ideally, these would be in a header file (e.g., RotationExecution.h or RotationTargetingAPI.h)
// that would be included here.
namespace Rotation {
    // Forward declarations for types used in the function signatures if not already fully included.
    // class WowUnit; // Assumed to be included via ../types/wowunit.h
    // class WowPlayer; // Assumed to be included via ../types/WowPlayer.h
    // class ObjectManager; // Assumed to be included via objectManager.h (itself included by targeting.h)
    // namespace Spells { class TargetingManager; } // Spells::TargetingManager is defined in targeting.h

    std::shared_ptr<WowUnit> FindBestEnemyTarget(
        WowPlayer* player,
        ObjectManager& objectManager,
        Spells::TargetingManager& targetingManager,
        bool onlyTargetCombatUnits,
        bool isTankingMode
    );

    std::shared_ptr<WowUnit> FindBestFriendlyTarget(
        std::shared_ptr<WowPlayer> player,
        ObjectManager& objectManager,
        Spells::TargetingManager& targetingManager,
        bool includeSelf
    );
} // namespace Rotation

// Import Windows API for memory validation
#ifdef _WIN32
#include <Windows.h>
#else
// Define a fallback for non-Windows platforms
inline bool IsBadReadPtr(const void* ptr, size_t size) { return ptr == nullptr; }
#endif

// DEBUG: Define a specific GUID to get verbose logging for. Set to 0ULL to disable debug target.
// uint64_t g_debug_target_guid = 0ULL; // Definition of the global variable (Removed Spells:: qualifier) - OLD GLOBAL DEFINITION

namespace Spells {

uint64_t g_debug_target_guid = 0ULL; // Definition now correctly inside Spells namespace

// Function to set the global debug target GUID
void SetDebugTargetGUID(uint64_t new_guid) { 
    // Log the change
    std::stringstream ss;
    ss << "[SPELLS_DEBUG] SetDebugTargetGUID called with new value: 0x" << std::hex << new_guid << std::dec;
    Core::Log::Message(ss.str());
    
    g_debug_target_guid = new_guid; 
}

// Define the function pointer type matching the assembly signature
// Using __cdecl since that's the WoW convention
typedef void (__cdecl* handleTargetAcquisition_t)(uint64_t targetGuid);

// Define the function pointer at the specified address
handleTargetAcquisition_t handleTargetAcquisition_ptr = (handleTargetAcquisition_t)0x00524BF0;

// Update CURRENT_TARGET_GUID_ADDR from the disassembly (CurrentTargetGUID at 0x524F38)
constexpr uintptr_t CURRENT_TARGET_GUID_ADDR = 0x00BD07B0;

// Initialize the World_Intersect function pointer
// Based on LOSSTUFF.txt, using processWorldFrameTrace at 0x77F310
World_Intersect_t WorldIntersect = (World_Intersect_t)0x77F310; // processWorldFrameTrace

// Alternative potentially useful addresses
// traceLineAndProcess is the original function we were using (now wrapped by processWorldFrameTrace)
constexpr uintptr_t TRACE_LINE_AND_PROCESS = 0x007A3B70;
// The address previously labeled HANDLE_WORLD_RAYCAST was incorrect
// resolve_and_report_collisions might be the actual collision function
constexpr uintptr_t RESOLVE_AND_REPORT_COLLISIONS = 0x007C28F0; // Corrected address

// Add worldframe pointer definition
constexpr uintptr_t WORLDFRAME_PTR_ADDR = 0x00B7436C;
void** worldframePtr = (void**)WORLDFRAME_PTR_ADDR;

// Define the variable inside the namespace to match the header
uint64_t* CurrentTargetGUID_ptr = (uint64_t*)CURRENT_TARGET_GUID_ADDR;

namespace { // Anonymous namespace for IsValidFunctionAddress
    bool IsValidFunctionAddress(uintptr_t address) {
        if (address == 0) return false;
        
        try {
            // Check if the first few bytes are valid machine code
            // Most x86 function prologues start with these bytes
            unsigned char firstByte = Memory::Read<unsigned char>(address);
            // Common function prologue opcodes (PUSH EBP, MOV EBP,ESP, etc.)
            return (firstByte == 0x55 || firstByte == 0x53 || firstByte == 0x56 || firstByte == 0x57 || 
                    firstByte == 0x83 || firstByte == 0x8B || firstByte == 0xE9);
        } catch (...) {
            return false;
        }
    }
}

// Implementation of GetWorldFrame - must be defined before use
void* GetWorldFrame() {
    // Safety check to prevent invalid memory access
    try {
        if (worldframePtr != nullptr) {
            void* frame = *worldframePtr;
            return frame;
        }
    }
    catch (...) {
        // Catch any exceptions from invalid memory access
    }
    return nullptr;
}

// Simplified initialization that doesn't crash
void InitializeWorldIntersect() {
    static bool initialized = false;
    
    // Log every time this is called, regardless of previous initialization
    std::stringstream init_log;
    init_log << "[LOS_INIT] InitializeWorldIntersect called, previously initialized: " 
             << (initialized ? "YES" : "NO");
    Core::Log::Message(init_log.str());
    
    if (initialized) return;
    
    // Use processWorldFrameTrace as identified in LOSSTUFF.txt
    WorldIntersect = (World_Intersect_t)0x77F310; 
    Core::Log::Message("[LOS_INIT] Initialized WorldIntersect with 0x77F310 (processWorldFrameTrace) function for collision detection");
    
    initialized = true;
}

// Implementation of IsInLineOfSight function
bool IsInLineOfSight(const Vector3& startPos, const Vector3& endPos, bool forceLog) {
    // GUI::LOSTab* losTab = GUI::GetLOSTab(); // REMOVED
    uint64_t currentDebugGuid = 0ULL;
    bool isDebugTargetCurrentlySetByTab = false;
    /* REMOVED
    if (losTab && losTab->IsDebugTargetSet()) {
        isDebugTargetCurrentlySetByTab = true;
        currentDebugGuid = losTab->GetDebugTargetGUID();
    }
    */

    if (forceLog) { 
        std::stringstream flag_debug_log;
        flag_debug_log << "[LOS_FLAG_DEBUG] Value of Spells::GameGenericLOS is: 0x" << std::hex << static_cast<uint32_t>(Spells::GameGenericLOS) << std::dec;
        Core::Log::Message(flag_debug_log.str());
    }

    bool shouldLogDetails = forceLog || isDebugTargetCurrentlySetByTab;
    
    if (shouldLogDetails) {
        std::stringstream entry_log_ss;
        entry_log_ss << "[LOS_ENTRY] IsInLineOfSight called with forceLog=" << (forceLog ? "true" : "false")
                     << " TabDebugGUID=0x" << std::hex << currentDebugGuid << std::dec;
        Core::Log::Message(entry_log_ss.str());
        
        std::stringstream params_log_ss;
        params_log_ss << "[LOS_PARAMS] Start: (" << std::fixed << std::setprecision(3) << startPos.x << ", " << startPos.y << ", " << startPos.z << ") "
                     << "End: (" << endPos.x << ", " << endPos.y << ", " << endPos.z << ")";
        Core::Log::Message(params_log_ss.str());
    }
    
    InitializeWorldIntersect();
    
    void* worldframe = GetWorldFrame();
    if (!worldframe) {
        if (shouldLogDetails) { 
            Core::Log::Message("[LOS_DEBUG] ERROR: worldframe pointer is NULL!");
        }
        return false; 
    }
    
    // Calculate horizontal distance for distance-based decisions
    float horizontal_distance = std::sqrtf(
        std::powf(endPos.x - startPos.x, 2) + 
        std::powf(endPos.y - startPos.y, 2));
        
    Vector3 start = startPos;
    Vector3 end = endPos;
    
    // Use multiple different trace approaches and combine results
    
    // APPROACH 1: Trace with different height offsets
    const int NUM_HEIGHT_OFFSETS = 5;
    float offsets[NUM_HEIGHT_OFFSETS] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
    int successful_traces = 0;
    int total_traces = 0;
    
    for (int i = 0; i < NUM_HEIGHT_OFFSETS; i++) {
        Vector3 adjusted_start = start;
        Vector3 adjusted_end = end;
        
        // Apply height offset
        adjusted_start.z += offsets[i];
        adjusted_end.z += offsets[i];
        
        // Use both tracing flags for each height
        IntersectFlags flags_to_try[2] = {GameGenericLOS, GameObservedPlayerLOS};
        
        for (int j = 0; j < 2; j++) {
            float hitFraction = 1.0f;
            int param_a6 = 0;
            int param_a7 = 0;
            int result = 0;
            
            try {
                total_traces++;
                result = WorldIntersect(worldframe, &adjusted_start, &adjusted_end, &hitFraction, 
                                  flags_to_try[j], param_a6, param_a7);
                
                // Count trace as successful if:
                // 1. Result is 0 (clear path), OR
                // 2. HitFraction is 1.0 or very close to it (reached destination)
                if (result == 0 || hitFraction >= 0.99f) {
                    successful_traces++;
                    
                    if (shouldLogDetails) {
                        std::stringstream trace_log;
                        trace_log << "[LOS_TRACE] Height offset " << offsets[i]
                                 << " with flags 0x" << std::hex << flags_to_try[j] << std::dec
                                 << " result: " << result << ", fraction: " << hitFraction
                                 << " - SUCCESS";
                        Core::Log::Message(trace_log.str());
                    }
                }
                else if (shouldLogDetails) {
                    std::stringstream trace_log;
                    trace_log << "[LOS_TRACE] Height offset " << offsets[i]
                             << " with flags 0x" << std::hex << flags_to_try[j] << std::dec
                             << " result: " << result << ", fraction: " << hitFraction
                             << " - FAILED";
                    Core::Log::Message(trace_log.str());
                }
            }
            catch (...) {
                if (shouldLogDetails) {
                    Core::Log::Message("[LOS_DEBUG] ERROR: Exception in trace");
                }
                // Continue with other traces
            }
        }
    }
    
    // APPROACH 2: Multi-point ray test - check points along the ray
    bool midpoint_check_success = true;
    const int NUM_MIDPOINTS = 5;
    
    for (int i = 1; i < NUM_MIDPOINTS; i++) {
        float fraction = static_cast<float>(i) / NUM_MIDPOINTS;
        Vector3 midPoint;
        midPoint.x = start.x + (end.x - start.x) * fraction;
        midPoint.y = start.y + (end.y - start.y) * fraction;
        midPoint.z = start.z + (end.z - start.z) * fraction;
        
        float midPointFraction = 1.0f;
        int midPointResult = 0;
        int param_a6 = 0;
        int param_a7 = 0;
        
        try {
            total_traces++;
            midPointResult = WorldIntersect(worldframe, &start, &midPoint, &midPointFraction, 
                                     GameGenericLOS, param_a6, param_a7);
            
            // Count segment as successful if it reached close to destination
            if (midPointResult == 0 || midPointFraction >= 0.99f) {
                successful_traces++;
                
                if (shouldLogDetails && (i == NUM_MIDPOINTS-1 || i == 1)) {
                    std::stringstream trace_log;
                    trace_log << "[LOS_TRACE] Midpoint " << i << "/" << NUM_MIDPOINTS
                             << " result: " << midPointResult << ", fraction: " << midPointFraction
                             << " - SUCCESS";
                    Core::Log::Message(trace_log.str());
                }
            }
            else {
                midpoint_check_success = false;
                
                if (shouldLogDetails) {
                    std::stringstream trace_log;
                    trace_log << "[LOS_TRACE] Midpoint " << i << "/" << NUM_MIDPOINTS
                             << " result: " << midPointResult << ", fraction: " << midPointFraction
                             << " - FAILED";
                    Core::Log::Message(trace_log.str());
                }
            }
        }
        catch (...) {
            if (shouldLogDetails) {
                Core::Log::Message("[LOS_DEBUG] ERROR: Exception in midpoint trace");
            }
            midpoint_check_success = false;
        }
    }
    
    // DECISION MAKING BASED ON COMBINED TRACE RESULTS
    
    // Calculate success ratio of all traces
    float success_ratio = static_cast<float>(successful_traces) / total_traces;
    
    bool has_los = false;
    
    // For closer targets (under 20 yards), be more lenient
    if (horizontal_distance < 20.0f) {
        // Require 70% success rate for close targets
        has_los = (success_ratio >= 0.7f);
        
        if (shouldLogDetails) {
            std::stringstream decision_log;
            decision_log << "[LOS_DECISION] Close-range target (<20yd): " 
                       << successful_traces << "/" << total_traces << " traces successful ("
                       << (success_ratio * 100.0f) << "%). Decision: "
                       << (has_los ? "VISIBLE" : "BLOCKED");
            Core::Log::Message(decision_log.str());
        }
    }
    // For distant targets (over 20 yards), be more strict
    else {
        // Require 80% success rate for distant targets
        has_los = (success_ratio >= 0.8f);
        
        if (shouldLogDetails) {
            std::stringstream decision_log;
            decision_log << "[LOS_DECISION] Long-range target: " 
                       << successful_traces << "/" << total_traces << " traces successful ("
                       << (success_ratio * 100.0f) << "%). Decision: "
                       << (has_los ? "VISIBLE" : "BLOCKED");
            Core::Log::Message(decision_log.str());
        }
    }
    
    if (shouldLogDetails) {
        std::stringstream ss;
        ss << "[LOS_DEBUG] Final LOS status: " 
           << (has_los ? "VISIBLE" : "BLOCKED")
           << " (Distance: " << horizontal_distance << " yards)";
        Core::Log::Message(ss.str());
    }
    
    return has_los;
}

void TargetUnit(uint64_t targetGuid) {
    // Remove regular targeting logs to reduce log spam
    // Only log with drastically reduced frequency
    static int targetCounter = 0;
    targetCounter++;
    bool shouldLog = (targetCounter % 500 == 0); // Only log every 500th target action
    
    // Only prepare the hex string if we're actually going to log
    char guidHexBuffer[32] = {0};
    if (shouldLog) {
        sprintf_s(guidHexBuffer, sizeof(guidHexBuffer), "0x%llX", targetGuid);
    }
    
    try {
        // From looking at the disassembly, this function handles target selection
        // It will store the target GUID at CurrentTargetGUID (0xBD07B0)
        // This must be called from the main thread
        handleTargetAcquisition_ptr(targetGuid);
        
        // Only log on very reduced frequency
        if (shouldLog) {
            Core::Log::Message("TargetUnit: Target acquisition executed for GUID " + std::string(guidHexBuffer));
        }
        
        // Optional: Verify the target was set by reading memory
        // Only log verification failures, not successes
        try {
            uint64_t verifyGuid = Memory::Read<uint64_t>(CURRENT_TARGET_GUID_ADDR);
            if (verifyGuid != targetGuid) {
                // Always log targeting failures as these are important
                char actualBuffer[32];
                sprintf_s(actualBuffer, sizeof(actualBuffer), "0x%llX", verifyGuid);
                Core::Log::Message("TargetUnit: Target verification mismatch, got " + std::string(actualBuffer));
            }
        } catch (...) {
            // Only log verification exceptions on reduced frequency
            if (shouldLog) {
                Core::Log::Message("TargetUnit: Could not verify target was set");
            }
        }
    }
    catch (const std::exception& e) {
        // Always log exceptions as these are important
        Core::Log::Message("TargetUnit: Exception: " + std::string(e.what()));
    }
    catch (...) {
        // Always log exceptions as these are important
        Core::Log::Message("TargetUnit: Unknown exception");
    }
}

// Implementation of TargetingManager

TargetingManager::TargetingManager(ObjectManager& objMgr) : objectManager(objMgr) {
    // Core::Log::Message("TargetingManager initialized"); // Reduce noise
    // Initialize the unit name blacklist (all lowercase for case-insensitive comparison)
    unitNameBlacklist = {
        "deer",
        "sheep",
        "toad",
        "frog",
        "squirrel",
        "rat",
        "snake",
        "cow",
        "rabbit",
        "hare",
        "adder",
        "nightmarish book of ascension",
        "destined book of ascension",
        "lootbot 3000",
        "unholy champion",
        "putrid thrall",
        "kerg pebblecutter"
        // "totem", "whelp", "dragon" will be handled by substring check below
    };
}

bool TargetingManager::IsUnitAttackable(WowUnit* playerUnit, WowUnit* targetUnit) {
    if (!playerUnit || !targetUnit || targetUnit->IsDead()) {
        return false; // Basic invalid conditions
    }
    
    // If unit is blacklisted, it's not attackable.
    if (IsUnitBlacklisted(targetUnit)) {
        return false;
    }

    if (IsBGModeEnabled()) {
        // Ensure we have the local player's faction
        // UpdateLocalPlayerFaction should be called elsewhere periodically,
        // but we can try to get it here. If unknown, might default to false or old logic.
        FactionInfo::PlayerFaction localFaction = GetLocalPlayerFaction();
        
        if (localFaction == FactionInfo::PlayerFaction::UNKNOWN) {
            // Optionally, fall back to reaction-based if faction is unknown,
            // or simply return false as we can't determine hostility in BG mode.
            // For now, let's be cautious and say not attackable if we don't know our own faction.
            // Core::Log::Message("[TargetingManager-BGMode] IsUnitAttackable: Local player faction unknown. Defaulting to not attackable.");
            // Fallback to reaction for safety if own faction unknown:
             return playerUnit->GetReaction(targetUnit) <= 2; // HOSTILE or UNFRIENDLY
        }

        // Check target's faction
        // Only consider players for BG faction logic
        if (!targetUnit->IsPlayer()) {
             // For non-players in BG mode, rely on reaction (e.g. NPCs, pets)
            return playerUnit->GetReaction(targetUnit) <= 2; // HOSTILE or UNFRIENDLY
        }

        bool targetIsAlliance = Spells::UnitHasAura(targetUnit, FactionInfo::ALLIANCE_AURA_ID);
        bool targetIsHorde = Spells::UnitHasAura(targetUnit, FactionInfo::HORDE_AURA_ID);

        if (localFaction == FactionInfo::PlayerFaction::ALLIANCE) {
            return targetIsHorde; // Attackable if target is Horde
        } else if (localFaction == FactionInfo::PlayerFaction::HORDE) {
            return targetIsAlliance; // Attackable if target is Alliance
        }
        return false; // Should not reach here if localFaction is known
    } else {
        // Original reaction-based logic
        int reaction = GetCachedReaction(playerUnit, targetUnit);
        
        // Logging removed
        
        // HOSTILE (1), UNFRIENDLY (2), or NEUTRAL (3)
        // Changed to include NEUTRAL (3) as attackable
        return reaction <= 3;
    }
}

bool TargetingManager::IsUnitFriendly(WowUnit* playerUnit, WowUnit* targetUnit) {
    if (!playerUnit || !targetUnit) { // Note: Don't check IsDead here, friendly dead players are still friendly
        return false;
    }

    if (playerUnit->GetGUID64() == targetUnit->GetGUID64()) {
        return true; // Self is always friendly
    }

    if (IsBGModeEnabled()) {
        FactionInfo::PlayerFaction localFaction = GetLocalPlayerFaction();

        if (localFaction == FactionInfo::PlayerFaction::UNKNOWN) {
            // Fallback to reaction for safety if own faction unknown:
            return playerUnit->GetReaction(targetUnit) >= 4; // FRIENDLY or HONORED etc.
        }
        
        // Only consider players for BG faction logic
        if (!targetUnit->IsPlayer()) {
            // For non-players in BG mode, rely on reaction
            return playerUnit->GetReaction(targetUnit) >= 4; // FRIENDLY or HONORED etc.
        }

        bool targetIsAlliance = Spells::UnitHasAura(targetUnit, FactionInfo::ALLIANCE_AURA_ID);
        bool targetIsHorde = Spells::UnitHasAura(targetUnit, FactionInfo::HORDE_AURA_ID);

        if (localFaction == FactionInfo::PlayerFaction::ALLIANCE) {
            return targetIsAlliance; // Friendly if target is Alliance
        } else if (localFaction == FactionInfo::PlayerFaction::HORDE) {
            return targetIsHorde; // Friendly if target is Horde
        }
        return false;
    } else {
        // Original reaction-based logic
        int reaction = GetCachedReaction(playerUnit, targetUnit);
        // FRIENDLY (4) and above (HONORED, REVERED, EXALTED)
        return reaction >= 4;
    }
}

int TargetingManager::GetCachedReaction(WowUnit* player, WowUnit* target) {
    if (!player || !target) {
        return -1; // Invalid input
    }
    
    uint64_t targetGuid = target->GetGUID64();
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    // Try to find in cache first
    auto it = std::find_if(reactionCache.begin(), reactionCache.end(),
        [targetGuid](const ReactionCacheEntry& entry) {
            return entry.targetGuid == targetGuid;
        }
    );
    
    // If found in cache, return the cached reaction
    if (it != reactionCache.end()) {
        return it->reaction;
    }
    
    // Not in cache, get and cache the reaction
    int reaction = player->GetReaction(target);
    
    // Add to cache if we have space
    if (reactionCache.size() < static_cast<size_t>(MAX_CACHE_SIZE)) {
        ReactionCacheEntry entry;
        entry.targetGuid = targetGuid;
        entry.reaction = reaction;
        entry.timestamp = now;
        reactionCache.push_back(entry);
    }
    
    return reaction;
}

bool TargetingManager::ShouldHealTarget(WowUnit* target, float healthThreshold) {
    if (!target || target->IsDead()) {
        return false;
    }
    
    // Calculate health percentage
    int currentHealth = target->GetHealth();
    int maxHealth = target->GetMaxHealth();
    
    if (maxHealth <= 0) {
        return false; // Avoid division by zero
    }
    
    float healthPercent = static_cast<float>(currentHealth) / maxHealth * 100.0f;
    
    // Only heal if health is below the threshold
    return healthPercent < healthThreshold;
}

bool TargetingManager::FindHealingTargetForConditions(const std::vector<Rotation::Condition>& conditions, uint64_t& outTargetGuid) {
    // Static counter for logging frequency control
    static int logCounter = 0;
    const int LOG_FREQUENCY = 500;
    bool shouldLog = (++logCounter % LOG_FREQUENCY == 0);
    
    // Get player for reaction checks
    std::shared_ptr<WowPlayer> player = objectManager.GetLocalPlayer();
    if (!player) {
        return false;
    }
    
    // WowPlayer inherits from WowUnit, so we can safely cast
    WowUnit* playerUnit = static_cast<WowUnit*>(player.get());
    
    // Find healing conditions and get lowest threshold
    float lowestHealthThreshold = 100.0f;
    bool hasHealingCondition = false;
    
    for (const auto& cond : conditions) {
        if (cond.type == Rotation::Condition::Type::HEALTH_PERCENT_BELOW && 
            (cond.targetIsFriendly || !cond.targetIsPlayer)) {
            hasHealingCondition = true;
            if (cond.value < lowestHealthThreshold) {
                lowestHealthThreshold = cond.value;
            }
        }
    }
    
    if (!hasHealingCondition) {
        return false; // No healing conditions found
    }
    
    // Check current target first
    uint64_t currentTargetGuid = objectManager.GetCurrentTargetGUID();
    if (currentTargetGuid != 0) {
        std::shared_ptr<WowObject> currentTarget = objectManager.GetObjectByGUID(currentTargetGuid);
        auto targetUnit = std::dynamic_pointer_cast<WowUnit>(currentTarget);
        
        if (targetUnit && IsUnitFriendly(playerUnit, targetUnit.get()) && 
            ShouldHealTarget(targetUnit.get(), lowestHealthThreshold)) {
            outTargetGuid = currentTargetGuid;
            return true;
        }
    }
    
    // Look for other units that need healing
    uint64_t lowestHealthGuid = 0;
    float lowestHealth = 101.0f;
    
    auto units = objectManager.GetObjectsByType(WowObjectType::OBJECT_UNIT);
    for (const auto& objPtr : units) {
        auto unit = std::dynamic_pointer_cast<WowUnit>(objPtr);
        if (!unit || unit->GetGUID64() == player->GetGUID64() || 
            !IsUnitFriendly(playerUnit, unit.get()) || unit->IsDead()) {
            continue;
        }
        
        int maxHealth = unit->GetMaxHealth();
        float healthPercent = (maxHealth > 0) ? 
            (static_cast<float>(unit->GetHealth()) / maxHealth * 100.0f) : 0.0f;
        
        if (healthPercent < lowestHealthThreshold && healthPercent < lowestHealth) {
            lowestHealth = healthPercent;
            lowestHealthGuid = unit->GetGUID64();
        }
    }
    
    if (lowestHealthGuid != 0) {
        outTargetGuid = lowestHealthGuid;
        if (shouldLog) {
            Core::Log::Message("[Targeting] Found unit needing healing, health: " + 
                              std::to_string(lowestHealth) + "%");
        }
        return true;
    }
    
    return false;
}

// Find best target based on target type
uint64_t TargetingManager::FindBestTarget(Rotation::RotationEngine* engine_ptr, 
                                          Rotation::TargetType targetType,
                                          const std::string& nameFilter,
                                          bool useNameFilter,
                                          bool isTankingMode,
                                          bool isHealingSpellContext) {
    // GUI::LOSTab* losTab = GUI::GetLOSTab(); // REMOVED
    uint64_t currentDebugGuidFromTab = 0ULL;
    bool isDebugModeActiveViaTab = false;
    /* REMOVED
    if (losTab && losTab->IsDebugTargetSet()) {
        isDebugModeActiveViaTab = true;
        currentDebugGuidFromTab = losTab->GetDebugTargetGUID();
    }
    */

    static int find_best_target_counter = 0;
    bool shouldLogThisEntry = (++find_best_target_counter % 200 == 0);
    if (isDebugModeActiveViaTab) shouldLogThisEntry = true; // Always log if tab debug is on

    if (shouldLogThisEntry) { 
        std::stringstream entry_log_ss;
        entry_log_ss << "[FIND_BEST_TARGET_NO_LOS] Called (count: " << find_best_target_counter 
                     << "), target type: ";
        switch (targetType) {
            case Rotation::TargetType::ENEMY: entry_log_ss << "ENEMY"; break;
            case Rotation::TargetType::FRIENDLY: entry_log_ss << "FRIENDLY"; break;
            case Rotation::TargetType::SELF: entry_log_ss << "SELF"; break;
            case Rotation::TargetType::SELF_OR_FRIENDLY: entry_log_ss << "SELF_OR_FRIENDLY"; break;
            case Rotation::TargetType::ANY: entry_log_ss << "ANY"; break;
            case Rotation::TargetType::NONE: entry_log_ss << "NONE"; break;
            default: entry_log_ss << "UNKNOWN"; break;
        }
        if (isDebugModeActiveViaTab) {
            entry_log_ss << ", TabDebugGUID: 0x" << std::hex << currentDebugGuidFromTab << std::dec;
        }
        Core::Log::Message(entry_log_ss.str());
    }
    
    std::shared_ptr<WowPlayer> player = objectManager.GetLocalPlayer(); // Get player once at the top

    // Handle types that don't need unit iteration first
    switch (targetType) {
        case Rotation::TargetType::ENEMY:
            {
                if (player) {
                    bool onlyCombat = engine_ptr ? engine_ptr->IsOnlyTargetingCombatUnits() : true;
                    std::shared_ptr<WowUnit> foundUnit = Rotation::FindBestEnemyTarget(player.get(), objectManager, *this, onlyCombat, isTankingMode);
                    if (foundUnit) {
                        if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] ENEMY type: Found unit " + foundUnit->GetName() + " via Rotation::FindBestEnemyTarget. Returning its GUID.");
                        return foundUnit->GetGUID64();
                    }
                } else {
                    if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] ENEMY type: Player null, cannot find enemy target.");
                }
                return 0; // No enemy found
            }
            
        case Rotation::TargetType::FRIENDLY:
        case Rotation::TargetType::SELF_OR_FRIENDLY:
            {
                if (player) {
                    bool includeSelf = (targetType == Rotation::TargetType::SELF_OR_FRIENDLY);
                    std::shared_ptr<WowUnit> foundUnit = Rotation::FindBestFriendlyTarget(player, objectManager, *this, includeSelf);
                    if (foundUnit) {
                        if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] FRIENDLY/SELF_OR_FRIENDLY type: Found unit " + foundUnit->GetName() + " via Rotation::FindBestFriendlyTarget. Returning its GUID.");
                        return foundUnit->GetGUID64();
                    }
                } else {
                     if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] FRIENDLY/SELF_OR_FRIENDLY type: Player null, cannot find friendly target.");
                }
                return 0; // No friendly found
            }
        case Rotation::TargetType::SELF:
            if (player) {
                if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] SELF type: Returning player GUID.");
                return player->GetGUID64();
            }
            if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] SELF type: Player is null, returning 0.");
            return 0;
        case Rotation::TargetType::NONE:
            if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] NONE type: Returning 0.");
            return 0;
        case Rotation::TargetType::ANY:
            // ANY type will proceed to the general loop below.
            if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] ANY type: Proceeding to general unit iteration.");
            break; // Break to proceed to the loop
        default:
            if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] Unknown target type. Returning 0.");
            return 0; // Unknown type, return no target
    }

    // This loop is now ONLY for TargetType::ANY
    if (targetType != Rotation::TargetType::ANY) {
        return 0; // Should not reach here if other types are handled correctly above.
    }

    if (!player) { // Check player again specifically for ANY type loop
        if (shouldLogThisEntry) Core::Log::Message("[TargetingManager::FindBestTarget] ANY type loop: Player is null. Returning 0.");
        return 0;
    }

    uint64_t bestOverallGuid = 0;
    float closestOverallDistance = 1000.0f;
    Vector3 playerPos = player->GetPosition(); // Use player's base position for distance
    int validCount = 0;
    int rejectedCount = 0;

    auto units = objectManager.GetObjectsByType(WowObjectType::OBJECT_UNIT);

    for (const auto& objPtr : units) {
        auto unit = std::dynamic_pointer_cast<WowUnit>(objPtr);
        if (!unit || unit->GetGUID64() == player->GetGUID64() || unit->IsDead()) {
            rejectedCount++;
            continue;
        }

        std::string unitName = unit->GetName();
        std::string unitNameLower = unitName;
        std::transform(unitNameLower.begin(), unitNameLower.end(), unitNameLower.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        if (IsUnitBlacklisted(unit.get())) {
            if (shouldLogThisEntry) Core::Log::Message("[Targeting] Skipping blacklisted unit: " + unitName);
            rejectedCount++;
            continue;
        }
        
        if (useNameFilter && !nameFilter.empty()) {
            std::string filterLower = nameFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (unitNameLower.find(filterLower) == std::string::npos) {
                if (shouldLogThisEntry) Core::Log::Message("[Targeting] Skipping unit '" + unitName + "': Filter '" + nameFilter + "' not found.");
                rejectedCount++;
                continue; 
            }
        }

        // Consider units that are friendly
        if (player && IsUnitFriendly(player.get(), unit.get())) {
            // Core::Log::Message("[FBT] Unit " + unit->GetName() + " is friendly.");

            // +++ NEW HEALING BLACKLIST LOGIC +++
            if (isHealingSpellContext) {
                uint32_t unitFlags = unit->GetUnitFlags(); // Use existing GetUnitFlags()
                std::string unitName = unit->GetName();
                bool isFlagBlacklisted = (unitFlags & 0x8808) == 0x8808;
                bool isNameException = (unitName == "DonaldTrump");

                if (isFlagBlacklisted && !isNameException) {
                    // Optional: Log why a unit is skipped for healing
                    // Core::Log::Message("[FBT-Heal] Skipping blacklisted unit: " + unitName + " (Flags: 0x" + std::hex + unitFlags + std::dec + ")");
                    continue; // Skip this unit for healing
                }
            }
            // --- END HEALING BLACKLIST LOGIC ---

            validCount++;
            Vector3 targetPos = unit->GetPosition();
            if (targetPos.IsZero()) { 
                rejectedCount++; 
                continue; 
            }
            
            float distance = playerPos.Distance(targetPos);
            if (distance > 40.0f) { // Max targeting range for ANY type
                rejectedCount++; 
                continue; 
            }

            if (distance < closestOverallDistance) {
                closestOverallDistance = distance;
                bestOverallGuid = unit->GetGUID64();
            }
        }
    }
    
    if (shouldLogThisEntry) {
        if (bestOverallGuid != 0) {
            std::shared_ptr<WowObject> targetObj = objectManager.GetObjectByGUID(bestOverallGuid);
            if (auto targetUnit = std::dynamic_pointer_cast<WowUnit>(targetObj)) {
                std::stringstream ss;
                ss << "[Targeting] Selected target for ANY (No LOS): " << targetUnit->GetName() 
                   << " (0x" << std::hex << bestOverallGuid << std::dec
                   << ") at distance " << closestOverallDistance << "yd";
                ss << " - Candidates: " << validCount << ", Rejected: " << rejectedCount;
                Core::Log::Message(ss.str());
            }
        } else {
            Core::Log::Message("[Targeting] No suitable ANY target found (No LOS). Candidates: " + std::to_string(validCount) + ", Rejected: " + std::to_string(rejectedCount));
        }
    }

    return bestOverallGuid;
}

void TargetingManager::ClearReactionCache() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    reactionCache.clear();
    Core::Log::Message("[Targeting] Reaction cache cleared");
}

// Definition for IntersectFlagsToString
std::string IntersectFlagsToString(IntersectFlags flags) {
    if (flags == None) {
        return "None";
    }
    std::string result = "";
    if (flags & DoodadCollision) result += "DoodadCollision | ";
    if (flags & WmoCollision) result += "WmoCollision | ";
    if (flags & WmoRender) result += "WmoRender | ";
    if (flags & WmoNoCamCollision) result += "WmoNoCamCollision | ";
    if (flags & Terrain) result += "Terrain | ";
    if (flags & IgnoreWmoDoodad) result += "IgnoreWmoDoodad | ";
    if (flags & LiquidWaterWalkable) result += "LiquidWaterWalkable | ";
    if (flags & LiquidAll) result += "LiquidAll | ";
    if (flags & Cull) result += "Cull | ";
    if (flags & EntityCollision) result += "EntityCollision | ";
    if (flags & EntityRender) result += "EntityRender | ";
    // Check for combined/named flags if they are exact matches, 
    // or ensure the individual components cover them.
    // For example, if GameGenericLOS is just a combination of the above, this is fine.
    // If it's a unique value not made of the above, it needs its own check.
    // Based on current enum, the specific combined flags are just aliases or specific values.
    // This function will list the components.

    if (!result.empty()) {
        // Remove trailing " | "
        result.resize(result.length() - 3);
    }
    return result;
}

bool TargetingManager::IsUnitBlacklisted(WowUnit* unit) const {
    if (!unit) {
        return false;
    }
    std::string unitName = unit->GetName();
    if (unitName.empty()) {
        return false;
    }
    std::string unitNameLower = unitName;
    std::transform(unitNameLower.begin(), unitNameLower.end(), unitNameLower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // Check for exact matches first
    if (unitNameBlacklist.count(unitNameLower)) {
        return true;
    }

    // Check for substring matches for generic terms
    static const std::vector<std::string> substringBlacklist = {
        "totem",
        "whelp",
        "dragon"
    };

    for (const auto& substring : substringBlacklist) {
        if (unitNameLower.find(substring) != std::string::npos) {
            return true;
        }
    }

    return false;
}

// --- Implementation of New BG Mode methods ---

void TargetingManager::SetBGModeEnabled(bool enabled) {
    m_bgModeEnabled.store(enabled);
    Core::Log::Message(std::string("[TargetingManager] BG Mode ") + (enabled ? "Enabled" : "Disabled"));
    if (!enabled) { // If disabling, reset faction to unknown so it's re-checked if re-enabled.
        std::lock_guard<std::mutex> lock(m_factionMutex);
        m_localPlayerFaction = FactionInfo::PlayerFaction::UNKNOWN;
    }
}

bool TargetingManager::IsBGModeEnabled() const {
    return m_bgModeEnabled.load();
}

void TargetingManager::UpdateLocalPlayerFaction(WowPlayer* player) {
    if (!player) {
        std::lock_guard<std::mutex> lock(m_factionMutex);
        if (m_localPlayerFaction != FactionInfo::PlayerFaction::UNKNOWN) {
             Core::Log::Message("[TargetingManager] Player is null, setting faction to UNKNOWN.");
             m_localPlayerFaction = FactionInfo::PlayerFaction::UNKNOWN;
        }
        return;
    }

    FactionInfo::PlayerFaction determinedFaction = FactionInfo::PlayerFaction::UNKNOWN;
    if (Spells::UnitHasAura(player, FactionInfo::ALLIANCE_AURA_ID)) {
        determinedFaction = FactionInfo::PlayerFaction::ALLIANCE;
    } else if (Spells::UnitHasAura(player, FactionInfo::HORDE_AURA_ID)) {
        determinedFaction = FactionInfo::PlayerFaction::HORDE;
    }

    std::lock_guard<std::mutex> lock(m_factionMutex);
    if (m_localPlayerFaction != determinedFaction) {
        m_localPlayerFaction = determinedFaction;
        std::string factionStr = "UNKNOWN";
        if (m_localPlayerFaction == FactionInfo::PlayerFaction::ALLIANCE) factionStr = "ALLIANCE";
        else if (m_localPlayerFaction == FactionInfo::PlayerFaction::HORDE) factionStr = "HORDE";
        Core::Log::Message("[TargetingManager] Updated local player faction to: " + factionStr);
    }
}

FactionInfo::PlayerFaction TargetingManager::GetLocalPlayerFaction() const {
    std::lock_guard<std::mutex> lock(m_factionMutex);
    return m_localPlayerFaction;
}

} // End of the single, top-level namespace Spells that starts around line 22. 