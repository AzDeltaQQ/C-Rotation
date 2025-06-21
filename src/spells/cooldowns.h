#pragma once

#include <unordered_map>
#include <chrono> // For time tracking

namespace Spells {

class CooldownManager {
public:
    /**
     * Record when a spell is cast to track its cooldown
     * @param spellId The ID of the spell that was cast
     */
    void RecordSpellCast(int spellId);
    
    /**
     * Check if a spell is on cooldown according to WoW or GCD tracking
     * @param spellId The ID of the spell to check
     * @return true if the spell is on cooldown, false otherwise
     */
    bool IsSpellOnCooldown(int spellId);
    
    /**
     * Get the remaining cooldown time in milliseconds
     * @param spellId The ID of the spell to check
     * @return Remaining cooldown in milliseconds (0 if ready)
     */
    int GetRemainingCooldown(int spellId);

private:
    // Last time each spell was cast (for GCD tracking)
    std::unordered_map<int, std::chrono::steady_clock::time_point> spellLastCastTime;
};

} 