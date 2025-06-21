#pragma once

#include "RotationsTab.h"
#include "tabs/FishingTab.h"
#include "tabs/objects_tab.h" // Add include for full ObjectsTab definition
#include "tabs/logs_tab.h"    // Add include for full LogsTab definition
// #include "tabs/MovementTab.h" // Will be included in .cpp

// Forward declarations
namespace Rotation { class RotationEngine; }
namespace GUI { 
    class RotationsTab;
    class MovementTab; // Forward declare MovementTab
    class FishingTab;  // Forward declare FishingTab
    // Remove forward declarations for classes we now include directly
}

// Extern declarations for globals from hook.cpp
// ... existing code ...

namespace GUI {

    void Initialize();
    void Shutdown();
    void Render();
    void RenderStatusOverlay(); 
    // void RenderMovementTab(); // Remove this line
    void SetStatusOverlayEnabled(bool enabled);
    bool IsStatusOverlayEnabled();
    void ToggleVisibility();
    bool IsVisible();
    void SetVisibility(bool visible);
    
    // Status overlay settings
    void ToggleStatusOverlay(); // <<< ADDED Forward declaration

    // Access to the Rotations Tab instance
    RotationsTab* GetRotationsTab();

    FishingTab* GetFishingTab(); // <<< ADDED: Getter for FishingTab
    ObjectsTab* GetObjectsTab(); // ADDED
    LogsTab* GetLogsTab();       // ADDED

    void RenderRotationsTab(); // Renders the content of the Rotations tab
    void RenderMovementTab();  // <<< ADDED: Renders the content of the Movement tab

    // GUI state variables
    extern RotationsTab rotationsTab; 
    // extern FishingTab fishingTab; // <<< REMOVED: Instance of FishingTab, using pointer fishingTabInstance from gui.cpp

    extern bool showDemoWindow;

    // Helper function to render the status overlay
} // namespace GUI 