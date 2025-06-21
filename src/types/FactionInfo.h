#pragma once

#include <cstdint>

namespace FactionInfo {

const uint32_t ALLIANCE_AURA_ID = 86475;
const uint32_t HORDE_AURA_ID = 86476;

enum class PlayerFaction {
    UNKNOWN,
    ALLIANCE,
    HORDE
};

} // namespace FactionInfo 