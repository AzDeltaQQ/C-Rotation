#pragma once


// Forward declaration if needed later
// class ObjectManager;

class SpellManager {
public:
    // Prevent instantiation of static class
    SpellManager() = delete;
    ~SpellManager() = delete;
    SpellManager(const SpellManager&) = delete;
    SpellManager& operator=(const SpellManager&) = delete;

    /**
     * @brief Gets the remaining cooldown for a player spell in milliseconds.
     *
     * Calls the game's internal get_spell_cooldown_proxy function (0x809000).
     *
     * @param spellId The ID of the spell to check.
     * @return The remaining cooldown in milliseconds, or 0 if the spell is ready.
     *         Returns -1 if an error occurs during the function call.
     */
    static int GetSpellCooldownMs(int spellId);

    /**
     * @brief Gets the remaining cooldown for a pet spell in milliseconds.
     *
     * Calls the game's internal get_spell_cooldown_proxy function (0x809000).
     *
     * @param spellId The ID of the pet spell to check.
     * @return The remaining cooldown in milliseconds, or 0 if the spell is ready.
     *         Returns -1 if an error occurs during the function call.
     */
    static int GetPetSpellCooldownMs(int spellId);

    /**
     * @brief Applies memory patches to fix cooldown display/query bugs.
     *
     * Should be called once during initialization after MinHook is initialized.
     */
    static void PatchCooldownBug_Final();

    /**
     * @brief Attempts to stop the currently casting spell.
     *
     * Calls an internal game function (likely handleSpellStopOrCompletion at 0x809EA0).
     * Use with caution, signature matching is critical.
     */
    static void StopCasting();

    // Add other spell-related static functions here if needed later
    // e.g., static bool IsSpellInRange(...) 
}; 