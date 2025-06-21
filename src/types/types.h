#pragma once

#include <cstdint>
#include <cmath> // For std::sqrt

// Basic Types based on WoWBot/Windows
typedef unsigned long DWORD;

// GUID Structure (Matches WoWBot's WGUID)
struct WGUID {
    uint32_t low;
    uint32_t high;

    // Default constructor
    WGUID() : low(0), high(0) {}

    // Constructor from parts
    WGUID(uint32_t l, uint32_t h) : low(l), high(h) {}

    // Constructor from uint64_t
    explicit WGUID(uint64_t guid64) {
        low = static_cast<uint32_t>(guid64 & 0xFFFFFFFF);
        high = static_cast<uint32_t>((guid64 >> 32) & 0xFFFFFFFF);
    }

    // Conversion to uint64_t
    uint64_t ToUint64() const {
        return (static_cast<uint64_t>(high) << 32) | low;
    }

    // Comparison operators
    bool operator==(const WGUID& other) const {
        return low == other.low && high == other.high;
    }
    bool operator!=(const WGUID& other) const {
        return !(*this == other);
    }
    bool operator<(const WGUID& other) const {
        if (high != other.high) {
            return high < other.high;
        }
        return low < other.low;
    }

    // Check if GUID is potentially valid (not zero)
    bool IsValid() const {
        return low != 0 || high != 0;
    }
};

// Specialization for std::hash<WGUID> to allow use in std::unordered_map
// namespace std {
//     template <>
//     struct hash<WGUID> {
//         std::size_t operator()(const WGUID& guid) const noexcept {
//             // Combine the hash values of low and high parts
//             // A simple combination strategy:
//             std::size_t h1 = std::hash<uint32_t>{}(guid.low);
//             std::size_t h2 = std::hash<uint32_t>{}(guid.high);
//             return h1 ^ (h2 << 1); // Or use boost::hash_combine pattern
//         }
//     };
// } // namespace std
// Removed std::hash specialization as it's not needed for std::map and caused compile errors without <functional>.
// Can be re-added (with #include <functional>) if std::unordered_map is used later.


// Vector3 Structure (Matches WoWBot's Vector3)
struct Vector3 {
    float x, y, z;

    // Default constructor
    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

    // Constructor from values
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    // Basic operations (add more as needed)
    float Distance(const Vector3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    float DistanceSq(const Vector3& other) const {
         float dx = x - other.x;
         float dy = y - other.y;
         float dz = z - other.z;
         return dx * dx + dy * dy + dz * dz;
    }
    
    bool IsZero() const {
        return x == 0.0f && y == 0.0f && z == 0.0f;
    }

    // Operator overloads for vector arithmetic
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }
};

// Object Type Enum (Matches WoWBot's WowObjectType, add more as needed)
enum WowObjectType : int {
    OBJECT_NONE = 0,
    OBJECT_ITEM = 1,
    OBJECT_CONTAINER = 2,
    OBJECT_UNIT = 3,       // Includes players and creatures
    OBJECT_PLAYER = 4,
    OBJECT_GAMEOBJECT = 5,
    OBJECT_DYNAMICOBJECT = 6,
    OBJECT_CORPSE = 7,
    OBJECT_TOTAL // Keep this last
};

// Forward declaration
class WowObject;

// Base structure for the game's object manager (Layout partially known)
struct ObjectManagerActual { // Adopt WoWBot's partial definition
    uint8_t padding1[0x1C];
    void* hashTableBase; // Offset 0x1C 
    uint8_t padding2[0x24 - 0x1C - 4]; // Calculate size of second padding block
    uint32_t hashTableMask; // Offset 0x24 
    // ... other unknown fields ...
};


// --- Game Function Pointer Types (Based on WoWBot) ---
// Callback for EnumVisibleObjects
typedef int(__cdecl* EnumVisibleObjectsCallback)(uint32_t guid_low, uint32_t guid_high, int callback_arg);

// EnumVisibleObjects function itself
typedef int(__cdecl* EnumVisibleObjectsFn)(EnumVisibleObjectsCallback callback, int callback_arg);

// GetObjectPtrByGuid (Inner function, often __thiscall)
typedef void* (__thiscall* GetObjectPtrByGuidInnerFn)(void* thisptr, uint32_t guid_low, WGUID* pGuidStruct);

// GetLocalPlayerGuid function (Assuming global function pointer)
typedef uint64_t(__cdecl* GetLocalPlayerGuidFn)();

// IsGameReady function (Returns char/bool)
typedef bool(__cdecl* IsGameReadyFn)();

// isLocalPlayerActiveAndInWorld function (Returns char/bool)
typedef bool(__cdecl* IsLocalPlayerActiveAndInWorldFn)(); 

// ADDED: Power Types Enum (Standard WoW 3.3.5a)
enum PowerType : uint8_t {
    POWER_TYPE_MANA = 0,
    POWER_TYPE_RAGE = 1,
    POWER_TYPE_FOCUS = 2,
    POWER_TYPE_ENERGY = 3,
    POWER_TYPE_HAPPINESS = 4,
    // Skip 5 (unknown/unused)
    POWER_TYPE_RUNE = 6,
    POWER_TYPE_RUNIC_POWER = 7,
    POWER_TYPE_COUNT // Keep last for iteration if needed
}; 

// REMOVED unnecessary #endif as #pragma once is used
// #endif // End of include guard if it exists 

namespace Offsets {
    // ... existing offsets ...

    // Player Global Data (WoW 3.3.5a)
    static constexpr uintptr_t COMBO_POINTS_ADDR = 0x00BD084D;             // db, stores current combo points
    static constexpr uintptr_t COMBO_POINTS_TARGET_GUID_ADDR = 0xBD08A8;    // uint64_t, stores GUID of the target combo points are on

    // ... other offsets ...
} 