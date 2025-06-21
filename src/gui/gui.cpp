#include "gui.h"
#include "../logs/log.h" // Ensure log header is included
#include "hook.h" // For GetFishingBotInstance()
#include "imgui.h"          // Include ImGui directly
#include "tabs/objects_tab.h" 
#include "tabs/logs_tab.h"   
#include "RotationsTab.h" 
#include "tabs/FishingTab.h"
#include "../rotations/RotationEngine.h"
#include "../game_state/GameStateManager.h"
#include <atomic> // Required for std::atomic
#include <string> // For std::to_string, std::stoull

// Extern declarations for globals from hook.cpp
extern Rotation::RotationEngine* rotationEngineInstance;
extern std::atomic<bool> g_shutdownRequested;

namespace GUI {

    // --- Global GUI State & Instances --- 
    static bool show_gui = true;
    static bool show_status_overlay = false;
    static GUI::RotationsTab* s_rotationsTab = nullptr;
    static GUI::ObjectsTab* s_objectsTab = nullptr;
    static GUI::LogsTab* s_logsTab = nullptr;
    static GUI::FishingTab* s_fishingTab = nullptr;

    // --- Function Implementations ---
    void Initialize() {
        Core::Log::Message("[GUI] Initializing GUI System...");
        ::ImGui::StyleColorsDark();

        if (rotationEngineInstance) { 
            s_rotationsTab = new RotationsTab(*rotationEngineInstance, g_shutdownRequested);
            Core::Log::Message("[GUI] RotationsTab Initialized.");
        }

        s_objectsTab = new ObjectsTab();
        Core::Log::Message("[GUI] ObjectsTab Initialized.");

        s_logsTab = new LogsTab();
        Core::Log::Message("[GUI] LogsTab Initialized.");

        s_fishingTab = new FishingTab(); 
        Core::Log::Message("[GUI] FishingTab Initialized.");

        if (s_fishingTab) {
            Fishing::FishingBot* botInstance = GetFishingBotInstance();
            if (botInstance) {
                s_fishingTab->SetFishingBotInstance(botInstance);
                Core::Log::Message("[GUI] Linked FishingBot instance to FishingTab.");
            } else {
                Core::Log::Message("[GUI] Warning: FishingBot instance is null, cannot link to FishingTab.");
            }
        }
        Core::Log::Message("[GUI] All tabs initialized.");
        Core::Log::Message("[GUI] GUI System Initialized Successfully.");
    }

    void Shutdown() {
        Core::Log::Message("[GUI] Shutting down GUI System...");
        delete s_rotationsTab; s_rotationsTab = nullptr;
        delete s_objectsTab; s_objectsTab = nullptr;
        delete s_logsTab; s_logsTab = nullptr;
        delete s_fishingTab; s_fishingTab = nullptr;
        Core::Log::Message("[GUI] All tabs destroyed.");
        Core::Log::Message("[GUI] GUI System Shutdown Complete.");
    }

    void Render() {
        if (!show_gui) {
            if (show_status_overlay) {
                RenderStatusOverlay();
            }
            return;
        }

        ::ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        ::ImGui::Begin("Main Controls", &show_gui, ImGuiWindowFlags_MenuBar);

        if (::ImGui::BeginMenuBar()) {
            if (::ImGui::BeginMenu("File")) {
                if (::ImGui::MenuItem("Toggle Overlay", "Ctrl+O")) { GUI::ToggleStatusOverlay(); } 
                if (::ImGui::MenuItem("Hide GUI", "Ctrl+H")) { GUI::SetVisibility(false); }
                if (::ImGui::MenuItem("Exit Application")) { 
                    if (g_shutdownRequested) g_shutdownRequested.store(true); 
                    show_gui = false;
                }
                ::ImGui::EndMenu();
            }
            ::ImGui::EndMenuBar();
        }

        if (::ImGui::BeginTabBar("MainTabs")) {
            if (::ImGui::BeginTabItem("Rotations")) {
                if (s_rotationsTab) {
                    s_rotationsTab->Render();
                }
                ::ImGui::EndTabItem();
            }

            if (::ImGui::BeginTabItem("Objects")) {
                if (s_objectsTab) {
                    s_objectsTab->Render();
                }
                ::ImGui::EndTabItem();
            }
            
            if (::ImGui::BeginTabItem("Logs")) {
                if (s_logsTab) {
                    s_logsTab->Render();
                }
                ::ImGui::EndTabItem();
            }

            if (::ImGui::BeginTabItem("Fishing")) {
                if (s_fishingTab) {
                    s_fishingTab->Render();
                }
                ::ImGui::EndTabItem();
            }

            if (::ImGui::BeginTabItem("Settings")) {
                ::ImGui::Text("General application settings would go here.");
                ::ImGui::Checkbox("Show Status Overlay", &show_status_overlay); 
                ::ImGui::EndTabItem();
            }

            ::ImGui::EndTabBar();
        }

        ::ImGui::End();

        if (show_status_overlay) {
            RenderStatusOverlay();
        }
    }

