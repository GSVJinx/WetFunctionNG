#pragma once

namespace WFNG
{
	struct Settings;
}

namespace WFNG::Visuals
{
	// What an actor should look like. Floats <= 0 mean "leave the actor's own value".
	struct Look
	{
		bool  active{ false };  // false: restore every dry value and drop every override
		bool  drops{ false };
		bool  sweat{ false };
		bool  pussy{ false };
		bool  schlong{ false };
		float specular{ -1.0f };
		float glossiness{ -1.0f };

		bool operator==(const Look&) const = default;
	};

	// Main thread only. Pushes only what changed since the last call for this actor.
	void Apply(RE::Actor* a_actor, const Look& a_look, const Settings& a_settings);
	void Clear(RE::Actor* a_actor, const Settings& a_settings);

	void Invalidate(RE::FormID a_actor);  // 3D rebuilt, outfit or race changed: push again, re-read dry values
	void Forget(RE::FormID a_actor);
	void ForgetAll();

	// Removes every WetFunction override from an actor, whatever put it there (old WFR, Dewpoint 1.0).
	std::size_t Clean(RE::Actor* a_actor);

	bool HasSchlong(RE::Actor* a_actor);
	void Dump(RE::Actor* a_actor, std::string_view a_title);
	void Out(const std::string& a_line);
}
