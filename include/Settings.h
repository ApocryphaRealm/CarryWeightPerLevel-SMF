#pragma once

namespace SKSE::log
{
	using level = spdlog::level::level_enum;
}
namespace logger = SKSE::log;

namespace settings
{
	// Reads the INI into the variables below. The values the variables hold when this is
	// called are remembered as the built-in defaults, so RestoreDefaults() can put them back.
	void Init(const std::string& a_iniFileName);

	// Writes every setting below back to the INI that Init() read, leaving the comments and
	// any unrelated keys in that file alone. Returns false if the file could not be written.
	bool Save();

	// Puts every setting back to its built-in default. This only touches the variables;
	// follow it with Save() to persist, and with UI::ApplyLiveSettings() to show it in game.
	void RestoreDefaults();

	// Re-reads the INI that Init() read, discarding any unsaved change made since. Returns
	// false if the file could not be read, leaving the current values alone.
	bool Reload();

	// Full path of the INI Init() read, or an empty string before Init() has run.
	const std::string& GetIniPath();

	namespace debug
	{
		// Ships at trace by default (project standard) so a submitted log carries the detail
		// needed to diagnose a compatibility, timing or stability report without asking the
		// reporter to change anything first - see CLAUDE.md rule 31.
		inline logger::level logLevel = logger::level::trace;
	}

	namespace leveling
	{
		// The mod's core mechanic: every time the player levels up, add a fixed amount of
		// carry weight, regardless of which attribute (Health/Magicka/Stamina) was chosen.
		// This is what the original "Increase Carry-Weight on Level-Up" (Nexus 2654) did with
		// a fixed, non-configurable GlobalVariable - here it is a real setting.
		inline bool enablePerLevelBonus = true;

		// Carry weight added per level gained. The original's own GlobalVariable had no
		// documented default visible from its Papyrus source (it lives only in the ESP), so
		// this defaults to 5.0 - the same per-level rate vanilla Skyrim itself grants for a
		// Stamina level-up, which is a reasonable, familiar starting point rather than an
		// arbitrary guess.
		inline float carryWeightPerLevel = 5.0F;

		// Native re-implementation of the original mod's one-time "retroactive adjustment"
		// potion (added in its own version 0.4, for characters who had already leveled up
		// before the mod - or, here, before this setting - was ever active). Rather than a
		// literal in-game potion item (which would need its own ESP/Alchemy record - out of
		// scope for a script-free native plugin), this applies as a top-up: on every save
		// load, and whenever "Recalculate now" is pressed, the plugin computes what the
		// player's total catch-up bonus *should* be for their current level and tops up
		// carry weight by the difference versus what has already been granted - so it is
		// safe to trigger repeatedly and never grants the same bonus twice.
		inline bool enableCatchUp = true;

		// Per-level rate used only for the retroactive catch-up above - deliberately a
		// separate value from carryWeightPerLevel so the ongoing per-level-up rate and the
		// one-time catch-up rate for a character who already had levels can be tuned
		// independently (e.g. a lower catch-up rate to avoid a large single jump in carry
		// weight on an already high-level existing character).
		inline float catchUpBonusPerLevel = 5.0F;
	}
}
