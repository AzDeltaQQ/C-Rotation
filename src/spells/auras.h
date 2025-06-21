#pragma once

#include <cstdint>
#include "types/types.h"
#include "types/wowobject.h"

namespace Spells {

// Matches the memory layout from WoW 3.3.5a client
struct Aura {
    uint64_t casterGuid;    // Offset 0x0 (Low DWORD), 0x4 (High DWORD)
    uint32_t spellId;       // Offset 0x8 - This is what we check for aura presence
    uint8_t  flags;         // Offset 0xC
    uint8_t  level;         // Offset 0xD
    uint8_t  stackCount;    // Offset 0xE
    uint8_t  unknown_F;     // Offset 0xF
    uint32_t duration;      // Offset 0x10
    uint32_t expireTime;    // Offset 0x14
}; // Total size: 0x18 (24 bytes)

// Constants for aura access (from reverse engineering)
constexpr uint32_t AURA_COUNT_1 = 0xDD0;   // If this is -1, use AURA_COUNT_2
constexpr uint32_t AURA_COUNT_2 = 0xC54;
constexpr uint32_t AURA_TABLE_1 = 0xC50;   // Embedded array when AURA_COUNT_1 != -1
constexpr uint32_t AURA_TABLE_2 = 0xC58;   // Pointer to array when AURA_COUNT_1 == -1
constexpr uint32_t AURA_SIZE = 0x18;       // Size of each AuraEntry (24 bytes)
constexpr uint32_t AURA_SPELL_ID = 0x8;    // Offset of spellId within AuraEntry

// Function declarations
bool UnitHasAura(WowObject* unit, uint32_t spellId, uint64_t casterGuid = 0);
bool UnitHasAuraWithMinStacks(WowObject* unit, uint32_t spellId, int minStacks, uint64_t casterGuid = 0);
uint32_t GetUnitAuraCount(WowObject* unit);
Aura* GetAuraByIndex(WowObject* unit, unsigned int index);
void DumpPlayerAuras(WowObject* player);

} 