#include "castspell.h"
#include "../logs/log.h"
#include "../objectManager/ObjectManager.h"
#include "../types/wowunit.h"
#include <sstream>

// Define the function pointer type matching the ACTUAL game function calling convention
// Change to __cdecl to match how WoW likely implements this function
typedef char (__cdecl* CastLocalPlayerSpell_t)(int spellId, int unknown, uint64_t targetGuid, char unknown2);

// Define the function pointer at the specified address
CastLocalPlayerSpell_t CastLocalPlayerSpell_ptr = (CastLocalPlayerSpell_t)0x0080DA40;

namespace Spells {

bool CastSpell(int spellId, uint64_t targetGuid, bool requiresTarget) {
    // Log in hex format for better debugging
    char guidHexBuffer[32];
    sprintf_s(guidHexBuffer, sizeof(guidHexBuffer), "0x%llX", targetGuid);
    
    // Pre-cast validation
    if (requiresTarget && targetGuid != 0) {
        ObjectManager* om = ObjectManager::GetInstance();
        if (om && om->IsInitialized()) {
            std::shared_ptr<WowUnit> targetUnit = om->GetUnitByGuid(WGUID(targetGuid));
            if (!targetUnit) {
                std::stringstream ss;
                ss << "[Spells::CastSpell] Target GUID " << guidHexBuffer 
                   << " for spell " << spellId << " (requiresTarget=true) not found or invalid. Aborting cast.";
                Core::Log::Message(ss.str());
                return false;
            }
        } else {
            std::stringstream ss;
            ss << "[Spells::CastSpell] ObjectManager not available or not initialized for spell " << spellId
               << " (requiresTarget=true) with target GUID " << guidHexBuffer << ". Aborting cast.";
            Core::Log::Message(ss.str());
            return false; // Cannot validate target if OM is not ready
        }
    } else if (requiresTarget && targetGuid == 0) {
        std::stringstream ss;
        ss << "[Spells::CastSpell] Spell " << spellId << " requires a target, but targetGuid is 0. Aborting cast.";
        Core::Log::Message(ss.str());
        return false;
    }

    // Remove excessive logging - too verbose for regular operation
    static int castSpellCounter = 0;
    castSpellCounter++;
    bool shouldLog = (castSpellCounter % 500 == 0); // Log only occasionally
    
    if (shouldLog) {
        Core::Log::Message("CastSpell: Called with spellId=" + std::to_string(spellId) + 
                         " targetGuid=" + std::string(guidHexBuffer));
    }
    
    try {
        // We need to pass placeholder values for the unknown parameters.
        // Common practice is to use 0 or -1, but the actual required value might differ.
        // The second parameter seems to be some kind of flag (0 = normal cast?)
        // The last parameter might be related to cast flags or interrupt status
        char result = CastLocalPlayerSpell_ptr(spellId, 0, targetGuid, 0);
        
        // Remove excessive logging for cast results
        if (shouldLog) {
            Core::Log::Message("CastSpell: Result = " + std::to_string((int)result));
        }
        
        return result != 0;
    }
    catch (...) {
        Core::Log::Message("CastSpell: Exception occurred during cast!");
        return false;
    }
}

// Simplified SpellExists implementation
bool SpellExists(int spellId) {
    // In a full implementation, this would check the player's spell book.
    // For now, just return true to assume all spells exist
    return true;
}

} 