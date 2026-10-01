#pragma once

// Consumer copy of Devious Devices NG's public plugin interface (IHateMyKite/DeviousDevicesNG,
// include/API.h, DD_APIVERSION 2). DeviousDevicesAPI is a plain virtual interface (not COM-style
// exports), so every virtual must be declared here in the exact original order even though we only
// call a few of them - the vtable slot is what matters, not whether we use the slot.
//
// Native-only for now: DeviousDevicesAPI exposes worn-device state (GetBondageState, GetWornDevices,
// gag/chastity flags) but not vibration strength, orgasm or "edge" events - those still live in the
// Devious Devices Integration/Expansion Papyrus quests, not in this DLL. A full port of WFR's
// "rate from vibration, orgasm/edge bonus" needs a Papyrus bridge (like SexLab's) and is not done here.

namespace WFNG::Devious
{
	// Bit flags from GetBondageState(); a_actor with sNone wears nothing DDNG tracks
	enum BondageState : std::uint32_t
	{
		sNone = 0x0000,
		sHandsBound = 0x0001,
		sHandsBoundNoAnim = 0x0002,
		sGaggedBlocking = 0x0004,
		sChastifiedGenital = 0x0008,
		sChastifiedAnal = 0x0010,
		sChastifiedBreasts = 0x0020,
		sBlindfolded = 0x0040,
		sMittens = 0x0080,
		sBoots = 0x0100,
		sTotal = 0x0200
	};

	class DeviousDevicesAPI
	{
	public:
		// API functions
		virtual std::size_t GetVersion() const;

		// Device Reader. GetDatabase's real return type is a map keyed on the device's DeviceUnitPrototype
		// (an internal struct we do not otherwise need); never called here, so the slot is kept with a
		// harmless signature instead of pulling in that struct just to satisfy the vtable layout.
		virtual void*               GetDatabase_Unused() const;
		virtual RE::TESObjectARMO* GetDeviceRender(RE::TESObjectARMO* a_invdevice) const;
		virtual RE::TESObjectARMO* GetDeviceInventory(RE::TESObjectARMO* a_renddevice) const;
		virtual RE::TESForm*       GetPropertyForm(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, RE::TESForm* a_defvalue, int a_mode) const;
		virtual int                GetPropertyInt(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_defvalue, int a_mode) const;
		virtual float              GetPropertyFloat(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, float a_defvalue, int a_mode) const;
		virtual bool               GetPropertyBool(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, bool a_defvalue, int a_mode) const;
		virtual std::string        GetPropertyString(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, std::string a_defvalue, int a_mode) const;
		virtual std::vector<RE::TESForm*> GetPropertyFormArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
		virtual std::vector<int>          GetPropertyIntArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
		virtual std::vector<float>        GetPropertyFloatArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
		virtual std::vector<bool>         GetPropertyBoolArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
		virtual std::vector<std::string>  GetPropertyStringArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;

		// Expressions
		virtual bool ApplyExpression(RE::Actor* a_actor, const std::vector<float> a_expression, float a_strength, bool a_openMouth, int a_priority) const;
		virtual bool ResetExpression(RE::Actor* a_actor, int a_priority) const;
		virtual void UpdateGagExpression(RE::Actor* a_actor) const;
		virtual void ResetGagExpression(RE::Actor* a_actor) const;
		virtual bool IsGagged(RE::Actor* a_actor) const;
		virtual bool RegisterGagType(RE::BGSKeyword* a_keyword, std::vector<RE::TESFaction*> a_factions, std::vector<int> a_defaults) const;
		virtual bool RegisterDefaultGagType(std::vector<RE::TESFaction*> a_factions, std::vector<int> a_defaults) const;

		// Hider
		virtual void SetActorStripped(RE::Actor* a_actor, bool a_stripped, int a_armorfilter, int a_devicefilter) const;
		virtual bool IsActorStripped(RE::Actor* a_actor) const;
		virtual bool IsValidForHide(RE::TESObjectARMO* a_armor) const;

		// Lib Functions
		virtual std::vector<RE::TESObjectARMO*> GetDevices(RE::Actor* a_actor, int a_mode, bool a_worn) const;
		virtual RE::TESObjectARMO*               GetWornDevice(RE::Actor* a_actor, RE::BGSKeyword* a_kw, bool a_fuzzy) const;
		virtual std::vector<RE::TESObjectARMO*>  GetWornDevices(RE::Actor* a_actor) const;
		virtual RE::TESObjectARMO*                GetHandRestrain(RE::Actor* a_actor) const;
		virtual BondageState                      GetBondageState(RE::Actor* a_actor) const;
		virtual bool                              IsDevice(RE::TESObjectARMO* a_obj) const;
		virtual bool                              ActorHasBlockingGag(RE::Actor* a_actor, RE::TESObjectARMO* a_gag = nullptr) const;
	};

	void Init();  // kPostPostLoad; loads DeviousDevices.dll and checks DD_APIVERSION
	bool IsAvailable();

	// sNone (0) when DD is not installed or the actor wears nothing tracked
	BondageState Get(RE::Actor* a_actor);

	// sChastifiedGenital or sChastifiedAnal: a belt physically covers what a wet crotch/schlong texture
	// would show
	bool BlocksGenitals(RE::Actor* a_actor);
}
