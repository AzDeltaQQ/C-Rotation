#pragma once

#include <cstdint>
#include <string>
#include <stdexcept> // For std::runtime_error
#include <Windows.h> // Required for SEH/IsBadReadPtr if used, memcpy
#include <sstream>   // For formatting error messages
#include <cstring>   // For memcpy

// Basic memory reading/writing exception
class MemoryAccessError : public std::runtime_error {
public:
    MemoryAccessError(const std::string& message) : std::runtime_error(message) {}
};

namespace Memory {

    // --- Direct Memory Read (Inspired by WoWBot) ---
    // Reads a value of type T directly from the specified address.
    // Includes volatile handling to prevent unwanted compiler optimizations.
    // WARNING: Direct pointer access. Invalid address WILL cause a crash.
    template<typename T>
    T Read(uintptr_t address) {
        if (address == 0) {
            // Consider logging this instead of throwing? Depends on usage context.
            throw MemoryAccessError("Attempted to read from null address.");
        }

        // Original direct read logic (without SEH)
        T localValue;
        volatile T* volatilePtr = reinterpret_cast<volatile T*>(address);
        memcpy(&localValue, const_cast<const void*>(reinterpret_cast<const volatile void*>(volatilePtr)), sizeof(T)); 
        return localValue; 
    }

    // --- Direct Memory Write (Inspired by WoWBot) ---
    // Writes a value of type T directly to the specified address.
    // WARNING: Direct pointer access. Invalid address WILL cause a crash.
    // Consider using VirtualProtect if writing to read-only memory.
    template<typename T>
    void Write(uintptr_t address, T value) {
        if (address == 0) {
            throw MemoryAccessError("Attempted to write to null address.");
        }

        // Original direct write (without SEH)
        *(reinterpret_cast<T*>(address)) = value;
    }
    
    // Helper function to get the base address of the main module (WoW.exe)
    // uintptr_t GetBaseAddress(); // Declaration if needed

    // --- String Reading Helper (Direct Access) --- 
    // Reads a standard null-terminated string from a given address
    // WARNING: Direct pointer access. Invalid address WILL cause a crash.
    inline std::string ReadString(uintptr_t address, size_t maxLength = 256) {
        if (address == 0) {
            return ""; // Return empty for null address
        }

        std::string result;
        result.reserve(64); 
        
        // Original direct read loop (without SEH)
        char* c_str = reinterpret_cast<char*>(address);
        // WARNING: No check here if c_str itself is valid before loop!
        // A preliminary IsBadReadPtr(c_str, 1) might be advisable but has limitations.
        for (size_t i = 0; i < maxLength; ++i) {
            // Read character by character - CRASHES if c_str[i] is invalid memory
            char currentChar = c_str[i]; 
            if (currentChar == '\0') {
                break; // End of string
            }
            result += currentChar;
        }
        // Consider adding error handling/logging if maxLength is reached without null terminator

        return result;
    }

    // Helper to convert a value to its hexadecimal string representation
    template<typename T>
    std::string to_hex_string(T value) {
        std::stringstream ss;
        ss << std::hex << value;
        return ss.str();
    }

} // namespace Memory