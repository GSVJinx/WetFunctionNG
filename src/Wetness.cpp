#include "Wetness.h"

#include "Arousal.h"
#include "Devious.h"
#include "Heat.h"

namespace WFNG::Wetness
{
	namespace
	{
		// wetfunctionfurniture10 (WetFunction.esp): anvils, grindstones, wood chopping, smelters, tanning racks...
		const std::unordered_set<RE::FormID> kWorkFurniture{
			0x0001A2AD, 0x000D932F, 0x000CAE0B, 0x000BBCF1, 0x000BF9E1, 0x0006E9C2, 0x0009C6CE, 0x000727A1, 0x0009C6DF, 0x000BD15D,
			0x000F9AF8, 0x000BD15C, 0x000613A6, 0x000613A7, 0x000E2BC7, 0x000F9ACC, 0x00071C47, 0x0007022E, 0x0010F575, 0x000A91BF
		};

		float Percent(RE::Actor* a_actor, RE::ActorValue a_value)
		{
			auto*       owner = a_actor->AsActorValueOwner();
			const float max = owner->GetPermanentActorValue(a_value);
			return max > 0.0f ? std::clamp(owner->GetActorValue(a_value) / max, 0.0f, 1.0f) : 1.0f;
		}

		// Weather.GetClassification: pleasant, cloudy, rainy, snow, none (also indoors)
		float WeatherRate(const RE::TESWeather* a_weather, bool a_interior, const Settings& a_settings, std::string_view& a_name)
		{
			if (a_interior) {
				a_name = "indoors"sv;
				return a_settings.fWeatherNone;
			}
			if (!a_weather) {
				a_name = "none"sv;
				return a_settings.fWeatherNone;
			}
			using Flag = RE::TESWeather::WeatherDataFlag;
			const auto flags = a_weather->data.flags;
			if (flags.any(Flag::kPleasant)) {
				a_name = "pleasant"sv;
				return a_settings.fWeatherPleasant;
			}
			if (flags.any(Flag::kCloudy)) {
				a_name = "cloudy"sv;
				return a_settings.fWeatherCloudy;
			}
			if (flags.any(Flag::kRainy)) {
				a_name = "rain"sv;
				return a_settings.fWeatherRainy;
			}
			if (flags.any(Flag::kSnow)) {
				a_name = "snow"sv;
				return a_settings.fWeatherSnow;
			}
			a_name = "unclassified"sv;
			return a_settings.fWeatherNone;
		}

		bool IsWorking(RE::Actor* a_actor)
		{
			if (a_actor->AsActorState()->GetSitSleepState() != RE::SIT_SLEEP_STATE::kIsSitting) {
				return false;
			}
			const auto furniture = a_actor->GetOccupiedFurniture().get();
			const auto* base = furniture ? furniture->GetBaseObject() : nullptr;
			return base && kWorkFurniture.contains(base->GetFormID());
		}

		void Enter(ActorRecord& a_record, Phase a_phase, const Settings& a_settings)
		{
			a_record.phase = a_phase;
			switch (a_phase) {
			case Phase::kDry:
				a_record.hasDrops = false;
				a_record.hasSweat = false;
				break;
			case Phase::kSweat:
				a_record.hasSweat = true;
				a_record.maxDyn = a_settings.fWetnessStart;
				a_record.dryDyn = a_record.maxDyn;
				break;
			case Phase::kSoaked:
				a_record.hasDrops = true;
				a_record.dryDyn = a_settings.fWetnessDry;
				if (!a_record.hasSweat) {
					a_record.recentStrength = Strength(a_record, a_settings);
				}
				break;
			}
		}

