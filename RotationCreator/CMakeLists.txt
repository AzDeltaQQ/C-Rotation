cmake_minimum_required(VERSION 3.10)
project(RotationCreator)

# Force 32-bit build (optional, but consistent with hook)
set(CMAKE_GENERATOR_PLATFORM Win32)
set(CMAKE_VS_PLATFORM_NAME "Win32")
set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x86")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Define source files for the creator
set(CREATOR_SOURCE_FILES
    src/main.cpp
    # Add other .cpp files for the creator here later
    # ../src/gui/RotationsTab.cpp       # Removed - Belongs to DLL
    # ../src/rotations/RotationEngine.cpp # Removed - Belongs to DLL
    # ../src/spells/castspell.cpp     # Removed - Belongs to DLL
    # ../src/spells/targeting.cpp     # Removed - Belongs to DLL
    # ../src/spells/cooldowns.cpp     # Removed - Belongs to DLL
)

# Define ImGui source files relative to the main project's dependencies
# Adjust relative path if your structure differs
set(IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/ImGui")
set(IMGUI_FILES
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

# Create the executable for Windows GUI (not console)
add_executable(RotationCreator WIN32 ${CREATOR_SOURCE_FILES} ${IMGUI_FILES})

# Include directories
target_include_directories(RotationCreator PRIVATE
    src # Local source files for RotationCreator
    ../src # Main project sources (for logs, types, etc.)
    "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/ImGui" # ImGui headers
    "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/ImGui/backends" # ImGui backend headers
    "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/json-develop/single_include" # nlohmann/json header
    "$ENV{DXSDK_DIR}/Include" # DirectX SDK Includes
)

# Link directories
target_link_directories(RotationCreator PRIVATE
    "$ENV{DXSDK_DIR}/Lib/x86" # DirectX SDK Libs (x86)
)

# Link libraries
target_link_libraries(RotationCreator PRIVATE
    d3d9        # Direct3D 9 library
    # user32      # Needed for windowing functions (usually linked by default)
    # gdi32       # Needed for some windowing aspects (usually linked by default)
) 