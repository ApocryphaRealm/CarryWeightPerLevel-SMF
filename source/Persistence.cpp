#include "Persistence.h"

#include "utils/Logger.h"

namespace persistence
{
	namespace
	{
		// Explicit big-endian byte construction rather than a multi-char literal ('CWPL') -
		// the value of a multi-char literal is implementation-defined, which is exactly the
		// kind of "looks portable but isn't" trap this project's own CLAUDE.md gotchas section
		// warns about (see the Windows-path/backslash-mangling gotcha for the same category of
		// problem). Building the bytes explicitly makes the on-disk record type unambiguous.
		constexpr std::uint32_t MakeSignature(char a_a, char a_b, char a_c, char a_d)
		{
			return (static_cast<std::uint32_t>(static_cast<unsigned char>(a_a)) << 24) |
			       (static_cast<std::uint32_t>(static_cast<unsigned char>(a_b)) << 16) |
			       (static_cast<std::uint32_t>(static_cast<unsigned char>(a_c)) << 8) |
			       static_cast<std::uint32_t>(static_cast<unsigned char>(a_d));
		}

		// This plugin's unique co-save signature, and the one record type it writes.
		constexpr std::uint32_t kPluginSignature = MakeSignature('C', 'W', 'P', 'L');
		constexpr std::uint32_t kCatchUpRecordType = MakeSignature('C', 'A', 'T', 'C');
		constexpr std::uint32_t kCatchUpRecordVersion = 1;

		float totalCatchUpGranted = 0.0F;

		void OnGameSaved(SKSE::SerializationInterface* a_intfc)
		{
			if (!a_intfc)
			{
				logger::error("OnGameSaved: null serialization interface; the catch-up total was not written to this save");

				return;
			}

			if (!a_intfc->OpenRecord(kCatchUpRecordType, kCatchUpRecordVersion))
			{
				logger::error("OnGameSaved: OpenRecord failed for the catch-up total; it will not be written to this save");

				return;
			}

			if (!a_intfc->WriteRecordData(totalCatchUpGranted))
			{
				logger::error("OnGameSaved: failed to write the catch-up total ({:.2f})", totalCatchUpGranted);

				return;
			}

			logger::debug("OnGameSaved: wrote catch-up total {:.2f}", totalCatchUpGranted);
		}

		void OnGameLoaded(SKSE::SerializationInterface* a_intfc)
		{
			if (!a_intfc)
			{
				logger::error("OnGameLoaded: null serialization interface; assuming no catch-up has been granted yet");

				totalCatchUpGranted = 0.0F;

				return;
			}

			// Reset first: a save with no record of ours at all (this plugin was installed
			// after that save was made) should read back as "nothing granted yet", not
			// whatever the previously loaded save happened to leave behind.
			totalCatchUpGranted = 0.0F;

			std::uint32_t type = 0;
			std::uint32_t version = 0;
			std::uint32_t length = 0;

			while (a_intfc->GetNextRecordInfo(type, version, length))
			{
				if (type != kCatchUpRecordType)
				{
					logger::trace("OnGameLoaded: skipping unrecognized record type {:#010x}", type);

					continue;
				}

				if (version != kCatchUpRecordVersion)
				{
					logger::warn("OnGameLoaded: catch-up record is version {}, expected {}; treating it as absent",
						version, kCatchUpRecordVersion);

					continue;
				}

				float value = 0.0F;

				if (a_intfc->ReadRecordData(value) != sizeof(value))
				{
					logger::warn("OnGameLoaded: catch-up record was the wrong size; treating it as absent");

					continue;
				}

				totalCatchUpGranted = value;

				logger::debug("OnGameLoaded: read catch-up total {:.2f}", totalCatchUpGranted);
			}
		}

		void OnRevert(SKSE::SerializationInterface*)
		{
			// Fired before a different save loads (or on returning to the main menu). Reset to
			// the safe default so a stale value from the previous save can never leak into
			// whichever save (or new game) comes next - OnGameLoaded supplies the real value
			// if the next save actually has one.
			logger::debug("OnRevert: clearing the in-memory catch-up total");

			totalCatchUpGranted = 0.0F;
		}
	}

	void Init()
	{
		const SKSE::SerializationInterface* serialization = SKSE::GetSerializationInterface();

		if (!serialization)
		{
			logger::error("Init: SKSE::GetSerializationInterface() returned null; the retroactive "
						  "catch-up feature will not persist across saves this session");

			return;
		}

		serialization->SetUniqueID(kPluginSignature);
		serialization->SetSaveCallback(OnGameSaved);
		serialization->SetLoadCallback(OnGameLoaded);
		serialization->SetRevertCallback(OnRevert);

		logger::debug("Init: registered SKSE co-save callbacks (signature {:#010x})", kPluginSignature);
	}

	float GetTotalCatchUpGranted()
	{
		return totalCatchUpGranted;
	}

	void SetTotalCatchUpGranted(float a_value)
	{
		totalCatchUpGranted = a_value;

		logger::debug("SetTotalCatchUpGranted: running total is now {:.2f} (persists on the next game save)", totalCatchUpGranted);
	}
}