		void UpdatePhase(ActorRecord& a_record, const Settings& a_settings)
		{
			const float wetness = a_record.wetness;
			switch (a_record.phase) {
			case Phase::kDry:
				if (wetness > a_settings.fWetnessSoaked) {
					Enter(a_record, Phase::kSoaked, a_settings);
				} else if (wetness > a_settings.fWetnessStart) {
					Enter(a_record, Phase::kSweat, a_settings);
				}
				break;
			case Phase::kSweat:
				if (wetness > a_settings.fWetnessSoaked) {
					Enter(a_record, Phase::kSoaked, a_settings);
					break;
				}
				if (wetness > a_record.maxDyn) {
					// the higher the sweat got, the closer to "dry" it has to fall before it vanishes
					const float dry = a_settings.fWetnessDry;
					const float start = a_settings.fWetnessStart;
					const float span = a_settings.fWetnessSoaked - start;
					a_record.maxDyn = wetness;
					a_record.dryDyn = span > 0.0f ? dry - (dry - start) * (1.0f - (wetness - start) / span) : dry;
				}
				if (wetness <= a_record.dryDyn) {
					Enter(a_record, Phase::kDry, a_settings);
				}
				break;
			case Phase::kSoaked:
				if (wetness <= a_record.dryDyn) {
					Enter(a_record, Phase::kDry, a_settings);
					break;
				}
				if (!a_record.hasSweat) {
					// late sweat: soaked by water while dry, then wetness climbs again by the given ratio.
					// WFR compared "strength + lateSweat > recent", which fired on the first tick.
					const float strength = Strength(a_record, a_settings);
					if (strength < a_record.recentStrength) {
						a_record.recentStrength = strength;
					} else if (strength > a_record.recentStrength + a_settings.fLateSweat) {
						a_record.hasSweat = true;
					}
				}
				break;
			}
		}
	}

	float GameDays()
	{
		auto* calendar = RE::Calendar::GetSingleton();
		return calendar ? calendar->GetCurrentGameTime() : 0.0f;
	}

	std::string PhaseName(Phase a_phase)
	{
		switch (a_phase) {
		case Phase::kSweat:
			return "sweat";
		case Phase::kSoaked:
			return "soaked";
		default:
			return "dry";
		}
	}

	float Strength(const ActorRecord& a_record, const Settings& a_settings)
	{
		if (a_record.phase == Phase::kDry) {
			return 0.0f;
		}
		const float span = a_settings.fWetnessCap - a_settings.fWetnessDry;
		return span > 0.0f ? std::clamp((a_record.wetness - a_record.dryDyn) / span, 0.0f, 1.0f) : 0.0f;
	}

	void Start(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings)
	{
		a_record.active = true;
		a_record.lastGameTime = GameDays();
		a_record.lastStamina = Percent(a_actor, RE::ActorValue::kStamina);
		a_record.lastMagicka = Percent(a_actor, RE::ActorValue::kMagicka);
		a_record.nextUpdate = {};
		// WetFunctionEffect.OnEffectStart always entered the Dry state; the phase follows on the first update
		a_record.phase = Phase::kDry;
		a_record.hasDrops = false;
		a_record.hasSweat = false;
		if (!a_record.seeded) {
			// first start: pretend the current rate already ran for a while (WFR subtracted the hours as
			// days), plus a flat and a random bonus
			a_record.seeded = true;
			a_record.lastGameTime -= a_settings.fAutoBonusHours / 24.0f;
			static std::mt19937 rng{ std::random_device{}() };
			const float         random = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
			a_record.wetness = std::clamp(a_settings.fAutoBonusNormal + a_settings.fAutoBonusRandom * random, 0.0f, a_settings.fWetnessCap);
		}
	}

