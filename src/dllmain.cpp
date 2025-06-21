#include <Windows.h>
#include "hook.h"
#include "logs/log.h" 
#include <string> // For std::string conversion
#include <vector> // For path buffer
#include <filesystem> // For path manipulation

// Define g_hCurrentModule globally
HMODULE g_hCurrentModule = NULL;

// Remove forward declaration
// extern "C" __declspec(dllexport) void RequestCleanup(); 

// Ensure cleanup happens
bool g_cleanupCalled = false;
HANDLE g_cleanupThread = NULL;

// Cleanup thread function to handle safe detachment
DWORD WINAPI CleanupThreadProc(LPVOID lpParam) {
    try {
        OutputDebugStringA("CleanupThread: Starting cleanup on separate thread\n");
        // Call CleanupHook with false (not a forced termination)
        CleanupHook(false);
        OutputDebugStringA("CleanupThread: Cleanup completed successfully\n");
    }
    catch (...) {
        OutputDebugStringA("CleanupThread: Unhandled exception during cleanup\n");
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            // Assign the module handle to our global variable
            g_hCurrentModule = hModule;

            // --- Initialize Logging FIRST --- 
            // (Log initialization needs to stay here, as it should be quick and non-blocking)
            std::vector<char> dllPathBuf(MAX_PATH);
            GetModuleFileNameA(hModule, dllPathBuf.data(), static_cast<DWORD>(dllPathBuf.size()));
            std::filesystem::path dllPath = dllPathBuf.data();
            std::filesystem::path logDirPath = dllPath.parent_path() / "logs"; 
            std::filesystem::path logFilePath = logDirPath / "WoWDX9Hook.log";
            Core::Log::Initialize(logDirPath.string(), logFilePath.filename().string()); 
            
            Core::Log::Message("DllMain: DLL_PROCESS_ATTACH - Creating InitializeHook thread..."); 
            DisableThreadLibraryCalls(hModule);
            
            // Mark cleanup as not called yet
            g_cleanupCalled = false;
            g_cleanupThread = NULL;
            
            // Initialize hooks in a separate thread to avoid DllMain issues
            // Pass hModule to the thread start routine, matching the updated InitializeHook signature
            HANDLE hookThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)InitializeHook, hModule, 0, nullptr);
            if (hookThread) {
                CloseHandle(hookThread);
            } else {
                Core::Log::Message("DllMain: ERROR - Failed to create InitializeHook thread!");
            }
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            // Prevent double-cleanup and handle forced process termination
            if (!g_cleanupCalled) {
                OutputDebugStringA("DllMain: DLL_PROCESS_DETACH Received\n");
                g_cleanupCalled = true;
                
                // Check if this is a forced termination - in that case, we need to be very quick
                if (lpReserved != NULL) { // Non-NULL indicates process termination
                    OutputDebugStringA("DllMain: Process termination detected, performing minimal cleanup\n");
                    // Just do minimal cleanup directly - the process is about to die anyway
                    // Since the process is terminating, we don't want to create new threads
                    // Pass true to indicate this is a forced termination
                    CleanupHook(true);
                } else {
                    // Normal DLL unload, use a thread with timeout
                    OutputDebugStringA("DllMain: Normal DLL unload, creating cleanup thread\n");
                    g_cleanupThread = CreateThread(NULL, 0, CleanupThreadProc, NULL, 0, NULL);
                    
                    if (g_cleanupThread) {
                        // Wait for cleanup with timeout (3 seconds max)
                        OutputDebugStringA("DllMain: Waiting for cleanup thread to complete (max 3 seconds)\n");
                        WaitForSingleObject(g_cleanupThread, 3000);
                        CloseHandle(g_cleanupThread);
                        OutputDebugStringA("DllMain: Cleanup thread completed or timed out\n");
                    } else {
                        OutputDebugStringA("DllMain: Failed to create cleanup thread, doing direct cleanup\n");
                        // Call CleanupHook with false (not a forced termination)
                        CleanupHook(false);
                    }
                }
            } else {
                OutputDebugStringA("DllMain: Cleanup already called, skipping\n");
            }
            break;
        }
        // We generally don't need thread attach/detach notifications
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
} 