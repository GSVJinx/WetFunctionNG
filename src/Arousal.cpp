#include "Arousal.h"

#include <Windows.h>

namespace WFNG::Arousal
{
	namespace
	{
		using GetArousalExtFn = float (*)(RE::Actor*);
		using SLAGetArousalIntFn = std::int32_t (*)(RE::Actor*);

		GetArousalExtFn   g_getArousalExt = nullptr;
		SLAGetArousalIntFn g_slaNGGetArousalInt = nullptr;
		RE::TESFaction*   g_slaArousal = nullptr;
	}

	void Init()
	{
		// SexLab Aroused NG (SexlabArousedNG.dll): native export, checked first as the most current source.
		// See include/ArousalAPI.h in crajjjj/SexlabArousedNG - SLA_GetArousalInt is already clamped 0-100.
		if (const auto module = ::GetModuleHandleW(L"SexlabArousedNG.dll")) {
			g_slaNGGetArousalInt = reinterpret_cast<SLAGetArousalIntFn>(::GetProcAddress(module, "SLA_GetArousalInt"));
		}
		if (!g_slaNGGetArousalInt) {
			if (const auto module = ::GetModuleHandleW(L"OSLAroused.dll")) {
				g_getArousalExt = reinterpret_cast<GetArousalExtFn>(::GetProcAddress(module, "GetArousalExt"));
			}
		}
		g_slaArousal = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(0x3FC36, "SexLabAroused.esm"sv);
		logger::info("Arousal source: {}", g_slaNGGetArousalInt ? "SexlabArousedNG.dll SLA_GetArousalInt" : (g_getArousalExt ? "OSLAroused.dll GetArousalExt" : (g_slaArousal ? "sla_Arousal faction rank" : "none")));
	}

	bool IsAvailable()
	{
		return g_slaNGGetArousalInt || g_getArousalExt || g_slaArousal;
	}

	std::int32_t Get(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return -1;
		}
		// fresh value without advancing the framework's own state; the faction rank lags behind its ticker.
		// A framework answers garbage (e.g. -1869) for actors it has not initialised yet, such as the player
		// right after a new game; anything outside 0-100 means "unknown"
		std::int32_t value = -1;
		if (g_slaNGGetArousalInt) {
			value = g_slaNGGetArousalInt(a_actor);
		} else if (g_getArousalExt) {
			const float raw = g_getArousalExt(a_actor);
			value = std::isfinite(raw) ? static_cast<std::int32_t>(raw) : -1;
		} else if (g_slaArousal) {
			value = a_actor->GetFactionRank(g_slaArousal, a_actor->IsPlayerRef());
		}
		return value >= 0 && value <= 100 ? value : -1;
	}
}
