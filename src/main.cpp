#include "Arousal.h"
#include "Devious.h"
#include "Heat.h"
#include "Manager.h"
#include "Menu.h"
#include "OStim.h"
#include "Papyrus.h"
#include "Settings.h"
#include "Skee.h"
#include "Widget.h"

namespace
{
	bool g_legacyInstalled = false;

	void InitializeLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
		}
		*path /= "WetFunctionNG.log";

		auto log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		using namespace WFNG;
		switch (a_message->type) {
		case SKSE::MessagingInterface::kPostPostLoad:
			Skee::Connect();
			Menu::Register();
			Widget::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Settings::Load();
			Arousal::Init();
			OStim::Init();
			Devious::Init();
			Heat::Refresh();
			Manager::Get().Init();
			g_legacyInstalled = RE::TESDataHandler::GetSingleton()->LookupModByName("WetFunction.esp"sv) != nullptr;
			if (g_legacyInstalled) {
				logger::warn("WetFunction.esp (Wet Function Redux) is still active: its scripts fight WetFunction NG over the same overrides");
			}
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			Manager::Get().SetReady(false);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Manager::Get().OnGameLoaded();
			Manager::Get().SetReady(true);
			if (!Skee::Overrides()) {
				RE::SendHUDMessage::ShowHUDMessage("WetFunction NG: RaceMenu (skee64) interface missing, wet visuals disabled");
			} else if (g_legacyInstalled) {
				RE::SendHUDMessage::ShowHUDMessage("WetFunction NG: disable Wet Function Redux (WetFunction.esp)");
			}
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLogging();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	logger::info("{} {} is loading", plugin->GetName(), plugin->GetVersion().string());

	SKSE::Init(a_skse);

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::stl::report_and_fail("Unable to register the SKSE message listener.");
	}
	WFNG::InstallSerialization();
	SKSE::GetPapyrusInterface()->Register(WFNG::Papyrus::Register);

	logger::info("{} loaded", plugin->GetName());
	return true;
}
