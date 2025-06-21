#pragma once

#include <thread>
#include <atomic>
#include <cstdint> // For uint64_t
#include <string>
#include <random> // Added for std::mt19937

// Forward declarations to avoid circular dependencies / heavy includes
class ObjectManager;
namespace Spells { class CooldownManager; }
class WowGameObject; // Keep for FindActiveBobber return type if it still returns raw ptr temporarily

namespace Fishing {

// Default fishing spell ID (will be configurable)
const uint32_t DEFAULT_FISHING_SPELL_ID = 7620;

class FishingBot {
public:
    FishingBot(ObjectManager& objMgr, Spells::CooldownManager& cdMgr);
    ~FishingBot();

    void Start();
    void Stop();
    bool IsRunning() const { return m_isRunning; }
    
    // Add getter and setter for fishing spell ID
    uint32_t GetFishingSpellID() const { return m_fishingSpellID; }
    void SetFishingSpellID(uint32_t spellID);

private:
    void RunFishingLoop();
    bool CastFishingSpell();
    WowGameObject* FindActiveBobber(); // Returns a raw pointer, its GUID should be extracted immediately
    bool MonitorBobber(uint64_t bobberGuid);    // Takes bobber's GUID
    bool InteractWithBobber(uint64_t bobberGuid); // Takes bobber's GUID

    ObjectManager& m_objectManager;
    Spells::CooldownManager& m_cooldownManager;

    std::thread m_fishingThread;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_isRunning{false};
    uint64_t m_lastBobberInteractedGuid{0};
    const std::string m_fishingBobberName = "Fishing Bobber"; // Default name
    
    // Configurable fishing spell ID
    std::atomic<uint32_t> m_fishingSpellID{DEFAULT_FISHING_SPELL_ID};

    // Random number generator
    std::random_device m_rd;
    std::mt19937 m_gen;
};

} // namespace Fishing 