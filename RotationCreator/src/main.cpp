#include <windows.h>
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "SpellData.h" // Include our Spell data structures
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <shlobj.h> // For SHCreateDirectoryExW
#include <windows.h> // Required for GetModuleFileNameW

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global variables for DirectX
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Global state for rotations
static std::vector<Rotation> g_rotations;
static int g_selectedRotationIndex = -1;
static int g_selectedStepIndex = -1; // Track selected step index
static std::string g_absoluteRotationSaveDirectory; // Will be initialized in WinMain
static std::string g_currentRotationFile = ""; // Track currently loaded file

// Helper function to get the directory of the executable
std::string GetExecutableDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring::size_type pos = std::wstring(path).find_last_of(L"\\\\/");
    std::wstring wExecutableDir = std::wstring(path).substr(0, pos);
    
    // Convert wstring to string
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wExecutableDir.c_str(), (int)wExecutableDir.length(), NULL, 0, NULL, NULL);
    std::string executableDir(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wExecutableDir.c_str(), (int)wExecutableDir.length(), &executableDir[0], size_needed, NULL, NULL);
    return executableDir;
}

// Helper function to sanitize filenames
std::string SanitizeFileName(const std::string& input) {
    std::string result = input;
    // Replace invalid filename characters with underscores
    const std::string invalidChars = "\\/:*?\"<>|";
    for (char c : invalidChars) {
        std::replace(result.begin(), result.end(), c, '_');
    }
    return result;
}

// Helper function to get rotation filename based on name and class
std::string GetRotationFileName(const Rotation& rotation) {
    std::string sanitizedName = SanitizeFileName(rotation.name);
    std::string sanitizedClass = SanitizeFileName(rotation.className);
    return g_absoluteRotationSaveDirectory + "/" + sanitizedClass + "_" + sanitizedName + ".json";
}

// Helper functions for Load/Save
bool EnsureDirectoryExists(const std::string& path) {
    // Convert std::string to std::wstring for Windows API
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &path[0], (int)path.size(), NULL, 0);
    std::wstring wpath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &path[0], (int)path.size(), &wpath[0], size_needed);
    
    // SHCreateDirectoryExW can create intermediate directories if needed
    // NULL means relative to the current working directory
    HRESULT hr = SHCreateDirectoryExW(NULL, wpath.c_str(), NULL);
    if (SUCCEEDED(hr) || hr == ERROR_ALREADY_EXISTS || hr == ERROR_FILE_EXISTS) {
        return true;
    } else {
        // Optional: Log error GetLastError()
        return false;
    }
}

// List all rotation files in the directory
std::vector<std::string> ListRotationFiles() {
    std::vector<std::string> files;
    if (!std::filesystem::exists(g_absoluteRotationSaveDirectory)) {
        EnsureDirectoryExists(g_absoluteRotationSaveDirectory); // Try to create it if it doesn't exist
        return files; // Return empty if it still doesn't exist or couldn't be created
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(g_absoluteRotationSaveDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path().string());
        }
    }
    return files;
}

// --- Helper functions for Rotation Load/Save ---
void LoadRotationFromFile(const std::string& filename) {
    if (!std::filesystem::exists(filename)) {
        return;
    }
    std::ifstream f(filename);
    if (!f.is_open()) {
        return;
    }
    
    try {
        nlohmann::json j;
        f >> j;
        
        // Create a new rotation and add it to the list
        Rotation newRotation;
        
        // For individual files, we don't store name/class in the JSON anymore,
        // so extract them from the filename
        std::string baseName = std::filesystem::path(filename).stem().string();
        size_t underscorePos = baseName.find('_');
        
        if (underscorePos != std::string::npos) {
            newRotation.className = baseName.substr(0, underscorePos);
            newRotation.name = baseName.substr(underscorePos + 1);
        } else {
            // Fallback if filename doesn't follow the expected format
            newRotation.name = baseName;
            newRotation.className = "Unknown";
        }
        
        // Load the steps array
        newRotation.steps = j.get<std::vector<RotationStep>>();
        
        // If no steps were loaded, add a default one
        if (newRotation.steps.empty()) {
            newRotation.steps.push_back(RotationStep());
        }
        
        g_rotations.push_back(newRotation);
        g_selectedRotationIndex = (int)(g_rotations.size() - 1);
        g_selectedStepIndex = 0; // Select the first step
        g_currentRotationFile = filename;
        
    } catch (const nlohmann::json::exception& /*e*/) {
        // Optional: Log error
    }
}

void LoadAllRotations() {
    g_rotations.clear();
    g_selectedRotationIndex = -1;
    g_selectedStepIndex = -1;
    g_currentRotationFile = "";
    
    std::vector<std::string> files = ListRotationFiles();
    for (const auto& file : files) {
        LoadRotationFromFile(file);
    }
}

