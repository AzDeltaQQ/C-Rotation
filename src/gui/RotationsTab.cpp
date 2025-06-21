#include "RotationsTab.h"
#include "rotations/RotationEngine.h"
#include "logs/log.h"
#include <imgui.h>
#include "gui.h"
#include <vector>
#include <string>
#include <windows.h>

namespace GUI {

RotationsTab::RotationsTab(::Rotation::RotationEngine& engine, std::atomic_bool& unload_flag)
    : rotationEngine(engine), unloadSignal(unload_flag), selectedRotationIndex(-1), rotationToggleKey(0x31), waitingForKeyBind(false) {
    targetingEnabledCheckbox = rotationEngine.IsTargetingEnabled();
    nameTargetingEnabledCheckbox = rotationEngine.IsNameBasedTargetingEnabled();
    targetNameFilter = rotationEngine.GetTargetNameFilter();
    onlyTargetCombatUnitsCheckbox = rotationEngine.IsOnlyTargetingCombatUnits();
    tankingModeEnabledCheckbox = rotationEngine.IsTankingModeEnabled();
    onlyCastOnCombatUnitsCheckbox = rotationEngine.IsOnlyCastOnCombatUnitsEnabled();
    onlyCastIfPlayerInCombatCheckbox = rotationEngine.IsOnlyCastingIfPlayerInCombatEnabled();
    autoReEnableCheckbox = rotationEngine.IsAutoReEnableAfterLoadScreenEnabled();
    singleTargetModeCheckbox = rotationEngine.IsSingleTargetModeEnabled();

    // Attempt to set the dropdown to the last selected rotation
    const auto& availableNames = rotationEngine.GetAvailableRotationNames();
    std::string lastSelectedName = rotationEngine.GetCurrentRotationName();

    if (!lastSelectedName.empty() && !availableNames.empty()) {
        for (size_t i = 0; i < availableNames.size(); ++i) {
            if (availableNames[i] == lastSelectedName) {
                selectedRotationIndex = static_cast<int>(i);
                Core::Log::Message("[GUI RotTab] Found last used rotation: " + lastSelectedName + ". Pre-selecting in dropdown.");
                rotationEngine.SelectRotation(lastSelectedName, true);
                Core::Log::Message("[GUI RotTab] Engine notified to select: " + lastSelectedName);
                break;
            }
        }
    }
}

// Handle key presses for rotation toggle
bool RotationsTab::HandleKeyPress(int vkCode) {
    if (waitingForKeyBind) {
        if (vkCode != VK_ESCAPE) {
            rotationToggleKey = vkCode;
            Core::Log::Message("Rotation toggle key bound to: " + GetKeyName(vkCode));
        }
        waitingForKeyBind = false;
        return true;
    }
    
    if (vkCode == rotationToggleKey) {
        if (rotationEngine.IsRunning()) {
            rotationEngine.Stop();
            rotationEngine.UserManuallyRequestedStop();
            Core::Log::Message("Rotation stopped by keybind");
        } else if (selectedRotationIndex != -1) {
            rotationEngine.Start();
            rotationEngine.UserManuallyRequestedStart();
            Core::Log::Message("Rotation started by keybind");
        }
        return true;
    }
    
    return false;
}

// Convert VK code to a readable string
std::string RotationsTab::GetKeyName(int vkCode) const {
    if (vkCode == 0) return "None";
    
    char keyName[32] = {0};
    
    // Handle special cases
    switch (vkCode) {
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_LSHIFT: return "Left Shift";
        case VK_RSHIFT: return "Right Shift";
        case VK_LCONTROL: return "Left Ctrl";
        case VK_RCONTROL: return "Right Ctrl";
        case VK_LMENU: return "Left Alt";
        case VK_RMENU: return "Right Alt";
        case VK_TAB: return "Tab";
        case VK_CAPITAL: return "Caps Lock";
        case VK_ESCAPE: return "Escape";
        case VK_SPACE: return "Space";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_END: return "End";
        case VK_HOME: return "Home";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_NUMPAD0: return "Numpad 0";
        case VK_NUMPAD1: return "Numpad 1";
        case VK_NUMPAD2: return "Numpad 2";
        case VK_NUMPAD3: return "Numpad 3";
        case VK_NUMPAD4: return "Numpad 4";
        case VK_NUMPAD5: return "Numpad 5";
        case VK_NUMPAD6: return "Numpad 6";
        case VK_NUMPAD7: return "Numpad 7";
        case VK_NUMPAD8: return "Numpad 8";
        case VK_NUMPAD9: return "Numpad 9";
    }
    
    // Get key name from Windows
    UINT scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
    GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName));
    
    // If we got a name, return it
    if (keyName[0] != '\0') return keyName;
    
    // Fallback: return the key code
    return "Key " + std::to_string(vkCode);
}