	void Update(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings)
	{
		const float now = GameDays();
		const float hours = a_record.lastGameTime >= 0.0f ? std::max(0.0f, (now - a_record.lastGameTime) * 24.0f) : 0.0f;
		a_record.lastGameTime = now;
		const float cap = a_settings.fWetnessCap;

		bool        direct = false;
		const float forced = a_record.forceWetness >= 0.0f ? a_record.forceWetness : a_settings.fWetnessForce;
		if (forced >= 0.0f) {
			a_record.wetness = std::min(forced, cap);
			direct = true;
		} else if (a_actor->AsActorState()->IsSwimming() && a_actor->IsInWater()) {
			// the swimming flag alone was set on the player during a new game start (wetness jumped to the cap)
			a_record.wetness = cap;
			a_record.recentStrength = 1.0f;  // no sweat just because the water came back
			direct = true;
		}

		// OSweat: a wet schlong from a quarter of the cap, the pussy texture from arousal when configured
		a_record.hasSchlong = a_record.wetness > cap * 0.25f;
		a_record.hasPussy = a_record.hasSchlong;
		const auto arousal = Arousal::Get(a_actor);
		if (arousal >= 0 && a_settings.bUseArousalThreshold) {
			a_record.hasPussy = static_cast<float>(arousal) > a_settings.fArousedThreshold;
		}
		// a chastity belt/cage physically covers what the pussy/schlong texture would show
		if (a_settings.bDeviousBlockGenitals && Devious::BlocksGenitals(a_actor)) {
			a_record.hasPussy = false;
			a_record.hasSchlong = false;
		}

		const float stamina = Percent(a_actor, RE::ActorValue::kStamina);
		const float magicka = Percent(a_actor, RE::ActorValue::kMagicka);
		if (!direct) {
			float used = 0.0f;
			if (stamina < a_record.lastStamina) {
				used += (a_record.lastStamina - stamina) * a_settings.fGenerateStamina;
			}
			if (magicka < a_record.lastMagicka) {
				used += (a_record.lastMagicka - magicka) * a_settings.fGenerateMagicka;
			}
			float activity = hours > 0.0f ? used / hours : 0.0f;
			if (a_actor->AsActorState()->IsSprinting()) {
				activity += a_actor->IsOnMount() ? a_settings.fGenerateGallop : a_settings.fGenerateSprinting;
			} else if (a_actor->IsRunning()) {
				activity += a_settings.fGenerateRunning;
			} else if (a_actor->IsSneaking()) {
				activity += a_settings.fGenerateSneaking;
			}
			if (IsWorking(a_actor)) {
				activity += a_settings.fGenerateWorking;
			}

			float weather = 0.0f;
			if (auto* sky = RE::Sky::GetSingleton()) {
				auto*            cell = a_actor->GetParentCell();
				const bool       interior = cell && cell->IsInteriorCell();
				const float      t = std::clamp(sky->currentWeatherPct, 0.0f, 1.0f);
				std::string_view outgoing;
				weather += t * WeatherRate(sky->currentWeather, interior, a_settings, a_record.weather);
				if (t < 1.0f) {
					weather += (1.0f - t) * WeatherRate(sky->lastWeather, interior, a_settings, outgoing);
				}
			}

			// arousal makes skin sweaty but does not keep an actor soaked (WFR/OSweat added it without limit,
			// so an aroused actor never dried in the rain or after swimming)
			const float aroused = arousal >= 0 && a_record.wetness < a_settings.fWetnessSoaked ? static_cast<float>(arousal) * a_settings.fGenerateArousal * 0.01f : 0.0f;

			// a fire dries an actor faster; nothing to dry, nothing to look up
			const float heat = a_record.wetness > 0.0f && Heat::IsNear(a_actor, a_settings) ? -a_settings.fHeatDrying : 0.0f;

			a_record.rateActivity = activity;
			a_record.rateWeather = weather;
			a_record.rateHeat = heat;
			a_record.rateArousal = aroused;
			a_record.arousal = arousal;
			float rate = activity + weather + heat + aroused + a_record.sexRate;
			if (!a_actor->IsPlayerRef()) {
				rate += a_settings.fOtherAdd;
				rate *= a_settings.fOtherMult;
			}
			rate *= a_settings.fMultGlobal;

			a_record.rate = rate;
			a_record.wetness = std::clamp(a_record.wetness + rate * hours, 0.0f, cap);
		}
		a_record.lastStamina = stamina;
		a_record.lastMagicka = magicka;

		UpdatePhase(a_record, a_settings);
	}

	Visuals::Look ComputeLook(const ActorRecord& a_record, const Settings& a_settings)
	{
		Visuals::Look look;
		look.active = a_record.active;
		look.drops = a_record.hasDrops;
		look.sweat = a_record.hasSweat;
		look.pussy = a_record.hasPussy;
		look.schlong = a_record.hasSchlong;

		const bool  wet = a_record.phase != Phase::kDry;
		const float strength = Strength(a_record, a_settings);
		const float forcedSpecular = a_record.forceSpecular > 0.0f ? a_record.forceSpecular : a_settings.fSpecularForce;
		const float forcedGlossiness = a_record.forceGlossiness > 0.0f ? a_record.forceGlossiness : a_settings.fGlossinessForce;
		// dry actors keep their own shader values; WFR forced specularMin on them
		look.specular = forcedSpecular > 0.0f ? forcedSpecular : (wet ? a_settings.fSpecularMin + (a_settings.fSpecularMax - a_settings.fSpecularMin) * strength : -1.0f);
		look.glossiness = forcedGlossiness > 0.0f ? forcedGlossiness : (wet ? a_settings.fGlossinessMin + (a_settings.fGlossinessMax - a_settings.fGlossinessMin) * strength : -1.0f);
		return look;
	}
}
