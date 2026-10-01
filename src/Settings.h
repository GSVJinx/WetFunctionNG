#pragma once

namespace WFNG
{
	// X(section, key, default). Keys match Data/SKSE/Plugins/WetFunctionNG.ini.
	// Keep one entry per line so the defaults remain easy to validate against the shipped INI.
#define WFNG_FLOAT_SETTINGS(X)                  \
	X(Wetness, fWetnessDry, 1.0f)               \
	X(Wetness, fWetnessStart, 2.0f)             \
	X(Wetness, fWetnessSoaked, 4.0f)            \
	X(Wetness, fWetnessCap, 10.0f)              \
	X(Wetness, fWetnessForce, -1.0f)            \
	X(Wetness, fLateSweat, 0.1f)                \
	X(Sources, fGenerateStamina, 1.5f)          \
	X(Sources, fGenerateMagicka, 2.0f)          \
	X(Sources, fGenerateSprinting, 1.0f)        \
	X(Sources, fGenerateRunning, 0.5f)          \
	X(Sources, fGenerateSneaking, 0.3f)         \
	X(Sources, fGenerateGallop, 0.3f)           \
	X(Sources, fGenerateWorking, 0.0f)          \
	X(Sources, fOtherAdd, 0.0f)                 \
	X(Sources, fOtherMult, 1.0f)                \
	X(Sources, fMultGlobal, 1.0f)               \
	X(Weather, fWeatherPleasant, -2.0f)         \
	X(Weather, fWeatherCloudy, -0.2f)           \
	X(Weather, fWeatherRainy, 3.0f)             \
	X(Weather, fWeatherSnow, 0.5f)              \
	X(Weather, fWeatherNone, -2.0f)             \
	X(Heat, fHeatDrying, 4.0f)                  \
	X(Heat, fHeatRange, 400.0f)                 \
	X(Arousal, fGenerateArousal, 1.5f)          \
	X(Arousal, fArousedThreshold, 75.0f)        \
	X(SexLab, fSexLabBase, 3.0f)                \
	X(SexLab, fSexLabEnjoyment, 6.0f)           \
	X(SexLab, fSexLabPain, -1.0f)               \
	X(SexLab, fSexLabOrgasm, 2.0f)              \
	X(SexLab, fSexLabAutoLinger, 12.0f)         \
	X(OStim, fOStimBase, 3.0f)                  \
	X(OStim, fOStimExcitement, 6.0f)            \
	X(OStim, fOStimOrgasm, 2.0f)                \
	X(OStim, fOStimAutoLinger, 12.0f)           \
	X(Specular, fSpecularMin, 3.0f)             \
	X(Specular, fSpecularMax, 12.0f)            \
	X(Specular, fSpecularForce, 0.0f)           \
	X(Specular, fHeadSpecularMult, 1.0f)        \
	X(Specular, fHandSpecularMult, 1.0f)        \
	X(Glossiness, fGlossinessMin, 40.0f)        \
	X(Glossiness, fGlossinessMax, 25.0f)        \
	X(Glossiness, fGlossinessForce, 0.0f)       \
	X(General, fLoopTimePC, 2.0f)               \
	X(General, fLoopTimeNPC, 4.0f)              \
	X(General, fLoopTimeForced, 10.0f)          \
	X(AutoApply, fAutoTimeout, 8.0f)            \
	X(AutoApply, fAutoRange, 5000.0f)           \
	X(AutoApply, fAutoDuration, 1.0f)           \
	X(AutoApply, fAutoBonusHours, 1.0f)         \
	X(AutoApply, fAutoBonusNormal, 0.0f)        \
	X(AutoApply, fAutoBonusRandom, 0.0f)        \
	X(Widget, fWidgetX, 0.035f)                 \
	X(Widget, fWidgetY, 0.62f)                  \
	X(Widget, fWidgetSize, 46.0f)               \
	X(Widget, fWidgetOpacity, 0.9f)

#define WFNG_BOOL_SETTINGS(X)                   \
	X(Arousal, bUseArousalThreshold, true)      \
	X(Heat, bHeatDrying, true)                  \
	X(Heat, bHeatSunHelm, true)                 \
	X(Widget, bWidgetEnabled, true)             \
	X(Widget, bWidgetAlways, false)             \
	X(SexLab, bSexLabEnabled, true)             \
	X(SexLab, bSexLabPlayerOnly, false)         \
	X(SexLab, bSexLabAuto, true)                \
	X(SexLab, bSexLabAutoPlayer, false)         \
	X(OStim, bOStimEnabled, true)               \
	X(OStim, bOStimPlayerOnly, false)           \
	X(OStim, bOStimAuto, true)                  \
	X(OStim, bOStimAutoPlayer, false)           \
	X(Devious, bDeviousBlockGenitals, true)     \
	X(Devious, bAutoDevious, true)              \
	X(Specular, bSpecularBody, true)            \
	X(Specular, bSpecularHands, true)           \
	X(Specular, bSpecularFeet, true)            \
	X(Specular, bSpecularHead, true)            \
	X(Glossiness, bGlossinessBody, false)       \
	X(Glossiness, bGlossinessHands, false)      \
	X(Glossiness, bGlossinessFeet, false)       \
	X(Glossiness, bGlossinessHead, false)       \
	X(Textures, bTextureBody, true)             \
	X(Textures, bTextureBodyDrops, true)        \
	X(Textures, bTextureBodySweat, true)        \
	X(Textures, bTextureBodyPussy, true)        \
	X(Textures, bTextureFeetDrops, true)        \
	X(Textures, bTextureFeetSweat, true)        \
	X(Textures, bTextureHands, true)            \
	X(Textures, bTextureHead, true)             \
	X(Textures, bTextureSchlong, true)          \
	X(Textures, bTextureFutaSchlong, false)     \
	X(Textures, bPreloadTextures, true)         \
	X(General, bPlayerEnabled, true)            \
	X(General, bFirstPerson, true)              \
	X(AutoApply, bAutoGlobal, false)            \
	X(AutoApply, bAutoNaked, false)             \
	X(AutoApply, bAutoFollower, true)           \
	X(AutoApply, bAutoFemale, true)             \
	X(AutoApply, bAutoMale, true)               \
	X(AutoApply, bAutoBeast, false)             \
	X(AutoApply, bAutoVampire, false)           \
	X(AutoApply, bAutoKeepWetness, true)

#define WFNG_INT_SETTINGS(X)                    \
	X(General, iLogLevel, 0)

	struct Settings
	{
#define WFNG_DECLARE(a_section, a_key, a_default) decltype(a_default) a_key{ a_default };
		WFNG_FLOAT_SETTINGS(WFNG_DECLARE)
		WFNG_BOOL_SETTINGS(WFNG_DECLARE)
		WFNG_INT_SETTINGS(WFNG_DECLARE)
#undef WFNG_DECLARE

		// snapshot used by the main thread; Load() replaces it
		static Settings& Get();
		static void      Load();
		static bool      Save();
		static bool      Reset();

		bool Set(std::string_view a_section, std::string_view a_key, const std::string& a_value);
	};
}
