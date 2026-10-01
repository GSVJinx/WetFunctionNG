#include "Widget.h"

#include "Manager.h"
#include "Settings.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace WFNG::Widget
{
	namespace
	{
		using ImGuiMCP::ImVec2;

		// A teardrop in unit space, y pointing down: tip at the top, a circle below it. The tangent lines from the
		// tip touch the circle at acos(radius / distance) from the tip's direction, which keeps the outline convex.
		constexpr float kRadius = 0.6f;
		constexpr float kCentre = 0.25f;
		constexpr float kTip = -1.0f;
		constexpr float kBottom = kCentre + kRadius;
		constexpr float kHeight = kBottom - kTip;
		constexpr int   kArc = 28;

		using Outline = std::array<ImVec2, kArc + 2>;

		Outline BuildOutline()
		{
			constexpr float tau = 6.28318531f;
			const float     start = std::acos(kRadius / (kCentre - kTip));
			Outline         shape{};
			shape[0] = { 0.0f, kTip };
			for (int i = 0; i <= kArc; ++i) {
				const float angle = start + (tau - 2.0f * start) * static_cast<float>(i) / kArc;
				shape[i + 1] = { kRadius * std::sin(angle), kCentre - kRadius * std::cos(angle) };
			}
			return shape;
		}

		// keeps the part of a convex polygon at or below the line y = a_cut
		std::size_t ClipBelow(const ImVec2* a_in, std::size_t a_count, float a_cut, ImVec2* a_out)
		{
			std::size_t count = 0;
			for (std::size_t i = 0; i < a_count; ++i) {
				const auto& from = a_in[i];
				const auto& to = a_in[(i + 1) % a_count];
				const bool  fromInside = from.y >= a_cut;
				if (fromInside) {
					a_out[count++] = from;
				}
				if (fromInside != (to.y >= a_cut)) {
					const float t = (a_cut - from.y) / (to.y - from.y);
					a_out[count++] = { from.x + (to.x - from.x) * t, a_cut };
				}
			}
			return count;
		}

		ImGuiMCP::ImU32 Colour(int a_r, int a_g, int a_b, float a_alpha)
		{
			return IM_COL32(a_r, a_g, a_b, static_cast<int>(std::clamp(a_alpha, 0.0f, 1.0f) * 255.0f));
		}

		// the HUD callback runs on the render thread and only reads settings and the lock-free gauge
		void __stdcall Draw()
		{
			static const Outline outline = BuildOutline();
			static float         visible = 0.0f;  // fade in and out
			static float         filled = 0.0f;   // eased fill level, 0..1
			static int           trend = 0;       // 1 wetter, -1 drying, 0 steady

			const auto& settings = Settings::Get();
			auto*       io = ImGuiMCP::GetIO();
			const float dt = std::min(io->DeltaTime, 0.1f);

			auto* ui = RE::UI::GetSingleton();
			const auto  gauge = Manager::Get().PlayerGauge();
			const bool  shown = settings.bWidgetEnabled && gauge.active && !(ui && ui->GameIsPaused()) && (settings.bWidgetAlways || gauge.wetness > 0.01f);

			visible += ((shown ? 1.0f : 0.0f) - visible) * std::min(1.0f, dt * 6.0f);
			if (visible < 0.01f) {
				return;
			}
			const float cap = std::max(0.1f, settings.fWetnessCap);
			filled += (std::clamp(gauge.wetness / cap, 0.0f, 1.0f) - filled) * std::min(1.0f, dt * 4.0f);
			// the rate hovers around zero when balanced: switch at 0.1 and release at 0.03 so the arrow does not flicker
			if (gauge.rate > 0.1f) {
				trend = 1;
			} else if (gauge.rate < -0.1f) {
				trend = -1;
			} else if (std::abs(gauge.rate) < 0.03f) {
				trend = 0;
			}
			if (gauge.wetness <= 0.01f) {
				trend = 0;
			}

			const float alpha = visible * std::clamp(settings.fWidgetOpacity, 0.05f, 1.0f);
			const float scale = settings.fWidgetSize / kHeight * (io->DisplaySize.y / 1080.0f);
			const float centreX = settings.fWidgetX * io->DisplaySize.x;
			const float centreY = settings.fWidgetY * io->DisplaySize.y - (kTip + kBottom) * 0.5f * scale;
			const auto  toScreen = [&](const ImVec2& a_point) { return ImVec2{ centreX + a_point.x * scale, centreY + a_point.y * scale }; };

			Outline drop{};
			for (std::size_t i = 0; i < outline.size(); ++i) {
				drop[i] = toScreen(outline[i]);
			}

			auto* list = ImGuiMCP::GetForegroundDrawList();
			ImGuiMCP::ImDrawListManager::AddConvexPolyFilled(list, drop.data(), static_cast<int>(drop.size()), Colour(10, 18, 30, 0.45f * alpha));

			if (filled > 0.005f) {
				std::array<ImVec2, kArc + 4> water{};
				const auto count = ClipBelow(drop.data(), drop.size(), toScreen({ 0.0f, kBottom - filled * kHeight }).y, water.data());
				if (count >= 3) {
					ImGuiMCP::ImDrawListManager::AddConvexPolyFilled(list, water.data(), static_cast<int>(count), Colour(64, 168, 255, 0.92f * alpha));
				}
			}

			const float line = std::max(1.5f, scale * 0.07f);
			ImGuiMCP::ImDrawListManager::AddPolyline(list, drop.data(), static_cast<int>(drop.size()), Colour(232, 244, 255, 0.85f * alpha), ImGuiMCP::ImDrawFlags_Closed, line);
			ImGuiMCP::ImDrawListManager::AddLine(list, toScreen({ -0.32f, 0.02f }), toScreen({ -0.38f, 0.34f }), Colour(255, 255, 255, 0.4f * alpha), line);

			if (trend != 0) {
				const float half = 0.3f * scale;
				const float height = 0.36f * scale;
				const float x = centreX + (kRadius + 0.55f) * scale;
				const float y = centreY + (kTip + kBottom) * 0.5f * scale;
				const float dir = trend > 0 ? -1.0f : 1.0f;  // screen y grows downwards
				const auto  arrow = [&](float a_grow) {
					return std::array<ImVec2, 3>{ ImVec2{ x, y + dir * height * a_grow }, ImVec2{ x - half * a_grow, y - dir * height * a_grow }, ImVec2{ x + half * a_grow, y - dir * height * a_grow } };
				};
				const auto back = arrow(1.3f);
				const auto front = arrow(1.0f);
				ImGuiMCP::ImDrawListManager::AddTriangleFilled(list, back[0], back[1], back[2], Colour(10, 18, 30, 0.5f * alpha));
				const auto tint = trend > 0 ? Colour(112, 204, 255, alpha) : Colour(255, 172, 84, alpha);
				ImGuiMCP::ImDrawListManager::AddTriangleFilled(list, front[0], front[1], front[2], tint);
			}
		}
	}

	void Register()
	{
		if (!GetMenuFrameworkModule()) {
			return;
		}
		static auto* element = SKSEMenuFramework::AddHudElement(Draw);
		(void)element;
		logger::info("Registered the HUD wetness droplet");
	}
}
