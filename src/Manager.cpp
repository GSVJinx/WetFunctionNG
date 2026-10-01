#include "Manager.h"

#include "Devious.h"
#include "Heat.h"
#include "Settings.h"
#include "Visuals.h"

namespace WFNG
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr std::uint32_t kUniqueID = 'WFNG';
		constexpr std::uint32_t kActorsRecord = 'ACTR';
		constexpr std::uint32_t kRecordVersion = 1;

		RE::Actor* LookupActor(RE::FormID a_id)
		{
			return a_id ? RE::TESForm::LookupByID<RE::Actor>(a_id) : nullptr;
		}

		bool HasKeyword(RE::Actor* a_actor, const RE::BGSKeyword* a_keyword)
		{
			if (!a_actor || !a_keyword) {
				return false;
			}
			const auto* base = a_actor->GetActorBase();
			const auto* race = a_actor->GetRace();
			return (base && base->HasKeyword(a_keyword)) || (race && race->HasKeyword(a_keyword));
		}

		// WFR's naked test: no worn body-slot item flagged ArmorCuirass or ClothingBody
		bool WornHasKeyword(RE::Actor* a_actor, RE::BGSKeyword* a_keyword)
		{
			if (!a_keyword) {
				return false;
			}
			const auto* armor = a_actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
			return armor && armor->HasKeyword(a_keyword);
		}

		std::chrono::milliseconds Seconds(float a_seconds)
		{
			return std::chrono::milliseconds(static_cast<std::int64_t>(std::max(a_seconds, 0.25f) * 1000.0f));
		}

		class EventSink final :
			public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
			public RE::BSTEventSink<RE::TESEquipEvent>,
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static EventSink* Get()
			{
				static EventSink singleton;
				return std::addressof(singleton);
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
			{
				if (a_event && a_event->loaded && Manager::Get().IsTracked(a_event->formID)) {
					const auto id = a_event->formID;
					SKSE::GetTaskInterface()->AddTask([id]() { Manager::Get().Invalidate(id, 500ms); });
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				if (a_event && a_event->actor) {
					const auto id = a_event->actor->GetFormID();
					SKSE::GetTaskInterface()->AddTask([id]() { Manager::Get().Invalidate(id, 300ms); });
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && !a_event->opening && a_event->menuName == RE::RaceSexMenu::MENU_NAME) {
					SKSE::GetTaskInterface()->AddTask([]() { Manager::Get().Invalidate(0x14, 200ms); });
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void OnSave(SKSE::SerializationInterface* a_intfc) { Manager::Get().Save(a_intfc); }
		void OnLoad(SKSE::SerializationInterface* a_intfc) { Manager::Get().Load(a_intfc); }
		void OnRevert(SKSE::SerializationInterface*) { Manager::Get().Revert(); }
	}

	void InstallSerialization()
	{
		auto* serialization = SKSE::GetSerializationInterface();
		serialization->SetUniqueID(kUniqueID);
		serialization->SetSaveCallback(OnSave);
		serialization->SetLoadCallback(OnLoad);
		serialization->SetRevertCallback(OnRevert);
	}

	Manager& Manager::Get()
	{
		static Manager singleton;
		return singleton;
	}

	void Manager::Init()
	{
		if (_started.exchange(true)) {
			return;
		}
		auto* data = RE::TESDataHandler::GetSingleton();
		_sexLabAvailable = data->LookupModByName("SexLab.esm"sv) != nullptr;
		logger::info("SexLab {}, OStim {}", _sexLabAvailable ? "found" : "absent", OStim::IsAvailable() ? "found" : "absent");
		_actorTypeNPC = data->LookupForm<RE::BGSKeyword>(0x013794, "Skyrim.esm"sv);
		_isBeastRace = data->LookupForm<RE::BGSKeyword>(0x0D61D1, "Skyrim.esm"sv);
		_vampire = data->LookupForm<RE::BGSKeyword>(0x0A82BB, "Skyrim.esm"sv);
		_armorCuirass = data->LookupForm<RE::BGSKeyword>(0x06C0EC, "Skyrim.esm"sv);
		_clothingBody = data->LookupForm<RE::BGSKeyword>(0x0A8657, "Skyrim.esm"sv);

		if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
			events->AddEventSink<RE::TESObjectLoadedEvent>(EventSink::Get());
			events->AddEventSink<RE::TESEquipEvent>(EventSink::Get());
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(EventSink::Get());
		}

		std::thread([this]() {
			for (;;) {
				std::this_thread::sleep_for(250ms);
				if (_ready) {
					QueueTick();
				}
			}
		}).detach();
		logger::info("Actor manager started");
	}

	void Manager::SetReady(bool a_ready)
	{
		_ready = a_ready;
		if (!a_ready) {
			_gaugeActive = false;
		}
	}

	Manager::Gauge Manager::PlayerGauge() const
	{
		return { _gaugeActive, _gaugeWetness, _gaugeRate };
	}

	void Manager::OnGameLoaded()
	{
		Heat::Refresh();
		std::scoped_lock lock(_lock);
		const auto       now = Clock::now();
		for (auto& [id, record] : _records) {
			record.lastGameTime = -1.0f;
			// Do not make every restored NPC traverse its skin on the same frame.
			record.nextUpdate = now + 5s + std::chrono::milliseconds(id % 2000);
			record.ostimClimaxes = -1;
		}
		_ostimThreads.clear();
		_sexLabThreads.clear();
	}

	void Manager::QueueTick()
	{
		if (_tickQueued.exchange(true)) {
			return;
		}
		SKSE::GetTaskInterface()->AddTask([this]() {
			_tickQueued = false;
			Tick();
		});
	}

	ActorRecord& Manager::Record(RE::FormID a_id)
	{
		return _records[a_id];
	}

	void Manager::Tick()
	{
		if (!_ready) {
			return;
		}
		if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Is3DLoaded()) {
			return;
		}
		const auto&      settings = Settings::Get();
		std::scoped_lock lock(_lock);
		const double     gameNow = Wetness::GameDays();
		const auto       now = Clock::now();

		const auto playerID = player->GetFormID();
		if (settings.bPlayerEnabled) {
			auto& record = Record(playerID);
			record.manual = true;
			record.autoStop = 0.0;
			if (!record.active) {
				Wetness::Start(player, record, settings);
				record.nextUpdate = now + 5s;  // a new game starts with unsettled actor states (false swimming flag)
			}
		} else if (const auto it = _records.find(playerID); it != _records.end() && it->second.active && it->second.manual) {
			Stop(playerID, true);
		}

		if (now >= _nextScan) {
			_nextScan = now + Seconds(settings.fAutoTimeout);
			Scan(settings);
		}

		static Clock::time_point nextOStim{};
		if (!_ostimThreads.empty() && now >= nextOStim) {
			nextOStim = now + 1s;
			std::vector<std::uint32_t> threads;
			for (const auto& [thread, scene] : _ostimThreads) {
				threads.push_back(thread);
			}
			for (const auto thread : threads) {
				if (OStim::IsThreadValid(thread)) {
					RefreshOStim(thread, settings);
				} else {
					OnOStim(OStim::Event::kEnded, thread);
				}
			}
		}

		std::vector<RE::FormID> stops;
		for (auto& [id, record] : _records) {
			if (!record.active) {
				continue;
			}
			auto* actor = LookupActor(id);
			if (!actor) {
				continue;
			}
			if (ShouldStop(actor, record, settings, gameNow)) {
				stops.push_back(id);
				continue;
			}
			if (actor->Is3DLoaded() && now >= record.nextUpdate) {
				UpdateActor(actor, record, settings);
			}
		}
		for (const auto id : stops) {
			Stop(id, settings.bAutoKeepWetness);
		}

		const auto playerRecord = _records.find(playerID);
		const bool tracked = playerRecord != _records.end() && playerRecord->second.active;
		_gaugeWetness = tracked ? playerRecord->second.wetness : 0.0f;
		_gaugeRate = tracked ? playerRecord->second.rate : 0.0f;
		_gaugeActive = tracked;

		std::erase_if(_records, [](const auto& a_entry) {
			const auto& record = a_entry.second;
			return !record.active && record.wetness <= 0.0f && record.sexRate == 0.0f && record.forceWetness < 0.0f && record.forceSpecular <= 0.0f &&
			       record.forceGlossiness <= 0.0f;
		});
	}

	void Manager::UpdateActor(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings)
	{
		Wetness::Update(a_actor, a_record, a_settings);
		Visuals::Apply(a_actor, Wetness::ComputeLook(a_record, a_settings), a_settings);
		if (a_settings.iLogLevel > 0) {
			logger::info("{}", Status(a_actor));
		}

		const bool  forced = a_record.forceWetness >= 0.0f || a_settings.fWetnessForce >= 0.0f;
		const float loop = forced ? a_settings.fLoopTimeForced : (a_actor->IsPlayerRef() ? a_settings.fLoopTimePC : a_settings.fLoopTimeNPC);
		a_record.nextUpdate = Clock::now() + Seconds(loop);
	}

	bool Manager::ShouldStop(RE::Actor* a_actor, const ActorRecord& a_record, const Settings& a_settings, double a_now) const
	{
		if (a_record.manual || a_record.autoStop <= 0.0) {
			return false;
		}
		if (a_record.autoStop < a_now || !a_actor->Is3DLoaded()) {
			return true;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (a_actor == player) {
			return false;
		}
		const auto* actorCell = a_actor->GetParentCell();
		const auto* playerCell = player->GetParentCell();
		if (actorCell != playerCell) {
			if ((actorCell && actorCell->IsInteriorCell()) || (playerCell && playerCell->IsInteriorCell())) {
				return true;
			}
			if (a_actor->GetPosition().GetDistance(player->GetPosition()) > 500.0f + 1.1f * a_settings.fAutoRange) {
				return true;
			}
		}
		return false;
	}

	void Manager::Stop(RE::FormID a_id, bool a_keepWetness)
	{
		const auto it = _records.find(a_id);
		if (it == _records.end()) {
			return;
		}
		auto&      record = it->second;
		const bool wasAuto = !record.manual;
		if (auto* actor = LookupActor(a_id); actor && actor->Is3DLoaded()) {
			Visuals::Clear(actor, Settings::Get());
		} else {
			Visuals::Forget(a_id);
		}
		record.active = false;
		record.manual = false;
		record.autoStop = 0.0;
		record.phase = Phase::kDry;
		record.hasDrops = false;
		record.hasSweat = false;
		if (!(a_keepWetness && wasAuto && record.wetness > 0.0f)) {
			record.wetness = 0.0f;
			record.seeded = false;
		}
	}

	bool Manager::PassesFilter(RE::Actor* a_actor, const Settings& a_settings) const
	{
		if (_actorTypeNPC && !HasKeyword(a_actor, _actorTypeNPC)) {
			return false;
		}
		if (!a_settings.bAutoBeast && HasKeyword(a_actor, _isBeastRace)) {
			return false;
		}
		if (!a_settings.bAutoVampire && HasKeyword(a_actor, _vampire)) {
			return false;
		}
		const auto* base = a_actor->GetActorBase();
		const bool  female = base && base->GetSex() == RE::SEX::kFemale;
		return female ? a_settings.bAutoFemale : a_settings.bAutoMale;
	}

	void Manager::Scan(const Settings& a_settings)
	{
		if (!(a_settings.bAutoGlobal || a_settings.bAutoNaked || a_settings.bAutoFollower)) {
			return;
		}
		auto*       player = RE::PlayerCharacter::GetSingleton();
		auto*       lists = RE::ProcessLists::GetSingleton();
		const auto  center = player->GetPosition();
		const auto* playerCell = player->GetParentCell();
		const bool  interior = playerCell && playerCell->IsInteriorCell();
		if (!lists) {
			return;
		}
		for (auto& handle : lists->highActorHandles) {
			const auto pointer = handle.get();
			auto*      actor = pointer.get();
			if (!actor || actor == player || actor->IsDead() || !actor->Is3DLoaded()) {
				continue;
			}
			if ((interior && actor->GetParentCell() != playerCell) || actor->GetPosition().GetDistance(center) > a_settings.fAutoRange) {
				continue;
			}
			if (!HasKeyword(actor, _actorTypeNPC)) {
				continue;
			}
			const bool follower = a_settings.bAutoFollower && actor->IsPlayerTeammate();
			const bool devious = a_settings.bAutoDevious && Devious::Get(actor) != Devious::sNone;
			const bool ordinary = a_settings.bAutoGlobal || devious || (a_settings.bAutoNaked && !WornHasKeyword(actor, _armorCuirass) && !WornHasKeyword(actor, _clothingBody));
			if (follower || ordinary) {
				const bool wasActive = IsActive(actor);
				if (AutoApply(actor, a_settings.fAutoDuration, true, follower) && !wasActive) {
					// Actors discovered by one scan are deliberately spread across the NPC update interval.
					auto& record = Record(actor->GetFormID());
					const auto spread = static_cast<std::uint32_t>(std::max(a_settings.fLoopTimeNPC, 0.25f) * 1000.0f);
					record.nextUpdate = Clock::now() + std::chrono::milliseconds(actor->GetFormID() % spread);
				}
			}
		}
	}

	bool Manager::StartManual(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		std::scoped_lock lock(_lock);
		auto&            record = Record(a_actor->GetFormID());
		record.manual = true;
		record.autoStop = 0.0;
		if (!record.active) {
			Wetness::Start(a_actor, record, Settings::Get());
		}
		record.nextUpdate = {};
		return true;
	}

	void Manager::StopManual(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		std::scoped_lock lock(_lock);
		Stop(a_actor->GetFormID(), false);
	}

	bool Manager::AutoApply(RE::Actor* a_actor, float a_hours, bool a_extend, bool a_ignoreFilter)
	{
		if (!a_actor) {
			return false;
		}
		std::scoped_lock lock(_lock);
		const auto&      settings = Settings::Get();
		if (const auto it = _records.find(a_actor->GetFormID()); it != _records.end() && it->second.active) {
			if (it->second.manual) {
				return false;
			}
		} else if (!a_ignoreFilter && !PassesFilter(a_actor, settings)) {
			return false;
		}
		auto&  record = Record(a_actor->GetFormID());
		double stop = Wetness::GameDays() + a_hours / 24.0;
		if (a_extend && record.autoStop > stop) {
			stop = record.autoStop;
		}
		record.autoStop = stop;
		if (!record.active) {
			Wetness::Start(a_actor, record, settings);
		}
		return true;
	}

	void Manager::SetWetness(RE::Actor* a_actor, float a_wetness)
	{
		if (!a_actor) {
			return;
		}
		std::scoped_lock lock(_lock);
		const auto&      settings = Settings::Get();
		auto&            record = Record(a_actor->GetFormID());
		record.wetness = std::clamp(a_wetness, 0.0f, settings.fWetnessCap);
		if (record.wetness <= settings.fWetnessDry) {
			// "dry" takes effect now, not after the dynamic drying limit is reached on a later update
			record.phase = Phase::kDry;
			record.hasDrops = false;
			record.hasSweat = false;
			record.hasPussy = false;
			record.hasSchlong = false;
		}
		record.nextUpdate = {};
	}

	void Manager::ForceValues(RE::Actor* a_actor, float a_wetness, float a_specular, float a_glossiness)
	{
		if (!a_actor) {
			return;
		}
		std::scoped_lock lock(_lock);
		auto&            record = Record(a_actor->GetFormID());
		record.forceWetness = a_wetness;
		record.forceSpecular = a_specular;
		record.forceGlossiness = a_glossiness;
		record.nextUpdate = {};
	}

	void Manager::StopAll(bool a_clearData)
	{
		std::scoped_lock        lock(_lock);
		std::vector<RE::FormID> ids;
		for (const auto& [id, record] : _records) {
			if (record.active) {
				ids.push_back(id);
			}
		}
		for (const auto id : ids) {
			Stop(id, !a_clearData);
		}
		if (a_clearData) {
			_records.clear();
			_ostimThreads.clear();
			_sexLabThreads.clear();
		}
		logger::info("Stopped {} effects{}", ids.size(), a_clearData ? " and cleared all data" : "");
	}

	void Manager::OnSettingsChanged()
	{
		std::scoped_lock lock(_lock);
		const auto&      settings = Settings::Get();
		const auto clearScenes = [this](const auto& a_scenes, bool a_ostim) {
			for (const auto& [thread, scene] : a_scenes) {
				for (const auto id : scene.actors) {
					if (const auto record = _records.find(id); record != _records.end()) {
						record->second.sexRate = 0.0f;
						if (a_ostim) {
							record->second.ostimClimaxes = -1;
						}
					}
				}
			}
		};
		if (!settings.bSexLabEnabled) {
			clearScenes(_sexLabThreads, false);
			_sexLabThreads.clear();
		}
		if (!settings.bOStimEnabled) {
			clearScenes(_ostimThreads, true);
			_ostimThreads.clear();
		}
		for (auto& [id, record] : _records) {
			if (record.active) {
				Visuals::Invalidate(id);
				record.nextUpdate = {};
			}
		}
	}

	void Manager::Invalidate(RE::FormID a_actor, std::chrono::milliseconds a_delay)
	{
		std::scoped_lock lock(_lock);
		const auto       it = _records.find(a_actor);
		if (it == _records.end() || !it->second.active) {
			return;
		}
		Visuals::Invalidate(a_actor);
		it->second.nextUpdate = Clock::now() + a_delay;
	}

	void Manager::Bump(RE::Actor* a_actor, float a_amount)
	{
		if (!a_actor) {
			return;
		}
		const auto it = _records.find(a_actor->GetFormID());
		if (it != _records.end() && it->second.active) {
			it->second.wetness = std::clamp(it->second.wetness + a_amount, 0.0f, Settings::Get().fWetnessCap);
		}
	}

	void Manager::OnSexLab(RE::FormID a_thread, const std::vector<RE::Actor*>& a_actors, const std::vector<float>& a_enjoyment, const std::vector<float>& a_pain, std::int32_t a_phase)
	{
		const auto& settings = Settings::Get();
		if (!settings.bSexLabEnabled || !_sexLabAvailable) {
			return;
		}
		std::scoped_lock lock(_lock);
		auto*            player = RE::PlayerCharacter::GetSingleton();
		auto&            scene = _sexLabThreads[a_thread];

		std::vector<RE::FormID> ids;
		for (auto* actor : a_actors) {
			ids.push_back(actor ? actor->GetFormID() : 0);
		}
		const bool anyActor = std::ranges::any_of(ids, [](RE::FormID a_id) { return a_id != 0; });
		if (anyActor) {
			scene.actors = ids;
			scene.hasPlayer = std::ranges::find(ids, player->GetFormID()) != ids.end();
		} else {
			ids = scene.actors;  // AnimationEnd may arrive after the positions were released
		}
		const bool autoStart = settings.bSexLabAuto && (!settings.bSexLabAutoPlayer || scene.hasPlayer);

		for (std::size_t i = 0; i < ids.size(); ++i) {
			auto* actor = LookupActor(ids[i]);
			if (!actor || (settings.bSexLabPlayerOnly && actor != player)) {
				continue;
			}
			auto& record = Record(ids[i]);
			if (a_phase == 2) {
				record.sexRate = 0.0f;
			} else if (a_phase == 0) {
				record.sexRate = settings.fSexLabBase;
			} else {
				const float enjoyment = i < a_enjoyment.size() ? a_enjoyment[i] : 0.0f;
				const float pain = i < a_pain.size() ? a_pain[i] : 0.0f;
				record.sexRate = settings.fSexLabBase + settings.fSexLabEnjoyment * 0.01f * enjoyment + settings.fSexLabPain * 0.01f * pain;
			}
			if (autoStart) {
				if (a_phase == 2) {
					AutoApply(actor, settings.fSexLabAutoLinger, false, true);
				} else {
					AutoApply(actor, 24.0f, true, true);
				}
			}
		}
		if (a_phase == 2) {
			_sexLabThreads.erase(a_thread);
		}
	}

	void Manager::OnSexLabOrgasm(RE::Actor* a_actor)
	{
		const auto& settings = Settings::Get();
		if (!settings.bSexLabEnabled || !_sexLabAvailable) {
			return;
		}
		std::scoped_lock lock(_lock);
		Bump(a_actor, settings.fSexLabOrgasm);
	}

	void Manager::OnOStim(OStim::Event a_event, std::uint32_t a_thread)
	{
		const auto& settings = Settings::Get();
		if (!settings.bOStimEnabled) {
			return;
		}
		std::scoped_lock lock(_lock);
		if (a_event != OStim::Event::kEnded) {
			RefreshOStim(a_thread, settings);
			return;
		}
		const auto it = _ostimThreads.find(a_thread);
		if (it == _ostimThreads.end()) {
			return;
		}
		const auto scene = it->second;
		_ostimThreads.erase(it);
		auto*      player = RE::PlayerCharacter::GetSingleton();
		const bool autoStart = settings.bOStimAuto && (!settings.bOStimAutoPlayer || scene.hasPlayer);
		for (const auto id : scene.actors) {
			auto* actor = LookupActor(id);
			if (!actor || (settings.bOStimPlayerOnly && actor != player)) {
				continue;
			}
			if (const auto record = _records.find(id); record != _records.end()) {
				record->second.sexRate = 0.0f;
				record->second.ostimClimaxes = -1;
			}
			if (autoStart) {
				AutoApply(actor, settings.fOStimAutoLinger, false, true);
			}
		}
	}

	void Manager::RefreshOStim(std::uint32_t a_thread, const Settings& a_settings)
	{
		const auto participants = OStim::Participants(a_thread);
		if (participants.empty()) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto& scene = _ostimThreads[a_thread];
		const auto previous = scene.actors;
		const bool previousAuto = a_settings.bOStimAuto && (!a_settings.bOStimAutoPlayer || scene.hasPlayer);
		scene.actors.clear();
		scene.hasPlayer = false;
		for (const auto& participant : participants) {
			scene.actors.push_back(participant.formID);
			scene.hasPlayer |= participant.formID == player->GetFormID();
		}
		const bool autoStart = a_settings.bOStimAuto && (!a_settings.bOStimAutoPlayer || scene.hasPlayer);
		for (const auto id : previous) {
			if (std::ranges::find(scene.actors, id) != scene.actors.end()) {
				continue;
			}
			if (const auto record = _records.find(id); record != _records.end()) {
				record->second.sexRate = 0.0f;
				record->second.ostimClimaxes = -1;
			}
			if (previousAuto) {
				AutoApply(LookupActor(id), a_settings.fOStimAutoLinger, false, true);
			}
		}
		for (const auto& participant : participants) {
			auto* actor = LookupActor(participant.formID);
			if (!actor || (a_settings.bOStimPlayerOnly && actor != player)) {
				continue;
			}
			if (autoStart) {
				AutoApply(actor, 24.0f, true, true);
			}
			auto& record = Record(participant.formID);
			record.sexRate = a_settings.fOStimBase + a_settings.fOStimExcitement * 0.01f * participant.excitement;
			if (record.ostimClimaxes >= 0 && participant.climaxes > record.ostimClimaxes) {
				Bump(actor, a_settings.fOStimOrgasm * static_cast<float>(participant.climaxes - record.ostimClimaxes));
			}
			record.ostimClimaxes = participant.climaxes;
		}
	}

	bool Manager::IsActive(RE::Actor* a_actor) const
	{
		if (!a_actor) {
			return false;
		}
		std::scoped_lock lock(_lock);
		const auto       it = _records.find(a_actor->GetFormID());
		return it != _records.end() && it->second.active;
	}

	bool Manager::IsTracked(RE::FormID a_actor) const
	{
		std::scoped_lock lock(_lock);
		const auto       it = _records.find(a_actor);
		return it != _records.end() && it->second.active;
	}

	float Manager::GetWetness(RE::Actor* a_actor) const
	{
		if (!a_actor) {
			return 0.0f;
		}
		std::scoped_lock lock(_lock);
		const auto       it = _records.find(a_actor->GetFormID());
		return it != _records.end() ? it->second.wetness : 0.0f;
	}

	std::string Manager::Status(RE::Actor* a_actor) const
	{
		if (!a_actor) {
			return "[WFNG] no actor";
		}
		std::scoped_lock lock(_lock);
		const auto       name = std::format("{} [{:08X}]", a_actor->GetName(), a_actor->GetFormID());
		const auto       it = _records.find(a_actor->GetFormID());
		if (it == _records.end()) {
			return std::format("[WFNG] {}: no effect", name);
		}
		const auto& record = it->second;
		const auto& settings = Settings::Get();
		std::string stop = record.manual ? "manual"s : std::format("auto, {:.1f} h left", std::max(0.0, (record.autoStop - Wetness::GameDays()) * 24.0));
		return std::format(
			"[WFNG] {}: {} ({}) wetness {:.2f}/{:.0f} {} rate {:+.2f}/h = weather {:+.2f} ({}) heat {:+.2f} activity {:+.2f} arousal {:+.2f} (arousal {}) sex {:+.2f} | drops {} sweat {} pussy {} schlong {} strength {:.2f} devices {:#06x}{}",
			name, record.active ? "active" : "inactive", stop, record.wetness, settings.fWetnessCap, Wetness::PhaseName(record.phase), record.rate, record.rateWeather, record.weather,
			record.rateHeat, record.rateActivity, record.rateArousal, record.arousal, record.sexRate, record.hasDrops, record.hasSweat, record.hasPussy, record.hasSchlong,
			Wetness::Strength(record, settings), static_cast<unsigned>(Devious::Get(a_actor)), record.forceWetness >= 0.0f ? std::format(" forced {:.1f}", record.forceWetness) : ""s);
	}

	void Manager::Save(SKSE::SerializationInterface* a_intfc) const
	{
		std::scoped_lock lock(_lock);
		if (!a_intfc->OpenRecord(kActorsRecord, kRecordVersion)) {
			logger::error("Could not open the actor record");
			return;
		}
		std::vector<std::pair<RE::FormID, const ActorRecord*>> saved;
		for (const auto& [id, record] : _records) {
			if (record.active || record.wetness > 0.0f || record.forceWetness >= 0.0f || record.forceSpecular > 0.0f || record.forceGlossiness > 0.0f) {
				saved.emplace_back(id, std::addressof(record));
			}
		}
		a_intfc->WriteRecordData(static_cast<std::uint32_t>(saved.size()));
		for (const auto& [id, record] : saved) {
			a_intfc->WriteRecordData(id);
			a_intfc->WriteRecordData(record->wetness);
			a_intfc->WriteRecordData(record->forceWetness);
			a_intfc->WriteRecordData(record->forceSpecular);
			a_intfc->WriteRecordData(record->forceGlossiness);
			a_intfc->WriteRecordData(record->autoStop);
			const std::array<std::uint8_t, 6> flags{ record->active, record->manual, record->seeded, static_cast<std::uint8_t>(record->phase), record->hasDrops, record->hasSweat };
			a_intfc->WriteRecordData(flags.data(), static_cast<std::uint32_t>(flags.size()));
			a_intfc->WriteRecordData(record->dryDyn);
			a_intfc->WriteRecordData(record->maxDyn);
			a_intfc->WriteRecordData(record->recentStrength);
		}
	}

	void Manager::Load(SKSE::SerializationInterface* a_intfc)
	{
		std::scoped_lock lock(_lock);
		std::uint32_t    type = 0;
		std::uint32_t    version = 0;
		std::uint32_t    length = 0;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type != kActorsRecord || version != kRecordVersion) {
				logger::warn("Skipping co-save record {:08X} v{}", type, version);
				continue;
			}
			std::uint32_t count = 0;
			a_intfc->ReadRecordData(count);
			std::size_t restored = 0;
			for (std::uint32_t i = 0; i < count; ++i) {
				RE::FormID  id = 0;
				ActorRecord record;
				a_intfc->ReadRecordData(id);
				a_intfc->ReadRecordData(record.wetness);
				a_intfc->ReadRecordData(record.forceWetness);
				a_intfc->ReadRecordData(record.forceSpecular);
				a_intfc->ReadRecordData(record.forceGlossiness);
				a_intfc->ReadRecordData(record.autoStop);
				std::array<std::uint8_t, 6> flags{};
				a_intfc->ReadRecordData(flags.data(), static_cast<std::uint32_t>(flags.size()));
				record.active = flags[0];
				record.manual = flags[1];
				record.seeded = flags[2];
				record.phase = static_cast<Phase>(std::min<std::uint8_t>(flags[3], 2));
				record.hasDrops = flags[4];
				record.hasSweat = flags[5];
				a_intfc->ReadRecordData(record.dryDyn);
				a_intfc->ReadRecordData(record.maxDyn);
				a_intfc->ReadRecordData(record.recentStrength);

				RE::FormID resolved = 0;
				if (id && a_intfc->ResolveFormID(id, resolved)) {
					_records[resolved] = record;
					++restored;
				}
			}
			logger::info("Restored {} of {} actors from the co-save", restored, count);
		}
	}

	void Manager::Revert()
	{
		std::scoped_lock lock(_lock);
		_records.clear();
		_ostimThreads.clear();
		_sexLabThreads.clear();
		Visuals::ForgetAll();
	}
}
