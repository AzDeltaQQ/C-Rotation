cmake_minimum_required(VERSION 3.10)
project(WoWDX9Hook)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Ensure compile_commands.json is generated

# Force 32-bit build
set(CMAKE_GENERATOR_PLATFORM Win32)
set(CMAKE_VS_PLATFORM_NAME "Win32")
set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x86")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add source files
set(SOURCE_FILES
    src/dllmain.cpp
    src/hook.cpp
    src/spells/SpellManager.cpp
    src/spells/auras.cpp
    src/spells/castspell.cpp
    src/spells/targeting.cpp
    src/spells/cooldowns.cpp
    src/gui/gui.cpp
    src/gui/RotationsTab.cpp
    src/gui/tabs/objects_tab.cpp
    src/gui/tabs/FishingTab.cpp
    src/logs/log.cpp
    src/objectManager/objectManager.cpp
    src/lua/lua_interface.cpp
    src/rotations/RotationEngine.cpp
    src/rotations/RotationParser.cpp
    src/rotations/RotationLifecycle.cpp
    src/rotations/RotationSettings.cpp
    src/rotations/RotationExecution.cpp
    src/rotations/RotationPriority.cpp
    src/rotations/RotationConditions.cpp
    src/types/wowobject.cpp
    src/types/wowplayer.cpp
    src/types/wowunit.cpp
    src/types/wowgameobject.cpp
    src/utils/memory.cpp
    src/fishing/FishingBot.cpp
    src/game_state/GameStateManager.cpp
)

# ImGui source files
set(IMGUI_FILES
    dependencies/ImGui/imgui.cpp
    dependencies/ImGui/imgui_demo.cpp
    dependencies/ImGui/imgui_draw.cpp
    dependencies/ImGui/imgui_tables.cpp
    dependencies/ImGui/imgui_widgets.cpp
    dependencies/ImGui/backends/imgui_impl_dx9.cpp
    dependencies/ImGui/backends/imgui_impl_win32.cpp
)

# Create shared library
add_library(WoWDX9Hook SHARED ${SOURCE_FILES} ${IMGUI_FILES})

# MinHook
add_library(minhook STATIC
    dependencies/MinHook/src/buffer.c
    dependencies/MinHook/src/hook.c
    dependencies/MinHook/src/trampoline.c
    dependencies/MinHook/src/hde/hde32.c
)

target_include_directories(minhook PUBLIC dependencies/MinHook/include)

# Include directories
target_include_directories(WoWDX9Hook PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spells
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies # Ensure dependencies base is included
    dependencies/ImGui                       # For imgui.h, imconfig.h
    dependencies/ImGui/backends            # For imgui_impl_*.h
    dependencies/MinHook/include
    dependencies/json-develop/include      # Corrected path to json include dir
    "$ENV{DXSDK_DIR}/Include"
)

# Link directories
target_link_directories(WoWDX9Hook PRIVATE
    "$ENV{DXSDK_DIR}/Lib/x86"
)

# Link libraries
target_link_libraries(WoWDX9Hook PRIVATE
    minhook
    d3d9
    d3dx9
)

# Add the RotationCreator subdirectory
add_subdirectory(RotationCreator) 