#include "Menu.h"

#include "Devious.h"
#include "Heat.h"
#include "Manager.h"
#include "OStim.h"
#include "Settings.h"
#include "Visuals.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace WFNG::Menu
{
	namespace
	{
		void Commit()
		{
			Settings::Save();
			Manager::Get().OnSettingsChanged();
		}

		void Help(const char* a_text)
		{
			if (a_text && ImGuiMCP::IsItemHovered()) {
				ImGuiMCP::SetTooltip("%s", a_text);
			}
		}

		void Toggle(const char* a_label, bool& a_value, const char* a_help = nullptr)
		{
			if (ImGuiMCP::Checkbox(a_label, std::addressof(a_value))) {
				Commit();
			}
			Help(a_help);
		}

		void Slider(const char* a_label, float& a_value, float a_min, float a_max, const char* a_format, const char* a_help = nullptr)
		{
			if (ImGuiMCP::SliderFloat(a_label, std::addressof(a_value), a_min, a_max, a_format)) {
				Commit();
			}
			Help(a_help);
		}

		void Heading(const char* a_text)
		{
			ImGuiMCP::SeparatorText(a_text);
		}

		void __stdcall General()
		{
			auto& s = Settings::Get();
			Heading("Actors and update rate");
			Toggle("Enable player", s.bPlayerEnabled, "The player is always managed while this is enabled.");
			Toggle("Update first-person model", s.bFirstPerson);
			Slider("Player update interval", s.fLoopTimePC, 0.25f, 30.0f, "%.2f s");
			Slider("NPC update interval", s.fLoopTimeNPC, 0.25f, 60.0f, "%.2f s");
			Slider("Forced-value interval", s.fLoopTimeForced, 0.25f, 60.0f, "%.2f s");

			Heading("Logging");
			bool verbose = s.iLogLevel > 0;
			if (ImGuiMCP::Checkbox("Per-actor debug logging", std::addressof(verbose))) {
				s.iLogLevel = verbose ? 1 : 0;
				Commit();
			}
			Help("Off by default. Lifecycle, warnings and errors are still logged; actor updates are not.");

			Heading("Maintenance");
			if (ImGuiMCP::Button("Reload INI")) {
				Settings::Load();
				Manager::Get().OnSettingsChanged();
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Reset defaults")) {
				Settings::Reset();
				Manager::Get().OnSettingsChanged();
			}
			if (ImGuiMCP::Button("Stop all effects")) {
				Manager::Get().StopAll(false);
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Clear all saved data")) {
				Manager::Get().StopAll(true);
			}
			if (ImGuiMCP::Button("Clean player overrides")) {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					const auto removed = Visuals::Clean(player);
					Visuals::Out(std::format("[WFNG] Player: removed {} leftover wetness overrides", removed));
				}
			}
			ImGuiMCP::TextWrapped("Settings: Data\\SKSE\\Plugins\\WetFunctionNG.ini");
		}

		void __stdcall Wetness()
		{
			auto& s = Settings::Get();
			Heading("Wetness points");
			Slider("Dry threshold", s.fWetnessDry, 0.0f, 30.0f, "%.1f");
			Slider("Visible sweat threshold", s.fWetnessStart, 0.0f, 30.0f, "%.1f");
			Slider("Soaked threshold", s.fWetnessSoaked, 0.0f, 30.0f, "%.1f");
			Slider("Maximum wetness", s.fWetnessCap, 0.1f, 30.0f, "%.1f");
			Slider("Force wetness", s.fWetnessForce, -1.0f, 30.0f, "%.1f", "-1 releases the forced value.");
			Slider("Late sweat ratio", s.fLateSweat, 0.0f, 1.0f, "%.2f");

			Heading("Initial automatic effect");
			Slider("Simulated previous hours", s.fAutoBonusHours, 0.0f, 24.0f, "%.1f h");
			Slider("Flat starting wetness", s.fAutoBonusNormal, 0.0f, 30.0f, "%.1f");
			Slider("Random starting wetness", s.fAutoBonusRandom, 0.0f, 30.0f, "%.1f");
		}

		void __stdcall Sources()
		{
			auto& s = Settings::Get();
			Heading("Activity per game hour");
			Slider("Stamina use", s.fGenerateStamina, -10.0f, 10.0f, "%.2f");
			Slider("Magicka use", s.fGenerateMagicka, -10.0f, 10.0f, "%.2f");
			Slider("Sprinting", s.fGenerateSprinting, -10.0f, 10.0f, "%.2f");
			Slider("Running", s.fGenerateRunning, -10.0f, 10.0f, "%.2f");
			Slider("Sneaking", s.fGenerateSneaking, -10.0f, 10.0f, "%.2f");
			Slider("Mounted gallop", s.fGenerateGallop, -10.0f, 10.0f, "%.2f");
			Slider("Working furniture", s.fGenerateWorking, -10.0f, 10.0f, "%.2f");
			Slider("NPC additive modifier", s.fOtherAdd, -10.0f, 10.0f, "%.2f");
			Slider("NPC multiplier", s.fOtherMult, 0.0f, 10.0f, "%.2f");
			Slider("Global multiplier", s.fMultGlobal, 0.0f, 10.0f, "%.2f");

			Heading("Weather per game hour");
			Slider("Pleasant", s.fWeatherPleasant, -10.0f, 10.0f, "%.2f");
			Slider("Cloudy", s.fWeatherCloudy, -10.0f, 10.0f, "%.2f");
			Slider("Rain", s.fWeatherRainy, -10.0f, 10.0f, "%.2f");
			Slider("Snow", s.fWeatherSnow, -10.0f, 10.0f, "%.2f");
			Slider("Interior / none", s.fWeatherNone, -10.0f, 10.0f, "%.2f");

			Heading("Drying near a fire");
			Toggle("Dry faster near a fire", s.bHeatDrying, "Campfires, fireplaces, braziers and forges. Works for the player and every managed NPC.");
			Slider("Extra drying per hour", s.fHeatDrying, 0.0f, 20.0f, "%.2f");
			Slider("Fire reach", s.fHeatRange, 50.0f, 1500.0f, "%.0f", "Game units. SunHelm heat sources keep the radius SunHelm gives them.");
			Toggle("Use SunHelm heat sources", s.bHeatSunHelm, "Takes SunHelm's lists of fires and their radii when SunHelm Survival is installed.");
			if (Heat::SunHelmSources() > 0) {
				ImGuiMCP::Text("SunHelm: detected, %zu heat sources", Heat::SunHelmSources());
			} else {
				ImGuiMCP::Text("SunHelm: not detected, fires are recognised by model name");
			}

			Heading("Arousal");
			Slider("Arousal generation", s.fGenerateArousal, -10.0f, 10.0f, "%.2f");
			Toggle("Use arousal threshold for pussy texture", s.bUseArousalThreshold);
			Slider("Arousal threshold", s.fArousedThreshold, 0.0f, 100.0f, "%.0f");
		}

		void __stdcall Integrations()
		{
			auto& s = Settings::Get();
			Heading("SexLab");
			ImGuiMCP::Text("Detected: %s", Manager::Get().SexLabAvailable() ? "yes" : "no");
			Toggle("Enable SexLab", s.bSexLabEnabled);
			Toggle("SexLab: player scenes only", s.bSexLabPlayerOnly);
			Toggle("SexLab: auto-manage scene actors", s.bSexLabAuto, "All participants bypass the ordinary NPC sex/race filters.");
			Toggle("SexLab auto only when player participates", s.bSexLabAutoPlayer);
			Slider("SexLab base generation", s.fSexLabBase, -10.0f, 20.0f, "%.2f");
			Slider("SexLab enjoyment generation", s.fSexLabEnjoyment, -10.0f, 20.0f, "%.2f");
			Slider("SexLab pain generation", s.fSexLabPain, -10.0f, 20.0f, "%.2f");
			Slider("SexLab orgasm bonus", s.fSexLabOrgasm, -10.0f, 20.0f, "%.2f");
			Slider("SexLab linger", s.fSexLabAutoLinger, 0.0f, 48.0f, "%.1f h");

			Heading("OStim Standalone");
			ImGuiMCP::Text("Detected: %s", OStim::IsAvailable() ? "yes" : "no");
			Toggle("Enable OStim", s.bOStimEnabled);
			Toggle("OStim: player scenes only", s.bOStimPlayerOnly);
			Toggle("OStim: auto-manage scene actors", s.bOStimAuto, "All participants bypass the ordinary NPC sex/race filters.");
			Toggle("OStim auto only when player participates", s.bOStimAutoPlayer);
			Slider("OStim base generation", s.fOStimBase, -10.0f, 20.0f, "%.2f");
			Slider("OStim excitement generation", s.fOStimExcitement, -10.0f, 20.0f, "%.2f");
			Slider("OStim orgasm bonus", s.fOStimOrgasm, -10.0f, 20.0f, "%.2f");
			Slider("OStim linger", s.fOStimAutoLinger, 0.0f, 48.0f, "%.1f h");

			Heading("Devious Devices");
			ImGuiMCP::Text("Detected: %s", Devious::IsAvailable() ? "yes" : "no");
			ImGuiMCP::TextWrapped("Native part only for now: worn-device detection and chastity. Vibration, orgasm and edge from a device are not wired up yet - that lives in Devious Devices' own Papyrus scripts, not its native API.");
			Toggle("Chastity blocks the wet crotch/schlong texture", s.bDeviousBlockGenitals, "A belt or cage physically covers what the texture would show.");
		}

		void __stdcall Automatic()
		{
			auto& s = Settings::Get();
			ImGuiMCP::TextWrapped("Default: followers are managed; unrelated NPCs are ignored. Sex scene participants are controlled on the Integrations page and always bypass these filters.");
			Heading("Who is scanned");
			Toggle("Followers", s.bAutoFollower, "Player teammates of either sex are always eligible.");
			Toggle("All nearby NPCs", s.bAutoGlobal, "Off by default. Enables weather, activity and arousal wetness for ordinary NPCs.");
			Toggle("Nearby naked NPCs", s.bAutoNaked, "Off by default.");
			Toggle("NPCs wearing a Devious Devices device", s.bAutoDevious, "Requires Devious Devices NG.");
			Toggle("Female ordinary NPCs", s.bAutoFemale);
			Toggle("Male ordinary NPCs", s.bAutoMale);
			Toggle("Beast-race ordinary NPCs", s.bAutoBeast);
			Toggle("Vampire ordinary NPCs", s.bAutoVampire);
			Toggle("Keep wetness after auto-stop", s.bAutoKeepWetness);

			Heading("Scan");
			Slider("Scan interval", s.fAutoTimeout, 0.25f, 60.0f, "%.1f s");
			Slider("Scan radius", s.fAutoRange, 100.0f, 20000.0f, "%.0f");
			Slider("Automatic duration", s.fAutoDuration, 0.1f, 24.0f, "%.1f h");
		}

		void __stdcall VisualsPage()
		{
			auto& s = Settings::Get();
			Heading("Specular");
			Slider("Specular minimum", s.fSpecularMin, 0.0f, 30.0f, "%.1f");
			Slider("Specular maximum", s.fSpecularMax, 0.0f, 30.0f, "%.1f");
			Slider("Force specular", s.fSpecularForce, 0.0f, 30.0f, "%.1f", "0 releases the forced value.");
			Slider("Head specular multiplier", s.fHeadSpecularMult, 0.0f, 5.0f, "%.2f");
			Slider("Hand specular multiplier", s.fHandSpecularMult, 0.0f, 5.0f, "%.2f");
			Toggle("Specular: body", s.bSpecularBody);
			Toggle("Specular: hands", s.bSpecularHands);
			Toggle("Specular: feet", s.bSpecularFeet);
			Toggle("Specular: head", s.bSpecularHead);

			Heading("Glossiness");
			Slider("Glossiness minimum", s.fGlossinessMin, 0.0f, 100.0f, "%.1f");
			Slider("Glossiness maximum", s.fGlossinessMax, 0.0f, 100.0f, "%.1f");
			Slider("Force glossiness", s.fGlossinessForce, 0.0f, 100.0f, "%.1f", "0 releases the forced value.");
			Toggle("Glossiness: body", s.bGlossinessBody);
			Toggle("Glossiness: hands", s.bGlossinessHands);
			Toggle("Glossiness: feet", s.bGlossinessFeet);
			Toggle("Glossiness: head", s.bGlossinessHead);

			Heading("Wet textures");
			Toggle("Body textures", s.bTextureBody);
			Toggle("Body drops", s.bTextureBodyDrops);
			Toggle("Body sweat", s.bTextureBodySweat);
			Toggle("Body pussy variant", s.bTextureBodyPussy);
			Toggle("Feet drops", s.bTextureFeetDrops);
			Toggle("Feet sweat", s.bTextureFeetSweat);
			Toggle("Hands", s.bTextureHands);
			Toggle("Head", s.bTextureHead);
			Toggle("Male schlong", s.bTextureSchlong);
			Toggle("Futanari schlong", s.bTextureFutaSchlong);
			Toggle("Preload wet textures", s.bPreloadTextures, "Avoids texture-swap stutter at the cost of retained texture memory.");
		}

		void __stdcall Hud()
		{
			auto& s = Settings::Get();
			ImGuiMCP::TextWrapped("A droplet that fills from the bottom with your wetness. The arrow beside it points up while you get wetter and down while you dry.");
			Heading("Droplet");
			Toggle("Show droplet", s.bWidgetEnabled);
			Toggle("Show while dry", s.bWidgetAlways, "Off: the droplet fades in when you get wet and out when you are dry.");
			Slider("Horizontal position", s.fWidgetX, 0.0f, 1.0f, "%.3f");
			Slider("Vertical position", s.fWidgetY, 0.0f, 1.0f, "%.3f");
			Slider("Size", s.fWidgetSize, 16.0f, 160.0f, "%.0f px", "Height at 1080p; it scales with the screen.");
			Slider("Opacity", s.fWidgetOpacity, 0.1f, 1.0f, "%.2f");
		}
	}

	bool Register()
	{
		if (!GetMenuFrameworkModule()) {
			logger::warn("SKSE Menu Framework is not loaded; configuration UI is unavailable");
			return false;
		}
		SKSEMenuFramework::SetSection("WetFunction NG");
		SKSEMenuFramework::AddSectionItem("General", General);
		SKSEMenuFramework::AddSectionItem("Wetness", Wetness);
		SKSEMenuFramework::AddSectionItem("Sources", Sources);
		SKSEMenuFramework::AddSectionItem("Integrations", Integrations);
		SKSEMenuFramework::AddSectionItem("Automatic actors", Automatic);
		SKSEMenuFramework::AddSectionItem("Visuals", VisualsPage);
		SKSEMenuFramework::AddSectionItem("HUD", Hud);
		logger::info("Registered SKSE Menu Framework configuration (framework {})", SKSEMenuFramework::GetMenuFrameworkVersion());
		return true;
	}
}
