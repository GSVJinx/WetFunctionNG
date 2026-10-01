#include "Visuals.h"

#include "Settings.h"
#include "Skee.h"

namespace WFNG::Visuals
{
	namespace
	{
		constexpr auto kWetFolder = "textures\\actors\\character\\WetFunction\\"sv;

		enum class Mode : std::uint8_t
		{
			kUnknown,
			kNone,      // no skin geometry
			kSpecular,  // model space normals: wet map in the specular slot (7), "_s"
			kNormal,    // tangent space UBE: wet map replaces the normal map (1), "_UBE_n"
			kFloats     // tangent space, not UBE: no matching wet map, specular/glossiness only
		};

		enum Part : std::size_t
		{
			kBody,
			kHands,
			kFeet,
			kSchlong,
			kHead,
			kPartCount
		};
		constexpr std::array<std::uint32_t, kPartCount> kMasks{ 0x04, 0x08, 0x80, 0x400000, 0 };

		struct PartState
		{
			std::string                tex;  // wet texture stored as override, "" none
			std::uint8_t               texIndex{ 0 };
			float                      spec{ -1.0f };
			float                      gloss{ -1.0f };
			std::optional<std::string> dryTex;
			std::optional<float>       drySpec;
			std::optional<float>       dryGloss;
		};

		struct ActorState
		{
			std::array<PartState, kPartCount> parts;
			std::string                       headNode;
			Mode                              mode{ Mode::kUnknown };
			std::optional<Look>               look;
			bool                              dirty{ true };
		};

		std::recursive_mutex                        g_lock;
		std::unordered_map<RE::FormID, ActorState>  g_states;
		std::unordered_map<std::string, bool>       g_files;

		// Wet textures stay referenced once loaded: RaceMenu loads a swapped texture synchronously on the main
		// thread and the engine drops it again as soon as no material uses it, so every swap of a 4K map
		// stuttered. Preloaded sets are queued to the engine's background loader.
		std::unordered_map<std::string, RE::NiPointer<RE::NiTexture>> g_textures;
		std::unordered_set<std::string>                               g_preloadedSets;

		void KeepLoaded(const std::string& a_path, bool a_demand)
		{
			if (g_textures.contains(a_path)) {
				return;
			}
			RE::NiPointer<RE::NiTexture> texture;
			RE::BSShaderManager::GetTexture(a_path.c_str(), a_demand, texture, false);
			if (texture) {
				g_textures.emplace(a_path, std::move(texture));
			}
		}

		bool IContains(std::string_view a_text, std::string_view a_needle)
		{
			return std::search(a_text.begin(), a_text.end(), a_needle.begin(), a_needle.end(), [](char a, char b) {
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			}) != a_text.end();
		}

		bool IsWet(std::string_view a_path) { return IContains(a_path, "\\WetFunction\\"); }

		bool IsFemale(RE::Actor* a_actor)
		{
			const auto* npc = a_actor->GetActorBase();
			return npc && npc->GetSex() == RE::SEX::kFemale;
		}

		std::string RaceEditorID(RE::Actor* a_actor)
		{
			const auto* race = a_actor->GetRace();
			const char* id = race ? race->GetFormEditorID() : nullptr;
			return id ? id : "";
		}

		std::string FaceNode(RE::Actor* a_actor)
		{
			auto* npc = a_actor->GetActorBase();
			auto* part = npc ? npc->GetHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace) : nullptr;
			return part ? part->formEditorID.c_str() : "";
		}

		RE::BSLightingShaderProperty* Shader(RE::BSGeometry* a_geometry)
		{
			auto* shader = netimmerse_cast<RE::BSLightingShaderProperty*>(a_geometry->GetGeometryRuntimeData().shaderProperty.get());
			return shader && shader->material ? shader : nullptr;
		}

		RE::BSLightingShaderMaterialBase* Material(RE::BSGeometry* a_geometry)
		{
			auto* shader = Shader(a_geometry);
			return shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
		}

