#include "OStim.h"

#include "Manager.h"
#include "OStimAPI.h"

#include <Windows.h>

namespace WFNG::OStim
{
	namespace
	{
		using namespace OstimNG_API::Thread;

		IThreadInterface* g_api = nullptr;

		void OnThreadEvent(ThreadEvent a_event, std::uint32_t a_thread, void*)
		{
			Event event;
			switch (a_event) {
			case ThreadEvent::ThreadStarted:
				event = Event::kStarted;
				break;
			case ThreadEvent::ThreadEnded:
				event = Event::kEnded;
				break;
			case ThreadEvent::NodeChanged:
				event = Event::kChanged;
				break;
			default:
				return;
			}
			SKSE::GetTaskInterface()->AddTask([event, a_thread]() { Manager::Get().OnOStim(event, a_thread); });
		}
	}

	bool Init()
	{
		const auto module = ::GetModuleHandleW(L"OStim.dll");
		if (!module) {
			logger::info("OStim not installed");
			return false;
		}
		const auto request = reinterpret_cast<RequestPluginAPI_Thread>(::GetProcAddress(module, "RequestPluginAPI_Thread"));
		if (!request) {
			logger::warn("OStim.dll has no RequestPluginAPI_Thread export (OStim Standalone 7.5+ needed); OStim integration disabled");
			return false;
		}
		const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
		g_api = request(InterfaceVersion::V1, plugin->GetName().data(), plugin->GetVersion());
		if (!g_api) {
			logger::warn("OStim refused the thread API request; OStim integration disabled");
			return false;
		}
		g_api->RegisterEventCallback(OnThreadEvent, nullptr);
		logger::info("Connected to OStim thread API");
		return true;
	}

	bool IsAvailable()
	{
		return g_api != nullptr;
	}

	bool IsThreadValid(std::uint32_t a_thread)
	{
		return g_api && g_api->IsThreadValid(a_thread);
	}

	std::uint32_t PlayerThread()
	{
		return g_api ? g_api->GetPlayerThreadID() : 0;
	}

	std::vector<Participant> Participants(std::uint32_t a_thread)
	{
		std::vector<Participant> result;
		if (!g_api || !g_api->IsThreadValid(a_thread)) {
			return result;
		}
		std::array<ActorData, 16> buffer{};
		const auto                count = g_api->GetActors(a_thread, buffer.data(), static_cast<std::uint32_t>(buffer.size()));
		for (std::uint32_t i = 0; i < count && i < buffer.size(); ++i) {
			const auto& data = buffer[i];
			result.push_back({ data.formID, data.excitement, data.isFemale, data.hasSchlong, data.timesClimaxed });
		}
		return result;
	}
}
