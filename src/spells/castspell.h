#pragma once

#include <cstdint>

namespace Spells {

// Address: 0x0080DA40
// Signature from assembly: char __stdcall CastLocalPlayerSpell(int spellId, int unknown, uint64_t targetGuid, char unknown2);
// Simplified wrapper: Casts a spell for the local player.
// Returns true on success (based on char return type), false otherwise.
bool CastSpell(int spellId, uint64_t targetGuid, bool requiresTarget);

// Check if spell exists in player's spellbook
bool SpellExists(int spellId);

// Add other spell-related functions here

} 