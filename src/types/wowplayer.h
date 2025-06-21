#pragma once

// #include "wowunit.h" // Players are Units --> Replace include with forward declaration
// class WowUnit; // Forward declaration --> No longer needed, include the header
#include "wowunit.h" // Include the full base class definition
// #include "wowobject.h" // Need WowObject definition --> REMOVE THIS, included via wowunit.h

// Represents Player objects (inherits from WowUnit)
class WowPlayer : public WowUnit {
public:
    // Constructor matching ObjectManager usage
    WowPlayer(uintptr_t baseAddress, WGUID guid);

    virtual ~WowPlayer() = default;

    // Override UpdateDynamicData (likely just calls WowUnit::UpdateDynamicData for now)
    // unless player-specific fields need reading.
    virtual void UpdateDynamicData() override;

    // Player-specific methods (or convenience wrappers)
    int GetMana() const { return GetPower(); } // Assumes Mana is PowerType 0
    int GetMaxMana() const { return GetMaxPower(); }
    int GetRage() const { return GetPower(); } // Assumes Rage is PowerType 1
    int GetEnergy() const { return GetPower(); } // Assumes Energy is PowerType 3
    // Add GetClass() when implemented
    // std::string GetClass() const;

    bool IsLooting() const; // Added to check if player is looting

private:
    // Add player-specific private members if any
}; 