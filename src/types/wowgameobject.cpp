#include "wowgameobject.h"
#include "../utils/memory.h" // Use relative path
#include "../logs/log.h"     // Use relative path
#include "wowobject.h"

// Placeholder for the actual offset of the bobbing flag.
// This is based on BitFish for 3.3.5a, VERIFY for your game version.
const uintptr_t BOBBING_FLAG_OFFSET = 0xBC; 

WowGameObject::WowGameObject(uintptr_t baseAddress, WGUID guid)
    : WowObject(baseAddress, guid, OBJECT_GAMEOBJECT)
{
    // UpdateDynamicData(); // Initial update on construction
}

// Override to read GameObject-specific data
void WowGameObject::UpdateDynamicData() {
    if (m_baseAddress == 0) {
        return; 
    }

    // No call to base class UpdateDynamicData needed here

    try {
        // --- Read GameObject Position --- 
        // Offsets need verification!
        m_cachedPosition.x = Memory::Read<float>(m_baseAddress + Offsets::GO_RAW_POS_X);
        m_cachedPosition.y = Memory::Read<float>(m_baseAddress + Offsets::GO_RAW_POS_Y);
        m_cachedPosition.z = Memory::Read<float>(m_baseAddress + Offsets::GO_RAW_POS_Z);

        // --- Read Name (Using Base Class VTable Method) --- 
        m_cachedName = ReadNameFromVTable(); 

        m_lastCacheUpdateTime = std::chrono::steady_clock::now();
    } 
    catch (const MemoryAccessError& e) {
        (void)e; // Mark as unused
        Core::Log::Message("[WowGameObject::UpdateDynamicData] Memory Read Exception for GUID 0x" + std::to_string(GetGUID64()));
        // Invalidate position on error
        m_cachedPosition = Vector3();
        m_cachedName = "[Read Error GO]"; 
        // Optionally log error
        // std::stringstream ss; ss << "MemoryAccessError reading GO data for 0x" << std::hex << m_baseAddress << ": " << e.what();
        // Core::Log::Message(ss.str());
    } catch (...) {
        // Catch any other potential errors
        m_cachedPosition = {0.0f, 0.0f, 0.0f};
        m_cachedName = "[Unknown Error GO]";
    }
} 

bool WowGameObject::IsBobbing() const {
    if (!m_baseAddress) {
        // Core::Log::Error("[WowGameObject::IsBobbing] Base address is null.");
        return false;
    }
    // Assumes WowObject (base) has a ReadMemory method like:
    // template<typename T> T ReadMemory(uintptr_t offset) const;
    // which reads from m_baseAddress + offset.
    // If ReadMemory is part of a global memory utility, adjust accordingly.
    
    // Read the byte flag that indicates bobbing status.
    // Typically, 0 = not bobbing, 1 = bobbing/fish on hook.
    uint8_t bobbingFlag = Memory::Read<uint8_t>(m_baseAddress + BOBBING_FLAG_OFFSET); 

    return bobbingFlag == 1;
}

// Implement other WowGameObject-specific methods here
// bool WowGameObject::IsLocked() { /* ... read lock state ... */ return false; }
// bool WowGameObject::CanHarvest() { /* ... read harvest state ... */ return false; } 