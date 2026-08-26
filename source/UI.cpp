#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Leveling.h"
#include "Persistence.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Toggle.h"

#include <algorithm>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		// The slider the arrow keys currently drive. Set by clicking one.
		std::string selectedSlider;

		constexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount = 7;

		// The framework renders from the renderer's present hook, which is not the thread the
		// game's own systems expect to be talked to from - anything beyond touching this
		// plugin's own settings variables has to be handed to the main thread first.
		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface())
			{
				taskInterface->AddTask(std::move(a_task));
			}
		}

		// Older SMF builds do not export every cimgui function a page needs, and calling
		// through a null function pointer crashes on the first draw rather than failing to
		// register - so every export this page's widgets resolve at runtime is probed here
		// first, by its *resolved* name (varargs widgets resolve to a "...V"-suffixed export).
		// See AutoDraw-SMF/source/UI.cpp's identical check (CLAUDE.md rule 24).
		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextDisabledV",
				"igTextWrappedV",
				"igSetTooltipV",
				"igSeparatorText",
				"igCombo_Str_arr",
				"igSliderFloat",
				"igIsItemHovered",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igPushItemWidth",
				"igPopItemWidth",
				// Needed by NudgeableSlider's arrow-key nudge (ported from Dragon's Eye
				// Minimap's UI.cpp - CLAUDE.md rule 24).
				"igIsKeyPressed_Bool",
				"igIsItemClicked",
				"igIsItemActive",
				// Needed by utils/Toggle.h's hand-drawn switch (CLAUDE.md rule 32 - on/off
				// switch, not a tick-box) - see that file's own header comment for why a
				// checkbox-style widget needs this much lower-level access.
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igGetFrameHeight",
				"igInvisibleButton",
				"igPushID_Str",
				"igPopID",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
			};

			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("SKSE Menu Framework does not export \"{}\"", name);

					return false;
				}
			}

			return true;
		}

		// A slider that the arrow keys can also nudge, once it has been clicked. Dragging is
		// hopeless for the last decimal place, and the framework does not turn on ImGui's own
		// keyboard navigation, so this tracks the selection itself rather than changing a
		// setting shared with every other mod's page. Ported verbatim from Dragon's Eye
		// Minimap's UI.cpp, which already had this working - see CLAUDE.md rule 24.
		bool NudgeableSlider(const char* a_label, float* a_value, float a_min, float a_max,
							 const char* a_format, float a_step)
		{
			bool changed = ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);

			if (ImGuiMCP::IsItemClicked() || ImGuiMCP::IsItemActive())
			{
				selectedSlider = a_label;
			}

			if (selectedSlider == a_label)
			{
				float nudge = 0.0F;

				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_DownArrow))
				{
					nudge -= a_step;
				}
				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_UpArrow))
				{
					nudge += a_step;
				}

				if (nudge != 0.0F)
				{
					*a_value = std::clamp(*a_value + nudge, a_min, a_max);
					changed = true;
				}

				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("<-->");
			}

			return changed;
		}

		void HelpMarker(const char* a_description)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("(?)");

			if (ImGuiMCP::IsItemHovered())
			{
				ImGuiMCP::SetTooltip("%s", a_description);
			}
		}

		void RenderPerLevelSection()
		{
			using namespace settings::leveling;

			ImGuiMCP::SeparatorText("Per-Level Bonus");

			ImGuiMCP::Toggle("Add carry weight on level-up", &enablePerLevelBonus);
			HelpMarker("Adds carry weight every time you level up, no matter which attribute (Health/Magicka/Stamina) you chose - unlike vanilla, where only choosing Stamina increases carry weight.");

			NudgeableSlider("Carry weight per level", &carryWeightPerLevel, 0.0F, 50.0F, "%.1f", 0.5F);
			HelpMarker("How much carry weight to add each time you level up. Applies live the moment you next level up.");
		}

		void RenderCatchUpSection()
		{
			using namespace settings::leveling;

			ImGuiMCP::SeparatorText("Retroactive Catch-Up");

			ImGuiMCP::Toggle("Enable retroactive catch-up", &enableCatchUp);
			HelpMarker("For a character who already had levels before this feature was turned on: tops up their carry weight to what they would have if the per-level bonus had applied the whole time. Never removes carry weight, and never grants the same bonus twice.");

			NudgeableSlider("Catch-up carry weight per level", &catchUpBonusPerLevel, 0.0F, 50.0F, "%.1f", 0.5F);
			HelpMarker("Rate used only for the retroactive catch-up above - separate from the ongoing per-level rate, so you can set a lower one-time catch-up if your character is already high level.");

			ImGuiMCP::TextDisabled("Total catch-up carry weight granted so far: %.2f", persistence::GetTotalCatchUpGranted());

			if (ImGuiMCP::Button("Recalculate now"))
			{
				OnMainThread([]() {
					Leveling::ApplyCatchUp(RE::PlayerCharacter::GetSingleton(), /* a_manualTrigger = */ true);

					statusMessage = "Catch-up recalculated. See the log for what (if anything) was granted.";
				});
			}
			HelpMarker("Recalculates and applies the catch-up bonus right now, using the settings above. Safe to press more than once - it only ever tops up to the current target, it never grants twice.");
		}

		void RenderStartingCarryWeightSection()
		{
			using namespace settings::leveling;

			ImGuiMCP::SeparatorText("Starting Carry Weight");

			ImGuiMCP::Toggle("Enable starting carry weight", &enableStartingCarryWeight);
			HelpMarker("Sets your carry weight to a specific target total, separate from the per-level bonus and catch-up above. Unlike those, changing this and pressing Apply now actually moves carry weight to match the new number - it can go up or down.");

			NudgeableSlider("Starting carry weight", &startingCarryWeight, 0.0F, 500.0F, "%.1f", 5.0F);
			HelpMarker("The target total this feature applies. Applied automatically the first time a save loads with this feature enabled, and any time you press Apply now afterward.");

			ImGuiMCP::TextDisabled("Total starting carry weight applied so far: %.2f", persistence::GetTotalStartingCarryWeightApplied());

			if (ImGuiMCP::Button("Apply now"))
			{
				OnMainThread([]() {
					Leveling::ApplyStartingCarryWeight(RE::PlayerCharacter::GetSingleton(), /* a_manualTrigger = */ true);

					statusMessage = "Starting carry weight applied. See the log for what (if anything) changed.";
				});
			}
			HelpMarker("Applies the starting carry weight target right now, using the setting above. Safe to press more than once - it only ever applies the difference between the current target and what has already been applied, in either direction.");
		}

		void RenderDebugSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Debug");

			int level = static_cast<int>(debug::logLevel);
			if (ImGuiMCP::Combo("Log level", &level, kLogLevelNames, kLogLevelCount))
			{
				debug::logLevel = static_cast<logger::level>(level);

				OnMainThread([]() { logger::set_level(settings::debug::logLevel, settings::debug::logLevel); });
			}
			HelpMarker("Applies to the log immediately. Ships at Trace by default - see CLAUDE.md rule 31.");
		}

		void RenderButtons()
		{
			if (ImGuiMCP::Button("Save"))
			{
				OnMainThread([]() {
					statusMessage = settings::Save() ? "Settings saved." : "Could not save the INI. See the log for why.";
				});
			}
			HelpMarker("Writes every setting above back to the INI. Comments and unrelated keys are left alone.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Reload from INI"))
			{
				OnMainThread([]() {
					statusMessage = settings::Reload()
										 ? "Settings reloaded from the INI."
										 : "Could not read the INI. See the log for why.";
				});
			}
			HelpMarker("Throws away any change made here since the last save and re-reads the INI from disk. Also picks up edits made to the file by hand.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Restore defaults"))
			{
				OnMainThread([]() { settings::RestoreDefaults(); });

				statusMessage = "Defaults restored. Press Save to keep them.";
			}
			HelpMarker("Puts every setting back to the value it has on a fresh install. Nothing is written until you press Save. Does not touch the catch-up or starting-carry-weight running totals - those are tracked separately, per character, in your save.");

			if (!statusMessage.empty())
			{
				ImGuiMCP::TextWrapped("%s", statusMessage.c_str());
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("SKSE Menu Framework is not installed; settings will be read from the INI only");

			return;
		}

		if (!HasRequiredExports())
		{
			logger::warn("The installed SKSE Menu Framework is older than this plugin's settings "
						 "menu needs. Update it to a newer version to configure Carry Weight Per Level in game.");

			return;
		}

		SKSEMenuFramework::SetSection("Carry Weight Per Level");
		SKSEMenuFramework::AddSectionItem("Settings", SettingsPanel::Render);

		logger::info("Registered the settings page with SKSE Menu Framework");
	}

	void __stdcall SettingsPanel::Render()
	{
		ImGuiMCP::TextWrapped("Most settings apply as soon as you make them. Press Save to keep them for the next time you play.");
		ImGuiMCP::Spacing();

		ImGuiMCP::PushItemWidth(260.0F);

		RenderPerLevelSection();
		ImGuiMCP::Spacing();

		RenderCatchUpSection();
		ImGuiMCP::Spacing();

		RenderStartingCarryWeightSection();
		ImGuiMCP::Spacing();

		RenderDebugSection();
		ImGuiMCP::Spacing();

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
