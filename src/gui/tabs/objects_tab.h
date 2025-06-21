// Placeholder header for objects tab
#pragma once
#include <imgui.h>
#include "../../types/types.h"
#include <string>

namespace GUI {

    namespace {
        // Selection state
        WGUID selected_object_guid = {};
        int selected_object_list_index = -1;

        // Filter state
        static float filter_max_distance = 100.0f;
        static bool filter_show_items = false;
        static bool filter_show_containers = false;
        static bool filter_show_units = true;
        static bool filter_show_players = true;
        static bool filter_show_gameobjects = true;
        static bool filter_show_dynamicobjects = true;
        static bool filter_show_corpses = true;
        static bool filter_show_other = false;

        // Helper to get object type as string
        std::string GetObjectTypeString(WowObjectType type) {
            switch (type) {
                case OBJECT_ITEM: return "Item";
                case OBJECT_CONTAINER: return "Container";
                case OBJECT_UNIT: return "Unit";
                case OBJECT_PLAYER: return "Player";
                case OBJECT_GAMEOBJECT: return "GameObject";
                case OBJECT_DYNAMICOBJECT: return "DynamicObject";
                case OBJECT_CORPSE: return "Corpse";
                case OBJECT_NONE: return "None";
                default: 
                    std::string unknown = "Unknown (" + std::to_string(type) + ")";
                    return unknown;
            }
        }
    } // anonymous namespace

class ObjectsTab {
public:
    ObjectsTab() {}
    ~ObjectsTab() {}
    
    // Only declare the Render method here, implementation is in the .cpp file
    void Render();
};

// For backward compatibility - will be replaced by the class-based approach
// void RenderObjectsTab(); 

} // namespace GUI