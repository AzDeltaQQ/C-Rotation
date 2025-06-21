#include "FishingBot.h"
#include "../logs/log.h"
#include "../objectManager/ObjectManager.h"
#include "../spells/castspell.h"      // For Spells::CastSpell
#include "../spells/cooldowns.h"    // For CooldownManager
#include "../types/WowPlayer.h"       // For WowPlayer
#include "../types/WowGameObject.h" // For WowGameObject
#include "../types/wowobject.h"     // For WowObject, needed for GetObjectMap iteration
#include "../types/types.h"    // For Vector3 and WGUID (instead of WowVector3.h)
#include "../types/wowobject.h"     // For WowObject, needed for GetObjectMap iteration
#include "../types/types.h"         // For Vector3 and WGUID
#include "../hook.h"                // For SubmitToEndScene
#include <chrono>                  // For std::chrono::milliseconds
#include <random>                  // For random delays, std::mt19937, std::random_device
#include <sstream>                 // For std::stringstream in logging
#include <iomanip>                 // For std::hex / std::setfill / std::setw

namespace Fishing {

// Helper function to convert WowObjectType to string for logging
std::string objTypeToString(WowObjectType type) {
    switch (type) {
        case OBJECT_NONE: return "NONE";
        case OBJECT_ITEM: return "ITEM";
        case OBJECT_CONTAINER: return "CONTAINER";
        case OBJECT_UNIT: return "UNIT";
        case OBJECT_PLAYER: return "PLAYER";
        case OBJECT_GAMEOBJECT: return "GAMEOBJECT";
        case OBJECT_DYNAMICOBJECT: return "DYNAMICOBJECT";
        case OBJECT_CORPSE: return "CORPSE";
        default: return "UNKNOWN_TYPE_" + std::to_string(static_cast<int>(type));
    }
}

// Helper for logging with formatting
void LogFishingMessage(const std::string& message) {
    Core::Log::Message(message); 
}

FishingBot::FishingBot(ObjectManager& objMgr, Spells::CooldownManager& cdMgr)
    : m_objectManager(objMgr), m_cooldownManager(cdMgr), m_gen(m_rd()) {
    LogFishingMessage("[FishingBot] Initialized.");
}

FishingBot::~FishingBot() {
    Stop(); 
}

void FishingBot::Start() {
    if (m_isRunning.load()) {
        LogFishingMessage("[FishingBot] Already running.");
        return;
    }
    LogFishingMessage("[FishingBot] Starting...");
    m_stopRequested.store(false);
    m_isRunning.store(true);
    m_fishingThread = std::thread(&FishingBot::RunFishingLoop, this);
}

void FishingBot::Stop() {
    if (!m_isRunning.load() && !m_fishingThread.joinable()) {
        return;
    }
    LogFishingMessage("[FishingBot] Stopping...");
    m_stopRequested.store(true);
    if (m_fishingThread.joinable()) {
        m_fishingThread.join();
    }
    m_isRunning.store(false);
    LogFishingMessage("[FishingBot] Stopped.");
}

// Implement the SetFishingSpellID method
void FishingBot::SetFishingSpellID(uint32_t spellID) {
    if (spellID == 0) {
        LogFishingMessage("[FishingBot] WARNING: Attempted to set invalid spell ID (0). Using default instead.");
        m_fishingSpellID.store(DEFAULT_FISHING_SPELL_ID);
    } else {
        m_fishingSpellID.store(spellID);
        LogFishingMessage("[FishingBot] Fishing spell ID changed to: " + std::to_string(spellID));
    }
}

void FishingBot::RunFishingLoop() {
    std::uniform_int_distribution<> cast_delay_dist(1500, 3000);
    std::uniform_int_distribution<> bobber_monitor_delay_dist(500, 1500);
    std::uniform_int_distribution<> post_loot_delay_dist(1000, 2500);
    std::uniform_int_distribution<> short_generic_delay_dist(250, 750);

    LogFishingMessage("[FishingBot] Fishing loop started.");
    while (!m_stopRequested.load()) {
        if (!CastFishingSpell()) {
            LogFishingMessage("[FishingBot] Failed to cast fishing or on cooldown, waiting...");
            std::this_thread::sleep_for(std::chrono::milliseconds(2000 + short_generic_delay_dist(m_gen)));
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(cast_delay_dist(m_gen)));
        if (m_stopRequested.load()) break;

        WowGameObject* bobber_ptr = FindActiveBobber();
        if (!bobber_ptr) {
            LogFishingMessage("[FishingBot] No bobber found. Recasting.");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 + short_generic_delay_dist(m_gen)));
            continue;
        }
        
