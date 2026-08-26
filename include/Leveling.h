#pragma once

// The mod's own core mechanic - a fresh native re-implementation of "carry weight scales with
// level", inspired by (but not ported from) Increase Carry-Weight on Level-Up (Nexus 2654).
// That mod did this with three fixed Papyrus scripts and a non-configurable GlobalVariable;
// this does the same kind of thing natively, driven by CommonLibSSE-NG's own RE::LevelIncrease
// event (include/RE/L/LevelIncrease.h in the vendored headers - a real, documented engine event
// fired once per level gained, distinct from - and considerably simpler than - the Story
// Manager's OnStoryIncreaseLevel event Papyrus scripts have to use instead, which the original
// mod's own comments describe working around: it only fires once even across a multi-level
// jump, and for the *first* of those levels rather than the last).
namespace Leveling
{
	// Sinks RE::LevelIncrease::Event and applies settings::leveling::carryWeightPerLevel to the
	// player's carry weight (RE::ActorValue::kCarryWeight) each time it fires, scaled by however
	// many levels were actually gained since the last event this session (normally 1, but
	// defensive against a hypothetical batched multi-level jump the same way the original mod
	// had to be).
	class LevelUpEventSink : public RE::BSTEventSink<RE::LevelIncrease::Event>
	{
	public:
		static LevelUpEventSink* GetSingleton();

		RE::BSEventNotifyControl ProcessEvent(const RE::LevelIncrease::Event* a_event,
			RE::BSTEventSource<RE::LevelIncrease::Event>* a_eventSource) override;

	private:
		LevelUpEventSink() = default;

		// In-memory only, reset to 0 every plugin load - safe because the event only fires
		// forward from whatever level the player is already at when a session starts, so the
		// very first event of a session is always a real, single level-up and needs no prior
		// state to interpret correctly.
		std::uint16_t lastKnownLevel = 0;
	};

	// Registers LevelUpEventSink with the engine's LevelIncrease event source. Idempotent -
	// safe to call more than once (AddEventSink itself de-duplicates by pointer).
	void InstallLevelUpHook();

	// The native equivalent of the original mod's one-time "retroactive adjustment" potion
	// (added in its own version 0.4, for a character who already had levels before the mod -
	// here, before this feature - was ever active).
	//
	// Idempotent by design: computes what the character's total catch-up bonus SHOULD be for
	// their current level (settings::leveling::catchUpBonusPerLevel * (level - 1)), compares it
	// against persistence::GetTotalCatchUpGranted(), and grants only the (non-negative)
	// difference. Safe to call unconditionally on every save load, and safe for the settings
	// page's "Recalculate now" button to call as many times as the player likes - neither can
	// ever grant the same bonus twice, and neither will ever remove carry weight, matching the
	// original potion's own "only ever tops up, never down" behaviour.
	//
	// a_manualTrigger only affects logging/diagnostics (which of the two call sites this was),
	// not the math.
	void ApplyCatchUp(RE::Actor* a_player, bool a_manualTrigger);

	// Sets the player's carry weight to settings::leveling::startingCarryWeight - a flat, one-time
	// target, independent of the per-level bonus and catch-up above (see Settings.h's own
	// comment on leveling::enableStartingCarryWeight for why this is a "set to X" feature rather
	// than an additive bonus like the other two).
	//
	// Idempotent the same way ApplyCatchUp() is, and applies the difference via the same
	// RE::ActorValueOwner::ModActorValue(RE::ActorValue::kCarryWeight, ...) call the other two
	// features already use (CLAUDE.md rule 24 - reuse the API this mod already validated for
	// reading/writing carry weight rather than reaching for a blind guess like SetActorValue or
	// SetBaseActorValue, which would stomp on whatever the per-level bonus/catch-up/vanilla have
	// already contributed to the same actor value instead of layering on top of it): computes the
	// delta between settings::leveling::startingCarryWeight and
	// persistence::GetTotalStartingCarryWeightApplied(), and applies only that delta. Safe to call
	// unconditionally on every save load, and safe for the settings page's "Apply now" button to
	// call as many times as the player likes - it never stacks or double-applies. Unlike
	// ApplyCatchUp(), the delta here CAN be negative: if the author lowers the configured value and
	// re-triggers, carry weight actually moves down to match, since this setting is a deliberate
	// target rather than a forgiving top-up.
	//
	// a_manualTrigger only affects logging/diagnostics (which of the two call sites this was),
	// not the math.
	void ApplyStartingCarryWeight(RE::Actor* a_player, bool a_manualTrigger);
}
