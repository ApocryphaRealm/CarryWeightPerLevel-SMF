#pragma once

// Tracks how much of the retroactive catch-up bonus (see Settings.h's leveling::enableCatchUp)
// has already been granted to the currently loaded character, using SKSE's own co-save
// mechanism - the standard, documented way for a native SKSE plugin to persist per-character
// data alongside the game's own save file (SKSE::SerializationInterface: SetUniqueID plus
// Save/Load/Revert callbacks). Per CLAUDE.md rule 24, this is the real, established pattern
// for this exact problem rather than an invented scheme - no other mod in this project has
// needed per-character persisted state yet, so this is the first use of it here, but the API
// itself is core SKSE, not something bespoke.
//
// Storing a running total (rather than a plain "has this been applied" flag) is what makes
// Leveling::ApplyCatchUp() safe to call unconditionally on every save load AND from a manual
// "Recalculate now" button: it always compares what SHOULD have been granted by now against
// what actually HAS been, and only ever grants the (non-negative) difference - so calling it
// twice in a row, or calling it automatically after it was already run manually this session,
// can never double-grant the bonus.
namespace persistence
{
	// Registers this plugin's co-save callbacks. Call once, early in SKSEPluginLoad - the
	// SerializationInterface itself is a core SKSE interface, available as soon as SKSE::Init()
	// has run, unlike an inter-plugin interface such as DevBench's that has to wait for another
	// plugin to have loaded.
	void Init();

	// Total carry weight already granted by the catch-up feature for the currently loaded
	// character. 0.0 for a character that has never received any (including one loaded before
	// this plugin's co-save data existed at all, or after Revert cleared the in-memory value).
	float GetTotalCatchUpGranted();

	// Records that a_value has now been granted in total (not incrementally - the caller passes
	// the new running total). Takes effect immediately in memory; only actually reaches disk the
	// next time the game itself saves, same as every other SKSE co-save plugin.
	void SetTotalCatchUpGranted(float a_value);

	// Same idea, tracking the starting-carry-weight feature (see Settings.h's
	// leveling::enableStartingCarryWeight) instead - its own separate running total, so it never
	// interacts with the catch-up total above. 0.0 for a character that has never had it applied
	// (including one loaded before this plugin's co-save data existed at all, or after Revert
	// cleared the in-memory value).
	float GetTotalStartingCarryWeightApplied();

	// Same contract as SetTotalCatchUpGranted() above, for the starting-carry-weight running
	// total instead.
	void SetTotalStartingCarryWeightApplied(float a_value);
}
