#include <Windows.h>
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib") // Link against d3d9

#include "hook.h"
#include "logs/log.h" // Keep our logging
#include "gui/gui.h"
#include "objectManager/ObjectManager.h"
#include "spells/cooldowns.h"
#include "spells/castspell.h"
#include "spells/targeting.h" // Added for Spells::IntersectFlagsToString
#include "rotations/RotationEngine.h"
#include "gui/RotationsTab.h"
#include "fishing/FishingBot.h"    // For FishingBot
#include "game_state/GameStateManager.h" // ++ ADDED INCLUDE ++

#include <MinHook.h> // Ensure this uses the correct path configured in CMakeLists.txt

// Define IMGUI_DISABLE_OBSOLETE_FUNCTIONS before including ImGui
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
// Add a define to disable assertions
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS

// Include ImGui after our defines
#include <imgui.h>
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stdio.h> // For sprintf_s
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip> // For std::fixed and std::setprecision
#include <cmath>   // For std::fabs
#include <limits>  // Required for std::numeric_limits

// Global Instances
ObjectManager* objectManagerInstance = nullptr;
Spells::CooldownManager* cooldownManagerInstance = nullptr;
Rotation::RotationEngine* rotationEngineInstance = nullptr;
GUI::RotationsTab* g_rotationsTab = nullptr;
std::atomic<bool> g_shutdownRequested{false}; // Ensure this is declared for RotationsTab
std::atomic<bool> g_isShuttingDown{false};
HWND gameHwnd = NULL; // Should be initialized in HookedEndScene primarily

// Fishing Bot Instance
Fishing::FishingBot* fishingBotInstance = nullptr;

// Game State Check
namespace Offsets {
    // Offset possibly related to world rendering being active
    constexpr uintptr_t WORLD_LOADED_FLAG = 0xBEBA40; 
}

// Remove logging counter
// static uint64_t isInGameCallCounter = 0;

bool IsInGame() {
    return GameStateManager::GetInstance().IsFullyInWorld();
}

// --- Globals (from previous version) ---
// Global flag to indicate if injection happened while already in-game
static bool g_lateInjectionDetected = false;

// Add state for delayed Object Manager activation
static bool g_isObjectManagerActive = false;
static std::chrono::steady_clock::time_point g_timeEnteredGameWorld;
constexpr std::chrono::seconds OBJECT_MANAGER_ACTIVATION_DELAY = std::chrono::seconds(2); // 2 second delay

// ImGui rendering guard
static bool g_imguiInFrame = false;
static DWORD g_lastFrameTime = 0;

// EndScene Execution Queue
namespace {
    std::vector<std::function<void()>> endSceneQueue;
    std::mutex endSceneMutex;
}

void SubmitToEndScene(std::function<void()> func) {
    if (g_isShuttingDown) {
        // Core::Log::Message("[SubmitToEndScene] Shutdown in progress. Task submission rejected."); // Optional: Log rejection
        return; // Do not queue tasks if shutting down
    }

    // Use a scoped lock to check queue size and avoid submissions during cleanup
    {
        std::lock_guard<std::mutex> lock(endSceneMutex);
        
        // Check shutdown state again after acquiring lock
        if (g_isShuttingDown) {
            return; // Double-check shutdown flag with the lock held
        }
        
        // Implement queue size limit to prevent excessive memory usage
        constexpr size_t MAX_QUEUE_SIZE = 1000;
        if (endSceneQueue.size() >= MAX_QUEUE_SIZE) {
            Core::Log::Message("[SubmitToEndScene] WARNING: Queue size limit reached (" + 
                              std::to_string(endSceneQueue.size()) + " items). Task dropped.");
            return; // Drop this task rather than growing the queue unbounded
        }
        
        // Add direct debug log to see if this function is being called - REDUCE THIS LATER OR MAKE CONDITIONAL
        static int submitCounter = 0;
        if (++submitCounter % 100 == 0) { // Log every 100th submission to reduce spam
            Core::Log::Message("[SubmitToEndScene] Task submitted to end scene queue (Count: " + std::to_string(submitCounter) + ")");
        }
        
        // Add the task to the queue
        endSceneQueue.push_back(std::move(func));
    } // Lock released here
}

// Original function pointers
typedef HRESULT(APIENTRY* EndScene)(LPDIRECT3DDEVICE9 pDevice);
EndScene oEndScene = nullptr;

typedef HRESULT(APIENTRY* Reset)(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
Reset oReset = nullptr;

bool imguiInitialized = false; // Renamed from 'initialized' to be specific

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WNDPROC oWndProc = NULL;

// Typedef for the function we are hooking (matches Spells::World_Intersect_t)
// We need to bring in Vector3 and IntersectFlags definitions or forward declare if used directly in signature.
// For simplicity here, assuming Spells::IntersectFlags and Vector3 are accessible
// If not, this typedef might need to be adjusted or placed where those types are known.
typedef int (*Original_ProcessWorldFrameTrace_t)(void* worldframe, Vector3* start, Vector3* end, float* hitFraction, Spells::IntersectFlags flags, int param_a6, int param_a7);
Original_ProcessWorldFrameTrace_t oProcessWorldFrameTrace = nullptr; // Trampoline for our LOS hook

// --- WndProc (Based on user example, simplified) ---
LRESULT APIENTRY WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        int vkCode = static_cast<int>(wParam);
        
        GUI::RotationsTab* currentRotationsTab = GUI::GetRotationsTab();
        if (currentRotationsTab) {
            bool handled = currentRotationsTab->HandleKeyPress(vkCode);
            if (handled) {
                return TRUE;
            }
        }
    }
    
    if (imguiInitialized && GUI::IsVisible()) { 
        if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
            return true; 

        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            if ((uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) && io.WantCaptureKeyboard) return TRUE;
            if ((uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) && io.WantCaptureMouse) return TRUE;
        }
    }
    return CallWindowProc(oWndProc, hwnd, uMsg, wParam, lParam);
}
// --- End WndProc ---

