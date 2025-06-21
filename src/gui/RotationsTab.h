#pragma once

#include "../rotations/RotationEngine.h"
#include <string>
#include <atomic> // For std::atomic_bool

namespace GUI {

// VK key constants for key handling
constexpr int MAX_KEYS = 256;

// Forward declarations
namespace Rotation { class RotationEngine; }

class RotationsTab {
public:
    RotationsTab(::Rotation::RotationEngine& engine, std::atomic_bool& unload_flag);

    void Render(); // Renders the ImGui content for this tab
    
    // Process a key press - returns true if handled
    bool HandleKeyPress(int vkCode);
    
    // Get the currently bound key for rotation toggle
    int GetToggleKey() const { return rotationToggleKey; }

    // Convert VK code to readable string
    std::string GetKeyName(int vkCode) const;

private:
    ::Rotation::RotationEngine& rotationEngine;
    std::atomic_bool& unloadSignal; // Reference to the unload signal flag from main UI

    // GUI state
    int selectedRotationIndex = -1; // Index in the GetAvailableRotationNames vector
    bool targetingEnabledCheckbox = true; // Mirror engine state
    
    // Name-based targeting state
    bool nameTargetingEnabledCheckbox = false; // Mirror engine state
    std::string targetNameFilter = ""; // Current name filter
    
    // Keybind state
    int rotationToggleKey = 0; // Default to 0 (no key)
    bool waitingForKeyBind = false; // Are we waiting for a new key press?
    bool onlyTargetCombatUnitsCheckbox = true; // Added for the new setting
    bool tankingModeEnabledCheckbox = false; // Added for Tanking Mode setting
    bool onlyCastOnCombatUnitsCheckbox = false; // Added for the new setting
    bool onlyCastIfPlayerInCombatCheckbox = false; // Added for player in combat check
    bool autoReEnableCheckbox = true; // NEW: For auto re-enable toggle
    bool singleTargetModeCheckbox = false; // Checkbox for the new mode
};

} 