        uint64_t currentBobberGuid = bobber_ptr->GetGUID64(); 
        {
            std::stringstream ss;
            ss << "[FishingBot] Bobber found: GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << currentBobberGuid;
            LogFishingMessage(ss.str());
        }

        if (m_stopRequested.load()) break;

        if (MonitorBobber(currentBobberGuid)) { // Pass GUID
            {
                std::stringstream ss;
                ss << "[FishingBot] Bite detected! Interacting with bobber 0x" << std::hex << std::setw(16) << std::setfill('0') << currentBobberGuid;
                LogFishingMessage(ss.str());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(bobber_monitor_delay_dist(m_gen) / 2));
            if (m_stopRequested.load()) break;
            
            InteractWithBobber(currentBobberGuid); // Pass GUID. m_lastBobberInteractedGuid is set inside.
                       
            std::this_thread::sleep_for(std::chrono::milliseconds(post_loot_delay_dist(m_gen))); 
        } else {
            if (!m_stopRequested.load()) { 
                std::stringstream ss;
                ss << "[FishingBot] No bite or bobber (GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << currentBobberGuid << ") timed out/disappeared.";
                LogFishingMessage(ss.str());
                // If MonitorBobber returns false, it means the bobber might have disappeared or timed out.
                // We should mark it as interacted to prevent FindActiveBobber from immediately picking it up if it briefly reappears.
                m_lastBobberInteractedGuid = currentBobberGuid; 
            }
        }
        
        if (m_stopRequested.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(short_generic_delay_dist(m_gen))); 
    }
    LogFishingMessage("[FishingBot] Fishing loop finished.");
}

bool FishingBot::CastFishingSpell() {
    // Get the current spell ID from the atomic
    uint32_t currentSpellId = m_fishingSpellID.load();
    
    if (m_cooldownManager.IsSpellOnCooldown(currentSpellId)) {
        LogFishingMessage("[FishingBot] Fishing spell is on cooldown.");
        // Sleep a bit if on cooldown before checking again or trying other logic
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + (m_gen() % 200)));
        return false;
    }

    LogFishingMessage("[FishingBot] Casting Fishing (Spell ID: " + std::to_string(currentSpellId) + ") via EndScene...");
    
    // Capture the current spell ID for use in the lambda
    uint32_t capturedSpellId = currentSpellId;
    
    SubmitToEndScene([this, capturedSpellId]() {
        // Fishing spell doesn't require a target, so pass false and 0 for GUID.
        bool castSuccess = Spells::CastSpell(capturedSpellId, 0, false);
        // Note: Logging success/failure from here might be tricky due to thread context.
        // Consider using a shared flag or event if immediate feedback to FishingBot's thread is needed.
        if (!castSuccess) {
            // This log will appear from the main thread (EndScene context)
            Core::Log::Message("[FishingBot-EndScene] Spells::CastSpell(" + std::to_string(capturedSpellId) + ") reported failure.");
        }
    });

    // Wait for a short duration to allow the cast to be processed and bobber to appear.
    // This is an optimistic wait. More robust solutions might involve game event checking.
    std::this_thread::sleep_for(std::chrono::milliseconds(1500 + (m_gen() % 500))); // Increased delay for cast + bobber appearance
    return true; // Assume cast was queued successfully.
}