// --- HookedEndScene (No Force Reset Implementation) ---
HRESULT APIENTRY HookedEndScene(LPDIRECT3DDEVICE9 pDevice) {
    if (!imguiInitialized) {
        static int initCounter = 0;
        if (++initCounter < 100) { // Wait for 100 frames (arbitrary delay)
            return oEndScene(pDevice);
        }
        
        Core::Log::Message("[HookedEndScene] Starting ImGui initialization...");
        
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename = NULL; // Disable imgui.ini saving/loading by default
        
        D3DDEVICE_CREATION_PARAMETERS params;
        if (FAILED(pDevice->GetCreationParameters(&params))) {
            Core::Log::Message("[HookedEndScene] Failed to get device creation parameters.");
            ImGui::DestroyContext(); // Clean up context if init fails
            return oEndScene(pDevice);
        }
        gameHwnd = params.hFocusWindow; // Get gameHwnd from D3DDevice

        if (!gameHwnd) { // Added check for null gameHwnd
            Core::Log::Message("[HookedEndScene] Failed to get valid game HWND from device parameters.");
            ImGui::DestroyContext();
            return oEndScene(pDevice);
        }
        
        if (!ImGui_ImplWin32_Init(gameHwnd)) {
            Core::Log::Message("[HookedEndScene] Failed to initialize ImGui Win32 backend.");
            ImGui::DestroyContext();
            return oEndScene(pDevice);
        }
        
        if (!ImGui_ImplDX9_Init(pDevice)) {
            Core::Log::Message("[HookedEndScene] Failed to initialize ImGui DX9 backend.");
            ImGui_ImplWin32_Shutdown(); // Clean up win32 backend
            ImGui::DestroyContext();
            return oEndScene(pDevice);
        }
        
        // Explicitly cast WndProc for SetWindowLongPtr
        oWndProc = (WNDPROC)SetWindowLongPtr(gameHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProc));
        if (!oWndProc) {
            Core::Log::Message("[HookedEndScene] Failed to set window hook (SetWindowLongPtr failed).");
            // Non-fatal, but input might not work as expected.
        }
        
        GUI::Initialize(); // Initialize your GUI system AFTER ImGui backends are ready
        
        imguiInitialized = true;
        Core::Log::Message("[HookedEndScene] ImGui and GUI successfully initialized.");
    }
    
    // --- Update GameStateManager first --- 
    GameStateManager& gsm = GameStateManager::GetInstance();
    gsm.Update(); // Ensure GSM is updated at the start of the frame logic

    // --- Log current game state immediately after update ---
    static int frameCounterForLog = 0;
    if (++frameCounterForLog % 150 == 0) { // Log every ~2.5 seconds if 60 FPS
        std::stringstream gsm_debug_ss;
        gsm_debug_ss << "[HookedEndScene_GSM_State] Frame: " << frameCounterForLog
                     << " WorldLoadedRaw: 0x" << std::hex << gsm.GetRawWorldLoadedDword()
                     << " IsLoadingRaw: 0x" << std::hex << gsm.GetRawIsLoadingValue() << std::dec
                     << " GameStateStr: '" << gsm.GetRawGameStateString() << "'"
                     << " IsFullyInWorld: " << (gsm.IsFullyInWorld() ? "T" : "F")
                     << " IsAtCharSelect: " << (gsm.IsAtCharSelectScreen() ? "T" : "F")
                     << " IsLoadingScreen: " << (gsm.IsLoadingScreen() ? "T" : "F");
        // Core::Log::Message(gsm_debug_ss.str()); // Commented out this log
        frameCounterForLog = 0; // Reset counter
    }
    // --- End GameState Logging ---

    // --- Handle toggle key for GUI visibility ---
    SHORT insertState = GetAsyncKeyState(VK_INSERT);
    static bool insertHeld = false;
    bool insertPressed = (insertState & 0x8000) != 0;
    
    if (insertPressed && !insertHeld) {
        GUI::ToggleVisibility();
        if (imguiInitialized) {
            ImGui::GetIO().MouseDrawCursor = GUI::IsVisible();
        }
    }
    insertHeld = insertPressed;
    
    // --- Core Systems Update Logic ---
    ObjectManager* objMgr = ObjectManager::GetInstance();
    if (objMgr) {
        bool isOmActuallyInitialized = objMgr->IsInitialized(); // Check actual OM init status

        // Determine current game state using GameStateManager
        bool fullyInWorld = gsm.IsFullyInWorld();
        bool atCharSelect = gsm.IsAtCharSelectScreen();
        bool atLogin = gsm.IsAtLoginScreen();
        bool isLoading = gsm.IsLoadingScreen();

        static bool wasFullyInWorld = false;
        bool justEnteredWorld = fullyInWorld && !wasFullyInWorld;
        bool justLeftWorld = !fullyInWorld && wasFullyInWorld;

        if (!isOmActuallyInitialized) {
            objMgr->TryFinishInitialization(); // Attempt to initialize if not already
            isOmActuallyInitialized = objMgr->IsInitialized(); // Re-check after attempt
            if (!isOmActuallyInitialized) { 
                 g_isObjectManagerActive = false; // Ensure OM is not active if not initialized
            }
        }

        if (justEnteredWorld && isOmActuallyInitialized) {
            Core::Log::Message("[HookedEndScene_Transition] JustEnteredWorld! State: FullyInWorld=" + std::string(fullyInWorld ? "T" : "F") + ", IsLoading=" + std::to_string(gsm.GetRawIsLoadingValue()) + ", GameStateStr='" + gsm.GetRawGameStateString() + "'. Resetting OM state and activation timer.");
            objMgr->ResetState();
            g_timeEnteredGameWorld = std::chrono::steady_clock::time_point{}; // Reset activation timer
            g_isObjectManagerActive = false; // Manager starts inactive for the delay
        }

        if (justLeftWorld) {
            Core::Log::Message("[HookedEndScene_Transition] JustLeftWorld! State: FullyInWorld=" + std::string(fullyInWorld ? "T" : "F") + ", IsLoading=" + std::to_string(gsm.GetRawIsLoadingValue()) + ", GameStateStr='" + gsm.GetRawGameStateString() + "'. Deactivating OM.");
            g_isObjectManagerActive = false;
            g_timeEnteredGameWorld = std::chrono::steady_clock::time_point{}; // Reset timer
            if (rotationEngineInstance) {
                Core::Log::Message("[HookedEndScene_Transition] Stopping RotationEngine due to JustLeftWorld.");
                rotationEngineInstance->Stop(); // Stop rotation engine too
            }
            // objMgr->ResetState(); // Optionally reset OM state here too
        }

        // Update wasFullyInWorld state *after* checking transitions
        wasFullyInWorld = fullyInWorld;

        // Manage ObjectManager activation delay only if OM is initialized and we are in the world
        if (isOmActuallyInitialized && fullyInWorld) {
            if (!g_isObjectManagerActive) {
                if (g_timeEnteredGameWorld == std::chrono::steady_clock::time_point{}) {
                    g_timeEnteredGameWorld = std::chrono::steady_clock::now();
                    Core::Log::Message("[HookedEndScene_OM_Activation] In-world, OM initialized. Starting activation timer. State: FullyInWorld=" + std::string(fullyInWorld ? "T" : "F") + ", IsLoading=" + std::to_string(gsm.GetRawIsLoadingValue()) + ", GameStateStr='" + gsm.GetRawGameStateString() + "'");
                }
                bool delayPassed = (std::chrono::steady_clock::now() - g_timeEnteredGameWorld >= OBJECT_MANAGER_ACTIVATION_DELAY);
                if (delayPassed) {
                    Core::Log::Message("[HookedEndScene_OM_Activation] Activation delay passed. Activating Object Manager. State: FullyInWorld=" + std::string(fullyInWorld ? "T" : "F") + ", IsLoading=" + std::to_string(gsm.GetRawIsLoadingValue()) + ", GameStateStr='" + gsm.GetRawGameStateString() + "'");
                    g_isObjectManagerActive = true;
                    // NEW: Conditional RotationEngine Start
                    if (rotationEngineInstance && 
                        rotationEngineInstance->HasUserManuallyRequestedActive() && 
                        rotationEngineInstance->IsAutoReEnableAfterLoadScreenEnabled()) {
                        Core::Log::Message("[HookedEndScene_OM_Activation] Auto-re-enabling RotationEngine as user had it active and toggle is ON.");
                        rotationEngineInstance->Start();
                    } else if (rotationEngineInstance) {
                        if (!rotationEngineInstance->HasUserManuallyRequestedActive()) {
                            Core::Log::Message("[HookedEndScene_OM_Activation] RotationEngine not started: User has not manually started it yet.");
                        }
                        if (!rotationEngineInstance->IsAutoReEnableAfterLoadScreenEnabled()) {
                            Core::Log::Message("[HookedEndScene_OM_Activation] RotationEngine not started: Auto Re-enable toggle is OFF.");
                        }
                    }
                }
            }
        } else {
            // If not in world or OM not initialized, ensure it's not active
            if (g_isObjectManagerActive) {
                 Core::Log::Message("[HookedEndScene_OM_Deactivation] Not IsFullyInWorld or OM not initialized. Deactivating Object Manager. State: FullyInWorld=" + std::string(fullyInWorld ? "T" : "F") + ", IsOmActuallyInitialized=" + std::string(isOmActuallyInitialized ? "T" : "F") + ", IsLoading=" + std::to_string(gsm.GetRawIsLoadingValue()) + ", GameStateStr='" + gsm.GetRawGameStateString() + "'");
                 g_isObjectManagerActive = false;
                 if (rotationEngineInstance && rotationEngineInstance->IsActive()) { // Check if active before deciding to stop/pause
                    // If user wants it active and auto-re-enable is on, don't stop it.
                    // The RunRotationLoop will self-pause.
                    if (rotationEngineInstance->HasUserManuallyRequestedActive() && 
                        rotationEngineInstance->IsAutoReEnableAfterLoadScreenEnabled()) {
                        Core::Log::Message("[HookedEndScene_RE_Pause] RotationEngine remains active but will self-pause due to game state (Not FullyInWorld). Auto-re-enable is ON.");
                        // No Stop() call here, RunRotationLoop handles pausing
                    } else {
                        Core::Log::Message("[HookedEndScene_RE_Stop] Stopping RotationEngine due to game state (Not FullyInWorld). Auto-re-enable is OFF or not manually started.");
                        rotationEngineInstance->Stop(); // Stop rotation engine if not auto-re-enabling or not manually started
                    }
                 } else if (rotationEngineInstance) {
                     // It's not active, so no need to stop it, but maybe log if it *was* manually requested and auto-re-enable is off
                     if (rotationEngineInstance->HasUserManuallyRequestedActive() && !rotationEngineInstance->IsAutoReEnableAfterLoadScreenEnabled()){
                        Core::Log::Message("[HookedEndScene_RE_Info] RotationEngine was not active, and auto-re-enable is OFF. Will not auto-start.");
                     }
                 }
            }
            // Reset timer if we are not in a state that should lead to activation
            if (!fullyInWorld) { 
                g_timeEnteredGameWorld = std::chrono::steady_clock::time_point{};
            }
        }

        // --- Perform updates if ObjectManager is initialized and active ---
        if (isOmActuallyInitialized && g_isObjectManagerActive) {
            objMgr->Update();
            objMgr->RefreshLocalPlayerCache();

            if (fishingBotInstance) { /* fishing bot update if any */ }
        } else {
            // If OM is not active, ensure critical systems that depend on it are also paused/reset if necessary.
            // For example, RotationEngine might need a specific call to pause or reset its current action.
            // Already handled stopping rotationEngineInstance when g_isObjectManagerActive becomes false.
        }

    } else { // objMgr is null
        Core::Log::Message("[HookedEndScene] ObjectManager instance is NULL!");
        g_isObjectManagerActive = false; // Cannot be active if OM doesn't exist
    }
    
    // Process queued tasks
    {
        std::vector<std::function<void()>> tasksToRun;
        {
            std::lock_guard<std::mutex> lock(endSceneMutex);
            if (!endSceneQueue.empty()) {
                // Add direct debug log when we have tasks in the queue - REDUCE THIS LATER OR MAKE CONDITIONAL
                static int processQueueCounter = 0;
                if(++processQueueCounter % 50 == 0) { // Log every 50th time queue is processed
                     Core::Log::Message("[HookedEndScene] Processing " + std::to_string(endSceneQueue.size()) + 
                                       " queued tasks (Process Count: " + std::to_string(processQueueCounter) + ")");
                }
                
                // NEW: Limit how many tasks we process in a single frame
                // This prevents the game from freezing if the queue grows large
                constexpr size_t MAX_TASKS_PER_FRAME = 50;
                
                if (endSceneQueue.size() <= MAX_TASKS_PER_FRAME) {
                    // Process all tasks if below threshold
                    tasksToRun.swap(endSceneQueue);
                } else {
                    // Process only a portion to avoid frame stutter
                    Core::Log::Message("[HookedEndScene] WARNING: Large queue detected (" + 
                                      std::to_string(endSceneQueue.size()) + 
                                      " items). Processing only " + std::to_string(MAX_TASKS_PER_FRAME) + " tasks this frame.");
                    
                    // Move the first MAX_TASKS_PER_FRAME tasks to our processing vector
                    tasksToRun.reserve(MAX_TASKS_PER_FRAME);
                    auto start = endSceneQueue.begin();
                    auto end = start + MAX_TASKS_PER_FRAME; // Limited to MAX_TASKS_PER_FRAME
                    tasksToRun.insert(tasksToRun.end(), 
                                     std::make_move_iterator(start), 
                                     std::make_move_iterator(end));
                    
                    // Remove the processed tasks from the queue
                    endSceneQueue.erase(start, end);
                }
            }
        }
        for (const auto& task : tasksToRun) {
            if (g_isShuttingDown.load()) { // Re-check shutdown flag before executing task
                // Core::Log::Message("[HookedEndScene] Shutdown in progress. Skipping queued task."); // Optional: Log skip
                continue; // Skip task execution if shutting down
            }
            if (task) {
                // Add direct debug log before executing each task - REDUCE THIS LATER OR MAKE CONDITIONAL
                // Core::Log::Message("[HookedEndScene] Executing queued task"); // Can be too verbose
                task();
            }
        }
    }
    
    // --- Spell Casting from RotationEngine Queue ---
    if (rotationEngineInstance && rotationEngineInstance->HasQueuedSpell()) {
        uint32_t spellIdToCast = rotationEngineInstance->GetQueuedSpellId();
        uint64_t targetGuidForCast = rotationEngineInstance->GetQueuedSpellTargetGuid();
        std::string spellNameToCast = rotationEngineInstance->GetQueuedSpellName();
        bool requiresTargetForCast = rotationEngineInstance->GetQueuedSpellRequiresTarget();
        bool isHealSpell = rotationEngineInstance->GetQueuedSpellIsHeal(); // Get this too for logging/logic

        char castLogBuffer[512];
        snprintf(castLogBuffer, sizeof(castLogBuffer),
                 "[HookedEndScene] Attempting to cast from RotationEngine queue: %s (ID: %u) on TargetGUID: 0x%llx. RequiresTarget: %s, IsHeal: %s",
                 spellNameToCast.c_str(), spellIdToCast, targetGuidForCast, 
                 requiresTargetForCast ? "true" : "false", isHealSpell ? "true" : "false");
        Core::Log::Message(castLogBuffer);

        // IMPORTANT: This assumes Spells::CastSpell is correctly linked and available.
        Spells::CastSpell(spellIdToCast, targetGuidForCast, requiresTargetForCast); // Calling Spells::CastSpell
                                                                                 // It doesn't directly return bool, but logs internally.

        Core::Log::Message("[HookedEndScene] Called Spells::CastSpell for: " + spellNameToCast + " (intended)"); // Adjusted log slightly
        
        // NEW: Record the cast with our CooldownManager to handle GCD and internal tracking
        if (cooldownManagerInstance) {
            cooldownManagerInstance->RecordSpellCast(spellIdToCast);
            // Optional: More verbose logging if needed for debugging this specific call
            // char recordLogBuffer[256];
            // snprintf(recordLogBuffer, sizeof(recordLogBuffer),
            //          "[HookedEndScene] Recorded spell cast with CooldownManager: %s (ID: %u)",
            //          spellNameToCast.c_str(), spellIdToCast);
            // Core::Log::Message(recordLogBuffer);
        } else {
            Core::Log::Message("[HookedEndScene] WARNING: cooldownManagerInstance is null. Cannot record spell cast for " + spellNameToCast);
        }
        
        // Consume the spell from the queue regardless of cast success to prevent re-casting the same queued item.
        rotationEngineInstance->ConsumeQueuedSpell();
        Core::Log::Message("[HookedEndScene] Consumed spell: " + spellNameToCast + " from RotationEngine queue.");
    }
    // --- End Spell Casting from RotationEngine Queue ---

    // Simplified ImGui rendering call
    if (imguiInitialized) {
        if (!g_imguiInFrame) {
            g_imguiInFrame = true;
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            ImGui::GetIO().MouseDrawCursor = false;
            GUI::Render(); // GUI::Render handles overlay and main window visibility
            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_imguiInFrame = false;
        }
    }
    
    return oEndScene(pDevice);
}
// --- End HookedEndScene ---