void SaveRotationToFile(const Rotation& rotation, const std::string& filename) {
    // Ensure the directory exists before trying to save
    if (!EnsureDirectoryExists(g_absoluteRotationSaveDirectory)) {
        // Optional: Log error creating directory
        return; 
    }

    std::ofstream f(filename);
    if (!f.is_open()) {
        // Optional: Log error opening file
        return;
    }
    
    try {
        // Store only the steps in the file, not the name/class (those are in the filename)
        nlohmann::json j = rotation.steps;
        f << std::setw(4) << j << std::endl; 
    } catch (const nlohmann::json::exception& /*e*/) {
        // Optional: Log error
    }
}

void SaveCurrentRotation() {
    if (g_selectedRotationIndex >= 0 && g_selectedRotationIndex < (int)g_rotations.size()) {
        Rotation& rotation = g_rotations[g_selectedRotationIndex];
        std::string filename = GetRotationFileName(rotation);
        SaveRotationToFile(rotation, filename);
        g_currentRotationFile = filename;
    }
}

void SaveAllRotations() {
    for (const auto& rotation : g_rotations) {
        std::string filename = GetRotationFileName(rotation);
        SaveRotationToFile(rotation, filename);
    }
}

// Helper function for showing tooltip help markers
void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Add this function before WinMain
void RenderPriorityConditionsUI(RotationStep& step) {
    ImGui::Separator();
    ImGui::Text("Priority Settings");
    
    // Base priority input
    int basePriority = step.basePriority;
    if (ImGui::SliderInt("Base Priority", &basePriority, 1, 100)) {
        step.basePriority = basePriority;
    }
    ImGui::SameLine(); HelpMarker("Higher values = higher priority in rotation. Higher priority spells are checked first.");
    
    // Priority boosts section
    if (ImGui::CollapsingHeader("Priority Boost Conditions", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Conditions that increase spell priority when met:");
        
        // Display each priority condition
        for (int i = 0; i < (int)step.priorityBoosts.size(); i++) {
            auto& condition = step.priorityBoosts[i];
            
            ImGui::PushID(i);
            ImGui::BeginGroup();
            
            // Condition type combo
            const char* typeItems[] = {
                "Player Has Aura", 
                "Target Has Aura", 
                "Target Health Below %", 
                "Player Health Below %",
                "Player Resource Above %",
                "Player Resource Below %",
                "Target Distance Below"
            };
            int currentType = static_cast<int>(condition.type);
            if (ImGui::Combo("Type##BoostType", &currentType, typeItems, IM_ARRAYSIZE(typeItems))) {
                condition.type = static_cast<PriorityCondition::Type>(currentType);
            }
            
            // Condition parameters based on type
            if (condition.type == PriorityCondition::Type::PLAYER_HAS_AURA || 
                condition.type == PriorityCondition::Type::TARGET_HAS_AURA) {
                // Aura ID for aura conditions
                int auraId = condition.auraId;
                if (ImGui::InputInt("Aura ID##BoostAuraID", &auraId)) {
                    condition.auraId = auraId;
                }
                ImGui::InputInt("Min Stacks##Prio", &condition.minStacks);
                ImGui::SameLine(); HelpMarker("For 'Has Aura' (PriorityCondition type is PLAYER_HAS_AURA or TARGET_HAS_AURA):\n  - Set to 0 to check if aura is just present (any stack count, including 0).\n  - Set to 1+ to require at least that many stacks."); // Simplified for priority boosts as they are always 'presence true'
            }
            else if (condition.type == PriorityCondition::Type::PLAYER_RESOURCE_PERCENT_ABOVE ||
                     condition.type == PriorityCondition::Type::PLAYER_RESOURCE_PERCENT_BELOW) {
                // Resource type selection for resource-based conditions
                const char* resourceItems[] = { "Mana", "Rage", "Energy", "Focus", "None" };
                int currentResource = static_cast<int>(condition.resourceType);
                if (ImGui::Combo("Resource Type##BoostResource", &currentResource, resourceItems, IM_ARRAYSIZE(resourceItems))) {
                    condition.resourceType = static_cast<ResourceType>(currentResource);
                }
                
                // Threshold value for percentage conditions
                float threshold = condition.thresholdValue;
                if (ImGui::SliderFloat(condition.type == PriorityCondition::Type::PLAYER_RESOURCE_PERCENT_ABOVE ? 
                                       "Above % Threshold##BoostThreshold" : "Below % Threshold##BoostThreshold", 
                                       &threshold, 0.0f, 100.0f, "%.1f")) {
                    condition.thresholdValue = threshold;
                }
            } else if (condition.type == PriorityCondition::Type::TARGET_DISTANCE_BELOW) {
                // Distance threshold for distance conditions
                float distThreshold = condition.distanceThreshold;
                if (ImGui::InputFloat("Distance Threshold##BoostDist", &distThreshold, 0.5f, 1.0f, "%.1f yd")) {
                    condition.distanceThreshold = (distThreshold < 0.0f) ? 0.0f : distThreshold; // Ensure non-negative
                }
            }
            
            // Priority boost amount
            int boost = condition.priorityBoost;
            if (ImGui::SliderInt("Priority Boost##BoostValue", &boost, 1, 100)) {
                condition.priorityBoost = boost;
            }
            
            // Delete button
            if (ImGui::Button("Delete##BoostDelete")) {
                step.priorityBoosts.erase(step.priorityBoosts.begin() + i);
                i--; // Adjust index after deletion
            }
            
            ImGui::EndGroup();
            ImGui::PopID();
            ImGui::Separator();
        }
        
        // Add new condition button
        if (ImGui::Button("Add Priority Boost")) {
            PriorityCondition newCondition;
            step.priorityBoosts.push_back(newCondition);
        }
    }
}

// Add this function if it doesn't exist
void RenderConditionsUI(RotationStep& step) {
    ImGui::Separator();
    ImGui::Text("Pre-cast Conditions");
    ImGui::SameLine(); HelpMarker("All these conditions must be met for the spell to be considered.");

    // Condition type names and values for the dropdown
    const char* conditionTypeNames[] = {
        "Health Percent Below", "Mana Percent Above", "Target Is Casting",
        "Player Has Aura", "Target Has Aura", "Player Missing Aura", "Target Missing Aura",
        "Spell Off Cooldown", "Melee Units Around Player >", "Units In Frontal Cone >",
        "Player Threat On Target Below %", "Spell Has Charges",
        "Player Is Facing Target", "Combo Points ≥", "UNKNOWN"
    };
    ConditionType conditionTypeValues[] = {
        ConditionType::HEALTH_PERCENT_BELOW, ConditionType::MANA_PERCENT_ABOVE, ConditionType::TARGET_IS_CASTING,
        ConditionType::PLAYER_HAS_AURA, ConditionType::TARGET_HAS_AURA, ConditionType::PLAYER_MISSING_AURA, ConditionType::TARGET_MISSING_AURA,
        ConditionType::SPELL_OFF_COOLDOWN, ConditionType::MELEE_UNITS_AROUND_PLAYER_GREATER_THAN, ConditionType::UNITS_IN_FRONTAL_CONE_GT,
        ConditionType::PLAYER_THREAT_ON_TARGET_BELOW_PERCENT, ConditionType::SPELL_HAS_CHARGES,
        ConditionType::PLAYER_IS_FACING_TARGET, ConditionType::COMBO_POINTS_GREATER_THAN_OR_EQUAL_TO, ConditionType::UNKNOWN
    };
    // Ensure conditionTypeNames and conditionTypeValues have the same number of elements
    static_assert(IM_ARRAYSIZE(conditionTypeNames) == IM_ARRAYSIZE(conditionTypeValues), "Condition type arrays mismatch!");

    // Temporary buffer for Aura ID input (can be reused or adapted for other string inputs)
    static char multiAuraIdInputBuf[256]; 

    for (int i = 0; i < (int)step.conditions.size(); ++i) {
        Condition& current_condition = step.conditions[i];
        ImGui::PushID(i); // Unique ID for widgets in this iteration

        ImGui::Text("Condition %d:", i + 1);
        ImGui::Indent();

        // Find current index for ConditionType combo
        int currentConditionTypeIndex = -1;
        for (int type_idx = 0; type_idx < IM_ARRAYSIZE(conditionTypeValues); ++type_idx) {
            if (conditionTypeValues[type_idx] == current_condition.type) {
                currentConditionTypeIndex = type_idx;
                break;
            }
        }

        if (ImGui::Combo("Type", &currentConditionTypeIndex, conditionTypeNames, IM_ARRAYSIZE(conditionTypeNames))) {
            if (currentConditionTypeIndex >= 0 && currentConditionTypeIndex < IM_ARRAYSIZE(conditionTypeValues)) {
                current_condition.type = conditionTypeValues[currentConditionTypeIndex];
                // Reset fields when type changes to avoid carrying over old values
                // current_condition = Condition(); // This would reset all, including type, be careful
                // Instead, selectively clear/reset fields based on the new type if necessary
            }
        }

        // --- Fields specific to condition types ---
        if (current_condition.type == ConditionType::HEALTH_PERCENT_BELOW) {
            ImGui::InputFloat("Health Threshold %", &current_condition.value, 1.0f, 5.0f, "%.1f%%");
            ImGui::Checkbox("On Player", &current_condition.targetIsPlayer);
            if (!current_condition.targetIsPlayer) {
                ImGui::Checkbox("On Friendly Target", &current_condition.targetIsFriendly);
            }
            HelpMarker("Condition met if specified unit's health is BELOW this percentage.");
        }
        else if (current_condition.type == ConditionType::MANA_PERCENT_ABOVE) {
            ImGui::InputFloat("Mana Threshold %", &current_condition.value, 1.0f, 5.0f, "%.1f%%");
            // Generally mana checks are for player
            current_condition.targetIsPlayer = true; 
            // ImGui::Checkbox("On Player", &current_condition.targetIsPlayer); // Could enable if target mana is a thing
            HelpMarker("Condition met if player's mana is ABOVE this percentage.");
        }
        else if (current_condition.type == ConditionType::PLAYER_HAS_AURA || 
                 current_condition.type == ConditionType::TARGET_HAS_AURA || 
                 current_condition.type == ConditionType::PLAYER_MISSING_AURA || 
                 current_condition.type == ConditionType::TARGET_MISSING_AURA) {
            
            const char* auraTargetTypes[] = { "Player", "Target", "Focus", "Friendly" };
            int currentAuraTargetIdx = static_cast<int>(current_condition.auraTarget);
            if (ImGui::Combo("Aura On", &currentAuraTargetIdx, auraTargetTypes, IM_ARRAYSIZE(auraTargetTypes))) {
                current_condition.auraTarget = static_cast<TargetUnit>(currentAuraTargetIdx);
            }

            ImGui::InputInt("Min Stacks", &current_condition.minStacks);
            HelpMarker("For 'Has Aura': 0 for presence (any stacks), 1+ for min stacks.\nFor 'Missing Aura': 0 for complete absence, 1+ if absent OR < N stacks.");

            const char* logicTypes[] = { "ANY_OF (OR)", "ALL_OF (AND)" };
            int currentLogic = static_cast<int>(current_condition.multiAuraLogic);
            if (ImGui::Combo("Logic for Aura IDs", &currentLogic, logicTypes, IM_ARRAYSIZE(logicTypes))) {
                current_condition.multiAuraLogic = static_cast<AuraConditionLogic>(currentLogic);
            }

            ImGui::Text("Aura IDs:");
            for (int k = 0; k < (int)current_condition.multiAuraIds.size(); ++k) {
                ImGui::PushID(k);
                ImGui::Text("ID: %u", current_condition.multiAuraIds[k]);
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    current_condition.multiAuraIds.erase(current_condition.multiAuraIds.begin() + k);
                    ImGui::PopID();
                    k--; 
                } else {
                    ImGui::PopID();
                }
            }
            ImGui::InputText("Add Aura ID", multiAuraIdInputBuf, sizeof(multiAuraIdInputBuf));
            ImGui::SameLine();
            if (ImGui::Button("Add ID")) {
                try {
                    uint32_t newId = std::stoul(multiAuraIdInputBuf);
                    if (newId > 0) {
                        current_condition.multiAuraIds.push_back(newId);
                        multiAuraIdInputBuf[0] = '\0'; // Clear buffer
                    }
                } catch (const std::exception&) {/* ignore */}
            }
        }
        else if (current_condition.type == ConditionType::SPELL_OFF_COOLDOWN) {
            ImGui::InputScalar("Spell ID (Cooldown)", ImGuiDataType_U32, &current_condition.spellId);
            HelpMarker("Condition met if this spell is NOT on cooldown.");
        }
        else if (current_condition.type == ConditionType::TARGET_IS_CASTING) {
            ImGui::InputScalar("Target Casting Spell ID (0 for any)", ImGuiDataType_U32, &current_condition.spellId);
            HelpMarker("Condition met if the target is casting this specific spell ID. Set to 0 to check for ANY cast.");
        }
        else if (current_condition.type == ConditionType::MELEE_UNITS_AROUND_PLAYER_GREATER_THAN) {
            // Value is used for Unit Count Threshold
            ImGui::InputFloat("Unit Count Threshold (>)", &current_condition.value, 0.0f, 1.0f, "%.0f"); 
            ImGui::InputFloat("Melee Range (yds)", &current_condition.meleeRangeValue, 0.1f, 0.5f, "%.1f");
            HelpMarker("Condition met if the number of hostile units within the specified 'Melee Range (yds)' of the player is GREATER than 'Unit Count Threshold'.");
        }
        else if (current_condition.type == ConditionType::UNITS_IN_FRONTAL_CONE_GT) {
            // Value is used for Unit Count Threshold
            ImGui::InputFloat("Unit Count Threshold (>)", &current_condition.value, 0.0f, 1.0f, "%.0f"); 
            HelpMarker("Condition met if the number of units (typically hostile) in the frontal cone is GREATER than this threshold.");
            ImGui::InputFloat("Cone Range (yds)", &current_condition.meleeRangeValue, 0.1f, 0.5f, "%.1f");
            HelpMarker("The maximum distance for the frontal cone check.");
            ImGui::InputFloat("Cone Angle (degrees)", &current_condition.coneAngleDegrees, 1.0f, 5.0f, "%.0f");
            HelpMarker("The angle of the frontal cone in degrees (e.g., 90 for a 90-degree cone).");
        }
        else if (current_condition.type == ConditionType::PLAYER_THREAT_ON_TARGET_BELOW_PERCENT) {
            ImGui::InputFloat("Threat Threshold %", &current_condition.value, 1.0f, 5.0f, "%.1f%%");
            HelpMarker("Condition met if player's threat percentage on the current spell target is BELOW this value.\nIf player is not on target's threat table, threat is considered 0%.");
        }
        else if (current_condition.type == ConditionType::PLAYER_IS_FACING_TARGET) {
            ImGui::InputFloat("Facing Cone Angle (Deg)", &current_condition.facingConeAngle, 1.0f, 5.0f, "%.1f");
            HelpMarker("The angle (in degrees) of the cone in front of the player. The target must be within this cone. E.g., 60 means +/- 30 degrees from center.");
        }
        else if (current_condition.type == ConditionType::SPELL_HAS_CHARGES) {
            ImGui::InputInt("Spell ID", (int*)&current_condition.spellId); // SpellID of the charge spell
            ImGui::InputInt("Min Charges Required", (int*)&current_condition.value); // Using .value as int for min charges
            if (current_condition.value < 1.0f) current_condition.value = 1.0f; // Ensure at least 1
            HelpMarker("Condition met if the specified Spell ID has at least 'Min Charges Required'.");
        }
        else if (current_condition.type == ConditionType::COMBO_POINTS_GREATER_THAN_OR_EQUAL_TO) {
            int comboPoints = (int)current_condition.value;
            if (ImGui::SliderInt("Combo Points Threshold", &comboPoints, 1, 5, "%d")) {
                current_condition.value = (float)comboPoints;
            }
            HelpMarker("Condition met if the player has at least this many combo points.");
        }

        // Remove condition button
        if (ImGui::Button("Remove This Condition")) {
            step.conditions.erase(step.conditions.begin() + i);
            i--; // Adjust index
        }
        ImGui::Unindent();
        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button("Add New Condition")) {
        step.conditions.emplace_back(); // Add a new default Condition
    }
    ImGui::Separator();

    // --- LEGACY UI - Keep for viewing/migrating old data, but hide by default? ---
    // For now, we'll remove the direct rendering of old auraConditions and healthConditions
    // to avoid UI clutter. A migration path would be needed for existing JSONs if they
    // aren't automatically compatible or if we want to convert them to the new `conditions` vector.

    // OLD Aura Conditions UI (previously in RenderConditionsUI)
    /*
    ImGui::Text("OLD Aura Condition Blocks (Read-Only - Migrate to New System):");
    for (int i = 0; i < (int)step.auraConditions.size(); ++i) {
        AuraCondition& legacy_condition = step.auraConditions[i];
        // ... simplified read-only display of legacy_condition ...
    }
    */

    // OLD Health Conditions UI (previously in StepDetailsPane)
    /*
    ImGui::Text("OLD Health Conditions (Read-Only - Migrate to New System):");
    for(size_t k = 0; k < step.healthConditions.size(); ++k) {
        const HealthCondition& legacy_hc = step.healthConditions[k];
        // ... simplified read-only display of legacy_hc ...
    }
    */
}

// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize the absolute path for saving rotations
    g_absoluteRotationSaveDirectory = GetExecutableDirectory() + "\\rotations";

    // Create application window
    // Use wide strings for class name
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"RotationCreatorClass", nullptr }; // Use WNDCLASSEXW and L prefix
    ::RegisterClassExW(&wc); // Use W version
    // Use wide strings for window title
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Rotation Creator", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr); // Use CreateWindowW and L prefix

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance); // Use W version
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, nCmdShow);
    ::UpdateWindow(hwnd);

    // Attempt to restore desired window size if it's too small after showing.
    // This can happen if the injection process or host application resizes it.
    if (hwnd) {
        RECT currentWindowRect;
        ::GetWindowRect(hwnd, &currentWindowRect);

        long currentWidth = currentWindowRect.right - currentWindowRect.left;
        long currentHeight = currentWindowRect.bottom - currentWindowRect.top;
        long targetWidth = 1280;
        long targetHeight = 800;

        // Check if the window is significantly smaller than intended.
        // Using a threshold like half the target size, or simply if it's not the exact target size.
        // For "scruntched up", checking if it's smaller than target should be sufficient.
        if (currentWidth < targetWidth || currentHeight < targetHeight) {
            // Preserve the window's current top-left position.
            ::SetWindowPos(hwnd, NULL, currentWindowRect.left, currentWindowRect.top, 
                           (int)targetWidth, (int)targetHeight, 
                           SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Load initial data
    LoadAllRotations();

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // --- Rotation Editor Window (Now the main window) ---
        {
            // Create a fullscreen window that always fills the entire application window
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::Begin("Rotation Editor", nullptr, 
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

            // --- Top bar for Load/Save Rotations ---
             if (ImGui::Button("Load Rotations")) {
                 LoadAllRotations();
             }
             ImGui::SameLine();
             if (ImGui::Button("Save All Rotations")) {
                 SaveAllRotations();
             }
             ImGui::SameLine();
             if (g_selectedRotationIndex >= 0 && g_selectedRotationIndex < (int)g_rotations.size()) {
                 if (ImGui::Button("Save Current Rotation")) {
                     SaveCurrentRotation();
                 }
             } else {
                 ImGui::BeginDisabled();
                 ImGui::Button("Save Current Rotation");
                 ImGui::EndDisabled();
             }
             ImGui::Separator();

            // --- Main Area with Columns --- Changed to 3 columns
             // Calculate table height to use all available space minus the top and bottom button rows
             float bottomButtonsHeight = ImGui::GetFrameHeightWithSpacing() * 2; // Reserve space for bottom buttons (2 rows)
             float tableHeight = ImGui::GetContentRegionAvail().y - bottomButtonsHeight;
             if (ImGui::BeginTable("RotationLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, tableHeight))) 
             {
                 // Column Setup
                 ImGui::TableSetupColumn("Rotations", ImGuiTableColumnFlags_WidthFixed, 200.0f); 
                 ImGui::TableSetupColumn("Steps", ImGuiTableColumnFlags_WidthFixed, 200.0f); 
                 ImGui::TableSetupColumn("Step Details", ImGuiTableColumnFlags_WidthStretch);
                 ImGui::TableSetupScrollFreeze(0, 1); // Freeze the header row

                 ImGui::TableHeadersRow();
                 ImGui::TableNextRow();

                 // --- Column 1: Rotation List --- 
                 ImGui::TableSetColumnIndex(0);
                 ImGui::BeginChild("RotationListPane", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                 
                 for (size_t i = 0; i < g_rotations.size(); ++i) {
                     std::string rot_label = g_rotations[i].name + " (" + g_rotations[i].className + ")";
                     if (ImGui::Selectable(rot_label.c_str(), g_selectedRotationIndex == (int)i)) {
                         g_selectedRotationIndex = (int)i;
                         g_selectedStepIndex = -1; // Reset step selection when rotation changes
                     }
                 }
                 ImGui::EndChild();

                 // --- Column 2: Steps List --- 
                 ImGui::TableSetColumnIndex(1);
                 ImGui::BeginChild("StepsListPane", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                 
                 // Only show steps if a rotation is selected
                 if (g_selectedRotationIndex >= 0 && g_selectedRotationIndex < (int)g_rotations.size()) {
                    Rotation& selected_rotation = g_rotations[g_selectedRotationIndex];
                    for(size_t i = 0; i < selected_rotation.steps.size(); ++i) {
                        std::string step_label = std::to_string(i+1) + ": " + selected_rotation.steps[i].name + " (" + std::to_string(selected_rotation.steps[i].id) + ")";
                        if(ImGui::Selectable(step_label.c_str(), g_selectedStepIndex == (int)i)) {
                            g_selectedStepIndex = (int)i;
                        }
                    }
                 }
                 ImGui::EndChild();
                 
                 // --- Column 3: Step Details --- 
                 ImGui::TableSetColumnIndex(2);
                 ImGui::BeginChild("StepDetailsPane", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

                 // Only show details if a rotation AND a step are selected
                 if (g_selectedRotationIndex >= 0 && g_selectedRotationIndex < (int)g_rotations.size() &&
                     g_selectedStepIndex >= 0) 
                 {
                    Rotation& selected_rotation = g_rotations[g_selectedRotationIndex];
                    // Check if selected step index is still valid after potential removal
                    if (g_selectedStepIndex < (int)selected_rotation.steps.size()) {
                        RotationStep& selected_step = selected_rotation.steps[g_selectedStepIndex];
                        
                        ImGui::PushID(g_selectedStepIndex); // Use step index for unique ID scope

                        // --- Rotation Info (Moved here) ---
                        ImGui::Text("Rotation: %s (%s)", selected_rotation.name.c_str(), selected_rotation.className.c_str());
                        // Edit Name
                        char nameBuf[128];
                        strncpy_s(nameBuf, sizeof(nameBuf), selected_rotation.name.c_str(), sizeof(nameBuf) - 1);
                        if (ImGui::InputText("Rotation Name", nameBuf, sizeof(nameBuf))) {
                            selected_rotation.name = nameBuf;
                        }
                        // Edit Class Name 
                        char classBuf[128];
                        strncpy_s(classBuf, sizeof(classBuf), selected_rotation.className.c_str(), sizeof(classBuf) - 1);
                        if (ImGui::InputText("Class Name", classBuf, sizeof(classBuf))) {
                            selected_rotation.className = classBuf;
                        }
                        ImGui::Separator();
                        ImGui::Text("Editing Step %d: %s", g_selectedStepIndex + 1, selected_step.name.c_str());
                        ImGui::Separator();
                        // --- End Rotation Info ---
                        
                        // --- Spell details embedded in the step ---
                        ImGui::InputInt("Spell ID", &selected_step.id);
                        
                        char stepNameBuf[128];
                        strncpy_s(stepNameBuf, sizeof(stepNameBuf), selected_step.name.c_str(), sizeof(stepNameBuf) - 1);
                        if (ImGui::InputText("Spell Name", stepNameBuf, sizeof(stepNameBuf))) {
                            selected_step.name = stepNameBuf;
                        }

                        ImGui::Text("Range Settings:");
                        ImGui::SameLine();
                        HelpMarker("Set minimum and maximum range for spells with range requirements.\nMin range = 0 means no minimum range.\nMax range = spell's maximum effective range.");
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
                        ImGui::InputFloat("Min Range##Step", &selected_step.minRange, 0.1f, 1.0f, "%.1f");
                        ImGui::SameLine();
                        ImGui::InputFloat("Max Range##Step", &selected_step.maxRange, 0.1f, 1.0f, "%.1f");
                        ImGui::PopItemWidth();

                        ImGui::InputInt("Resource Cost", &selected_step.resourceCost);

                        // Dropdown for ResourceType
                        const char* resource_type_items[] = { "Mana", "Rage", "Energy", "Focus", "None" };
                        // Find current index for the combo
                        int current_resource_type_index = static_cast<int>(selected_step.resourceType);
                        if (ImGui::Combo("Resource Type", &current_resource_type_index, resource_type_items, IM_ARRAYSIZE(resource_type_items))) {
                            selected_step.resourceType = static_cast<ResourceType>(current_resource_type_index);
                        }

                        ImGui::InputFloat("Cast Time (s)", &selected_step.castTime, 0.1f, 0.5f, "%.1f");
                        ImGui::InputInt("Base Damage/Healing", &selected_step.baseDamage);
                        ImGui::Checkbox("Is Channeled", &selected_step.isChanneled);
                        ImGui::SameLine();
                        ImGui::Checkbox("Castable While Moving", &selected_step.castableWhileMoving);
                        ImGui::Checkbox("Is Healing Spell", &selected_step.isHeal);
                        
                        // --- Charge Mechanics --- 
                        ImGui::Separator();
                        ImGui::Text("Charge Mechanics:");
                        ImGui::InputInt("Max Charges", &selected_step.maxCharges);
                        ImGui::SameLine(); HelpMarker("Set to 1 for spells without charges (or with only a single charge that recharges).");
                        if (selected_step.maxCharges < 1) selected_step.maxCharges = 1; // Ensure at least 1 charge

                        ImGui::InputFloat("Recharge Time (s)", &selected_step.rechargeTime, 0.1f, 0.5f, "%.1f s");
                        if (selected_step.rechargeTime < 0.0f) selected_step.rechargeTime = 0.0f; // Ensure non-negative
                        ImGui::SameLine(); HelpMarker("Time for one charge to become available. Set to 0 if charges don't recharge (e.g. fixed number of uses per encounter) or if it's a standard cooldown spell with Max Charges = 1.");
                        ImGui::Separator();
                        // --- End Charge Mechanics ---

                        // Add Target Type dropdown
                        const char* target_type_items[] = { "Self", "Friendly", "Enemy", "Self or Friendly", "Any", "None" }; // Added Self or Friendly here
                        int current_target_type = -1;

                        // Determine the current index based on the step's targetType
                        switch (selected_step.targetType) {
                            case SpellTargetType::Self:           current_target_type = 0; break;
                            case SpellTargetType::Friendly:       current_target_type = 1; break;
                            case SpellTargetType::Enemy:          current_target_type = 2; break;
                            case SpellTargetType::SelfOrFriendly: current_target_type = 3; break; // Added case
                            case SpellTargetType::Any:            current_target_type = 4; break;
                            case SpellTargetType::None:           current_target_type = 5; break;
                            default: current_target_type = 2; break; // Default to Enemy if unset
                        }

                        if (ImGui::Combo("Target Type##Step", &current_target_type, target_type_items, IM_ARRAYSIZE(target_type_items))) { 
                            switch (current_target_type) {
                                case 0: selected_step.targetType = SpellTargetType::Self; break;
                                case 1: selected_step.targetType = SpellTargetType::Friendly; break;
                                case 2: selected_step.targetType = SpellTargetType::Enemy; break;
                                case 3: selected_step.targetType = SpellTargetType::SelfOrFriendly; break; // Added case
                                case 4: selected_step.targetType = SpellTargetType::Any; break;
                                case 5: selected_step.targetType = SpellTargetType::None; break;
                            }
                        }
                        
                        // --- End Spell Details ---
                        
                        // This is the correct, single call to RenderConditionsUI
                        RenderConditionsUI(selected_step); 

                        // Add the priority conditions UI
                        RenderPriorityConditionsUI(selected_step);

                        ImGui::PopID(); // End step ID scope
                    } else {
                         // Selected step index became invalid (likely due to removal)
                         g_selectedStepIndex = -1;
                         ImGui::Text("Select a step from the list to edit its details.");
                    }
                 } else {
                    // No rotation or no step selected
                    if (g_selectedRotationIndex == -1) {
                        ImGui::Text("Select a rotation from the list.");
                    } else {
                        ImGui::Text("Select a step from the list to edit its details.");
                    }
                 }

                 ImGui::EndChild(); // StepDetailsPane

                 ImGui::EndTable(); // RotationLayout
             }
             
             // Add action buttons at the bottom of the window
             ImGui::Spacing();
             ImGui::Separator();
             ImGui::Spacing();
             
             // Using fixed height buttons with consistent layout
             float buttonHeight = ImGui::GetFrameHeight();
             float windowWidth = ImGui::GetWindowWidth();
             float buttonWidth = (windowWidth - 24) / 4; // 4 buttons with some spacing
             
             // First row - Rotation management
             if (ImGui::Button("Add Rotation", ImVec2(buttonWidth, buttonHeight))) { 
                 Rotation newRotation;
                 // Automatically add a default step to the rotation
                 newRotation.steps.push_back(RotationStep());
                 g_rotations.push_back(newRotation); 
                 g_selectedRotationIndex = (int)(g_rotations.size() - 1);
                 g_selectedStepIndex = 0; // Select the default step we just added
             }
             
             ImGui::SameLine();
             
             if (g_selectedRotationIndex >= 0 && g_selectedRotationIndex < (int)g_rotations.size()) {
                 // Store rotation details before potential erasure for filename generation
                Rotation rotation_to_delete = g_rotations[g_selectedRotationIndex];
                std::string file_to_delete = GetRotationFileName(rotation_to_delete);

                // Change button color to red
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));

                if (ImGui::Button("Remove Rotation", ImVec2(buttonWidth, buttonHeight))) { 
                    // Delete the file first
                    if (std::filesystem::exists(file_to_delete)) {
                        std::filesystem::remove(file_to_delete);
                    }
                    
                    g_rotations.erase(g_rotations.begin() + g_selectedRotationIndex);
                    
                    // Adjust selected index if it was the last element
                    if (g_selectedRotationIndex >= (int)g_rotations.size() && !g_rotations.empty()) {
                        g_selectedRotationIndex = (int)g_rotations.size() - 1;
                    } else if (g_rotations.empty()) {
                        g_selectedRotationIndex = -1;
                    }
                    // If the selection is now out of bounds (e.g. an item from middle was removed,
                    // and previous selection was > new size)
                    // or if list is not empty, but nothing is selected (was -1, or item deleted made it -1 implicitly)
                    // default to selecting the first item if available, or -1 if list is now empty.
                    if (g_selectedRotationIndex >= (int)g_rotations.size()) {
                         g_selectedRotationIndex = g_rotations.empty() ? -1 : 0;
                    }


                    g_selectedStepIndex = -1; // Reset step selection
                }
                ImGui::PopStyleColor(3); // Pop the 3 pushed colors
             } else {
                 ImGui::BeginDisabled();
                 ImGui::Button("Remove Rotation", ImVec2(buttonWidth, buttonHeight));
                 ImGui::EndDisabled();
             }
             
             ImGui::SameLine();
             
             // Step management buttons
             if (g_selectedRotationIndex >= 0 && g_selectedRotationIndex < (int)g_rotations.size()) {
                 Rotation& selected_rotation = g_rotations[g_selectedRotationIndex];
                 if (ImGui::Button("Add Step", ImVec2(buttonWidth, buttonHeight))) { 
                     selected_rotation.steps.push_back(RotationStep()); 
                     g_selectedStepIndex = (int)(selected_rotation.steps.size() - 1); // Select the new step
                 }
                 
                 ImGui::SameLine();
                 
                 // Only enable remove if a step is actually selected
                 if (g_selectedStepIndex >= 0 && g_selectedStepIndex < (int)selected_rotation.steps.size()) {
                     if (ImGui::Button("Remove Step", ImVec2(buttonWidth, buttonHeight))) { 
                         selected_rotation.steps.erase(selected_rotation.steps.begin() + g_selectedStepIndex);
                         g_selectedStepIndex = -1; // Deselect step
                     }
                 } else {
                     ImGui::BeginDisabled();
                     ImGui::Button("Remove Step", ImVec2(buttonWidth, buttonHeight));
                     ImGui::EndDisabled();
                 }
             } else {
                 ImGui::BeginDisabled();
                 ImGui::Button("Add Step", ImVec2(buttonWidth, buttonHeight));
                 ImGui::SameLine();
                 ImGui::Button("Remove Step", ImVec2(buttonWidth, buttonHeight));
                 ImGui::EndDisabled();
             }

            ImGui::End(); // Rotation Editor
        }
        // --- End Rotation Editor Window ---

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA(114, 144, 154, 255); // A slightly nicer background color
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);

        // Handle loss of D3D9 device
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance); // Use W version

    return 0;
}

// Helper functions for DirectX initialization/cleanup
bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    // Specify an explicit back buffer format that is commonly supported.
    // D3DFMT_X8R8G8B8 is a good choice for compatibility.
    g_d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8; 
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // Present without vsync

    HRESULT hr;
    // Try hardware vertex processing
    hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice);
    if (FAILED(hr)) {
        // Fallback to software vertex processing
        hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice);
        if (FAILED(hr)) {
             // Further fallback to reference device (slow, mostly for debugging)
             hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice);
             if (FAILED(hr)){
                 g_pD3D->Release();
                 g_pD3D = nullptr;
                 // Consider logging the HRESULT hr here
                 return false;
             }
        }
    }
    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    // Handle specific errors if necessary, e.g., log or try different settings
    if (hr == D3DERR_INVALIDCALL) {
        // This might indicate an issue with the presentation parameters
        IM_ASSERT(0); 
    } else if (FAILED(hr)) {
         // Handle other Reset errors
         // Perhaps log the error or attempt recovery
    }
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        // Check if device exists and window is not minimized.
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) 
        {
            // Check if size actually changed to avoid unnecessary resets
            UINT newWidth = LOWORD(lParam);
            UINT newHeight = HIWORD(lParam);
            if (newWidth != g_d3dpp.BackBufferWidth || newHeight != g_d3dpp.BackBufferHeight) {
                g_d3dpp.BackBufferWidth = newWidth;
                g_d3dpp.BackBufferHeight = newHeight;
                ResetDevice();
            }
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
} 