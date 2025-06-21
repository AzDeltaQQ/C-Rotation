#include "cooldowns.h"
#include "SpellManager.h"
// #include "logs/log.h" // Ensure Log.h is included for Core::Log
// #include <sstream>   // For std::stringstream
#include <Windows.h> // For OutputDebugStringA

namespace Spells {

void CooldownManager::RecordSpellCast(int spellId) {
    // Record cast time for debugging and GCD tracking
    spellLastCastTime[spellId] = std::chrono::steady_clock::now();
    
    // Removed logging - no more cast logs
}

bool CooldownManager::IsSpellOnCooldown(int spellId) {
    // std::stringstream ss_cd_check; // Use a stringstream for structured logging
    // ss_cd_check << "[CooldownCheck] Checking SpellID: " << spellId;

    // Use WoW's internal cooldown system via SpellManager
    int remainingCooldownMs = SpellManager::GetSpellCooldownMs(spellId);
    // ss_cd_check << ", GameCD_Ms: " << remainingCooldownMs;
    
    // If cooldown is reported by the game, use that
    if (remainingCooldownMs > 0) {
        // ss_cd_check << ". Result: true (Game reports cooldown).";
        // OutputDebugStringA(ss_cd_check.str().c_str()); // Direct debug output
        // Core::Log::Message(ss_cd_check.str());
        return true;
    }
    
    // If the game says spell is ready, check for GCD (basic 1.5s rule)
    // This is a safety check in case WoW's cooldown system doesn't properly report GCD
    if (spellLastCastTime.count(spellId) > 0) {
        auto now = std::chrono::steady_clock::now();
        auto lastCastPoint = spellLastCastTime[spellId]; // Get the time point
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCastPoint);
        
        // ss_cd_check << ", GCD_Check: lastCastTime found, Elapsed_Ms: " << elapsed.count();

        // If less than 1.5s has passed since last cast, consider it on GCD
        if (elapsed.count() < 1500) {
            // ss_cd_check << ". Result: true (GCD active < 1500ms).";
            // OutputDebugStringA(ss_cd_check.str().c_str()); // Direct debug output
            // Core::Log::Message(ss_cd_check.str());
            return true;
        }
        // ss_cd_check << " (GCD > 1500ms)";
    } // else {
        // ss_cd_check << ", GCD_Check: No lastCastTime recorded for this spell.";
    // }
    
    // Not on cooldown
    // ss_cd_check << ". Result: false (Not on game CD, not on GCD).";
    // OutputDebugStringA(ss_cd_check.str().c_str()); // Direct debug output
    // Core::Log::Message(ss_cd_check.str());
    return false;
}

int CooldownManager::GetRemainingCooldown(int spellId) {
    // Always use the game's cooldown data first
    int gameCooldown = SpellManager::GetSpellCooldownMs(spellId);
    if (gameCooldown > 0) {
        return gameCooldown;
    }
    
    // Check GCD
    if (spellLastCastTime.count(spellId) > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - spellLastCastTime[spellId]);
        
        // If within GCD window, return remaining GCD time
        if (elapsed.count() < 1500) {
            return 1500 - static_cast<int>(elapsed.count());
        }
    }
    
    // Not on cooldown
    return 0;
}

} 