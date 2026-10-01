Scriptname WetFunctionNGPlayerAlias extends ReferenceAlias
{Re-registers the lightweight SexLab bridge after loading a save. No SkyUI or MCM dependency.}

Event OnPlayerLoadGame()
	WetFunctionNGMCM bridge = GetOwningQuest() as WetFunctionNGMCM
	If bridge
		bridge.RegisterEvents()
	EndIf
EndEvent
