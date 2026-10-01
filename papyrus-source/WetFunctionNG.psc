Scriptname WetFunctionNG Hidden
{WetFunction NG - native SKSE port of Wet Function Redux with OSweat's OStim and futanari support.
 Global API and console commands. Wetness logic, visuals, SexLab/OStim handling and saved data live in
 WetFunctionNG.dll; the legacy-named WetFunctionNGMCM script only forwards SexLab scene events.}

; ------------------------------------------------------------------
; Settings
; ------------------------------------------------------------------
; Re-reads Data\SKSE\Plugins\WetFunctionNG.ini.
Function ReloadSettings() Global Native

; True when the framework is installed; the MCM greys out its settings otherwise.
Bool Function IsSexLabAvailable() Global Native
Bool Function IsOStimAvailable() Global Native

; ------------------------------------------------------------------
; SexLab P+ bridge (called by WetFunctionNGMCM)
; ------------------------------------------------------------------
; aiPhase: 0 scene start / actors joined, 1 stage update, 2 scene end / actors leaving
Function SexLabUpdate(Form akThread, Actor[] akActors, Float[] afEnjoyment, Float[] afPain, Int aiPhase) Global Native
Function SexLabOrgasm(Actor akActor) Global Native

; ------------------------------------------------------------------
; Actors
; ------------------------------------------------------------------
; Manually started effects never stop by themselves (the player's is controlled by the MCM).
Bool Function StartEffect(Actor akActor) Global Native
Function StopEffect(Actor akActor) Global Native
Bool Function IsActive(Actor akActor) Global Native
Float Function GetWetness(Actor akActor) Global Native
Function SetWetness(Actor akActor, Float afWetness) Global Native
; afWetness < 0, afSpecular <= 0 and afGlossiness <= 0 release the forced value
Function ForceValues(Actor akActor, Float afWetness, Float afSpecular, Float afGlossiness) Global Native
String Function GetStatus(Actor akActor) Global Native
Function PrintStatus(Actor akActor) Global Native
Function Report(Actor akActor) Global Native
; Removes every WetFunction texture/specular override, including ones Wet Function Redux or Dewpoint 1.0 left behind.
Function CleanActor(Actor akActor) Global Native
Function StopAll(Bool abClearData) Global Native

Actor Function Target() Global
	Actor target = Game.GetCurrentCrosshairRef() as Actor
	If target == None
		target = Game.GetPlayer()
	EndIf
	Return target
EndFunction

; ------------------------------------------------------------------
; Console (Custom Console): wfng status|start|stop|soak|dry|release|report|clean|reload|stopall
; Acts on the actor under the crosshair, or the player.
; ------------------------------------------------------------------
Function ConsoleStatus() Global
	PrintStatus(Target())
EndFunction

Function ConsoleStart() Global
	Actor target = Target()
	StartEffect(target)
	PrintStatus(target)
EndFunction

Function ConsoleStop() Global
	StopEffect(Target())
EndFunction

Function ConsoleSoak() Global
	Actor target = Target()
	StartEffect(target)
	SetWetness(target, 1000.0)
EndFunction

Function ConsoleDry() Global
	SetWetness(Target(), 0.0)
EndFunction

Function ConsoleRelease() Global
	ForceValues(Target(), -1.0, 0.0, 0.0)
EndFunction

Function ConsoleReport() Global
	Report(Target())
EndFunction

Function ConsoleClean() Global
	Actor target = Target()
	CleanActor(target)
	If target != Game.GetPlayer()
		CleanActor(Game.GetPlayer())
	EndIf
EndFunction

Function ConsoleReload() Global
	ReloadSettings()
EndFunction

Function ConsoleStopAll() Global
	StopAll(False)
EndFunction

; Legacy helper names retained for scripts or saves that called the former MCM actions.
Function McmStopAll() Global
	StopAll(False)
EndFunction

Function McmClearAll() Global
	StopAll(True)
EndFunction

Function McmCleanPlayer() Global
	CleanActor(Game.GetPlayer())
EndFunction