		std::string Tex(RE::BSGeometry* a_geometry, std::uint32_t a_index)
		{
			auto*       material = Material(a_geometry);
			auto*       set = material ? material->textureSet.get() : nullptr;
			const char* path = set ? set->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(a_index)) : nullptr;
			return path ? path : "";
		}

		bool IsSkinShader(RE::BSGeometry* a_geometry)
		{
			auto* material = Material(a_geometry);
			return material && material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint;
		}

		// the geometry SKEE's slot-matched skin overrides act on (skee IsSlotMatch + FaceGenRGBTint)
		std::vector<RE::BSGeometry*> SlotGeometries(RE::Actor* a_actor, std::uint32_t a_mask)
		{
			std::vector<RE::BSGeometry*> result;
			auto*                        root = a_actor->Get3D(false);
			auto*                        armor = a_actor->GetSkin(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(a_mask));
			if (!root || !armor) {
				return result;
			}
			for (auto* addon : armor->armorAddons) {
				if (!addon || (addon->bipedModelData.bipedObjectSlots.underlying() & a_mask) == 0) {
					continue;
				}
				char name[260]{};
				addon->GetNodeName(name, a_actor, armor, -1.0f);
				auto* node = root->GetObjectByName(RE::BSFixedString(name));
				if (!node) {
					continue;
				}
				RE::BSVisit::TraverseScenegraphGeometries(node, [&](RE::BSGeometry* a_geometry) {
					if (IsSkinShader(a_geometry) && std::ranges::find(result, a_geometry) == result.end()) {
						result.push_back(a_geometry);
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			}
			return result;
		}

		std::vector<RE::BSGeometry*> HeadGeometries(RE::Actor* a_actor, const std::string& a_node)
		{
			std::vector<RE::BSGeometry*> result;
			auto*                        root = a_actor->Get3D(false);
			if (!root || a_node.empty()) {
				return result;
			}
			RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geometry) {
				if (a_node == a_geometry->name.c_str() && Material(a_geometry)) {
					result.push_back(a_geometry);
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return result;
		}

		Mode ModeFor(RE::Actor* a_actor)
		{
			const auto body = SlotGeometries(a_actor, 0x04);
			if (body.empty()) {
				return Mode::kNone;
			}
			auto* shader = Shader(body.front());
			if (shader && shader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals)) {
				return Mode::kSpecular;
			}
			if (IContains(RaceEditorID(a_actor), "UBE") || IContains(Tex(body.front(), 1), "UBE")) {
				return Mode::kNormal;
			}
			return Mode::kFloats;
		}

		std::string_view ModeName(Mode a_mode)
		{
			switch (a_mode) {
			case Mode::kSpecular:
				return "specular slot 7 (_s)"sv;
			case Mode::kNormal:
				return "normal slot 1 (_UBE_n)"sv;
			case Mode::kFloats:
				return "floats only"sv;
			case Mode::kNone:
				return "no skin"sv;
			default:
				return "unknown"sv;
			}
		}

		std::string WetTexture(bool a_female, Mode a_mode, std::string_view a_base)
		{
			return std::format("{}{}{}{}", kWetFolder, a_female ? "" : "male_", a_base, a_mode == Mode::kNormal ? "_UBE_n.dds"sv : "_s.dds"sv);
		}

		bool FileExists(const std::string& a_path);

		// every map one body type can receive: body/feet variants, hands, head, schlong
		void PreloadSet(bool a_female, Mode a_mode)
		{
			if (a_mode != Mode::kSpecular && a_mode != Mode::kNormal) {
				return;
			}
			const auto key = std::format("{}{}", a_female ? 'f' : 'm', static_cast<int>(a_mode));
			if (!g_preloadedSets.insert(key).second) {
				return;
			}
			std::vector<std::string> bases = a_female ? std::vector<std::string>{ "wet_001", "wet_010", "wet_011", "wet_100", "wet_101", "wet_110", "wet_111" } :
			                                            std::vector<std::string>{ "wet_01", "wet_10", "wet_11" };
			bases.insert(bases.end(), { "wethand", "wethead", "wetschlong" });
			std::size_t queued = 0;
			for (const auto& base : bases) {
				const auto path = WetTexture(a_female, a_female && base == "wetschlong" ? Mode::kSpecular : a_mode, base);
				if (FileExists(path)) {
					KeepLoaded(path, false);
					++queued;
				}
			}
			logger::info("Preloading {} wet textures ({} {})", queued, a_female ? "female" : "male", a_mode == Mode::kNormal ? "UBE" : "specular");
		}

		bool FileExists(const std::string& a_path)
		{
			if (const auto it = g_files.find(a_path); it != g_files.end()) {
				return it->second;
			}
			std::error_code ec;
			const bool      exists = std::filesystem::exists(std::filesystem::path("Data") / a_path, ec);
			if (!exists) {
				logger::warn("Missing wet texture {}", a_path);
			}
			return g_files.emplace(a_path, exists).first->second;
		}

		struct Want
		{
			std::string  tex;
			std::uint8_t index{ 7 };
			float        spec{ -1.0f };
			float        gloss{ -1.0f };
		};

		class Writer
		{
		public:
			Writer(RE::Actor* a_actor, const ActorState& a_state, bool a_firstPerson) :
				_actor(a_actor), _state(a_state), _female(IsFemale(a_actor)), _firstPerson(a_firstPerson), _skee(Skee::Overrides())
			{}

			void Tex(Part a_part, std::uint8_t a_index, const std::string& a_path)
			{
				Skee::StringValue value(a_path);
				if (a_part == kHead) {
					_skee->AddNodeOverride(_actor, _female, _state.headNode.c_str(), Skee::kKeyTexture, a_index, value);
					_head = true;
					return;
				}
				for (const bool fp : { false, true }) {
					if (fp && !_firstPerson) {
						continue;
					}
					_skee->AddSkinOverride(_actor, _female, fp, kMasks[a_part], Skee::kKeyTexture, a_index, value);
				}
				_skin = true;
			}

			void Float(Part a_part, std::uint16_t a_key, float a_value)
			{
				Skee::FloatValue value(a_value);
				if (a_part == kHead) {
					_skee->AddNodeOverride(_actor, _female, _state.headNode.c_str(), a_key, Skee::kNoIndex, value);
					_head = true;
					return;
				}
				for (const bool fp : { false, true }) {
					if (fp && !_firstPerson) {
						continue;
					}
					_skee->AddSkinOverride(_actor, _female, fp, kMasks[a_part], a_key, Skee::kNoIndex, value);
				}
				_skin = true;
			}

			// dropped only after the stored value was pushed: removing an override leaves the mesh as it is
			void Remove(Part a_part, std::uint16_t a_key, std::uint8_t a_index)
			{
				_removals.emplace_back(a_part, a_key, a_index);
				(a_part == kHead ? _head : _skin) = true;
			}

			void Touch(bool a_head) { (a_head ? _head : _skin) = true; }

			void Commit()
			{
				if (_skin) {
					_skee->SetSkinProperties(_actor, false);
				}
				if (_head && !_state.headNode.empty()) {
					_skee->SetNodeProperties(_actor, false);
				}
				for (const auto& [part, key, index] : _removals) {
					if (part == kHead) {
						_skee->RemoveNodeOverride(_actor, _female, _state.headNode.c_str(), key, index);
						continue;
					}
					for (const bool fp : { false, true }) {
						_skee->RemoveSkinOverride(_actor, _female, fp, kMasks[part], key, index);
					}
				}
			}

		private:
			RE::Actor*                                                  _actor;
			const ActorState&                                           _state;
			bool                                                        _female;
			bool                                                        _firstPerson;
			Skee::IOverrideInterface*                                   _skee;
			bool                                                        _skin{ false };
			bool                                                        _head{ false };
			std::vector<std::tuple<Part, std::uint16_t, std::uint8_t>>  _removals;
		};

		void PushFloat(Writer& a_writer, Part a_part, std::uint16_t a_key, float a_want, float& a_have, std::optional<float>& a_dry, float a_current, bool a_dirty)
		{
			if (a_want > 0.0f) {
				if (a_have <= 0.0f && !a_dry) {
					a_dry = a_current;
				}
				if (a_dirty || std::abs(a_want - a_have) > 0.01f) {
					a_writer.Float(a_part, a_key, a_want);
					a_have = a_want;
				}
			} else if (a_have > 0.0f) {
				if (a_dry) {
					a_writer.Float(a_part, a_key, *a_dry);
				}
				a_writer.Remove(a_part, a_key, Skee::kNoIndex);
				a_have = -1.0f;
				a_dry.reset();
			}
		}
	}

	void Out(const std::string& a_line)
	{
		logger::info("{}", a_line);
		if (auto* console = RE::ConsoleLog::GetSingleton()) {
			console->Print("%s", a_line.c_str());
		}
	}

	bool HasSchlong(RE::Actor* a_actor)
	{
		return a_actor && a_actor->Get3D(false) && !SlotGeometries(a_actor, 0x400000).empty();
	}

	void Apply(RE::Actor* a_actor, const Look& a_look, const Settings& a_settings)
	{
		if (!a_actor || !Skee::Overrides() || !a_actor->Get3D(false)) {
			return;
		}
		std::scoped_lock lock(g_lock);
		const auto       id = a_actor->GetFormID();
		if (!a_look.active && !g_states.contains(id)) {
			return;
		}
		auto&      state = g_states[id];
		const bool female = IsFemale(a_actor);
		if (!state.dirty && state.look && *state.look == a_look) {
			return;
		}
		if (state.dirty || state.mode == Mode::kUnknown) {
			const auto mode = ModeFor(a_actor);
			if (mode != state.mode && state.mode != Mode::kUnknown) {
				logger::debug("{:08X} skin mode {} -> {}", id, ModeName(state.mode), ModeName(mode));
			}
			state.mode = mode;
			state.headNode = FaceNode(a_actor);
		}
		if (state.mode == Mode::kNone) {
			return;
		}
		if (a_look.active && a_settings.bPreloadTextures) {
			PreloadSet(female, state.mode);
		}

		const std::uint8_t     index = state.mode == Mode::kNormal ? 1 : 7;
		std::array<Want, kPartCount> want{};
		for (auto& w : want) {
			w.index = index;
		}

		if (a_look.active && state.mode != Mode::kFloats) {
			const auto bodyTexture = [&](bool a_drops, bool a_sweat, bool a_pussy) -> std::string {
				// the pussy digit is only a variant of the sweat/drop maps; on its own (OSweat) it put a wet
				// body on dry actors while head, hands and feet stayed dry
				if (!a_settings.bTextureBody || !(a_drops || a_sweat)) {
					return {};
				}
				const auto base = female ? std::format("wet_{}{}{}", int(a_drops), int(a_sweat), int(a_pussy)) : std::format("wet_{}{}", int(a_drops), int(a_sweat));
				return WetTexture(female, state.mode, base);
			};
			const bool drops = a_look.drops && a_settings.bTextureBodyDrops;
			const bool sweat = a_look.sweat && a_settings.bTextureBodySweat;
			const bool pussy = a_look.pussy && a_settings.bTextureBodyPussy;
			want[kBody].tex = bodyTexture(drops, sweat, pussy);

			const bool feetDrops = a_look.drops && a_settings.bTextureFeetDrops;
			const bool feetSweat = a_look.sweat && a_settings.bTextureFeetSweat;
			want[kFeet].tex = bodyTexture(feetDrops, feetSweat, pussy && (feetDrops || feetSweat));

			if (a_look.drops || a_look.sweat) {
				if (a_settings.bTextureHands) {
					want[kHands].tex = WetTexture(female, state.mode, "wethand");
				}
				if (a_settings.bTextureHead) {
					want[kHead].tex = WetTexture(female, state.mode, "wethead");
				}
			}
			if (a_look.schlong && (female ? a_settings.bTextureFutaSchlong : a_settings.bTextureSchlong) && HasSchlong(a_actor)) {
				// a futanari schlong is an SOS/TNG mesh, not UBE skin: it keeps the specular map
				const auto mode = female ? Mode::kSpecular : state.mode;
				want[kSchlong].tex = WetTexture(female, mode, "wetschlong");
				want[kSchlong].index = mode == Mode::kNormal ? 1 : 7;
			}
			for (auto& w : want) {
				if (!w.tex.empty() && !FileExists(w.tex)) {
					w.tex.clear();
				}
			}
		}

		if (a_look.active) {
			const auto floats = [&](Part a_part, bool a_spec, bool a_gloss, float a_mult) {
				if (a_spec && a_look.specular > 0.0f) {
					want[a_part].spec = a_look.specular * a_mult;
				}
				if (a_gloss && a_look.glossiness > 0.0f) {
					want[a_part].gloss = a_look.glossiness;
				}
			};
			floats(kBody, a_settings.bSpecularBody, a_settings.bGlossinessBody, 1.0f);
			floats(kSchlong, a_settings.bSpecularBody, a_settings.bGlossinessBody, 1.0f);
			floats(kHands, a_settings.bSpecularHands, a_settings.bGlossinessHands, a_settings.fHandSpecularMult);
			floats(kFeet, a_settings.bSpecularFeet, a_settings.bGlossinessFeet, 1.0f);
			floats(kHead, a_settings.bSpecularHead, a_settings.bGlossinessHead, a_settings.fHeadSpecularMult);
		}

		Writer writer(a_actor, state, a_settings.bFirstPerson);
		for (std::size_t i = 0; i < kPartCount; ++i) {
			const auto part = static_cast<Part>(i);
			auto&      have = state.parts[i];
			const auto& w = want[i];
			if (part == kHead && state.headNode.empty()) {
				continue;
			}
			const auto geometries = part == kHead ? HeadGeometries(a_actor, state.headNode) : SlotGeometries(a_actor, kMasks[i]);
			if (geometries.empty()) {
				continue;
			}
			auto* const front = geometries.front();

			// texture: drop the old one first when it goes away or moves to another texture index
			const bool moved = !have.tex.empty() && !w.tex.empty() && w.index != have.texIndex;
			if (!have.tex.empty() && (w.tex.empty() || moved)) {
				if (have.dryTex) {
					writer.Tex(part, have.texIndex, *have.dryTex);
				}
				writer.Remove(part, Skee::kKeyTexture, have.texIndex);
				have.tex.clear();
				have.dryTex.reset();
			}
			if (!w.tex.empty()) {
				if (const auto current = Tex(front, w.index); !current.empty() && !IsWet(current) && (!have.dryTex || state.dirty)) {
					have.dryTex = current;
				}
				if (w.tex != have.tex || state.dirty) {
					KeepLoaded(w.tex, true);
					writer.Tex(part, w.index, w.tex);
					have.tex = w.tex;
					have.texIndex = w.index;
				}
			}

			auto* material = Material(front);
			PushFloat(writer, part, Skee::kKeySpecular, w.spec, have.spec, have.drySpec, material ? material->specularColorScale : 1.0f, state.dirty);
			PushFloat(writer, part, Skee::kKeyGlossiness, w.gloss, have.gloss, have.dryGloss, material ? material->specularPower : 30.0f, state.dirty);
			if (state.dirty && (!have.tex.empty() || have.spec > 0.0f || have.gloss > 0.0f)) {
				writer.Touch(part == kHead);
			}
		}
		writer.Commit();
		state.look = a_look;
		state.dirty = false;

		if (!a_look.active) {
			g_states.erase(id);
		}
	}

	void Clear(RE::Actor* a_actor, const Settings& a_settings)
	{
		if (!a_actor) {
			return;
		}
		if (a_actor->Get3D(false)) {
			Apply(a_actor, Look{}, a_settings);
		}
		Forget(a_actor->GetFormID());
	}

	void Invalidate(RE::FormID a_actor)
	{
		std::scoped_lock lock(g_lock);
		if (const auto it = g_states.find(a_actor); it != g_states.end()) {
			it->second.dirty = true;
		}
	}

	void Forget(RE::FormID a_actor)
	{
		std::scoped_lock lock(g_lock);
		g_states.erase(a_actor);
	}

	void ForgetAll()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
		g_files.clear();
	}

	std::size_t Clean(RE::Actor* a_actor)
	{
		auto* skee = Skee::Overrides();
		if (!a_actor || !skee) {
			return 0;
		}
		std::size_t removed = 0;
		for (const bool female : { false, true }) {
			for (const auto mask : { 0x04u, 0x08u, 0x80u, 0x400000u }) {
				for (const bool fp : { false, true }) {
					for (const std::uint8_t index : { 0, 1, 7 }) {
						Skee::Reader reader;
						if (skee->GetSkinOverride(a_actor, female, fp, mask, Skee::kKeyTexture, index, reader) && reader.text && IsWet(*reader.text)) {
							skee->RemoveSkinOverride(a_actor, female, fp, mask, Skee::kKeyTexture, index);
							++removed;
						}
					}
					for (const auto key : { Skee::kKeySpecular, Skee::kKeyGlossiness }) {
						if (skee->HasSkinOverride(a_actor, female, fp, mask, key, Skee::kNoIndex)) {
							skee->RemoveSkinOverride(a_actor, female, fp, mask, key, Skee::kNoIndex);
							++removed;
						}
					}
				}
			}
			std::vector<std::string> nodes{ FaceNode(a_actor), "00UBE_FemaleHead", "00UBE_FemaleHeadNPC", "00UBE_MaleHead", "PSQHumanHead", "PSQSuccubusHead" };
			for (const auto& node : nodes) {
				if (node.empty()) {
					continue;
				}
				for (const std::uint8_t index : { 1, 7 }) {
					Skee::Reader reader;
					if (skee->GetNodeOverride(a_actor, female, node.c_str(), Skee::kKeyTexture, index, reader) && reader.text && IsWet(*reader.text)) {
						skee->RemoveNodeOverride(a_actor, female, node.c_str(), Skee::kKeyTexture, index);
						++removed;
					}
				}
				for (const auto key : { Skee::kKeySpecular, Skee::kKeyGlossiness }) {
					if (skee->HasNodeOverride(a_actor, female, node.c_str(), key, Skee::kNoIndex)) {
						skee->RemoveNodeOverride(a_actor, female, node.c_str(), key, Skee::kNoIndex);
						++removed;
					}
				}
			}
		}
		Forget(a_actor->GetFormID());
		if (removed && a_actor->Is3DLoaded()) {
			a_actor->Update3DModel();
		}
		return removed;
	}

	void Dump(RE::Actor* a_actor, std::string_view a_title)
	{
		if (!a_actor) {
			return;
		}
		std::scoped_lock lock(g_lock);
		const auto node = FaceNode(a_actor);
		Out(std::format("[WFNG] {} {} [{:08X}] female {} race {} mode {} schlong {}", a_title, a_actor->GetName(), a_actor->GetFormID(), IsFemale(a_actor),
			RaceEditorID(a_actor), ModeName(ModeFor(a_actor)), HasSchlong(a_actor)));
		const auto dump = [](std::string_view a_label, const std::vector<RE::BSGeometry*>& a_geometries) {
			for (auto* geometry : a_geometries) {
				auto* shader = Shader(geometry);
				auto* material = Material(geometry);
				Out(std::format("  {} [{}] specScale {:.2f} gloss {:.1f} t1 {} | t7 {}", a_label, geometry->name.c_str(), material->specularColorScale, material->specularPower,
					Tex(geometry, 1), Tex(geometry, 7)));
				(void)shader;
			}
		};
		dump("body", SlotGeometries(a_actor, 0x04));
		dump("hands", SlotGeometries(a_actor, 0x08));
		dump("feet", SlotGeometries(a_actor, 0x80));
		dump("schlong", SlotGeometries(a_actor, 0x400000));
		dump("head", HeadGeometries(a_actor, node));
	}
}
