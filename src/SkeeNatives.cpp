#include "SkeeNatives.h"

namespace WFNG::Skee
{
	namespace
	{
		// Mirror of SKSE 2.0.x BSFixedString (StringCache::Ref), which skee64 0.4.16 was compiled against: one
		// pointer, user-declared constructors, trivial copy and destructor. On x64 MSVC that shape is passed by value
		// in a register and returned through a hidden pointer. RE::BSFixedString has a non-trivial copy/destructor
		// and is passed by reference instead, so it must never appear in the signatures below.
		struct SkseString
		{
			SkseString() noexcept {}
			explicit SkseString(const char* a_data) noexcept : data(a_data) {}

			const char* data = nullptr;
		};
		static_assert(sizeof(SkseString) == 8);
		static_assert(std::is_trivially_copyable_v<SkseString> && std::is_trivially_destructible_v<SkseString>);

		using Refr = RE::TESObjectREFR*;
		using U32 = std::uint32_t;

		// papyrusNiOverride::* in skee64 0.4.16; the first parameter is SKSE's StaticFunctionTag*, always unused
		using AddSkinString = void (*)(void*, Refr, bool, bool, U32, U32, U32, SkseString, bool);
		using AddSkinFloat = void (*)(void*, Refr, bool, bool, U32, U32, U32, float, bool);
		using GetSkinString = SkseString (*)(void*, Refr, bool, bool, U32, U32, U32);
		using GetSkinFloat = float (*)(void*, Refr, bool, bool, U32, U32, U32);
		using HasSkin = bool (*)(void*, Refr, bool, bool, U32, U32, U32);
		using RemoveSkin = void (*)(void*, Refr, bool, bool, U32, U32, U32);
		using AddNodeString = void (*)(void*, Refr, bool, SkseString, U32, U32, SkseString, bool);
		using AddNodeFloat = void (*)(void*, Refr, bool, SkseString, U32, U32, float, bool);
		using GetNodeString = SkseString (*)(void*, Refr, bool, SkseString, U32, U32);
		using GetNodeFloat = float (*)(void*, Refr, bool, SkseString, U32, U32);
		using HasNode = bool (*)(void*, Refr, bool, SkseString, U32, U32);
		using RemoveNode = void (*)(void*, Refr, bool, SkseString, U32, U32);
		using ApplyRefr = void (*)(void*, Refr);

		struct Natives
		{
			AddSkinString addSkinString = nullptr;
			AddSkinFloat  addSkinFloat = nullptr;
			GetSkinString getSkinString = nullptr;
			GetSkinFloat  getSkinFloat = nullptr;
			HasSkin       hasSkin = nullptr;
			RemoveSkin    removeSkin = nullptr;
			ApplyRefr     applySkin = nullptr;
			AddNodeString addNodeString = nullptr;
			AddNodeFloat  addNodeFloat = nullptr;
			GetNodeString getNodeString = nullptr;
			GetNodeFloat  getNodeFloat = nullptr;
			HasNode       hasNode = nullptr;
			RemoveNode    removeNode = nullptr;
			ApplyRefr     applyNode = nullptr;
		};

		// signature letters: v none, o object, b bool, i int, f float, s string (the Papyrus side of each native)
		struct Spec
		{
			std::string_view name;
			char             ret;
			std::string_view params;
			void**           slot;
		};

		// SKSE 2.0.x NativeFunction: NativeFunctionBase is 0x50 bytes, the C++ callback pointer follows it
		constexpr std::uintptr_t kCallbackOffset = 0x50;

		// kKeyTexture is the only string key; every other key WetFunctionNG reads (specular, glossiness) is a float
		bool IsStringKey(std::uint16_t a_key) { return a_key == kKeyTexture; }

		struct Image
		{
			std::uintptr_t begin = 0;
			std::uintptr_t end = 0;

			[[nodiscard]] bool Contains(std::uintptr_t a_address) const { return a_address >= begin && a_address < end; }
		};

		std::optional<Image> SkeeImage()
		{
			const auto module = ::GetModuleHandleW(L"skee64.dll");
			if (!module) {
				return std::nullopt;
			}
			// SizeOfImage straight from the PE header: e_lfanew at 0x3C, OptionalHeader at NT+0x18, SizeOfImage at +0x38
			const auto base = reinterpret_cast<std::uintptr_t>(module);
			const auto nt = base + *reinterpret_cast<const std::uint32_t*>(base + 0x3C);
			const auto size = *reinterpret_cast<const std::uint32_t*>(nt + 0x18 + 0x38);
			return Image{ base, base + size };
		}