// --- HookedReset (Simple invalidate/recreate) ---
HRESULT APIENTRY HookedReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    // Check if ImGui is initialized before doing anything
    if (imguiInitialized) {
        // Just invalidate device objects, don't destroy the context
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
    
    // Call original
    HRESULT result = oReset(pDevice, pPresentationParameters);
    
    // Recreate device objects if successful
    if (SUCCEEDED(result) && imguiInitialized) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    
    // Always make sure we're not in a frame
    g_imguiInFrame = false;
    
    return result;
}

// --- InitializeHook (Using new VTable method + our init) ---
void InitializeHook(HMODULE hModule) {
    // char buffer[256]; // Ensure this is declared if using sprintf_s below, like in backup
    Core::Log::Message("[InitializeHook] Starting hook initialization...");
    g_shutdownRequested.store(false);
    g_isShuttingDown.store(false);

    // DLL Path and Rotations Directory (this part is good)
    std::vector<char> dllPathBuf(MAX_PATH + 1, 0); 
    GetModuleFileNameA(hModule, dllPathBuf.data(), MAX_PATH);
    std::filesystem::path dllPath(dllPathBuf.data());
    std::filesystem::path baseDir = dllPath.parent_path();
    std::filesystem::path rotationsDir = baseDir / "rotations";
    Core::Log::Message("[InitializeHook] DLL Path: " + dllPath.string());
    Core::Log::Message("[InitializeHook] Rotations Directory determined as: " + rotationsDir.string());

    // ObjectManager, CooldownManager, RotationEngine initialization (as per recent correct version)
    objectManagerInstance = ObjectManager::GetInstance();
    if (!objectManagerInstance) {
        Core::Log::Message("[InitializeHook] FATAL - Failed to get ObjectManager instance! Cannot proceed.");
        return;
    }
    Core::Log::Message("[InitializeHook] ObjectManager instance obtained.");
    
    // Assuming InitializeFunctions and TryFinishInitialization are correct methods for ObjectManager
    if (!objectManagerInstance->InitializeFunctions(
        GameOffsets::ENUM_VISIBLE_OBJECTS_ADDR,
        GameOffsets::GET_OBJECT_BY_GUID_INNER_ADDR,
        GameOffsets::GET_LOCAL_PLAYER_GUID_ADDR)) {
        Core::Log::Message("[InitializeHook] WARNING - Failed to initialize ObjectManager function pointers!");
    } else {
        Core::Log::Message("[InitializeHook] ObjectManager function pointers initialized successfully.");
    }
    if (objectManagerInstance->TryFinishInitialization()) {
        Core::Log::Message("[InitializeHook] ObjectManager immediate initialization SUCCESSFUL.");
        g_lateInjectionDetected = (objectManagerInstance->GetLocalPlayer() != nullptr);
        Core::Log::Message(g_lateInjectionDetected ? "[InitializeHook] Player pointer valid, likely in-game. Flagging late injection." : "[InitializeHook] Player pointer null after init. Assuming pre-login/char screen.");
    } else {
        Core::Log::Message("[InitializeHook] ObjectManager immediate initialization FAILED. Will retry in EndScene.");
        g_lateInjectionDetected = false; 
    }

    if (!cooldownManagerInstance) {
        cooldownManagerInstance = new Spells::CooldownManager();
        Core::Log::Message("[InitializeHook] CooldownManager initialized.");
    }

    // Initialize FishingBot
    if (!fishingBotInstance && objectManagerInstance && cooldownManagerInstance) {
        fishingBotInstance = new Fishing::FishingBot(*objectManagerInstance, *cooldownManagerInstance);
        Core::Log::Message("[InitializeHook] FishingBot initialized successfully.");
    } else if (!fishingBotInstance) {
        Core::Log::Message("[InitializeHook] ERROR: Cannot initialize FishingBot due to missing dependencies.");
    }

    if (!rotationEngineInstance) {
        if (objectManagerInstance && cooldownManagerInstance) {
            rotationEngineInstance = new Rotation::RotationEngine(*objectManagerInstance, *cooldownManagerInstance, hModule);
            Core::Log::Message("[InitializeHook] RotationEngine base initialized.");
            rotationEngineInstance->LoadRotations(rotationsDir.string()); 
            if (std::filesystem::exists(rotationsDir)) {
                Core::Log::Message("[InitializeHook] Rotations directory exists. LoadRotations called.");
            } else {
                Core::Log::Message("[InitializeHook] WARNING: Rotations directory does not exist: " + rotationsDir.string() + ". LoadRotations called to set path.");
            }
        } else {
            Core::Log::Message("[InitializeHook] ERROR: Cannot initialize RotationEngine due to missing dependencies.");
        }
    }

    if (!g_rotationsTab && rotationEngineInstance) {
        g_rotationsTab = new GUI::RotationsTab(*rotationEngineInstance, g_shutdownRequested); 
        Core::Log::Message("[InitializeHook] RotationsTab initialized.");
    }

    Core::Log::Message("[InitializeHook] GUI initialization will occur in HookedEndScene.");

    // MinHook and D3D Hooking (should largely follow your backup logic)
    // Ensure `char buffer[256];` is present if `sprintf_s` is used, or convert to Core::Log.
    if (MH_Initialize() != MH_OK) {
        Core::Log::Message("[InitializeHook] MH_Initialize failed!"); // Keep Core::Log
        OutputDebugStringA("InitializeHook: MH_Initialize failed! (ODS)\n"); // Add ODS for MinHook errors
        return;
    }
    Core::Log::Message("[InitializeHook] MinHook Initialized.");

    // --- D3D9 VTable & Hook Creation (Simplified, ensure your backup's detail if issues persist) ---
    LPDIRECT3D9 pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    // ... (error check pD3D) ...
    HWND tempHwnd = FindWindowA("GxWindowClass", NULL); if (!tempHwnd) tempHwnd = GetDesktopWindow();
    D3DPRESENT_PARAMETERS d3dpp = {0}; /* ... setup d3dpp ... */
    d3dpp.Windowed = TRUE; d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD; d3dpp.hDeviceWindow = tempHwnd;
    LPDIRECT3DDEVICE9 pDevice = nullptr;
    HRESULT result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, tempHwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &d3dpp, &pDevice);
    // ... (error check result/pDevice) ...
    void** vTable = *reinterpret_cast<void***>(pDevice);
    // ... (error check vTable) ...

    void* targetEndScene = vTable[42]; 
    void* targetReset = vTable[16]; 
    char mhBuffer[256]; // Buffer for MinHook error messages

    // Explicitly cast function pointers for MH_CreateHook
    MH_STATUS mhStatus = MH_CreateHook(
        reinterpret_cast<LPVOID>(targetEndScene), 
        reinterpret_cast<LPVOID>(&HookedEndScene), 
        reinterpret_cast<LPVOID*>(&oEndScene)
    );
    if (mhStatus != MH_OK) {
        sprintf_s(mhBuffer, sizeof(mhBuffer), "InitializeHook: MH_CreateHook for EndScene failed! Status: %s", MH_StatusToString(mhStatus));
        OutputDebugStringA(mhBuffer); Core::Log::Message(mhBuffer);
        pDevice->Release(); pD3D->Release(); MH_Uninitialize(); return;
    }
    mhStatus = MH_CreateHook(
        reinterpret_cast<LPVOID>(targetReset), 
        reinterpret_cast<LPVOID>(&HookedReset), 
        reinterpret_cast<LPVOID*>(&oReset)
    );
    if (mhStatus != MH_OK) {
        sprintf_s(mhBuffer, sizeof(mhBuffer), "InitializeHook: MH_CreateHook for Reset failed! Status: %s", MH_StatusToString(mhStatus));
        OutputDebugStringA(mhBuffer); Core::Log::Message(mhBuffer);
        MH_RemoveHook(targetEndScene); pDevice->Release(); pD3D->Release(); MH_Uninitialize(); return;
    }

    // Enable Hooks
    mhStatus = MH_EnableHook(targetEndScene);
    if (mhStatus != MH_OK) {
        sprintf_s(mhBuffer, sizeof(mhBuffer), "InitializeHook: MH_EnableHook for EndScene failed! Status: %s", MH_StatusToString(mhStatus));
        OutputDebugStringA(mhBuffer); Core::Log::Message(mhBuffer);
        /* cleanup other hooks */ MH_RemoveHook(targetReset); pDevice->Release(); pD3D->Release(); MH_Uninitialize(); return;
    }
    mhStatus = MH_EnableHook(targetReset);
    if (mhStatus != MH_OK) {
        sprintf_s(mhBuffer, sizeof(mhBuffer), "InitializeHook: MH_EnableHook for Reset failed! Status: %s", MH_StatusToString(mhStatus));
        OutputDebugStringA(mhBuffer); Core::Log::Message(mhBuffer);
        /* cleanup other hooks */ MH_DisableHook(targetEndScene); MH_RemoveHook(targetEndScene); pDevice->Release(); pD3D->Release(); MH_Uninitialize(); return;
    }

    pDevice->Release(); pD3D->Release();
    Core::Log::Message("[InitializeHook] D3D Hooks placed and enabled. Dummy device released.");

    // ... (OM Init, RotationEngine Init, RotationsTab Init as before) ...

    Core::Log::Message("[InitializeHook] Full Initialization Complete (excluding ImGui HWND-dependent parts).");
}
// --- End InitializeHook ---

