#include "Diagnostics.h"
#include "Leveling.h"
#include "Settings.h"
#include "UI.h"
#include "utils/Logger.h"

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	if (!a_msg)
	{
		return;
	}

	switch (a_msg->type)
	{
	case SKSE::MessagingInterface::kPostLoad:
		// DevBenchAPI's own contract: the interface can only be requested once SKSE has sent
		// kPostLoad, since that's the earliest point every plugin (DevBench included) has had
		// its own SKSEPluginLoad run.
		logger::debug("kPostLoad received; registering live diagnostics with DevBench if present");
		diagnostics::Init();
		break;

	case SKSE::MessagingInterface::kPostPostLoad:
		// By kPostPostLoad every plugin has finished its own post-load work, so SKSE Menu
		// Framework's module is guaranteed to be in the process if it is installed at all.
		logger::debug("kPostPostLoad received; registering settings page with SKSE Menu Framework");
		UI::Register();

		// Rule-17 retry: devbench's own server can still be finishing startup a moment after
		// kPostLoad fires, which is early enough to lose the race even though kPostLoad is
		// DevBenchAPI's own documented earliest-safe point (see AutoDraw-SMF's identical
		// comment - CLAUDE.md rule 24). Cheap no-op if the kPostLoad attempt already succeeded.
		diagnostics::Init();
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		// The engine's own event sources (RE::LevelIncrease's included) are expected to be
		// ready by kDataLoaded - this is the same point AutoDraw-SMF registers its combat event
		// sink at.
		logger::debug("kDataLoaded received; installing the level-up carry weight hook");
		Leveling::InstallLevelUpHook();

		// Last retry point - if DevBench still isn't found here, conclude it isn't installed
		// and say so, rather than staying silent about it forever.
		diagnostics::Init(/* a_lastAttempt = */ true);
		break;

	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
	{
		// A save load (or a brand new game) is exactly when the retroactive catch-up needs to
		// run: it is idempotent (see Leveling::ApplyCatchUp's own header comment), so calling
		// it unconditionally here - on every load, not just the first - is safe and self-
		// correcting rather than needing its own "has this run yet" tracking on top of
		// Persistence's own running total.
		logger::debug("{} received; running the retroactive catch-up pass",
			a_msg->type == SKSE::MessagingInterface::kNewGame ? "kNewGame" : "kPostLoadGame");

		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

		if (!player)
		{
			logger::error("{}: RE::PlayerCharacter::GetSingleton() returned null; could not run the catch-up pass",
				a_msg->type == SKSE::MessagingInterface::kNewGame ? "kNewGame" : "kPostLoadGame");

			break;
		}

		Leveling::ApplyCatchUp(player, /* a_manualTrigger = */ false);

		break;
	}

	default:
		break;
	}
}
