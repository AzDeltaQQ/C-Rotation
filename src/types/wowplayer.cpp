#include "wowplayer.h"
#include "../utils/memory.h"

// Define the specific looting flag bit
const uint32_t UNIT_FLAG_IS_LOOTING = 0x400;

// Constructor
WowPlayer::WowPlayer(uintptr_t baseAddress, WGUID guid)
    : WowUnit(baseAddress, guid) // CORRECTED: Call base constructor with exactly two arguments
{
    m_type = WowObjectType::OBJECT_PLAYER; // Explicitly set type for WowPlayer after base construction
    // Player-specific initialization, if any
}

// Override UpdateDynamicData 
void WowPlayer::UpdateDynamicData() {
    // First call the base unit update to get basic data
    WowUnit::UpdateDynamicData(); 

    // If we're a player, we need special handling for power types
    try {
        if (!m_baseAddress) return;
        
        // Get descriptor pointer
        uintptr_t descriptorPtr = Memory::Read<uintptr_t>(m_baseAddress + Offsets::OBJECT_DESCRIPTOR_PTR);
        if (!descriptorPtr) {
            // Core::Log::Message("[WowPlayer] Invalid descriptor pointer");
            return;
        }
        
        // Read player class from descriptor
        // In WoW, UNIT_FIELD_BYTES_0 contains race, class, gender, and power type
        uint32_t bytes0 = Memory::Read<uint32_t>(descriptorPtr + 0x38 * 4); // UNIT_FIELD_BYTES_0
        uint8_t playerClass = (bytes0 >> 8) & 0xFF; // Class is second byte
        
        // Read current power type again, directly from the correct field
        m_cachedPowerType = Memory::Read<uint8_t>(descriptorPtr + Offsets::DESCRIPTOR_FIELD_POWTYPE);
        
        // For Project Ascension, players can have multiple power types
        // Read power type from memory but ensure all power arrays are properly updated

        // Special case for Ascension - enable all power types that have a max value greater than 0
        bool hasMultipleResources = false;
        int activeResourceCount = 0;

        for (uint8_t i = 0; i < PowerType::POWER_TYPE_COUNT; i++) {
            if (m_hasPowerType[i] && m_cachedMaxPowers[i] > 0) {
                activeResourceCount++;
                if (activeResourceCount > 1) {
                    hasMultipleResources = true;
                    break;
                }
            }
        }
        
        // Now update power values using the determined power type
        uintptr_t powerOffset = Offsets::UNIT_FIELD_POWER_BASE + (m_cachedPowerType * 4);
        uintptr_t maxPowerOffset = Offsets::UNIT_FIELD_MAXPOWER_BASE + (m_cachedPowerType * 4);
        
        // Use the array versions instead of the individual variables
        m_cachedPowers[m_cachedPowerType] = Memory::Read<int>(descriptorPtr + powerOffset);
        m_cachedMaxPowers[m_cachedPowerType] = Memory::Read<int>(descriptorPtr + maxPowerOffset);
        
    } catch (const std::exception& /*e*/) {
        // Core::Log::Message("[WowPlayer] Exception reading player data: " + std::string(e.what()));
    } catch (...) {
        // Core::Log::Message("[WowPlayer] Unknown exception reading player data");
    }
} 

// Removed definitions for HasAura and HasAuraWithMinStacks as they are inherited from WowUnit

bool WowPlayer::IsLooting() const {
    // Check if the player has the specific looting flag bit (0x400)
    // This requires GetUnitFlags() to be available in WowUnit and return the flags correctly.
    uint32_t currentFlags = this->GetUnitFlags(); // Assuming this method exists and gives current flags
    bool isLooting = (currentFlags & UNIT_FLAG_IS_LOOTING) != 0;

    // Optional: Infrequent logging to verify, can be uncommented if needed.
    /*
    static int RARE_LOG_COUNTER_ISLOOTING_FLAG = 0;
    if (++RARE_LOG_COUNTER_ISLOOTING_FLAG % 75 == 0) { // Log less frequently
        std::stringstream log_s;
        log_s << "[WowPlayer::IsLooting] GUID: 0x" << std::hex << GetGUID64() << std::dec
              << ", CurrentFlags: 0x" << std::hex << currentFlags
              << ", IsLooting (0x400 bit check): " << (isLooting ? "true" : "false");
        Core::Log::Message(log_s.str());
        RARE_LOG_COUNTER_ISLOOTING_FLAG = 0;
    }
    */

    return isLooting;
} 