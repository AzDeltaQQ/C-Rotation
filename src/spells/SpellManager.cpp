#include "SpellManager.h"
#include "utils/memory.h" // Use the updated memory header
#include "logs/log.h"       // Use the project's logging system

#include <windows.h>
#include <vector>
#include <string>
#include <sstream>   // Include for LogStream equivalent
#include <cstring>   // For memcpy

namespace {
    // Define the function pointer type for get_spell_cooldown_proxy (0x809000)
    typedef bool(__cdecl* GetSpellCooldownProxyFn)(int spellId, int playerOrPetFlag, int* ptr_remainingDuration, int* ptr_startTime, DWORD* ptr_isActive);
    const uintptr_t GET_SPELL_COOLDOWN_PROXY_ADDR = 0x809000;

    // Helper to apply a single patch (adapted from WoWBot SpellManager)
    bool ApplyPatch(uintptr_t address, const std::vector<unsigned char>& patchBytes) {
        size_t patchSize = patchBytes.size();
        DWORD oldProtect;

        // 1. Change memory protection
        if (!VirtualProtect((LPVOID)address, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            std::stringstream ssErr;
            ssErr << "Error: Failed to change memory protection for patch at 0x" << std::hex << address << ". Error code: " << GetLastError();
            Core::Log::Message(ssErr.str());
            return false;
        }

        // 2. Write the patch using Memory::Write for SEH protection
        // Note: This writes byte-by-byte which is less efficient but works with the current Write<T>
        // A Memory::WriteBytes function would be better here.
        bool write_success = true;
        try {
             for (size_t i = 0; i < patchSize; ++i) {
                Memory::Write<unsigned char>(address + i, patchBytes[i]);
             }
             // Alternatively, using memcpy within SEH:
             // __try { memcpy((void*)address, patchBytes.data(), patchSize); } 
             // __except (EXCEPTION_EXECUTE_HANDLER) { write_success = false; }
        } catch (const MemoryAccessError& e) {
             std::stringstream ssErr;
             ssErr << "Error: Exception during patch write at 0x" << std::hex << address << ": " << e.what();
             Core::Log::Message(ssErr.str());
             write_success = false;
        }

        // 3. Restore original protection
        DWORD temp; // VirtualProtect requires a non-null pointer for the old protection value
        VirtualProtect((LPVOID)address, patchSize, oldProtect, &temp);

        if (!write_success) {
            // Don't log success if writing failed.
             return false;
        }

        std::stringstream ssSuccess;
        ssSuccess << "Successfully applied " << patchSize << "-byte patch at 0x" << std::hex << address;
        Core::Log::Message(ssSuccess.str());
        return true;
    }

    // Internal cooldown function (adapted from WoWBot SpellManager)
    int GetSpellCooldownInternal(int spellId, int playerOrPetFlag) {
        GetSpellCooldownProxyFn get_spell_cooldown_proxy = reinterpret_cast<GetSpellCooldownProxyFn>(GET_SPELL_COOLDOWN_PROXY_ADDR);

        int remainingDuration = 0;
        int startTime = 0;
        DWORD isActive = 0; // Using DWORD as per analysis

        // Remove SEH - Direct call is inherently risky if address is bad
        // but SEH conflicts with C++ exceptions/objects.
        // Ensure GET_SPELL_COOLDOWN_PROXY_ADDR (0x809000) is correct for the game version.
        try { // Keep standard C++ try/catch for other potential issues
            bool isOnCooldown = get_spell_cooldown_proxy(
                spellId,
                playerOrPetFlag,
                &remainingDuration,
                &startTime,
                &isActive);
            
            if (isOnCooldown) {
                // Ensure duration is non-negative
                return (remainingDuration > 0) ? remainingDuration : 0;
            } else {
                return 0; // Spell is ready
            }
        } catch (...) {
            // Catch other potential C++ exceptions 
             std::stringstream ssErr;
             ssErr << "SpellManager: C++ Exception calling get_spell_cooldown_proxy at address 0x"
                   << std::hex << GET_SPELL_COOLDOWN_PROXY_ADDR;
             Core::Log::Message(ssErr.str());
             return -1; // Indicate an error
        }
    }

} // Anonymous namespace


// --- Public Static Methods --- 

// Address for handlePlayerSpellCastCompletion
const uintptr_t HANDLE_PLAYER_SPELL_CAST_COMPLETION_ADDR = 0x809AC0;
// Define the function pointer type for the handler.
// It takes a void* argument, which might be a this pointer or unused.
typedef void(__cdecl* PlayerStopCastingFn)(void*);

void SpellManager::StopCasting() {
    if (!HANDLE_PLAYER_SPELL_CAST_COMPLETION_ADDR) {
        Core::Log::Message("SpellManager::StopCasting: Address (HANDLE_PLAYER_SPELL_CAST_COMPLETION_ADDR) is 0.");
        return;
    }

    PlayerStopCastingFn stopCastingFunc = reinterpret_cast<PlayerStopCastingFn>(HANDLE_PLAYER_SPELL_CAST_COMPLETION_ADDR);
    
    std::stringstream ss;
    ss << "SpellManager: Attempting to call handlePlayerSpellCastCompletion(nullptr) at 0x" << std::hex << HANDLE_PLAYER_SPELL_CAST_COMPLETION_ADDR;
    Core::Log::Message(ss.str());

    try {
        stopCastingFunc(nullptr); // Call the function with nullptr for the void* argument
        Core::Log::Message("SpellManager::StopCasting: Called handlePlayerSpellCastCompletion(nullptr) successfully (no immediate crash).");
    } catch (const std::exception& e) {
        std::stringstream ssErr;
        ssErr << "SpellManager::StopCasting: C++ Exception calling handler - " << e.what();
        Core::Log::Message(ssErr.str());
    } catch (...) {
        // This catch block is for SEH if compiled with /EHa
        // or for other non-standard C++ exceptions if SEH is not used for this.
        // For direct memory calls, SEH is often needed to catch access violations.
        // Without SEH, a crash here is likely if the call is bad.
        Core::Log::Message("SpellManager::StopCasting: Unknown exception / SEH event calling handler.");
    }
}

int SpellManager::GetSpellCooldownMs(int spellId) {
    return GetSpellCooldownInternal(spellId, 0); // 0 for player
}

int SpellManager::GetPetSpellCooldownMs(int spellId) {
    return GetSpellCooldownInternal(spellId, 1); // 1 for pet
}

void SpellManager::PatchCooldownBug_Final() {
    Core::Log::Message("Applying final cooldown display patches...");
    bool success = true;

    // --- GCD Block Patches (Addresses: 0x807BD4, 0x807BD7, 0x807BDB) --- 
    Core::Log::Message("Applying GCD block patches...");
    success &= ApplyPatch(0x807BD4, {0x8B, 0x45, 0x10}); // mov eax,[ebp+10h]
    success &= ApplyPatch(0x807BD7, {0x85, 0xC0});       // test eax,eax
    success &= ApplyPatch(0x807BDB, {0x89, 0x10});       // mov [eax],edx 

    // --- Category Block Patches (Addresses: 0x807B84, 0x807B87, 0x807B8B) ---
    Core::Log::Message("Applying Category block patches...");
    success &= ApplyPatch(0x807B84, {0x8B, 0x45, 0x10}); // mov eax,[ebp+10h]
    success &= ApplyPatch(0x807B87, {0x85, 0xC0});       // test eax,eax
    success &= ApplyPatch(0x807B8B, {0x89, 0x10});       // mov [eax],edx 

    if (success) {
        Core::Log::Message("All cooldown display patches applied successfully.");
    } else {
        Core::Log::Message("Error: One or more cooldown display patches failed. Check logs.");
    }
} 