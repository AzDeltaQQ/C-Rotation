#pragma once

#include "wowobject.h"

// Represents GameObject objects (Chests, Doors, Herbs, Ore, etc.)
class WowGameObject : public WowObject {
public:
    // Constructor matching ObjectManager usage
    WowGameObject(uintptr_t baseAddress, WGUID guid);

    virtual ~WowGameObject() = default;

    // Override to read GameObject-specific data
    void UpdateDynamicData() override;

    // Add GameObject-specific methods here (e.g., IsLocked, IsHarvestable)
    // Example:
    // virtual bool IsLocked();
    // virtual bool CanHarvest();

    // Check if the fishing bobber is splashing (fish on hook)
    bool IsBobbing() const;
}; 