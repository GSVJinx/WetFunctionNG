#include "Skee.h"

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
			logger::critical("RaceMenu (skee64) did not answer the interface exchange; wetness visuals are disabled");
			return false;
		}

		auto* found = message.interfaceMap->QueryInterface("Override");
		if (!found) {
			logger::critical("skee64 has no Override interface");
			return false;
		}
		const auto version = found->GetVersion();
		if (version < IOverrideInterface::kPluginVersion2) {
			logger::critical("skee64 Override interface version {} is too old (need 2, RaceMenu 0.4.19+)", version);
			return false;
		}
		g_overrides = static_cast<IOverrideInterface*>(found);
		logger::info("Connected to skee64 Override interface version {}", version);
		return true;
	}

	IOverrideInterface* Overrides()
	{
		return g_overrides;
	}
}
