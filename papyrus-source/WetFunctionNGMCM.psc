Scriptname WetFunctionNGMCM extends Quest
{Legacy script name retained for save compatibility. This is not an MCM: configuration is native through
 SKSE Menu Framework. The script only forwards SexLab / SexLab P+ scene data that is exposed through Papyrus.}

Event OnInit()
	RegisterEvents()
EndEvent

Function RegisterEvents()
	UnregisterForAllModEvents()
	RegisterForModEvent("AnimationStart", "OnSexLabScene")
	RegisterForModEvent("ActorChangeStart", "OnSexLabScene")
	RegisterForModEvent("ActorChangeEnd", "OnSexLabScene")
	RegisterForModEvent("AnimationEnd", "OnSexLabScene")
	RegisterForModEvent("StageStart", "OnSexLabScene")
	RegisterForModEvent("SexLabOrgasm", "OnSexLabOrgasm")
EndFunction

Event OnSexLabScene(String asEvent, String asThread, Float afValue, Form akSender)
	sslThreadController thread = akSender as sslThreadController
	If thread == None
		Return
	EndIf
	Int phase = 1
	If asEvent == "AnimationStart" || asEvent == "ActorChangeEnd"
		phase = 0
	ElseIf asEvent == "AnimationEnd" || asEvent == "ActorChangeStart"
		phase = 2
	EndIf
	Actor[] positions = thread.GetPositions()
	Int count = positions.Length
	Float[] enjoyment = Utility.CreateFloatArray(count)
	Float[] pain = Utility.CreateFloatArray(count)
	If phase == 1
		Int i = 0
		While i < count
			If positions[i]
				enjoyment[i] = thread.GetEnjoyment(positions[i]) as Float
				pain[i] = thread.GetPain(positions[i]) as Float
			EndIf
			i += 1
		EndWhile
	EndIf
	WetFunctionNG.SexLabUpdate(akSender, positions, enjoyment, pain, phase)
EndEvent

Event OnSexLabOrgasm(Form akActor, Int aiEnjoyment, Int aiOrgasms)
	WetFunctionNG.SexLabOrgasm(akActor as Actor)
EndEvent
