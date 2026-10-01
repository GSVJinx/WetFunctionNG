#pragma once

#include "Settings.h"

namespace WFNG::Heat
{
	// Loads SunHelm's heat-source lists when SunHelmSurvival.esp is installed (data loaded and every game load).
	void Refresh();

	// Main thread. True while the actor stands within reach of a fire: SunHelm's lists when present, otherwise
	// campfires, fireplaces, braziers and forges recognised by their model name.
	bool IsNear(RE::Actor* a_actor, const Settings& a_settings);

	// Number of base objects taken from SunHelm; any thread.
	std::size_t SunHelmSources();
}
