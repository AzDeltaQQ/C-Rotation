#include "objects_tab.h"
#include "imgui.h"
#include "../../objectManager/objectManager.h"
#include "../../types/types.h"
#include "../../types/wowunit.h"
#include "../../types/WowPlayer.h"
#include "../../types/wowobject.h"
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>

namespace GUI {

    // The free function is implemented to maintain backwards compatibility
    // This will be deprecated in favor of the class-based approach
    // void RenderObjectsTab() {
    //     static ObjectsTab objTab;
    //     objTab.Render();
    // }

    void ObjectsTab::Render() {
        ObjectManager* objMgr = ObjectManager::GetInstance();
        if (!objMgr || !objMgr->IsInitialized()) {
            ImGui::Text("Object Manager not initialized.");
            return;
        }

        try {
            // Get the object map
            auto objectMap = objMgr->GetAllObjects();
            
            ImGui::Text("%zu objects currently tracked", objectMap.size());
            ImGui::Separator();

            // Filter Controls
            ImGui::TextUnformatted("Filters:");
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            ImGui::SliderFloat("Max Distance", &filter_max_distance, 0.0f, 200.0f, "%.1f yd");
            ImGui::PopItemWidth();

            // Type Filters
            ImGui::Checkbox("Units", &filter_show_units); ImGui::SameLine();
            ImGui::Checkbox("Players", &filter_show_players); ImGui::SameLine();
            ImGui::Checkbox("GameObjects", &filter_show_gameobjects); ImGui::SameLine();
            ImGui::Checkbox("Corpses", &filter_show_corpses);
            ImGui::NewLine();
            ImGui::Checkbox("Items", &filter_show_items); ImGui::SameLine();
            ImGui::Checkbox("Containers", &filter_show_containers); ImGui::SameLine();
            ImGui::Checkbox("DynamicObj", &filter_show_dynamicobjects); ImGui::SameLine();
            ImGui::Checkbox("Other", &filter_show_other);
            ImGui::Separator();

            // Get player position for distance calculations
            Vector3 playerPos;
            bool playerPosValid = false;
            
            auto player = objMgr->GetLocalPlayer();
            if (player) {
                playerPos = player->GetPosition();
                playerPosValid = !playerPos.IsZero();
            }
            
            // Split view into two panes
            float listHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 2;
            
            // Left side: Object list
            ImGui::BeginChild("ObjectListPane", ImVec2(ImGui::GetContentRegionAvail().x * 0.65f, listHeight), true);
            
            if (objectMap.empty()) {
                ImGui::Text("Object cache is empty.");
            } else {
                int displayed_index = 0;
                for (const auto& pair : objectMap) {
                    try {
                        const WGUID& currentGuid = pair.first;
                        const auto& objPtr = pair.second;
                        if (!objPtr) continue;

                        WowObjectType currentType = objPtr->GetType();
                        uint64_t guid64 = objPtr->GetGUID64();

                        // Log every object being considered by the tab
                        // Safe to call GetName() here as it's within a try-catch block, though ensure it handles errors gracefully.
                        std::string objName = "N/A";
                        try { objName = objPtr->GetName(); } catch(...) {} // Simple error guard for GetName

                        // Create a log stream for debug info (if needed)
                        std::stringstream log_ss; 

                        bool isUnit = false;
                        bool isGameObject = false;

                        // Apply Type Filter
                        bool show_object = false;
                        switch (currentType) {
                            case OBJECT_ITEM:          show_object = filter_show_items; break;
                            case OBJECT_CONTAINER:     show_object = filter_show_containers; break;
                            case OBJECT_UNIT:          show_object = filter_show_units; isUnit = true; break;
                            case OBJECT_PLAYER:        show_object = filter_show_players; isUnit = true; break;
                            case OBJECT_GAMEOBJECT:
                                show_object = filter_show_gameobjects; isGameObject = true;
                                break;
                            case OBJECT_DYNAMICOBJECT: show_object = filter_show_dynamicobjects; break;
                            case OBJECT_CORPSE:        show_object = filter_show_corpses; break;
                            default:                   show_object = filter_show_other; break;
                        }
                        
                        if (!show_object) {
                            continue;
                        }

                        // Apply Distance Filter (only for objects with position)
                        float distance = -1.0f;
                        bool has_position = (currentType == OBJECT_UNIT || currentType == OBJECT_PLAYER || 
                                            currentType == OBJECT_GAMEOBJECT || currentType == OBJECT_DYNAMICOBJECT || 
                                            currentType == OBJECT_CORPSE);

                        if (has_position && playerPosValid) {
                            Vector3 objPos = objPtr->GetPosition();
                            distance = playerPos.Distance(objPos);
                            if (distance > filter_max_distance) {
                                continue;
                            }
                        }

                        // Format display row
                        std::stringstream ssLabel;
                        ssLabel << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << guid64;
                        ssLabel << " | T:" << GetObjectTypeString(currentType);
                        ssLabel << " | N: '" << objName << "'";

                        // Add status indicator for units and players
                        if (currentType == OBJECT_UNIT || currentType == OBJECT_PLAYER) {
                            if (auto unit = objPtr->ToUnit()) {
                                if (unit->IsInCombat()) {
                                    ssLabel << " [C]"; // Combat indicator
                                }
                                if (unit->IsFleeing()) {
                                    ssLabel << " [F]"; // Fleeing indicator
                                }
                            }
                        }

                        // Add distance info
                        if (has_position) {
                            if (playerPosValid) {
                                ssLabel << " | D: " << std::fixed << std::setprecision(1) << distance;
                            } else {
                                ssLabel << " | D: ?";
                            }
                        } else {
                            ssLabel << " | D: N/A";
                        }

                        std::string label = ssLabel.str();
                        if (ImGui::Selectable(label.c_str(), selected_object_guid == currentGuid)) {
                            selected_object_guid = currentGuid;
                            selected_object_list_index = displayed_index;
                        }
                        displayed_index++;
                    } catch (const std::exception& e) {
                        ImGui::TextColored(ImVec4(1,0,0,1), "Error processing object: %s", e.what());
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // Right side: Object details
            ImGui::BeginChild("ObjectDetailsPane", ImVec2(0, listHeight), true);
            ImGui::Text("Details:");
            ImGui::Separator();

            // Check if the selected GUID is valid
            if (selected_object_guid.IsValid()) {
                auto it = objectMap.find(selected_object_guid);
                if (it != objectMap.end() && it->second) {
                    auto selectedObj = it->second;
                    
                    ImGui::Text("GUID: 0x%016llX", selectedObj->GetGUID64());
                    ImGui::Text("Name: %s", selectedObj->GetName().c_str());
                    ImGui::Text("Type: %s (%d)", GetObjectTypeString(selectedObj->GetType()).c_str(), selectedObj->GetType());
                    
                    // Display position and distance
                    Vector3 pos = selectedObj->GetPosition();
                    ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);

                    // Only show distance if player pos is valid AND object type has a world position
                    WowObjectType selectedType = selectedObj->GetType();
                    if (selectedType == OBJECT_UNIT || selectedType == OBJECT_PLAYER || 
                        selectedType == OBJECT_GAMEOBJECT || selectedType == OBJECT_DYNAMICOBJECT || 
                        selectedType == OBJECT_CORPSE)
                    {
                        if(playerPosValid) {
                            float distance = playerPos.Distance(pos);
                            ImGui::Text("Distance: %.1f", distance);
                        } else {
                            ImGui::Text("Distance: ?");
                        }
                    } else {
                        ImGui::Text("Distance: N/A");
                    }
                    
                    ImGui::Text("Base Addr: 0x%p", (void*)selectedObj->GetBaseAddress());
                    
                    // Display Type-Specific Info
                    if (auto unit = selectedObj->ToUnit()) {
                        ImGui::Separator();
                        ImGui::TextUnformatted("Unit Info:");
                        ImGui::Text("Level: %d", unit->GetLevel());
                        ImGui::Text("Health: %d / %d", unit->GetHealth(), unit->GetMaxHealth());

                        // Display all available power types
                        for (uint8_t i = 0; i < PowerType::POWER_TYPE_COUNT; ++i) {
                            if (unit->HasPowerType(i)) {
                                std::string powerName = unit->GetPowerTypeString(i);
                                int currentPower = unit->GetPowerByType(i);
                                int maxPower = unit->GetMaxPowerByType(i);
                                
                                if (static_cast<PowerType>(i) == PowerType::POWER_TYPE_RAGE) {
                                    ImGui::Text("%s: %d / 100", powerName.c_str(), currentPower); // Display Rage directly, Max always 100
                                } else if (static_cast<PowerType>(i) == PowerType::POWER_TYPE_RUNIC_POWER) {
                                    // ImGui::Text("%s: %d / %d", powerName.c_str(), currentPower, maxPower); // Commented out
                                } else if (static_cast<PowerType>(i) == PowerType::POWER_TYPE_RUNE) {
                                    // ImGui::Text("%s: %d / %d", powerName.c_str(), currentPower, maxPower); // Commented out
                                } else if (maxPower > 0) { // Only display if max power > 0 for others
                                    ImGui::Text("%s: %d / %d", powerName.c_str(), currentPower, maxPower);
                                }
                            }
                        }

                        ImGui::Text("Target GUID: 0x%016llX", unit->GetTargetGUID().ToUint64());
                        ImGui::Text("Flags: 0x%X", unit->GetUnitFlags()); 
                        ImGui::Text("Flags 2: 0x%X", unit->GetUnitFlags2()); // Assuming GetUnitFlags2 exists
                        ImGui::Text("Dynamic Flags: 0x%X", unit->GetDynamicFlags()); // Assuming GetDynamicFlags exists
                        ImGui::Text("Facing: %.2f", unit->GetFacing()); // Display Facing
                        
                        // Display combat and fleeing status with colored indicators
                        bool inCombat = unit->IsInCombat();
                        bool isFleeing = unit->IsFleeing();
                        bool isLootingPlayer = false;
                        if (auto player_obj = unit->ToPlayer()) { 
                            isLootingPlayer = player_obj->IsLooting();
                        }
                        
                        ImGui::Text("Status:");
                        ImGui::SameLine();
                        
                        bool statusDisplayed = false;
                        if (inCombat) {
                            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "In Combat");
                            ImGui::SameLine();
                            statusDisplayed = true;
                        }
                        
                        if (isFleeing) {
                            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Fleeing");
                            ImGui::SameLine();
                            statusDisplayed = true;
                        }
                        
                        if (!statusDisplayed) { 
                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Normal");
                        } else {
                            ImGui::NewLine(); 
                        }
                        
                        // Get casting and channeling status (0 or 1)
                        int castingStatus = unit->IsCasting() ? 1 : 0;
                        int channelingStatus = unit->IsChanneling() ? 1 : 0;
                        ImGui::Text("Casting: %d", castingStatus);
                        ImGui::Text("Channeling: %d", channelingStatus);
                        ImGui::Text("Is Moving: %s", unit->IsMoving() ? "Yes" : "No");
                        ImGui::Text("Is Dead: %s", unit->IsDead() ? "Yes" : "No");

                        // Always display looting status for players
                        if (auto player_for_looting_display = unit->ToPlayer()) {
                            bool lootingState = player_for_looting_display->IsLooting();
                            ImGui::Text("Is Looting: %s", lootingState ? "Yes" : "No");
                        } else {
                            // Could optionally display "Is Looting: N/A" for non-players if desired
                        }

                        // --- Display Threat Information ---
                        ImGui::Separator();
                        ImGui::TextUnformatted("Threat Info (This Unit's threat ON others):");
                        WGUID highestThreatTargetGuid = unit->GetHighestThreatTargetGUID();
                        if (highestThreatTargetGuid.IsValid()) {
                            ImGui::Text("Highest Threat Target GUID: 0x%016llX", highestThreatTargetGuid.ToUint64());
                        } else {
                            ImGui::TextUnformatted("Highest Threat Target GUID: None");
                        }
                        // ImGui::Text("Threat Manager Ptr: 0x%p", (void*)unit->GetThreatManagerBasePtr());
                        // ImGui::Text("Top Threat Entry Ptr: 0x%p", (void*)unit->GetTopThreatEntryPtr());

                        const auto& threatEntries = unit->GetThreatTableEntries();
                        if (!threatEntries.empty()) {
                            ImGui::TextUnformatted("Top Threat Entry Details:");
                            for (const auto& entry : threatEntries) { // Should typically be one entry for now
                                ImGui::Text("  Target: %s (0x%016llX)", entry.targetName.c_str(), entry.targetGUID.ToUint64());
                                ImGui::Text("  Status: %u", static_cast<unsigned int>(entry.status));
                                ImGui::Text("  Percent: %u%%", static_cast<unsigned int>(entry.percentage));
                                ImGui::Text("  Raw Value: %u", entry.rawValue);
                            }
                        } else {
                            ImGui::TextUnformatted("No top threat entry found (or unit not tanking).");
                        }
                        // --- End Threat Information ---
                    }
                    
                    if (auto selPlayer = selectedObj->ToPlayer()) {
                        ImGui::TextUnformatted("Player Info:");
                        ImGui::TextDisabled("(Add class etc.)");
                    }
                    
                    if (auto go = selectedObj->ToGameObject()) {
                        ImGui::Separator();
                        ImGui::TextUnformatted("GameObject Info:");
                        ImGui::TextDisabled("(Add locked status etc.)");
                    }
                } else {
                    ImGui::Text("Selected object (GUID: 0x%016llX) no longer found.", selected_object_guid.ToUint64());
                }
            } else {
                ImGui::Text("Select an object from the list.");
            }

            ImGui::EndChild();
        } catch (const std::exception& e) {
            // Core::Log::Message(std::string("[ObjectsTabDebug] Exception in RenderObjectsTab: ") + e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Exception: %s", e.what());
        } catch (...) {
            // Core::Log::Message("[ObjectsTabDebug] Unknown exception in RenderObjectsTab");
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "An unknown exception occurred.");
        }
    }

} // namespace GUI 