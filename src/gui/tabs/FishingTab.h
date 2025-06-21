#pragma once

#include "../../fishing/FishingBot.h" // Include FishingBot for the pointer type

namespace GUI {

class FishingTab {
public:
    FishingTab();
    void Render();

    // Method to link the FishingBot instance
    void SetFishingBotInstance(Fishing::FishingBot* botInstance);

    bool IsBotEnabled() const { return botEnabled; } // This might become redundant if status is driven by FishingBot::IsRunning()
    // void SetBotEnabled(bool enabled) { botEnabled = enabled; } // This local state might be removed

private:
    Fishing::FishingBot* m_fishingBotInstanceRef = nullptr; // Pointer to the actual bot
    bool botEnabled = false; // Local checkbox state - will reflect the bot's running state
    
    // Fishing spell ID configuration
    int spellIdInput = Fishing::DEFAULT_FISHING_SPELL_ID; // Default value for the input field
    int lastAppliedSpellId = Fishing::DEFAULT_FISHING_SPELL_ID; // Track the last spell ID actually applied
    
    // Future settings to implement
    // float autoFishDelayMin = 1.0f;
    // float autoFishDelayMax = 2.5f;
    // int fishCaughtCount = 0;
    // std::string statusText = "Idle";
};

} // namespace GUI 