    void RenderStatusOverlay() {
        if (!show_status_overlay) {
            return;
        }

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        
        const float PAD = 10.0f;
        const ::ImGuiViewport* viewport = ::ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; 
        ImVec2 window_pos, window_pos_pivot;
        
        window_pos.x = work_pos.x + PAD;
        window_pos.y = work_pos.y + PAD;
        window_pos_pivot.x = 0.0f; 
        window_pos_pivot.y = 0.0f; 
        ::ImGui::SetNextWindowPos(window_pos, ImGuiCond_FirstUseEver, window_pos_pivot);
        ::ImGui::SetNextWindowBgAlpha(0.35f); 
        ::ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_FirstUseEver);

        if (::ImGui::Begin("Status Overlay", &show_status_overlay, window_flags)) {
            if (!rotationEngineInstance) {
                ::ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Rotation Engine: Not Initialized");
            } else {
                bool isRunning = rotationEngineInstance->IsRunning();
                ImVec4 color = isRunning ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                const char* statusText = isRunning ? "Enabled" : "Disabled";
                
                ::ImGui::Text("Rotation Engine: ");
                ::ImGui::SameLine();
                ::ImGui::TextColored(color, "%s", statusText);
            }

            ::ImGui::Separator();

            GameStateManager& gsm = GameStateManager::GetInstance();
            ::ImGui::Text("Game States:");
            uint32_t rawWorldLoadedDword = gsm.GetRawWorldLoadedDword();
            bool rawWorldLoadedFlag = gsm.GetRawWorldLoadedFlag(); 
            ::ImGui::Text("WorldLoaded: %s (Raw DWORD: 0x%X)", rawWorldLoadedFlag ? "TRUE" : "FALSE", rawWorldLoadedDword);
            
            uint32_t rawIsLoading = gsm.GetRawIsLoadingValue();
            ::ImGui::Text("IsLoading: %u (0x%X)", rawIsLoading, rawIsLoading);

            std::string rawGameStateStr = gsm.GetRawGameStateString();
            ::ImGui::TextWrapped("GameState: %s", rawGameStateStr.c_str());
        }
        ::ImGui::End();
    }
    
    void SetStatusOverlayEnabled(bool enabled) {
        show_status_overlay = enabled;
    }
    
    bool IsStatusOverlayEnabled() {
        return show_status_overlay;
    }

    void ToggleVisibility() {
        show_gui = !show_gui;
        std::string msg = "[GUI] Toggled main window visibility to: ";
        msg += show_gui ? "Visible" : "Hidden";
        Core::Log::Message(msg);
    }

    bool IsVisible() {
        return show_gui;
    }

    void SetVisibility(bool visible) {
        show_gui = visible;
    }

    void ToggleStatusOverlay() {
        show_status_overlay = !show_status_overlay;
        std::string msg = "[GUI] Toggled status overlay visibility to: ";
        msg += show_status_overlay ? "Visible" : "Hidden";
        Core::Log::Message(msg);
    }

    RotationsTab* GetRotationsTab() {
        return s_rotationsTab;
    }

    ObjectsTab* GetObjectsTab() {
        return s_objectsTab;
    }

    LogsTab* GetLogsTab() {
        return s_logsTab;
    }

    FishingTab* GetFishingTab() {
        return s_fishingTab;
    }

    // Implementation of free-function APIs that use the static instances
    void RenderObjectsTab() {
        if (s_objectsTab) {
            s_objectsTab->Render();
        }
    }

    void RenderLogsTab() {
        if (s_logsTab) {
            s_logsTab->Render();
        }
    }

} // namespace GUI 