#include "Heat.h"

namespace WFNG::Heat
{
	namespace
	{
		constexpr auto kSunHelm = "SunHelmSurvival.esp"sv;

		// local FormIDs in SunHelmSurvival.esp: heat source lists and the global that holds each list's radius
		struct SunHelmList
		{
			RE::FormID list;
			RE::FormID radius;
			float      fallback;
		};
		constexpr SunHelmList kSmall{ 0x78511B, 0x743404, 250.0f };
		constexpr SunHelmList kNormal{ 0x78511A, 0x743403, 450.0f };
		constexpr SunHelmList kLarge{ 0x785119, 0x78A21C, 600.0f };
		constexpr SunHelmList kAll{ 0x785118, 0x743403, 450.0f };  // sources that sit in no size list

		constexpr std::array kHeatModels{ "campfire"sv, "fireplace"sv, "firepit"sv, "firebowl"sv, "brazier"sv, "hearth"sv, "bonfire"sv, "forge"sv };
		constexpr std::array kColdModels{ "burnt"sv, "burned"sv, "unlit"sv, "ashes"sv, "dead"sv, "extinguish"sv, "_off"sv };

		std::unordered_map<RE::FormID, float> g_sunHelm;  // base object -> reach
		std::unordered_map<RE::FormID, bool>  g_models;   // base object -> looks like a fire
		float                                 g_sunHelmReach{ 0.0f };
		std::atomic_size_t                    g_sunHelmCount{ 0 };

		bool Contains(std::string_view a_text, std::string_view a_part)
		{
			return a_text.find(a_part) != std::string_view::npos;
		}

		bool LooksLikeFire(const RE::TESBoundObject* a_base)
		{
			if (!a_base->Is(RE::FormType::Activator, RE::FormType::Furniture, RE::FormType::Static, RE::FormType::MovableStatic)) {
				return false;
			}
			const auto* model = skyrim_cast<const RE::TESModel*>(a_base);
			const char* path = model ? model->GetModel() : nullptr;
			if (!path || !*path) {
				return false;
			}
			std::string name(path);
			std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const auto has = [&](std::string_view a_part) { return Contains(name, a_part); };
			return std::ranges::any_of(kHeatModels, has) && !std::ranges::any_of(kColdModels, has);
		}

		// how far the heat of a base object reaches; 0 means it is not a fire
		float Reach(const RE::TESBoundObject* a_base, bool a_sunHelm, const Settings& a_settings)
		{
			if (a_sunHelm) {
				if (const auto it = g_sunHelm.find(a_base->GetFormID()); it != g_sunHelm.end()) {
					return it->second;
				}
			}
			auto it = g_models.find(a_base->GetFormID());
			if (it == g_models.end()) {
				it = g_models.emplace(a_base->GetFormID(), LooksLikeFire(a_base)).first;
			}
			return it->second ? a_settings.fHeatRange : 0.0f;
		}

		void Load(RE::TESDataHandler* a_data, const SunHelmList& a_source, bool a_keepExisting)
		{
			auto* list = a_data->LookupForm<RE::BGSListForm>(a_source.list, kSunHelm);
			if (!list) {
				return;
			}
			const auto* global = a_data->LookupForm<RE::TESGlobal>(a_source.radius, kSunHelm);
			const float reach = global ? global->value : a_source.fallback;
			g_sunHelmReach = std::max(g_sunHelmReach, reach);
			list->ForEachForm([&](RE::TESForm* a_form) {
				if (a_form) {
					if (a_keepExisting) {
						g_sunHelm.emplace(a_form->GetFormID(), reach);
					} else {
						g_sunHelm[a_form->GetFormID()] = reach;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
	}

	void Refresh()
	{
		g_sunHelm.clear();
		g_sunHelmReach = 0.0f;
		auto* data = RE::TESDataHandler::GetSingleton();
		if (data && data->LookupModByName(kSunHelm)) {
			Load(data, kSmall, false);
			Load(data, kNormal, false);
			Load(data, kLarge, false);
			Load(data, kAll, true);
		}
		g_sunHelmCount = g_sunHelm.size();
		logger::info("Heat sources: SunHelm {} ({} base objects)", g_sunHelmCount ? "found" : "absent", g_sunHelmCount.load());
	}

	std::size_t SunHelmSources()
	{
		return g_sunHelmCount;
	}

	bool IsNear(RE::Actor* a_actor, const Settings& a_settings)
	{
		auto* cell = a_settings.bHeatDrying ? a_actor->GetParentCell() : nullptr;
		if (!cell) {
			return false;
		}
		const bool  sunHelm = a_settings.bHeatSunHelm && !g_sunHelm.empty();
		const float scan = sunHelm ? std::max(a_settings.fHeatRange, g_sunHelmReach) : a_settings.fHeatRange;
		const auto  position = a_actor->GetPosition();

		bool found = false;
		cell->ForEachReferenceInRange(position, scan, [&](RE::TESObjectREFR* a_ref) {
			const auto* base = a_ref->GetBaseObject();
			if (!base || a_ref == a_actor) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const float reach = Reach(base, sunHelm, a_settings);
			if (reach > 0.0f && a_ref->GetPosition().GetDistance(position) <= reach && !a_ref->IsDisabled() && a_ref->Is3DLoaded()) {
				found = true;
				return RE::BSContainer::ForEachResult::kStop;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		return found;
	}
}
