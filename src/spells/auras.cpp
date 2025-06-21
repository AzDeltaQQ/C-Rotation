#include "auras.h"
#include "../logs/log.h"
#include <sstream>

// Define the function pointer type using __thiscall
using getAuraAtIndex_t = Spells::Aura* (__thiscall*)(void* unit, unsigned int index);

// Map the function pointer to the WoW client function address
getAuraAtIndex_t getAuraAtIndex_ptr = reinterpret_cast<getAuraAtIndex_t>(0x00556E10);

// Define offsets based on disassembly provided
// These need to match the assembly code exactly
const uintptr_t AURA_COUNT_1 = 0xDD0;  // Primary count [ecx+0DD0h]
const uintptr_t AURA_COUNT_2 = 0xC54;  // Secondary count [ecx+0C54h]
const uintptr_t AURA_TABLE_1 = 0xC50;  // Primary array when count at DD0 != -1
const uintptr_t AURA_TABLE_2 = 0xC58;  // Secondary array when count at DD0 == -1
const uintptr_t AURA_SIZE = 0x18;      // Size of each aura entry

namespace Spells {

Aura* GetAuraByIndex(WowObject* unit, unsigned int index) {
    if (!unit) {
        return nullptr;
    }
                     
    // Instead of using the game function, implement our own version using direct memory access
    try {
        uintptr_t baseAddr = unit->GetBaseAddress();
        if (baseAddr == 0) {
            return nullptr;
        }
        
        // Get aura count to validate the index
        uint32_t auraCount = GetUnitAuraCount(unit);
        if (index >= auraCount) {
            return nullptr;
        }
        
        // First determine which aura table to use
        uint32_t auraCount1 = *reinterpret_cast<uint32_t*>(baseAddr + AURA_COUNT_1);
        
        uintptr_t auraTableBase = 0;
        if (auraCount1 == 0xFFFFFFFF) {
            // Use secondary table (pointer-based)
            uintptr_t* tablePtr = *reinterpret_cast<uintptr_t**>(baseAddr + AURA_TABLE_2);
            if (!tablePtr) {
                return nullptr;
            }
            auraTableBase = reinterpret_cast<uintptr_t>(tablePtr);
        } else {
            // Use primary table (embedded)
            auraTableBase = baseAddr + AURA_TABLE_1;
        }
        
        // Calculate the address of the aura at the specified index
        uintptr_t auraAddr = auraTableBase + (index * AURA_SIZE);
        
        // Create a static Aura instance to return (avoids memory leaks)
        static Aura auraResult;
        
        // Read aura data into our structure
        auraResult.casterGuid = *reinterpret_cast<uint64_t*>(auraAddr);
        auraResult.spellId = *reinterpret_cast<uint32_t*>(auraAddr + 0x8);
        auraResult.flags = *reinterpret_cast<uint8_t*>(auraAddr + 0xC);
        auraResult.level = *reinterpret_cast<uint8_t*>(auraAddr + 0xD);
        auraResult.stackCount = *reinterpret_cast<uint8_t*>(auraAddr + 0xE);
        auraResult.unknown_F = *reinterpret_cast<uint8_t*>(auraAddr + 0xF);
        auraResult.duration = *reinterpret_cast<uint32_t*>(auraAddr + 0x10);
        auraResult.expireTime = *reinterpret_cast<uint32_t*>(auraAddr + 0x14);
        
        return &auraResult;
    }
    catch (...) {
        return nullptr;
    }
}

// Get the number of auras on a unit by directly reading memory
// This matches the logic from the disassembly
uint32_t GetUnitAuraCount(WowObject* unit) {
    if (!unit) {
        return 0;
    }

    uintptr_t baseAddr = unit->GetBaseAddress();
    if (baseAddr == 0) {
        return 0;
    }

    // First check auraCount1 at 0xDD0
    uint32_t auraCount1 = *reinterpret_cast<uint32_t*>(baseAddr + AURA_COUNT_1);
    
    // If it's -1 (0xFFFFFFFF), use auraCount2 at 0xC54
    if (auraCount1 == 0xFFFFFFFF) {
        return *reinterpret_cast<uint32_t*>(baseAddr + AURA_COUNT_2);
    }
    
    // Otherwise, use auraCount1
    return auraCount1;
}

bool UnitHasAura(WowObject* unit, uint32_t spellId, uint64_t casterGuid) {
    if (!unit) {
        return false;
    }

    uint32_t auraCount = GetUnitAuraCount(unit);
    
    // Iterate through all auras
    for (uint32_t i = 0; i < auraCount; i++) {
        Aura* aura = GetAuraByIndex(unit, i);
        
        if (!aura) {
            continue;
        }
        
        // Check if this aura matches our spell ID
        if (aura->spellId == spellId) {
            // If caster GUID is specified, also check that
            if (casterGuid != 0 && aura->casterGuid != casterGuid) {
                continue;
            }
            
            // Remove excessive logging
            // if (spellId == 21084) {
            //     Core::Log::Message("UnitHasAura: Found Seal of Righteousness");
            // }
            return true;
        }
    }
    
    return false;
}

bool UnitHasAuraWithMinStacks(WowObject* unit, uint32_t spellId, int minStacks, uint64_t casterGuid) {
    if (!unit) {
        return false;
    }
    uint32_t auraCount = GetUnitAuraCount(unit);

    // --- CONSOLIDATED DEBUG LOGGING BLOCK (Initially Commented Out) ---
    // bool enableAuraDebugLogging = false; // TODO: Tie this to a global debug flag or config
    // if (enableAuraDebugLogging && unit) { 
    //     std::stringstream ss_debug_log;
    //     ss_debug_log << "[AuraCheck_Verbose] Entry: UnitGUID=" << unit->GetGUID64()
    //                    << ", IsPlayer=" << (unit->IsPlayer() ? "Yes" : "No")
    //                    << ", TargetSpellID=" << spellId
    //                    << ", RequiredMinStacks=" << minStacks
    //                    << ", RequiredCasterGUID=" << casterGuid
    //                    << ", TotalAurasOnUnit=" << auraCount << "\n";
    //
    //     if (auraCount > 0) {
    //         ss_debug_log << "    --- Listing all auras on unit ---\n";
    //         for (uint32_t j = 0; j < auraCount; j++) {
    //             Aura* current_aura = GetAuraByIndex(unit, j);
    //             if (current_aura) {
    //                 ss_debug_log << "    Index[" << j << "]: "
    //                                << "SpellID=" << current_aura->spellId
    //                                << ", Stacks=" << static_cast<int>(current_aura->stackCount)
    //                                << ", CasterGUID=" << current_aura->casterGuid
    //                                << ", Flags=" << static_cast<int>(current_aura->flags) << "\n";
    //             } else {
    //                 ss_debug_log << "    Index[" << j << "]: GetAuraByIndex returned nullptr\n";
    //             }
    //         }
    //         ss_debug_log << "    --- End of aura list ---\n";
    //     }
    //     // To log the outcome, this stringstream would need to be passed around or 
    //     // the logging call made just before each return statement.
    //     // For now, this consolidated block primarily handles entry and list.
    //     // Core::Log::Message(ss_debug_log.str()); // Single call to log if enabled
    // }
    // --- END CONSOLIDATED DEBUG LOGGING BLOCK ---

    for (uint32_t i = 0; i < auraCount; i++) {
        Aura* aura = GetAuraByIndex(unit, i);

        if (!aura) {
            continue;
        }

        if (aura->spellId == spellId) {
            // Spell ID matches. Now check caster.
            if (casterGuid != 0 && aura->casterGuid != casterGuid) {
                // Caster GUID specified and doesn't match.
                // if (enableAuraDebugLogging && unit) {
                //     std::stringstream ss_temp_log;
                //     ss_temp_log << "[AuraCheck_Verbose] Info: SpellID " << spellId << " matched, but CasterGUID " << aura->casterGuid << " != " << casterGuid << ". Continuing.\n";
                //     // Core::Log::Message(ss_temp_log.str());
                // }
                continue; // Try next aura instance.
            }

            // Caster GUID matches (or wasn't specified). Now check stacks.
            if (minStacks <= 0) {
                // No positive stack requirement (0 or negative means check presence only).
                // If we are here, spellId and casterId (if specified) matched. So, condition met.
                // if (enableAuraDebugLogging && unit) {
                //     std::stringstream ss_temp_log;
                //     ss_temp_log << "[AuraCheck_Verbose] Outcome: MetNoStackReq. UnitGUID=" << unit->GetGUID64() << ", SpellID=" << spellId << ". Returning true.\n";
                //     // Core::Log::Message(ss_temp_log.str());
                // }
                return true;
            } else {
                // Positive stack requirement (minStacks > 0).
                if (aura->stackCount >= static_cast<uint32_t>(minStacks)) {
                    // Stacks are sufficient.
                    // if (enableAuraDebugLogging && unit) {
                    //     std::stringstream ss_temp_log;
                    //     ss_temp_log << "[AuraCheck_Verbose] Outcome: StacksMet. UnitGUID=" << unit->GetGUID64() << ", SpellID=" << spellId << ", Stacks=" << aura->stackCount << " >= " << minStacks << ". Returning true.\n";
                    //     // Core::Log::Message(ss_temp_log.str());
                    // }
                    return true; 
                }
                // Stacks insufficient for *this particular aura instance*.
                // Loop continues to check other aura instances.
                // if (enableAuraDebugLogging && unit) {
                //     std::stringstream ss_temp_log;
                //     ss_temp_log << "[AuraCheck_Verbose] Info: SpellID " << spellId << " matched, Caster ok, but StacksInsufficient (" << aura->stackCount << " < " << minStacks << "). Continuing.\n";
                //     // Core::Log::Message(ss_temp_log.str());
                // }
            }
        }
    }

    // If loop completes, no matching aura found that met all criteria.
    // if (enableAuraDebugLogging && unit) {
    //     std::stringstream ss_outcome_log;
    //     ss_outcome_log << "[AuraCheck_Verbose] Outcome: AuraNotFoundOrFailed. UnitGUID=" << unit->GetGUID64()
    //                    << ", TargetSpellID=" << spellId
    //                    << ", RequiredMinStacks=" << minStacks
    //                    << ". Returning false.\n";
    //     // Core::Log::Message(ss_outcome_log.str());
    // }
    return false;
}

// Add a function to check for specific known auras and their memory patterns
void ScanForSpecificAura(WowObject* unit, uint32_t targetSpellId) {
    if (!unit) return;
    
    uintptr_t baseAddr = unit->GetBaseAddress();
    if (baseAddr == 0) return;
    
    // For Seal of Righteousness (21084), scan a wider range at various offsets
    // Scan in chunks of unit descriptor memory where auras typically reside
    const uintptr_t scanRanges[][2] = {
        {0x0C00, 0x1000},    // Main descriptor block
        {0x3000, 0x4000},    // Extended buffs area
        {0x10000, 0x11000},  // Dynamic memory region
    };
    
    for (const auto& range : scanRanges) {
        uintptr_t start = baseAddr + range[0];
        uintptr_t end = baseAddr + range[1];
        
        for (uintptr_t offset = start; offset < end; offset += 4) {
            try {
                uint32_t value = *reinterpret_cast<uint32_t*>(offset);
                if (value == targetSpellId) {
                    Core::Log::Message("Found spell ID match at offset 0x" + std::to_string(offset - baseAddr));
                }
            }
            catch (...) {
                // Silently skip invalid memory
            }
        }
    }
}

// Update the DumpPlayerAuras function to include the memory scan for specific auras
void DumpPlayerAuras(WowObject* player) {
    if (!player) {
        return;
    }

    uintptr_t baseAddr = player->GetBaseAddress();
    if (baseAddr == 0) {
        return;
    }
    
    // Get actual count and active table
    uint32_t auraCount = GetUnitAuraCount(player);
    Core::Log::Message("Player has " + std::to_string(auraCount) + " auras");
    
    // Dump all auras
    for (uint32_t i = 0; i < auraCount; i++) {
        Aura* aura = GetAuraByIndex(player, i);
        if (!aura) {
            continue;
        }
        
        std::stringstream ss;
        ss << "Aura[" << i << "]: SpellID=" << std::dec << aura->spellId;
        Core::Log::Message(ss.str());
    }
    
    // Brief scan for Seal of Righteousness
    ScanForSpecificAura(player, 21084);
}

} 