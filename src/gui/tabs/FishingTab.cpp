#include "FishingTab.h"
#include "../../logs/log.h" // For potential logging
// #include "../../fishing/FishingBot.h" // Already included in FishingTab.h
#include "imgui.h"
#include "../../hook.h" // For GetFishingBotInstance()

namespace GUI {

FishingTab::FishingTab() {
    Core::Log::Message("[FishingTab] Initialized.");
}

void FishingTab::SetFishingBotInstance(Fishing::FishingBot* botInstance) {
    m_fishingBotInstanceRef = botInstance;
    if (m_fishingBotInstanceRef) {
        Core::Log::Message("[FishingTab] FishingBot instance linked.");
        // Initialize our input with the current spell ID
        spellIdInput = static_cast<int>(m_fishingBotInstanceRef->GetFishingSpellID());
        lastAppliedSpellId = spellIdInput;
    } else {
        Core::Log::Message("[FishingTab] Warning: Attempted to link a null FishingBot instance.");
    }
}

void FishingTab::Render() {
    ImGui::Text("Fishing Bot Controls");
    ImGui::Separator();

    // Attempt to link the FishingBot instance if not already linked
    if (!m_fishingBotInstanceRef) {
        m_fishingBotInstanceRef = GetFishingBotInstance();
        if (m_fishingBotInstanceRef) {
            Core::Log::Message("[FishingTab::Render] Successfully linked FishingBot instance.");
            // Initialize our input with the current spell ID
            spellIdInput = static_cast<int>(m_fishingBotInstanceRef->GetFishingSpellID());
            lastAppliedSpellId = spellIdInput;
        }
        // Removed the logging for failed linking as it would spam the log
    }

    if (m_fishingBotInstanceRef) {
        // Update the checkbox state to match the bot's actual running state
        botEnabled = m_fishingBotInstanceRef->IsRunning();

        if (ImGui::Checkbox("Enable Fishing Bot", &botEnabled)) {
            if (botEnabled) {
                Core::Log::Message("[FishingTab] User enabled fishing bot.");
                m_fishingBotInstanceRef->Start();
            } else {
                Core::Log::Message("[FishingTab] User disabled fishing bot.");
                m_fishingBotInstanceRef->Stop();
            }
        }

        ImGui::Spacing();
        ImGui::Text("Status: %s", m_fishingBotInstanceRef->IsRunning() ? "Active" : "Idle");

        // Add spell ID configuration
        ImGui::Separator();
        ImGui::Text("Fishing Spell Configuration");
        
        // Display the current spell ID
        ImGui::Text("Current Spell ID: %d", m_fishingBotInstanceRef->GetFishingSpellID());
        
        // Allow user to input a new spell ID
        if (ImGui::InputInt("New Spell ID", &spellIdInput, 1, 100)) {
            // Ensure the spell ID is not negative
            if (spellIdInput < 0) spellIdInput = 0;
        }
        
        // Apply button to change the spell ID
        if (ImGui::Button("Apply Spell ID")) {
            if (spellIdInput != lastAppliedSpellId) {
                Core::Log::Message("[FishingTab] User changed fishing spell ID to: " + std::to_string(spellIdInput));
                m_fishingBotInstanceRef->SetFishingSpellID(static_cast<uint32_t>(spellIdInput));
                lastAppliedSpellId = spellIdInput;
            }
        }
        
        ImGui::SameLine();
        
        // Reset button to restore default spell ID
        if (ImGui::Button("Reset to Default")) {
            spellIdInput = Fishing::DEFAULT_FISHING_SPELL_ID;
            if (lastAppliedSpellId != Fishing::DEFAULT_FISHING_SPELL_ID) {
                Core::Log::Message("[FishingTab] User reset fishing spell ID to default: " + std::to_string(Fishing::DEFAULT_FISHING_SPELL_ID));
                m_fishingBotInstanceRef->SetFishingSpellID(Fishing::DEFAULT_FISHING_SPELL_ID);
                lastAppliedSpellId = Fishing::DEFAULT_FISHING_SPELL_ID;
            }
        }
        
        // Add tooltip explaining what the fishing spell ID is
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Default is 7620 (Basic Fishing)");
            ImGui::Text("Use a different spell ID if you have higher fishing skill ranks.");
            ImGui::EndTooltip();
        }

    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Fishing Bot not linked!");
        // Keep the checkbox disabled or visually distinct if bot is not linked
        bool disabledCheckbox = false;
        ImGui::BeginDisabled();
        ImGui::Checkbox("Enable Fishing Bot", &disabledCheckbox);
        ImGui::EndDisabled();
        ImGui::Text("Status: Unlinked");
        
        // Also disable spell ID configuration when bot is not linked
        ImGui::Separator();
        ImGui::Text("Fishing Spell Configuration");
        ImGui::BeginDisabled();
        ImGui::InputInt("New Spell ID", &spellIdInput, 1, 100);
        ImGui::Button("Apply Spell ID");
        ImGui::SameLine();
        ImGui::Button("Reset to Default");
        ImGui::EndDisabled();
    }

    // ImGui::Text("Fish Caught: %d", fishCaughtCount);
    // ImGui::Spacing();
    // ImGui::Text("Settings:");
    // ImGui::SliderFloat("Min Delay (s)", &autoFishDelayMin, 0.5f, 5.0f);
    // ImGui::SliderFloat("Max Delay (s)", &autoFishDelayMax, 0.5f, 5.0f);
}

} // namespace GUI 