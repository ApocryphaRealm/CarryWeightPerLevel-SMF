#pragma once

// Backs the "carryweightperlevel.status" DevBench tool - see CLAUDE.md rule 31 (every mod's
// first version ships with live-queryable state, not just logs reconstructed after the fact).
//
// Every Record* function here is called from the main thread, at the exact point a decision is
// made, and only ever writes a mutex-guarded snapshot. The DevBench tool handler runs on
// devbench's own listener thread and only ever reads that snapshot - it never reaches back into
// game state itself.
namespace diagnostics
{
	// Looks up the DevBench interface (present only if the DevBench plugin is installed) and
	// registers "carryweightperlevel.status". Safe to call repeatedly - a rule-17 retry, not a
	// one-shot lookup: call again at kPostPostLoad and kDataLoaded too. Every call after the
	// first successful one is a cheap no-op; only the final call (a_lastAttempt = true) logs
	// that DevBench was never found, so the "not installed" conclusion is not reported before
	// every retry is exhausted.
	void Init(bool a_lastAttempt = false);

	// The player's LevelIncrease event fired and the per-level-up bonus was applied.
	void RecordLevelUpBonusApplied(std::uint16_t a_newLevel, float a_amountApplied);

	// ApplyCatchUp actually granted a non-zero top-up (a_manualTrigger distinguishes the
	// automatic on-load pass from the settings page's "Recalculate now" button).
	void RecordCatchUpApplied(std::uint16_t a_level, float a_amountGranted, bool a_manualTrigger);
}
