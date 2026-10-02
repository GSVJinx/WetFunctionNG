#include "Skee.h"

#include "SkeeNatives.h"

namespace WFNG::Skee
{
	namespace
	{
		IOverrideInterface* g_overrides = nullptr;
	}

	bool Connect()
	{
		InterfaceExchangeMessage message;
		// SKEE listens to every sender and fills interfaceMap in place
		SKSE::GetMessagingInterface()->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface, &message, sizeof(message), nullptr);
		if (!message.interfaceMap) {
			logger::warn("RaceMenu (skee64) did not answer the interface exchange; will try its NiOverride natives after data load");
			return false;
		}

		auto* found = message.interfaceMap->QueryInterface("Override");
		if (!found) {
			logger::info("skee64 has no Override interface (RaceMenu before 0.4.19); will use its NiOverride natives after data load");
			return false;
		}
		const auto version = found->GetVersion();
		if (version < IOverrideInterface::kPluginVersion2) {
			logger::warn("skee64 Override interface version {} is too old (need 2, RaceMenu 0.4.19+); will use its NiOverride natives after data load", version);
			return false;
		}
		g_overrides = static_cast<IOverrideInterface*>(found);
		logger::info("Connected to skee64 Override interface version {}", version);
		return true;
	}

	bool ConnectLegacy()
	{
		if (!g_overrides) {
			g_overrides = Skee::ConnectNatives();
		}
		return g_overrides != nullptr;
	}

	IOverrideInterface* Overrides()
	{
		return g_overrides;
	}
}