void RotationsTab::Render() {
    // Use full width for this pane now
    ImGui::BeginChild("RotationTopPane", ImVec2(0, 0), true); 

    ImGui::Text("Rotation Selection:");
    const auto& rotationNames = rotationEngine.GetAvailableRotationNames();
    std::vector<const char*> rotationNameCstrs;
    for (const std::string& name : rotationNames) {
        rotationNameCstrs.push_back(name.c_str());
    }
    if (ImGui::Combo("Select Rotation", &selectedRotationIndex, rotationNameCstrs.data(), static_cast<int>(rotationNameCstrs.size()))) {
        if (selectedRotationIndex >= 0 && selectedRotationIndex < static_cast<int>(rotationNames.size())) {
            Core::Log::Message("GUI: Selected rotation");
            rotationEngine.SelectRotation(rotationNames[selectedRotationIndex]);
        }
    }
    if (rotationEngine.IsRunning()) {
        if (ImGui::Button("Stop Rotation")) { 
            rotationEngine.Stop(); 
            rotationEngine.UserManuallyRequestedStop();
        }
    } else {
        if (ImGui::Button("Start Rotation")) {
            if (selectedRotationIndex != -1) { 
                rotationEngine.Start(); 
                rotationEngine.UserManuallyRequestedStart();
            }
            else { Core::Log::Message("GUI: Cannot start, no rotation selected."); }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload All Rotations")) {
        rotationEngine.ReloadRotationsFromDisk();
    }
    ImGui::SameLine();
    ImGui::Text(rotationEngine.IsRunning() ? "Status: Running" : "Status: Stopped");

    ImGui::Separator();
    ImGui::Text("Keybind:");
    ImGui::Text("Toggle Rotation: %s", GetKeyName(rotationToggleKey).c_str());
    if (waitingForKeyBind) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Press any key (ESC to cancel)")) { waitingForKeyBind = false; }
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button("Set Keybind")) { waitingForKeyBind = true; }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Keybind")) { rotationToggleKey = 0; }

    ImGui::Separator();
    ImGui::Text("Settings:");
    if (ImGui::Checkbox("Enable Auto-Targeting", &targetingEnabledCheckbox)) {
        rotationEngine.SetTargetingEnabled(targetingEnabledCheckbox);
    }
    static bool statusOverlayEnabled_local = false; 
    statusOverlayEnabled_local = ::GUI::IsStatusOverlayEnabled();
    if (ImGui::Checkbox("Show Status Overlay", &statusOverlayEnabled_local)) {
        ::GUI::SetStatusOverlayEnabled(statusOverlayEnabled_local);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Shows rotation status in a small overlay in the corner of the screen");
        ImGui::EndTooltip();
    }
    ImGui::Separator();
    ImGui::Text("Target Filtering:");
    if (ImGui::Checkbox("Filter targets by name", &nameTargetingEnabledCheckbox)) {
        rotationEngine.SetNameBasedTargetingEnabled(nameTargetingEnabledCheckbox);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Only target units whose names contain the specified text");
        ImGui::EndTooltip();
    }
    static char nameFilterBuffer[128] = "";
    if (nameFilterBuffer[0] == '\0' && !targetNameFilter.empty()) {
        strncpy_s(nameFilterBuffer, targetNameFilter.c_str(), sizeof(nameFilterBuffer) - 1);
    }
    if (nameTargetingEnabledCheckbox) {
        if (ImGui::InputText("Target name filter", nameFilterBuffer, sizeof(nameFilterBuffer))) {
            targetNameFilter = nameFilterBuffer;
            rotationEngine.SetTargetNameFilter(targetNameFilter);
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::InputText("Target name filter", nameFilterBuffer, sizeof(nameFilterBuffer));
        ImGui::EndDisabled();
    }

    // New Checkbox for targeting combat units only
    if (ImGui::Checkbox("Only Target Enemies in Combat", &onlyTargetCombatUnitsCheckbox)) {
        rotationEngine.SetOnlyTargetCombatUnits(onlyTargetCombatUnitsCheckbox);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("If checked, FindBestEnemyTarget will prefer enemies already in combat.\nThis primarily affects auto-targeting logic.");
        ImGui::EndTooltip();
    }

    // New Checkbox for only casting on units in combat
    if (ImGui::Checkbox("Only Cast Spells on Units in Combat", &onlyCastOnCombatUnitsCheckbox)) {
        rotationEngine.SetOnlyCastOnCombatUnits(onlyCastOnCombatUnitsCheckbox);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Only cast on targets that are currently in combat.");
    }

    // New Checkbox for only casting if player is in combat
    if (ImGui::Checkbox("Only Cast Spells if Player is in Combat", &onlyCastIfPlayerInCombatCheckbox)) {
        rotationEngine.SetOnlyCastIfPlayerInCombat(onlyCastIfPlayerInCombatCheckbox);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rotation will only attempt to cast spells if the player is in combat.");
    }

    // NEW: Checkbox for Auto Re-enable
    if (ImGui::Checkbox("Auto Re-enable After Load Screen", &autoReEnableCheckbox)) {
        rotationEngine.SetAutoReEnableAfterLoadScreen(autoReEnableCheckbox);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("If checked, and you manually started the rotation, it will attempt to resume after a loading screen.");
        ImGui::EndTooltip();
    }

    // Tanking Mode Toggle
    if (ImGui::Checkbox("Enable Tanking Mode", &tankingModeEnabledCheckbox)) {
        rotationEngine.SetTankingModeEnabled(tankingModeEnabledCheckbox);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Prioritizes generating threat on nearby enemies that are in combat.");
        ImGui::EndTooltip();
    }

    // NEW: Checkbox for Single Target Mode
    if (ImGui::Checkbox("Single Target Mode", &singleTargetModeCheckbox)) {
        rotationEngine.SetSingleTargetModeEnabled(singleTargetModeCheckbox);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("If checked, the rotation will prioritize your game's currently selected target.\\nIf unchecked (Dynamic Mode), it considers your current target first, then others.");
        ImGui::EndTooltip();
    }

    ImGui::EndChild(); // End of RotationTopPane

    // These buttons were below the debug pane, keep them outside the child?
    // if (!rotationEngine.IsRunning()) {
    //     if (ImGui::Button("Start Rotation Engine")) {
    //         rotationEngine.Start();
    //     }
    // } else {
    //     if (ImGui::Button("Stop Rotation Engine")) {
    //         rotationEngine.Stop();
    //     }
    // }
    // ImGui::SameLine();
    // if (ImGui::Button("Reload All Rotations")) {
    //     rotationEngine.ReloadRotationsFromDisk();
    // }
}

} // namespace GUI 