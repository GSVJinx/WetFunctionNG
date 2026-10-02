#pragma once

// Consumer copy of RaceMenu's public plugin interface (skee64 IPluginInterface.h, 0.4.19+).
// Only the interfaces used here are declared; the declaration order of every virtual must match
// the original exactly, because SKEE hands out its own objects and we call through their vtables.

namespace WFNG::Skee
{
	class IPluginInterface
	{
	public:
		IPluginInterface() {}
		virtual ~IPluginInterface() {}

		virtual std::uint32_t GetVersion() = 0;
		virtual void          Revert() = 0;
	};

	class IInterfaceMap
	{
	public:
		virtual IPluginInterface* QueryInterface(const char* name) = 0;
		virtual bool              AddInterface(const char* name, IPluginInterface* pluginInterface) = 0;
		virtual IPluginInterface* RemoveInterface(const char* name) = 0;
	};

	struct InterfaceExchangeMessage
	{
		enum : std::uint32_t
		{
			kMessage_ExchangeInterface = 0x9E3779B9
		};

		IInterfaceMap* interfaceMap = nullptr;
	};

	class IOverrideInterface : public IPluginInterface
	{
	public:
		enum
		{
			kPluginVersion1 = 1,
			kPluginVersion2,  // wrapper interface below
		};

		class GetVariant
		{
		public:
			virtual void Int(const std::int32_t i) = 0;
			virtual void Float(const float f) = 0;
			virtual void String(const char* str) = 0;
			virtual void Bool(const bool b) = 0;
			virtual void TextureSet(const RE::BGSTextureSet* textureSet) = 0;
		};

		class SetVariant
		{
		public:
			enum class Type
			{
				None,
				Int,
				Float,
				String,
				Bool,
				TextureSet
			};
			virtual Type               GetType() { return Type::None; }
			virtual std::int32_t       Int() { return 0; }
			virtual float              Float() { return 0.0f; }
			virtual const char*        String() { return nullptr; }
			virtual bool               Bool() { return false; }
			virtual RE::BGSTextureSet* TextureSet() { return nullptr; }
		};

		virtual bool HasArmorAddonNode(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, bool debug) = 0;

		virtual bool HasArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, std::uint16_t key, std::uint8_t index) = 0;
		virtual void AddArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, std::uint16_t key, std::uint8_t index, SetVariant& value) = 0;
		virtual bool GetArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, std::uint16_t key, std::uint8_t index, GetVariant& visitor) = 0;
		virtual void RemoveArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, std::uint16_t key, std::uint8_t index) = 0;
		virtual void SetArmorProperties(RE::TESObjectREFR* refr, bool immediate) = 0;
		virtual void SetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, std::uint16_t key, std::uint8_t index, SetVariant& value, bool immediate) = 0;
		virtual bool GetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, std::uint16_t key, std::uint8_t index, GetVariant& value) = 0;
		virtual void ApplyArmorOverrides(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool immediate) = 0;
		virtual void RemoveAllArmorOverrides() = 0;
		virtual void RemoveAllArmorOverridesByReference(RE::TESObjectREFR* reference) = 0;
		virtual void RemoveAllArmorOverridesByArmor(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor) = 0;
		virtual void RemoveAllArmorOverridesByAddon(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon) = 0;
		virtual void RemoveAllArmorOverridesByNode(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName) = 0;

		virtual bool HasNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index) = 0;
		virtual void AddNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index, SetVariant& value) = 0;
		virtual bool GetNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index, GetVariant& visitor) = 0;
		virtual void RemoveNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index) = 0;
		virtual void SetNodeProperties(RE::TESObjectREFR* refr, bool immediate) = 0;
		virtual void SetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName, std::uint16_t key, std::uint8_t index, SetVariant& value, bool immediate) = 0;
		virtual bool GetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName, std::uint16_t key, std::uint8_t index, GetVariant& value) = 0;
		virtual void ApplyNodeOverrides(RE::TESObjectREFR* refr, RE::NiAVObject* object, bool immediate) = 0;
		virtual void RemoveAllNodeOverrides() = 0;
		virtual void RemoveAllNodeOverridesByReference(RE::TESObjectREFR* reference) = 0;
		virtual void RemoveAllNodeOverridesByNode(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName) = 0;

		virtual bool HasSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index) = 0;
		virtual void AddSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, SetVariant& value) = 0;
		virtual bool GetSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, GetVariant& visitor) = 0;
		virtual void RemoveSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index) = 0;
		virtual void SetSkinProperties(RE::TESObjectREFR* refr, bool immediate) = 0;
		virtual void SetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, SetVariant& value, bool immediate) = 0;
		virtual bool GetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, GetVariant& value) = 0;
		virtual void ApplySkinOverrides(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, std::uint32_t slotMask, RE::NiAVObject* object, bool immediate) = 0;
		virtual void RemoveAllSkinOverrides() = 0;
		virtual void RemoveAllSkinOverridesByReference(RE::TESObjectREFR* reference) = 0;
		virtual void RemoveAllSkinOverridesBySlot(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask) = 0;
	};

	// NiOverride keys (skee OverrideVariant kParam_*)
	inline constexpr std::uint16_t kKeyGlossiness = 2;
	inline constexpr std::uint16_t kKeySpecular = 3;
	inline constexpr std::uint16_t kKeyTexture = 9;
	inline constexpr std::uint8_t  kNoIndex = 0xFF;

	struct StringValue final : IOverrideInterface::SetVariant
	{
		explicit StringValue(std::string a_value) : value(std::move(a_value)) {}
		Type        GetType() override { return Type::String; }
		const char* String() override { return value.c_str(); }
		std::string value;
	};

	struct FloatValue final : IOverrideInterface::SetVariant
	{
		explicit FloatValue(float a_value) : value(a_value) {}
		Type  GetType() override { return Type::Float; }
		float Float() override { return value; }
		float value;
	};

	struct Reader final : IOverrideInterface::GetVariant
	{
		void Int(const std::int32_t i) override { number = static_cast<float>(i); }
		void Float(const float f) override { number = f; }
		void String(const char* s) override { text = s ? s : ""; }
		void Bool(const bool b) override { number = b ? 1.0f : 0.0f; }
		void TextureSet(const RE::BGSTextureSet*) override {}

		std::optional<std::string> text;
		std::optional<float>       number;
	};

	// Sends the SKEE interface exchange message; call at kPostPostLoad.
	bool                Connect();
	// RaceMenu before 0.4.19 (Skyrim 1.5.97): call its NiOverride natives directly; call at kDataLoaded or later
	bool                ConnectLegacy();
	IOverrideInterface* Overrides();
}
