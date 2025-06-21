# C-Rotation - World of Warcraft 3.3.5 Rotation System

A comprehensive rotation and automation system for World of Warcraft 3.3.5 (Wrath of the Lich King), featuring advanced spell casting, targeting, and bot capabilities.

## Features

### Core Systems
- **Advanced Rotation Engine**: Intelligent spell casting with priority-based rotation execution
- **Smart Targeting**: Automatic target selection with line-of-sight checking and threat management
- **Spell Management**: Cooldown tracking, charge-based spell support, and resource management
- **Aura System**: Complete buff/debuff tracking and condition checking
- **Fishing Bot**: Automated fishing with bobber detection and interaction

### User Interface
- **ImGui-based GUI**: Modern, responsive interface for configuration and monitoring
- **Rotation Creator**: Standalone tool for creating and editing custom rotations
- **Real-time Monitoring**: Live status displays and logging system
- **Hot-key Support**: Configurable key bindings for quick control

### Advanced Features
- **Game State Management**: Monitors loading screens, world state, and player status
- **Object Manager**: Efficient tracking of all game objects with caching
- **Memory Safety**: Protected memory operations with error handling
- **Multi-threading**: Non-blocking operations for smooth gameplay

## Architecture

```
├── src/
│   ├── dllmain.cpp           # DLL entry point and initialization
│   ├── hook.cpp/h            # DirectX hooking and rendering
│   ├── fishing/              # Automated fishing system
│   ├── game_state/           # Game state monitoring
│   ├── gui/                  # User interface components
│   ├── logs/                 # Logging system
│   ├── lua/                  # Lua integration
│   ├── objectManager/        # Game object tracking
│   ├── rotations/            # Rotation engine and execution
│   ├── spells/               # Spell casting and management
│   ├── types/                # Game object definitions
│   └── utils/                # Utility functions
├── RotationCreator/          # Standalone rotation editor
└── dependencies/             # External libraries (not included)
```

## Dependencies

This project requires the following external libraries (not included in repository):

- **ImGui**: Immediate mode GUI library
- **nlohmann/json**: JSON parsing and serialization
- **MinHook**: Windows API hooking library
- **DirectX 9 SDK**: DirectX development headers and libraries

## Building

### Prerequisites
- Visual Studio 2019 or later
- CMake 3.10 or higher
- DirectX 9 SDK installed
- Windows 10 SDK

### Environment Setup
Set the `DXSDK_DIR` environment variable to your DirectX SDK installation path.

### Build Steps
```bash
# Clone the repository
git clone https://github.com/AzDeltaQQ/C-Rotation.git
cd C-Rotation

# Create build directory
mkdir build
cd build

# Generate project files
cmake .. -G "Visual Studio 16 2019" -A Win32

# Build the project
cmake --build . --config Release
```

### Build Targets
- **WoWDX9Hook**: Main DLL injection library
- **RotationCreator**: Standalone rotation editor application

## Installation

1. Build the project following the steps above
2. Injct `WoWDX9Hook.dll` to your WoW 
3.  `RotationCreator.exe` to create rotations. external.

## Usage

### Basic Operation
1. Inject the DLL into WoW 3.3.5
2. Press the configured hotkey to open the GUI (default: Insert)
3. Navigate to the Rotations tab to load or create rotations
4. Enable the rotation engine and configure targeting options

### Creating Rotations
1. Run `RotationCreator.exe` to open the rotation editor
2. Create a new rotation or load an existing one
3. Configure spell conditions, priorities, and targeting
4. Save the rotation to the `rotations/` directory
5. Load the rotation in-game through the main interface

### Fishing Bot
1. Navigate to the Fishing tab in the main interface
2. Configure your fishing spell ID
3. Position your character near water
4. Enable the fishing bot

## Configuration

### Rotation Files
Rotations are stored as JSON files in the `rotations/` directory. Each rotation contains:
- Spell definitions with IDs and targeting information
- Conditional logic for spell casting
- Priority systems for optimal spell selection
- Resource management settings

### Settings
- **Targeting Options**: Enemy/friendly targeting, range checks, line-of-sight
- **Combat Modes**: Single target, AoE, tanking, battleground modes
- **Safety Features**: Combat-only casting, target validation
- **Hotkeys**: Customizable key bindings for all major functions


## Disclaimer

This software is for educational purposes only. Use at your own risk. The authors are not responsible for any consequences resulting from the use of this software, including but not limited to account suspensions or bans.

## License

This project is provided as-is without any warranty. See the repository for license details. 