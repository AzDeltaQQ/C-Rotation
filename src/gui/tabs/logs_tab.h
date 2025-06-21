// Placeholder header for logs tab
#pragma once
#include <imgui.h>
#include "../../logs/log.h"
#include <vector>
#include <string>

namespace GUI {

class LogsTab {
public:
    LogsTab() {}
    ~LogsTab() {}
    
    void Render() {
        // Button to clear the log buffer - wrap in try-catch for safety
        if (::ImGui::Button("Clear Log View")) {
            try {
                Core::Log::ClearBuffer();
            }
            catch (const std::exception&) {
                // Don't call Log::Message here as it could cause recursive issues
                // Just silently handle the error
            }
        }
        ::ImGui::SameLine();
        
        // Add copy button to copy all visible logs
        static std::string copyBuffer;
        if (::ImGui::Button("Copy All Logs")) {
            try {
                copyBuffer.clear();
                std::vector<std::string> messages = Core::Log::GetMessages();
                for (const auto& msg : messages) {
                    copyBuffer += msg + "\n";
                }
                ::ImGui::SetClipboardText(copyBuffer.c_str());
            }
            catch (const std::exception&) {
                // Handle exception silently
            }
        }
        
        ::ImGui::SameLine();

        // ADDED Checkboxes for logging options
        bool currentFileLoggingState = Core::Log::IsFileLoggingEnabled();
        if (::ImGui::Checkbox("Enable File Logging", &currentFileLoggingState)) {
            Core::Log::SetFileLoggingEnabled(currentFileLoggingState);
        }
        ::ImGui::SameLine();
        bool currentConsoleLoggingState = Core::Log::IsConsoleLoggingEnabled();
        if (::ImGui::Checkbox("Enable Console Logging", &currentConsoleLoggingState)) {
            Core::Log::SetConsoleLoggingEnabled(currentConsoleLoggingState);
        }
        // END ADDED Checkboxes

        // Add other controls like filtering later if needed
        ::ImGui::Text("Log Output:");
        
        ::ImGui::Separator();

        // Create a scrolling region for logs with selectable text
        // Set up child window to make log contents scrollable
        ::ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 0), true, 
                          ImGuiWindowFlags_HorizontalScrollbar);
                          
        // Safely get messages
        std::vector<std::string> messages;
        try {
            messages = Core::Log::GetMessages();
        }
        catch (const std::exception&) {
            // If GetMessages fails, just show a placeholder message
            messages.push_back("Error loading log messages");
        }
        
        // Display each log line individually so user can select specific lines
        for (size_t i = 0; i < messages.size(); i++) {
            // Use SelectableTextUnformatted to combine the benefits of both
            bool selected = false;
            ::ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0, 0));
            
            // Make the entire line selectable
            ::ImGui::PushID(static_cast<int>(i));
            ::ImGui::Selectable(messages[i].c_str(), &selected, ImGuiSelectableFlags_AllowItemOverlap);
            
            // Allow double-click to copy individual line
            if (::ImGui::IsItemHovered() && ::ImGui::IsMouseDoubleClicked(0)) {
                ::ImGui::SetClipboardText(messages[i].c_str());
            }
            
            ::ImGui::PopID();
            ::ImGui::PopStyleVar();
        }

        // Auto-scroll to the bottom if the user hasn't scrolled up
        if (::ImGui::GetScrollY() >= ::ImGui::GetScrollMaxY()) {
            ::ImGui::SetScrollHereY(1.0f);
        }

        ::ImGui::EndChild();
    }
};

// Keep the function for backward compatibility -- REMOVE THIS LATER IF NOT NEEDED
// void RenderLogsTab(); 

} // namespace GUI 