// --- CleanupHook (Integrates our logic) ---
void CleanupHook(bool isForceTermination) {
    g_isShuttingDown = true; // Set shutdown flag at the very beginning
    OutputDebugStringA("CleanupHook: Starting cleanup (g_isShuttingDown=true)...\n");

    if (isForceTermination) {
        OutputDebugStringA("CleanupHook: Forced termination - skipping ImGui cleanup\n");
        imguiInitialized = false; 
        gameHwnd = NULL;
        oWndProc = NULL;
    } else {
        // Use try-catch blocks to prevent any exceptions from stopping the cleanup process
        try {
            // Stop Rotation Engine first
            if (rotationEngineInstance) {
                OutputDebugStringA("CleanupHook: Stopping Rotation Engine...\n");
                rotationEngineInstance->Stop(); 
            }

            // Stop Fishing Bot
            if (fishingBotInstance) {
                OutputDebugStringA("CleanupHook: Stopping Fishing Bot...\n");
                fishingBotInstance->Stop();
                // Deletion will happen later with other instance deletions
            }

            // NEW: Wait a moment to let any pending tasks finish naturally
            {
                const int MAX_TASK_DRAIN_ATTEMPTS = 5;
                bool tasksStillPresent = true;
                for (int i = 0; i < MAX_TASK_DRAIN_ATTEMPTS && tasksStillPresent; i++) {
                    // Check if there are still tasks in the queue
                    {
                        std::lock_guard<std::mutex> lock(endSceneMutex);
                        tasksStillPresent = !endSceneQueue.empty();
                    }
                    
                    if (tasksStillPresent) {
                        char buffer[128];
                        sprintf_s(buffer, sizeof(buffer), "CleanupHook: Waiting for EndScene queue to drain naturally (attempt %d/%d)...\n", 
                                 i+1, MAX_TASK_DRAIN_ATTEMPTS);
                        OutputDebugStringA(buffer);
                        Core::Log::Message(buffer);
                        
                        // Wait a bit for EndScene to process tasks
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            }

            // Clear any pending tasks to prevent them from running after engine/dependencies are gone
            std::vector<std::function<void()>> cleared_tasks; 
            {
                std::lock_guard<std::mutex> lock(endSceneMutex);
                if (!endSceneQueue.empty()) {
                    cleared_tasks.swap(endSceneQueue); // Move tasks out
                    std::string clear_msg = "[CleanupHook] Cleared " + std::to_string(cleared_tasks.size()) + " tasks from endSceneQueue during shutdown.";
                    Core::Log::Message(clear_msg);
                    OutputDebugStringA((clear_msg + "\n").c_str());
                }
            }
            cleared_tasks.clear(); // Actually delete them (std::functions will clean up)

        } catch (const std::exception& e) {
            OutputDebugStringA(("CleanupHook: Exception during Rotation Engine stop or queue clear: " + std::string(e.what()) + "\n").c_str());
        } catch (...) {
            OutputDebugStringA("CleanupHook: Unknown exception during Rotation Engine stop or queue clear\n");
        }

        // Cleanup ImGui and WndProc Hook
        // OutputDebugStringA("CleanupHook: Cleaning up ImGui & WndProc... (ALL IMGUI/WNDPROC CLEANUP TEMPORARILY COMMENTED OUT)\n");
        try {
            if (imguiInitialized) {
                OutputDebugStringA("CleanupHook: Cleaning up ImGui & WndProc...\n");
                
                // First safely restore the WndProc
                if (oWndProc && gameHwnd && IsWindow(gameHwnd)) {
                    // Only restore if the window is still valid
                    WNDPROC currentProc = (WNDPROC)GetWindowLongPtr(gameHwnd, GWLP_WNDPROC);
                    if (currentProc == WndProc) { // Only restore if our proc is still active
                        SetWindowLongPtr(gameHwnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);
                        OutputDebugStringA("CleanupHook: Original WndProc restored.\n");
                    }
                }
                
                // Now shutdown ImGui components with defensive checks
                try {
                    OutputDebugStringA("CleanupHook: ImGui_ImplDX9_Shutdown...\n");
                    ImGui_ImplDX9_Shutdown();
                } catch (...) {
                    OutputDebugStringA("CleanupHook: Exception during ImGui_ImplDX9_Shutdown\n");
                }
                
                try {
                    OutputDebugStringA("CleanupHook: ImGui_ImplWin32_Shutdown...\n");
                    ImGui_ImplWin32_Shutdown();
                } catch (...) {
                    OutputDebugStringA("CleanupHook: Exception during ImGui_ImplWin32_Shutdown\n");
                }
                
                try {
                    OutputDebugStringA("CleanupHook: ImGui::DestroyContext...\n");
                    ImGui::DestroyContext();
                } catch (...) {
                    OutputDebugStringA("CleanupHook: Exception during ImGui::DestroyContext\n");
                }
                
                imguiInitialized = false;
                OutputDebugStringA("CleanupHook: ImGui cleanup complete.\n");
            } else {
                OutputDebugStringA("CleanupHook: ImGui not initialized, skipping cleanup.\n");
            }
        } catch (...) {
            OutputDebugStringA("CleanupHook: General exception during ImGui cleanup\n");
        }
    }

    // Disable and Remove Hooks using MinHook - with safety checks
    // OutputDebugStringA("CleanupHook: Disabling and removing hooks via MinHook... (ALL MINHOOK CALLS TEMPORARILY COMMENTED OUT FOR CRASH DIAGNOSIS)\n");
    try {
        OutputDebugStringA("CleanupHook: Disabling and removing hooks via MinHook...\n");
        
        // It's generally safer to disable all hooks first, then remove all.
        // Using MH_ALL_HOOKS is fine here if we only hooked D3D stuff.
        MH_STATUS mhStatusDisable = MH_DisableHook(MH_ALL_HOOKS);
        if (mhStatusDisable != MH_OK && mhStatusDisable != MH_ERROR_DISABLED) { // Ignore error if already disabled
            char buffer[128];
            sprintf_s(buffer, sizeof(buffer),"CleanupHook: MH_DisableHook(MH_ALL_HOOKS) failed! Status: %d\n", mhStatusDisable);
            OutputDebugStringA(buffer);
        }

        MH_STATUS mhStatusRemove = MH_RemoveHook(MH_ALL_HOOKS);
        if (mhStatusRemove != MH_OK && mhStatusRemove != MH_ERROR_NOT_CREATED) { // Ignore error if already removed
            char buffer[128];
            sprintf_s(buffer, sizeof(buffer),"CleanupHook: MH_RemoveHook(MH_ALL_HOOKS) failed! Status: %d\n", mhStatusRemove);
            OutputDebugStringA(buffer);
        }

        OutputDebugStringA("CleanupHook: Uninitializing MinHook...\n");
        MH_STATUS mhStatusUninit = MH_Uninitialize();
        if (mhStatusUninit != MH_OK) {
            char buffer[128];
            sprintf_s(buffer, sizeof(buffer),"CleanupHook: MH_Uninitialize failed! Status: %d\n", mhStatusUninit);
            OutputDebugStringA(buffer);
        }
        OutputDebugStringA("CleanupHook: MinHook cleanup complete.\n");
    } catch (...) {
        OutputDebugStringA("CleanupHook: Exception during MinHook cleanup\n");
    }

    // Cleanup OUR Systems
    // OutputDebugStringA("CleanupHook: Cleaning up Rotation System... (TEMPORARILY COMMENTED OUT PARTS)\n");
    try {
        OutputDebugStringA("CleanupHook: Cleaning up Rotation System...\n");
        delete g_rotationsTab; g_rotationsTab = nullptr; 
        delete rotationEngineInstance; rotationEngineInstance = nullptr; 
        delete cooldownManagerInstance; cooldownManagerInstance = nullptr; 
        delete fishingBotInstance; fishingBotInstance = nullptr; // Delete FishingBot instance
        OutputDebugStringA("CleanupHook: Rotation System cleanup complete.\n");
    } catch (...) {
        OutputDebugStringA("CleanupHook: Exception during Rotation System cleanup\n");
    }

    // Cleanup Object Manager Singleton
    // OutputDebugStringA("CleanupHook: Shutting down Object Manager... (TEMPORARILY COMMENTED OUT)\n");
    try {
        OutputDebugStringA("CleanupHook: Shutting down Object Manager...\n");
        ObjectManager::Shutdown(); 
        OutputDebugStringA("CleanupHook: Object Manager shutdown call complete.\n");
    } catch (...) {
        OutputDebugStringA("CleanupHook: Exception during Object Manager shutdown\n");
    }

    OutputDebugStringA("CleanupHook: Full cleanup complete. Logging shutdown shortly...\n");
    
    try {
        Core::Log::Shutdown(); // Shutdown logging last
    } catch (...) {
        OutputDebugStringA("CleanupHook: Exception during Log shutdown\n");
    }
    
    OutputDebugStringA("CleanupHook: Process can safely terminate now.\n");
}
// --- End CleanupHook --- 

// Accessor for FishingBot
Fishing::FishingBot* GetFishingBotInstance() {
    return fishingBotInstance;
} 