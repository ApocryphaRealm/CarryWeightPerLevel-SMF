#include "Leveling.h"

#include "Diagnostics.h"
#include "Persistence.h"
#include "Settings.h"
#include "utils/Logger.h"

namespace Leveling
{
	LevelUpEventSink* LevelUpEventSink::GetSingleton()
	{
		static LevelUpEventSink singleton;

		return &singleton;
	}

	RE::BSEventNotifyControl LevelUpEventSink::ProcessEvent(const RE::LevelIncrease::Event* a_event,
		RE::BSTEventSource<RE::LevelIncrease::Event>*)
	{
		if (!a_event)
		{
			logger::warn("LevelUpEventSink::ProcessEvent: null event; ignoring");

			return RE::BSEventNotifyControl::kContinue;
		}

		if (!a_event->player)
		{
			logger::warn("LevelUpEventSink::ProcessEvent: event carried a null player pointer; ignoring");

			return RE::BSEventNotifyControl::kContinue;
		}

		if (!settings::leveling::enablePerLevelBonus)
		{
			logger::debug("LevelUpEventSink::ProcessEvent: per-level bonus is disabled; ignoring level-up to {}",
				a_event->newLevel);

			// Still track the level so a re-enable mid-session does not immediately think a
			// large number of levels were "gained" since the last time this ran.
			lastKnownLevel = a_event->newLevel;

			return RE::BSEventNotifyControl::kContinue;
		}

		const std::uint16_t newLevel = a_event->newLevel;

		// Normally exactly 1 - the event is expected to fire once per real level gained. Kept
		// as a delta anyway (rather than assuming 1 outright) so a hypothetical batched jump
		// (e.g. a script or console command that grants several levels in one call) still gets
		// the correct total bonus rather than only one level's worth, the same defensive intent
		// as the original mod's own Math.abs(...) delta - just against a cleaner, more reliable
		// native event instead of the Story Manager's known "fires once, for the first of
		// several levels" quirk.
		std::uint16_t levelsGained = 1;

		if (lastKnownLevel != 0 && newLevel > lastKnownLevel)
		{
			levelsGained = newLevel - lastKnownLevel;
		}
		else if (lastKnownLevel != 0 && newLevel <= lastKnownLevel)
		{
			logger::debug("LevelUpEventSink::ProcessEvent: new level {} is not greater than the last known level {}; "
						  "treating this as a single level to stay safe", newLevel, lastKnownLevel);
		}

		lastKnownLevel = newLevel;

		const float bonus = settings::leveling::carryWeightPerLevel * static_cast<float>(levelsGained);

		logger::debug("LevelUpEventSink::ProcessEvent: player reached level {} ({} level(s) gained since last seen); "
					  "applying {:.2f} carry weight ({:.2f} per level)",
			newLevel, levelsGained, bonus, settings::leveling::carryWeightPerLevel);

		RE::ActorValueOwner* avOwner = a_event->player->AsActorValueOwner();

		if (!avOwner)
		{
			logger::error("LevelUpEventSink::ProcessEvent: player's ActorValueOwner interface was null; "
						  "could not apply the level-up carry weight bonus");

			return RE::BSEventNotifyControl::kContinue;
		}

		avOwner->ModActorValue(RE::ActorValue::kCarryWeight, bonus);

		logger::info("LevelUpEventSink::ProcessEvent: applied {:.2f} carry weight for reaching level {}",
			bonus, newLevel);

		diagnostics::RecordLevelUpBonusApplied(newLevel, bonus);

		return RE::BSEventNotifyControl::kContinue;
	}

	void InstallLevelUpHook()
	{
		RE::BSTEventSource<RE::LevelIncrease::Event>* source = RE::LevelIncrease::GetEventSource();

		if (!source)
		{
			logger::error("InstallLevelUpHook: RE::LevelIncrease::GetEventSource() returned null; "
						  "the per-level-up carry weight bonus will not function this session");

			return;
		}

		source->AddEventSink(LevelUpEventSink::GetSingleton());

		logger::debug("InstallLevelUpHook: registered the level-up event sink with RE::LevelIncrease's event source");
	}

	void ApplyCatchUp(RE::Actor* a_player, bool a_manualTrigger)
	{
		if (!a_player)
		{
			logger::error("ApplyCatchUp: null player ({})", a_manualTrigger ? "manual" : "automatic");

			return;
		}

		if (!settings::leveling::enableCatchUp)
		{
			logger::debug("ApplyCatchUp: catch-up is disabled; nothing to do ({})",
				a_manualTrigger ? "manual" : "automatic");

			return;
		}

		const std::uint16_t level = a_player->GetLevel();
		const float desiredTotal = level > 1
										? static_cast<float>(level - 1) * settings::leveling::catchUpBonusPerLevel
										: 0.0F;
		const float alreadyGranted = persistence::GetTotalCatchUpGranted();
		const float delta = desiredTotal - alreadyGranted;

		logger::debug("ApplyCatchUp ({}): level {}, desired total {:.2f}, already granted {:.2f}, delta {:.2f}",
			a_manualTrigger ? "manual" : "automatic", level, desiredTotal, alreadyGranted, delta);

		if (delta <= 0.0F)
		{
			logger::debug("ApplyCatchUp: nothing to grant - the running total already covers this "
						  "character's current level");

			return;
		}

		RE::ActorValueOwner* avOwner = a_player->AsActorValueOwner();

		if (!avOwner)
		{
			logger::error("ApplyCatchUp: player's ActorValueOwner interface was null; "
						  "could not apply the catch-up bonus");

			return;
		}

		avOwner->ModActorValue(RE::ActorValue::kCarryWeight, delta);
		persistence::SetTotalCatchUpGranted(desiredTotal);

		logger::info("ApplyCatchUp: granted {:.2f} carry weight ({} catch-up, level {}, {:.2f} per level, "
					 "running total now {:.2f})",
			delta, a_manualTrigger ? "manual" : "automatic", level, settings::leveling::catchUpBonusPerLevel,
			desiredTotal);

		diagnostics::RecordCatchUpApplied(level, delta, a_manualTrigger);
	}
}
