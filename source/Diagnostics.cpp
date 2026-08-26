#include "Diagnostics.h"

#include "DevBench/DevBenchAPI.h"
#include "Persistence.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <mutex>

namespace diagnostics
{
	namespace
	{
		using clock = std::chrono::steady_clock;

		std::mutex mtx;

		struct State
		{
			std::uint64_t levelUpBonusesApplied = 0;
			std::optional<clock::time_point> lastLevelUpBonus;
			std::uint16_t lastLevelUpBonusLevel = 0;
			float lastLevelUpBonusAmount = 0.0F;

			std::uint64_t catchUpsApplied = 0;
			std::optional<clock::time_point> lastCatchUp;
			std::uint16_t lastCatchUpLevel = 0;
			float lastCatchUpAmount = 0.0F;
			bool lastCatchUpWasManual = false;
		};

		State state;

		// Renders "field": null or "field": <seconds ago>, so a query can tell "never
		// happened" apart from "happened a long time ago" instead of both looking like a
		// missing/zero field.
		std::string SecondsAgoField(const char* a_name, const std::optional<clock::time_point>& a_when)
		{
			if (!a_when)
			{
				return std::format("\"{}SecondsAgo\": null", a_name);
			}

			const double seconds = std::chrono::duration<double>(clock::now() - *a_when).count();

			return std::format("\"{}SecondsAgo\": {:.1f}", a_name, seconds);
		}

		void StatusTool(void*, const char*, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			std::string json;

			{
				std::scoped_lock lock(mtx);

				json = std::format(
					"{{"
					"\"settings\":{{"
					"\"enablePerLevelBonus\":{},"
					"\"carryWeightPerLevel\":{:.2f},"
					"\"enableCatchUp\":{},"
					"\"catchUpBonusPerLevel\":{:.2f}"
					"}},"
					"\"catchUp\":{{"
					"\"totalGranted\":{:.2f}"
					"}},"
					"\"levelUpBonus\":{{"
					"\"count\":{},"
					"\"lastLevel\":{},"
					"\"lastAmount\":{:.2f},"
					"{}"
					"}},"
					"\"catchUpApplications\":{{"
					"\"count\":{},"
					"\"lastLevel\":{},"
					"\"lastAmount\":{:.2f},"
					"\"lastWasManual\":{},"
					"{}"
					"}}"
					"}}",
					settings::leveling::enablePerLevelBonus ? "true" : "false",
					settings::leveling::carryWeightPerLevel,
					settings::leveling::enableCatchUp ? "true" : "false",
					settings::leveling::catchUpBonusPerLevel,
					persistence::GetTotalCatchUpGranted(),
					state.levelUpBonusesApplied,
					state.lastLevelUpBonusLevel,
					state.lastLevelUpBonusAmount,
					SecondsAgoField("last", state.lastLevelUpBonus),
					state.catchUpsApplied,
					state.lastCatchUpLevel,
					state.lastCatchUpAmount,
					state.lastCatchUpWasManual ? "true" : "false",
					SecondsAgoField("last", state.lastCatchUp));
			}

			a_write(a_sink, json.c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;

		if (registered)
		{
			return;
		}

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();

		if (!devBench)
		{
			if (a_lastAttempt)
			{
				logger::info("DevBench not detected; skipping the \"carryweightperlevel.status\" live-diagnostics "
							 "tool (logging alone still covers this session - see CLAUDE.md rule 31)");
			}
			else
			{
				logger::debug("DevBench not detected yet; will retry at the next message");
			}

			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Live Carry Weight Per Level state: current settings, the "
			"retroactive catch-up's running total, and the last per-level-up bonus and "
			"catch-up application.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{}},"
			"\"readOnly\":true"
			"}";

		if (devBench->RegisterTool("carryweightperlevel.status", descriptor, &StatusTool, nullptr))
		{
			logger::info("Registered \"carryweightperlevel.status\" with DevBench (build {})", devBench->GetBuildNumber());
		}
		else
		{
			logger::warn("DevBench reported \"carryweightperlevel.status\" replaced an existing tool of the same name");
		}

		registered = true;
	}

	void RecordLevelUpBonusApplied(std::uint16_t a_newLevel, float a_amountApplied)
	{
		std::scoped_lock lock(mtx);

		++state.levelUpBonusesApplied;
		state.lastLevelUpBonusLevel = a_newLevel;
		state.lastLevelUpBonusAmount = a_amountApplied;
		state.lastLevelUpBonus = clock::now();
	}

	void RecordCatchUpApplied(std::uint16_t a_level, float a_amountGranted, bool a_manualTrigger)
	{
		std::scoped_lock lock(mtx);

		++state.catchUpsApplied;
		state.lastCatchUpLevel = a_level;
		state.lastCatchUpAmount = a_amountGranted;
		state.lastCatchUpWasManual = a_manualTrigger;
		state.lastCatchUp = clock::now();
	}
}
