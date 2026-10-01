#include "Devious.h"

#include <Windows.h>

namespace WFNG::Devious
{
	namespace
	{
		constexpr std::uint32_t kApiVersion = 2;  // DD_APIVERSION in IHateMyKite/DeviousDevicesNG include/API.h

		DeviousDevicesAPI* g_api = nullptr;
	}

	void Init()
	{
		const auto module = ::LoadLibraryW(L"DeviousDevices.dll");
		if (!module) {
			logger::info("Devious Devices: absent");
			return;
		}
		const auto getAPI = reinterpret_cast<DeviousDevicesAPI* (*)()>(::GetProcAddress(module, "GetAPI"));
		auto*      api = getAPI ? getAPI() : nullptr;
		if (!api) {
			logger::warn("Devious Devices: DeviousDevices.dll found but GetAPI failed");
			return;
		}
		if (api->GetVersion() != kApiVersion) {
			logger::warn("Devious Devices: API version {} does not match the {} this build expects; disabling the integration", api->GetVersion(), kApiVersion);
			return;
		}
		g_api = api;
		logger::info("Devious Devices: found, API version {}", kApiVersion);
	}

	bool IsAvailable()
	{
		return g_api != nullptr;
	}

	BondageState Get(RE::Actor* a_actor)
	{
		return g_api && a_actor ? g_api->GetBondageState(a_actor) : sNone;
	}

	bool BlocksGenitals(RE::Actor* a_actor)
	{
		return (Get(a_actor) & (sChastifiedGenital | sChastifiedAnal)) != 0;
	}
}