		char Letter(const RE::BSScript::TypeInfo& a_type)
		{
			if (a_type.IsBool()) {
				return 'b';
			}
			if (a_type.IsInt()) {
				return 'i';
			}
			if (a_type.IsFloat()) {
				return 'f';
			}
			if (a_type.IsString()) {
				return 's';
			}
			if (a_type.IsObject()) {
				return 'o';
			}
			if (a_type.GetRawType() == RE::BSScript::TypeInfo::RawType::kNone) {
				return 'v';
			}
			return '?';
		}

		// Finds a_spec in NiOverride's global function table and stores its C++ callback in a_spec.slot. Every check
		// must pass: the Papyrus signature matches the function pointer type we will call through, and both the
		// object's vtable and the callback live inside skee64.dll (so it is an SKSE NativeFunction skee registered).
		bool Resolve(const RE::BSScript::ObjectTypeInfo& a_type, const Image& a_skee, const Spec& a_spec)
		{
			const auto* funcs = a_type.GetGlobalFuncIter();
			for (std::uint32_t i = 0; funcs && i < a_type.GetNumGlobalFuncs(); ++i) {
				auto* fn = funcs[i].func.get();
				if (!fn || fn->GetName() != a_spec.name) {
					continue;
				}
				if (!fn->GetIsNative() || !fn->GetIsStatic()) {
					logger::error("NiOverride.{} is not a global native", a_spec.name);
					return false;
				}
				std::string signature(1, Letter(fn->GetReturnType()));
				signature += ':';
				for (std::uint32_t p = 0; p < fn->GetParamCount(); ++p) {
					RE::BSFixedString        name;
					RE::BSScript::TypeInfo   type;
					fn->GetParam(p, name, type);
					signature += Letter(type);
				}
				const auto expected = std::string(1, a_spec.ret) + ':' + std::string(a_spec.params);
				if (signature != expected) {
					logger::error("NiOverride.{} has signature {}, expected {}", a_spec.name, signature, expected);
					return false;
				}
				const auto object = reinterpret_cast<std::uintptr_t>(fn);
				const auto vtable = *reinterpret_cast<const std::uintptr_t*>(object);
				const auto callback = *reinterpret_cast<const std::uintptr_t*>(object + kCallbackOffset);
				if (!a_skee.Contains(vtable) || !a_skee.Contains(callback)) {
					logger::error("NiOverride.{} is not skee64's own native (vtable {:X}, callback {:X}, skee64 {:X}-{:X})",
						a_spec.name, vtable, callback, a_skee.begin, a_skee.end);
					return false;
				}
				*a_spec.slot = reinterpret_cast<void*>(callback);
				logger::info("  NiOverride.{} -> skee64+{:X}", a_spec.name, callback - a_skee.begin);
				return true;
			}
			logger::error("NiOverride.{} not found", a_spec.name);
			return false;
		}

		class NativeOverrides final : public IOverrideInterface
		{
		public:
			explicit NativeOverrides(const Natives& a_natives) : _n(a_natives) {}

			std::uint32_t GetVersion() override { return kPluginVersion2; }
			void          Revert() override {}

			// --- skin overrides (body, hands, feet, genitals by slot mask)

			bool HasSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index) override
			{
				return refr && _n.hasSkin(nullptr, refr, isFemale, firstPerson, slotMask, key, index);
			}

			void AddSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, SetVariant& value) override
			{
				if (!refr) {
					return;
				}
				switch (value.GetType()) {
				case SetVariant::Type::String:
					{
						const RE::BSFixedString interned(value.String() ? value.String() : "");
						_n.addSkinString(nullptr, refr, isFemale, firstPerson, slotMask, key, index, SkseString(interned.data()), true);
						break;
					}
				case SetVariant::Type::Float:
					_n.addSkinFloat(nullptr, refr, isFemale, firstPerson, slotMask, key, index, value.Float(), true);
					break;
				default:
					Unsupported("AddSkinOverride with a non string/float value");
					break;
				}
			}

