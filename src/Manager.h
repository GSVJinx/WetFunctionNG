#pragma once

#include "OStim.h"
#include "Wetness.h"

namespace WFNG
{
	// Replaces the WetFunctionAbility spell, WetFunctionEffect's update loop and the MCM quest's auto-apply,
	// SexLab and OStim handlers. Game state is touched on the main thread only.
	class Manager
	{
	public:
		static Manager& Get();

		void Init();  // kDataLoaded
		void SetReady(bool a_ready);
		void OnGameLoaded();

		// main thread
		bool StartManual(RE::Actor* a_actor);
		void StopManual(RE::Actor* a_actor);
		bool AutoApply(RE::Actor* a_actor, float a_hours, bool a_extend, bool a_ignoreFilter = false);
		void SetWetness(RE::Actor* a_actor, float a_wetness);
		void ForceValues(RE::Actor* a_actor, float a_wetness, float a_specular, float a_glossiness);
		void StopAll(bool a_clearData);
		void OnSettingsChanged();
		void OnSexLab(RE::FormID a_thread, const std::vector<RE::Actor*>& a_actors, const std::vector<float>& a_enjoyment, const std::vector<float>& a_pain, std::int32_t a_phase);
		void OnSexLabOrgasm(RE::Actor* a_actor);
		void OnOStim(OStim::Event a_event, std::uint32_t a_thread);
		void Invalidate(RE::FormID a_actor, std::chrono::milliseconds a_delay);

		// any thread
		struct Gauge
		{
			bool  active{ false };
			float wetness{ 0.0f };
			float rate{ 0.0f };  // wetness points per game hour, negative while drying
		};
		Gauge       PlayerGauge() const;  // lock-free: the HUD reads it every frame while Tick holds the lock
		bool        IsActive(RE::Actor* a_actor) const;
		bool        IsTracked(RE::FormID a_actor) const;
		bool        SexLabAvailable() const { return _sexLabAvailable; }
		float       GetWetness(RE::Actor* a_actor) const;
		std::string Status(RE::Actor* a_actor) const;

		void Save(SKSE::SerializationInterface* a_intfc) const;
		void Load(SKSE::SerializationInterface* a_intfc);
		void Revert();

	private:
		struct SceneActors
		{
			std::vector<RE::FormID> actors;
			bool                    hasPlayer{ false };
		};

		void         QueueTick();
		void         Tick();
		void         UpdateActor(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings);
		bool         ShouldStop(RE::Actor* a_actor, const ActorRecord& a_record, const Settings& a_settings, double a_now) const;
		void         Stop(RE::FormID a_id, bool a_keepWetness);
		void         Scan(const Settings& a_settings);
		bool         PassesFilter(RE::Actor* a_actor, const Settings& a_settings) const;
		void         Bump(RE::Actor* a_actor, float a_amount);
		void         RefreshOStim(std::uint32_t a_thread, const Settings& a_settings);
		ActorRecord& Record(RE::FormID a_id);

		mutable std::recursive_mutex                        _lock;
		std::unordered_map<RE::FormID, ActorRecord>         _records;
		std::unordered_map<std::uint32_t, SceneActors>      _ostimThreads;
		std::unordered_map<RE::FormID, SceneActors>         _sexLabThreads;
		std::chrono::steady_clock::time_point               _nextScan{};
		std::atomic_bool                                    _ready{ false };
		std::atomic_bool                                    _tickQueued{ false };
		std::atomic_bool                                    _started{ false };

		std::atomic<float> _gaugeWetness{ 0.0f };
		std::atomic<float> _gaugeRate{ 0.0f };
		std::atomic_bool   _gaugeActive{ false };

		bool            _sexLabAvailable{ false };
		RE::BGSKeyword* _actorTypeNPC{ nullptr };
		RE::BGSKeyword* _isBeastRace{ nullptr };
		RE::BGSKeyword* _vampire{ nullptr };
		RE::BGSKeyword* _armorCuirass{ nullptr };
		RE::BGSKeyword* _clothingBody{ nullptr };
	};

	void InstallSerialization();
}
