#pragma once

#include "Settings.h"
#include "Visuals.h"

namespace WFNG
{
	enum class Phase : std::uint8_t
	{
		kDry,
		kSweat,
		kSoaked
	};

	// Per-actor state of WetFunctionEffect.psc (Wet Function Redux + OSweat), kept by the plugin.
	struct ActorRecord
	{
		// saved in the co-save
		float  wetness{ 0.0f };
		float  forceWetness{ -1.0f };
		float  forceSpecular{ 0.0f };
		float  forceGlossiness{ 0.0f };
		double autoStop{ 0.0 };  // game days; 0 = not auto-applied
		bool   active{ false };
		bool   manual{ false };  // started by the player/console, never auto-stops
		bool   seeded{ false };  // first start bonus already granted
		Phase  phase{ Phase::kDry };
		bool   hasDrops{ false };
		bool   hasSweat{ false };
		float  dryDyn{ 0.0f };
		float  maxDyn{ 0.0f };
		float  recentStrength{ 0.0f };

		// runtime
		bool                                  hasPussy{ false };
		bool                                  hasSchlong{ false };
		float                                 rate{ 0.0f };
		float                                 rateActivity{ 0.0f };  // stamina, magicka, movement, work
		float                                 rateWeather{ 0.0f };
		float                                 rateHeat{ 0.0f };  // drying next to a fire
		float                                 rateArousal{ 0.0f };
		std::int32_t                          arousal{ -1 };
		std::string_view                      weather{ "?" };
		float                                 sexRate{ 0.0f };
		float                                 lastGameTime{ -1.0f };
		float                                 lastStamina{ 1.0f };
		float                                 lastMagicka{ 1.0f };
		std::int32_t                          ostimClimaxes{ -1 };
		std::chrono::steady_clock::time_point nextUpdate{};
	};

	namespace Wetness
	{
		void          Start(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings);
		void          Update(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings);
		float         Strength(const ActorRecord& a_record, const Settings& a_settings);
		Visuals::Look ComputeLook(const ActorRecord& a_record, const Settings& a_settings);
		float         GameDays();
		std::string   PhaseName(Phase a_phase);
	}
}
