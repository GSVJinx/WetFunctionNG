#include "Papyrus.h"

#include "Manager.h"
#include "OStim.h"
#include "Settings.h"
#include "Visuals.h"

namespace WFNG::Papyrus
{
	namespace
	{
		constexpr auto kScript = "WetFunctionNG"sv;

		using Tag = RE::StaticFunctionTag;

		// natives run on a VM thread: every game change is queued onto the main thread
		void Queue(RE::Actor* a_actor, std::function<void(RE::Actor*)> a_work)
		{
			if (!a_actor) {
				return;
			}
			SKSE::GetTaskInterface()->AddTask([handle = a_actor->GetHandle(), work = std::move(a_work)]() {
				if (const auto actor = handle.get()) {
					work(actor.get());
				}
			});
		}

		void ReloadSettings(Tag*)
		{
			SKSE::GetTaskInterface()->AddTask([]() {
				Settings::Load();
				Manager::Get().OnSettingsChanged();
			});
		}

		void SexLabUpdate(Tag*, RE::TESForm* a_thread, std::vector<RE::Actor*> a_actors, std::vector<float> a_enjoyment, std::vector<float> a_pain, std::int32_t a_phase)
		{
			std::vector<RE::ActorHandle> handles;
			for (auto* actor : a_actors) {
				handles.push_back(actor ? actor->GetHandle() : RE::ActorHandle{});
			}
			const auto thread = a_thread ? a_thread->GetFormID() : 0;
			SKSE::GetTaskInterface()->AddTask([thread, handles, enjoyment = std::move(a_enjoyment), pain = std::move(a_pain), a_phase]() {
				std::vector<RE::Actor*> actors;
				for (const auto& handle : handles) {
					const auto actor = handle.get();
					actors.push_back(actor.get());
				}
				Manager::Get().OnSexLab(thread, actors, enjoyment, pain, a_phase);
			});
		}

		bool IsSexLabAvailable(Tag*)
		{
			return Manager::Get().SexLabAvailable();
		}

		bool IsOStimAvailable(Tag*)
		{
			return OStim::IsAvailable();
		}

		void SexLabOrgasm(Tag*, RE::Actor* a_actor)
		{
			Queue(a_actor, [](RE::Actor* a) { Manager::Get().OnSexLabOrgasm(a); });
		}

		bool StartEffect(Tag*, RE::Actor* a_actor)
		{
			Queue(a_actor, [](RE::Actor* a) { Manager::Get().StartManual(a); });
			return a_actor != nullptr;
		}

		void StopEffect(Tag*, RE::Actor* a_actor)
		{
			Queue(a_actor, [](RE::Actor* a) { Manager::Get().StopManual(a); });
		}

		bool IsActive(Tag*, RE::Actor* a_actor)
		{
			return Manager::Get().IsActive(a_actor);
		}

		float GetWetness(Tag*, RE::Actor* a_actor)
		{
			return Manager::Get().GetWetness(a_actor);
		}

		void SetWetness(Tag*, RE::Actor* a_actor, float a_wetness)
		{
			Queue(a_actor, [a_wetness](RE::Actor* a) { Manager::Get().SetWetness(a, a_wetness); });
		}

		void ForceValues(Tag*, RE::Actor* a_actor, float a_wetness, float a_specular, float a_glossiness)
		{
			Queue(a_actor, [=](RE::Actor* a) { Manager::Get().ForceValues(a, a_wetness, a_specular, a_glossiness); });
		}

		std::string GetStatus(Tag*, RE::Actor* a_actor)
		{
			return Manager::Get().Status(a_actor);
		}

		void PrintStatus(Tag*, RE::Actor* a_actor)
		{
			Queue(a_actor, [](RE::Actor* a) { Visuals::Out(Manager::Get().Status(a)); });
		}

		void Report(Tag*, RE::Actor* a_actor)
		{
			Queue(a_actor, [](RE::Actor* a) {
				Visuals::Out(Manager::Get().Status(a));
				Visuals::Dump(a, "report");
			});
		}

		void CleanActor(Tag*, RE::Actor* a_actor)
		{
			Queue(a_actor, [](RE::Actor* a) {
				const auto removed = Visuals::Clean(a);
				Visuals::Out(std::format("[WFNG] {}: removed {} leftover wetness overrides", a->GetName(), removed));
				Manager::Get().Invalidate(a->GetFormID(), 500ms);
			});
		}

		void StopAll(Tag*, bool a_clearData)
		{
			SKSE::GetTaskInterface()->AddTask([a_clearData]() { Manager::Get().StopAll(a_clearData); });
		}
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("ReloadSettings"sv, kScript, ReloadSettings);
		a_vm->RegisterFunction("IsSexLabAvailable"sv, kScript, IsSexLabAvailable);
		a_vm->RegisterFunction("IsOStimAvailable"sv, kScript, IsOStimAvailable);
		a_vm->RegisterFunction("SexLabUpdate"sv, kScript, SexLabUpdate);
		a_vm->RegisterFunction("SexLabOrgasm"sv, kScript, SexLabOrgasm);
		a_vm->RegisterFunction("StartEffect"sv, kScript, StartEffect);
		a_vm->RegisterFunction("StopEffect"sv, kScript, StopEffect);
		a_vm->RegisterFunction("IsActive"sv, kScript, IsActive);
		a_vm->RegisterFunction("GetWetness"sv, kScript, GetWetness);
		a_vm->RegisterFunction("SetWetness"sv, kScript, SetWetness);
		a_vm->RegisterFunction("ForceValues"sv, kScript, ForceValues);
		a_vm->RegisterFunction("GetStatus"sv, kScript, GetStatus);
		a_vm->RegisterFunction("PrintStatus"sv, kScript, PrintStatus);
		a_vm->RegisterFunction("Report"sv, kScript, Report);
		a_vm->RegisterFunction("CleanActor"sv, kScript, CleanActor);
		a_vm->RegisterFunction("StopAll"sv, kScript, StopAll);
		return true;
	}
}