			bool GetSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, GetVariant& visitor) override
			{
				// the natives return "" / 0 for a missing override, so existence comes from Has first
				if (!HasSkinOverride(refr, isFemale, firstPerson, slotMask, key, index)) {
					return false;
				}
				if (IsStringKey(key)) {
					const auto result = _n.getSkinString(nullptr, refr, isFemale, firstPerson, slotMask, key, index);
					visitor.String(result.data ? result.data : "");
				} else {
					visitor.Float(_n.getSkinFloat(nullptr, refr, isFemale, firstPerson, slotMask, key, index));
				}
				return true;
			}

			void RemoveSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index) override
			{
				if (refr) {
					_n.removeSkin(nullptr, refr, isFemale, firstPerson, slotMask, key, index);
				}
			}

			void SetSkinProperties(RE::TESObjectREFR* refr, bool) override
			{
				if (refr) {
					_n.applySkin(nullptr, refr);
				}
			}

			// --- node overrides (the head, by node name)

			bool HasNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index) override
			{
				if (!refr || !nodeName) {
					return false;
				}
				const RE::BSFixedString node(nodeName);
				return _n.hasNode(nullptr, refr, isFemale, SkseString(node.data()), key, index);
			}

			void AddNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index, SetVariant& value) override
			{
				if (!refr || !nodeName) {
					return;
				}
				const RE::BSFixedString node(nodeName);
				switch (value.GetType()) {
				case SetVariant::Type::String:
					{
						const RE::BSFixedString interned(value.String() ? value.String() : "");
						_n.addNodeString(nullptr, refr, isFemale, SkseString(node.data()), key, index, SkseString(interned.data()), true);
						break;
					}
				case SetVariant::Type::Float:
					_n.addNodeFloat(nullptr, refr, isFemale, SkseString(node.data()), key, index, value.Float(), true);
					break;
				default:
					Unsupported("AddNodeOverride with a non string/float value");
					break;
				}
			}

			bool GetNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index, GetVariant& visitor) override
			{
				if (!HasNodeOverride(refr, isFemale, nodeName, key, index)) {
					return false;
				}
				const RE::BSFixedString node(nodeName);
				if (IsStringKey(key)) {
					const auto result = _n.getNodeString(nullptr, refr, isFemale, SkseString(node.data()), key, index);
					visitor.String(result.data ? result.data : "");
				} else {
					visitor.Float(_n.getNodeFloat(nullptr, refr, isFemale, SkseString(node.data()), key, index));
				}
				return true;
			}

			void RemoveNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, std::uint16_t key, std::uint8_t index) override
			{
				if (!refr || !nodeName) {
					return;
				}
				const RE::BSFixedString node(nodeName);
				_n.removeNode(nullptr, refr, isFemale, SkseString(node.data()), key, index);
			}

			void SetNodeProperties(RE::TESObjectREFR* refr, bool) override
			{
				if (refr) {
					_n.applyNode(nullptr, refr);
				}
			}

			// --- not used by WetFunctionNG; not wired to RaceMenu 0.4.16

			bool HasArmorAddonNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, bool) override { return Unsupported("HasArmorAddonNode"); }
			bool HasArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t) override { return Unsupported("HasArmorOverride"); }
			void AddArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, SetVariant&) override { Unsupported("AddArmorOverride"); }
			bool GetArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, GetVariant&) override { return Unsupported("GetArmorOverride"); }
			void RemoveArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t) override { Unsupported("RemoveArmorOverride"); }
			void SetArmorProperties(RE::TESObjectREFR*, bool) override { Unsupported("SetArmorProperties"); }
			void SetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, SetVariant&, bool) override { Unsupported("SetArmorProperty"); }
			bool GetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, GetVariant&) override { return Unsupported("GetArmorProperty"); }
			void ApplyArmorOverrides(RE::TESObjectREFR*, RE::TESObjectARMO*, RE::TESObjectARMA*, RE::NiAVObject*, bool) override { Unsupported("ApplyArmorOverrides"); }
			void RemoveAllArmorOverrides() override { Unsupported("RemoveAllArmorOverrides"); }
			void RemoveAllArmorOverridesByReference(RE::TESObjectREFR*) override { Unsupported("RemoveAllArmorOverridesByReference"); }
			void RemoveAllArmorOverridesByArmor(RE::TESObjectREFR*, bool, RE::TESObjectARMO*) override { Unsupported("RemoveAllArmorOverridesByArmor"); }
			void RemoveAllArmorOverridesByAddon(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*) override { Unsupported("RemoveAllArmorOverridesByAddon"); }
			void RemoveAllArmorOverridesByNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*) override { Unsupported("RemoveAllArmorOverridesByNode"); }

			void SetNodeProperty(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, SetVariant&, bool) override { Unsupported("SetNodeProperty"); }
			bool GetNodeProperty(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, GetVariant&) override { return Unsupported("GetNodeProperty"); }
			void ApplyNodeOverrides(RE::TESObjectREFR*, RE::NiAVObject*, bool) override { Unsupported("ApplyNodeOverrides(object)"); }
			void RemoveAllNodeOverrides() override { Unsupported("RemoveAllNodeOverrides"); }
			void RemoveAllNodeOverridesByReference(RE::TESObjectREFR*) override { Unsupported("RemoveAllNodeOverridesByReference"); }
			void RemoveAllNodeOverridesByNode(RE::TESObjectREFR*, bool, const char*) override { Unsupported("RemoveAllNodeOverridesByNode"); }

			void SetSkinProperty(RE::TESObjectREFR*, bool, std::uint32_t, std::uint16_t, std::uint8_t, SetVariant&, bool) override { Unsupported("SetSkinProperty"); }
			bool GetSkinProperty(RE::TESObjectREFR*, bool, std::uint32_t, std::uint16_t, std::uint8_t, GetVariant&) override { return Unsupported("GetSkinProperty"); }
			void ApplySkinOverrides(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, std::uint32_t, RE::NiAVObject*, bool) override { Unsupported("ApplySkinOverrides(object)"); }
			void RemoveAllSkinOverrides() override { Unsupported("RemoveAllSkinOverrides"); }
			void RemoveAllSkinOverridesByReference(RE::TESObjectREFR*) override { Unsupported("RemoveAllSkinOverridesByReference"); }
			void RemoveAllSkinOverridesBySlot(RE::TESObjectREFR*, bool, bool, std::uint32_t) override { Unsupported("RemoveAllSkinOverridesBySlot"); }

		private:
			static bool Unsupported(std::string_view a_what)
			{
				static std::mutex                      lock;
				static std::unordered_set<std::string> reported;
				std::scoped_lock                       guard(lock);
				if (reported.emplace(a_what).second) {
					logger::warn("{} is not available through RaceMenu 0.4.16's NiOverride natives; ignored", a_what);
				}
				return false;
			}

			Natives _n;
		};
	}

	IOverrideInterface* ConnectNatives()
	{
		const auto skee = SkeeImage();
		if (!skee) {
			logger::critical("skee64.dll is not loaded; wetness visuals are disabled");
			return nullptr;
		}

		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
		if (!vm || !vm->GetScriptObjectType(RE::BSFixedString("NiOverride"), type) || !type) {
			logger::critical("Papyrus has no NiOverride script type (RaceMenu scripts missing?); wetness visuals are disabled");
			return nullptr;
		}

		Natives n;
		const Spec specs[] = {
			{ "AddSkinOverrideString", 'v', "obbiiisb", reinterpret_cast<void**>(&n.addSkinString) },
			{ "AddSkinOverrideFloat", 'v', "obbiiifb", reinterpret_cast<void**>(&n.addSkinFloat) },
			{ "GetSkinOverrideString", 's', "obbiii", reinterpret_cast<void**>(&n.getSkinString) },
			{ "GetSkinOverrideFloat", 'f', "obbiii", reinterpret_cast<void**>(&n.getSkinFloat) },
			{ "HasSkinOverride", 'b', "obbiii", reinterpret_cast<void**>(&n.hasSkin) },
			{ "RemoveSkinOverride", 'v', "obbiii", reinterpret_cast<void**>(&n.removeSkin) },
			{ "ApplySkinOverrides", 'v', "o", reinterpret_cast<void**>(&n.applySkin) },
			{ "AddNodeOverrideString", 'v', "obsiisb", reinterpret_cast<void**>(&n.addNodeString) },
			{ "AddNodeOverrideFloat", 'v', "obsiifb", reinterpret_cast<void**>(&n.addNodeFloat) },
			{ "GetNodeOverrideString", 's', "obsii", reinterpret_cast<void**>(&n.getNodeString) },
			{ "GetNodeOverrideFloat", 'f', "obsii", reinterpret_cast<void**>(&n.getNodeFloat) },
			{ "HasNodeOverride", 'b', "obsii", reinterpret_cast<void**>(&n.hasNode) },
			{ "RemoveNodeOverride", 'v', "obsii", reinterpret_cast<void**>(&n.removeNode) },
			{ "ApplyNodeOverrides", 'v', "o", reinterpret_cast<void**>(&n.applyNode) },
		};

		logger::info("Resolving RaceMenu NiOverride natives (skee64 {:X}-{:X})", skee->begin, skee->end);
		bool ok = true;
		for (const auto& spec : specs) {
			ok = Resolve(*type, *skee, spec) && ok;
		}
		if (!ok) {
			logger::critical("Some NiOverride natives did not validate; wetness visuals are disabled");
			return nullptr;
		}

		static NativeOverrides overrides(n);
		logger::info("Connected to RaceMenu NiOverride natives ({} of {})", std::size(specs), std::size(specs));
		return &overrides;
	}
}
