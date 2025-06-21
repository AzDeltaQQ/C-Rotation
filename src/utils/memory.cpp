// This file can be left empty as all functions in memory.h are inline templates,
// unless non-template helper functions are added to the Memory namespace later. 

#include <Windows.h> // Needed for GetModuleHandle
#include <TlHelp32.h> // For Module32First/Next if needed for GetBaseAddress

// REMOVED: Define the offset for the world loaded flag (Needs verification for specific WoW version!)
// constexpr uintptr_t WORLD_LOADED_FLAG_OFFSET = 0x00C79AF4; 

namespace Memory {

} 