#pragma once

#include "Skee.h"

// RaceMenu SE 0.4.16 (Skyrim 1.5.97) has no IOverrideInterface. Its NiOverride Papyrus natives are plain C++
// functions that call the same override code, so this resolves them through the Papyrus VM's function table and
// calls them directly (no script, no VM queue), behind the same IOverrideInterface the rest of the plugin uses.

namespace WFNG::Skee
{
	// Resolves and validates every NiOverride native this plugin needs; nullptr when anything is missing or
	// looks wrong. Call once the Papyrus VM exists (kDataLoaded or later).
	IOverrideInterface* ConnectNatives();
}
