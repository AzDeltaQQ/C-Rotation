#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <functional>
#include <atomic>
#include <cstdint>

// Forward declare Vector3 and Spells::IntersectFlags to avoid including full headers here
// if they are only used in function signatures exposed by hook.h.
// Ensure these are defined or fully included where the functions are implemented (hook.cpp)
struct Vector3; 
namespace Spells { enum IntersectFlags : uint32_t; }
namespace Fishing { class FishingBot; } // Forward declaration for FishingBot

// Global module handle, defined in dllmain.cpp
extern HMODULE g_hCurrentModule;

// Global shutdown flag
extern std::atomic<bool> g_isShuttingDown;

void InitializeHook(HMODULE hModule);
HRESULT APIENTRY HookedEndScene(LPDIRECT3DDEVICE9 pDevice);
void CleanupHook(bool isForceTermination = false);

// Accessor for the FishingBot instance
Fishing::FishingBot* GetFishingBotInstance();

// Function to submit tasks for execution in the next EndScene call
void SubmitToEndScene(std::function<void()> func);