WowGameObject* FishingBot::FindActiveBobber() {
    std::shared_ptr<WowPlayer> player = m_objectManager.GetLocalPlayer();
    if (!player) {
        LogFishingMessage("[FishingBot] Player object not found.");
        return nullptr;
    }
    Vector3 playerPos = player->GetPosition(); 

    WowGameObject* closest_bobber = nullptr;
    float min_dist_sq = 10000.0f; 
    const float max_fishing_dist_sq = 30.0f * 30.0f; 

    const auto objectMap = m_objectManager.GetAllObjects(); 
    // LogFishingMessage("[FishingBot::FindActiveBobber] Total objects in OM: " + std::to_string(objectMap.size())); // Can be verbose

    for (const auto& pair : objectMap) {
        if (m_stopRequested.load()) return nullptr;
        
        std::shared_ptr<WowObject> obj_ptr = pair.second;
        if (!obj_ptr) continue;

        if (obj_ptr->GetType() == OBJECT_GAMEOBJECT) {
            WowGameObject* gameObject = dynamic_cast<WowGameObject*>(obj_ptr.get());
            if (!gameObject) continue;

            if (gameObject->GetName() == m_fishingBobberName && gameObject->GetGUID64() != m_lastBobberInteractedGuid) {
                Vector3 bobberPos = gameObject->GetPosition(); 
                
                float dx = playerPos.x - bobberPos.x; 
                float dy = playerPos.y - bobberPos.y;
                float dz = playerPos.z - bobberPos.z; 
                float distance_sq = dx*dx + dy*dy + dz*dz;

                if (distance_sq < min_dist_sq && distance_sq < max_fishing_dist_sq) { 
                    min_dist_sq = distance_sq;
                    closest_bobber = gameObject;
                }
            }
        }
    }
    return closest_bobber;
}

bool FishingBot::MonitorBobber(uint64_t bobberGuid) { // Takes GUID
    if (bobberGuid == 0) { // Check for invalid GUID
        LogFishingMessage("[FishingBot::MonitorBobber] Received invalid (zero) bobber GUID.");
        return false;
    }
    {
        std::stringstream ss;
        ss << "[FishingBot] Monitoring bobber GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << bobberGuid << " for a bite...";
        LogFishingMessage(ss.str());
    }
    
    std::uniform_int_distribution<> bite_time_dist(5000, 20000); 
    int time_to_wait_ms = bite_time_dist(m_gen);
    int elapsed_ms = 0;
    const int check_interval_ms = 200;

    while(elapsed_ms < time_to_wait_ms) {
        if (m_stopRequested.load()) return false; 

        std::shared_ptr<WowObject> obj = m_objectManager.GetObjectByGUID(bobberGuid);
        WowGameObject* currentBobberState = obj ? dynamic_cast<WowGameObject*>(obj.get()) : nullptr;
        
        if (!currentBobberState) { 
            LogFishingMessage("[FishingBot] Bobber disappeared while monitoring (GUID: " + std::to_string(bobberGuid) + ")."); 
            return false; 
        }
        
        if (currentBobberState->IsBobbing()) { 
            std::stringstream ss;
            ss << "[FishingBot] Bite detected! Bobber GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << bobberGuid;
            LogFishingMessage(ss.str());
            return true; 
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
        elapsed_ms += check_interval_ms;
    }
    {
        std::stringstream ss;
        ss << "[FishingBot] Bobber GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << bobberGuid << " timed out without a bite.";
        LogFishingMessage(ss.str());
    }
    return false; 
}

bool FishingBot::InteractWithBobber(uint64_t bobberGuid) { // Takes GUID
    if (bobberGuid == 0) { // Check for invalid GUID
        LogFishingMessage("[FishingBot::InteractWithBobber] Received invalid (zero) bobber GUID.");
        return false;
    }
    
    {
        std::stringstream ss;
        ss << "[FishingBot] Attempting to interact with bobber GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << bobberGuid << " via EndScene...";
        LogFishingMessage(ss.str());
    }

    uint64_t capturedGuid = bobberGuid; // Ensure GUID is captured for lambda
    
    SubmitToEndScene([this, capturedGuid]() {
        std::shared_ptr<WowObject> mainThreadObj = m_objectManager.GetObjectByGUID(capturedGuid);
        WowGameObject* bobberFromMainThread = mainThreadObj ? dynamic_cast<WowGameObject*>(mainThreadObj.get()) : nullptr;
        
        if (bobberFromMainThread) {
            std::stringstream pre_ss;
            pre_ss << "[FishingBot-EndScene] VTable Interact on bobber GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << capturedGuid;
            Core::Log::Message(pre_ss.str());
            
            bobberFromMainThread->Interact();
            
            std::stringstream ss;
            ss << "[FishingBot-EndScene] Called Interact() on bobber GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << capturedGuid;
            Core::Log::Message(ss.str());
        } else {
            std::stringstream err_ss;
            err_ss << "[FishingBot-EndScene] Bobber GUID 0x" << std::hex << std::setw(16) << std::setfill('0') << capturedGuid << " not found for Interact().";
            Core::Log::Message(err_ss.str());
        }
    });

    m_lastBobberInteractedGuid = bobberGuid;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 + (m_gen() % 500)));
    return true;
}

} // namespace Fishing 