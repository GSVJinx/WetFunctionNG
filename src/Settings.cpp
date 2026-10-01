#include "Settings.h"

namespace WFNG
{
	namespace
	{
		constexpr auto kSettings = "Data/SKSE/Plugins/WetFunctionNG.ini"sv;

		std::string Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r\n");
			return std::string(a_text.substr(first, last - first + 1));
		}

		bool IEquals(std::string_view a_lhs, std::string_view a_rhs)
		{
			return a_lhs.size() == a_rhs.size() && std::equal(a_lhs.begin(), a_lhs.end(), a_rhs.begin(), [](char a, char b) {
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			});
		}

		void Parse(const std::string& a_value, float& a_out)
		{
			try {
				a_out = std::stof(a_value);
			} catch (...) {
			}
		}

		void Parse(const std::string& a_value, bool& a_out)
		{
			a_out = a_value == "1" || IEquals(a_value, "true");
		}

		void Parse(const std::string& a_value, std::int32_t& a_out)
		{
			try {
				a_out = std::stoi(a_value);
			} catch (...) {
			}
		}

		std::string Value(float a_value) { return std::format("{:.6g}", a_value); }
		std::string Value(bool a_value) { return a_value ? "1" : "0"; }
		std::string Value(std::int32_t a_value) { return std::to_string(a_value); }

		std::size_t ReadIni(std::string_view a_path, Settings& a_settings)
		{
			std::ifstream in{ std::filesystem::path(a_path) };
			if (!in) {
				return 0;
			}
			std::size_t applied = 0;
			std::string section;
			std::string line;
			while (std::getline(in, line)) {
				if (const auto comment = line.find_first_of(";#"); comment != std::string::npos) {
					line.erase(comment);
				}
				line = Trim(line);
				if (line.empty()) {
					continue;
				}
				if (line.front() == '[' && line.back() == ']') {
					section = Trim(std::string_view(line).substr(1, line.size() - 2));
					continue;
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				if (a_settings.Set(section, Trim(std::string_view(line).substr(0, eq)), Trim(std::string_view(line).substr(eq + 1)))) {
					++applied;
				}
			}
			return applied;
		}
	}

	Settings& Settings::Get()
	{
		static Settings singleton;
		return singleton;
	}

	bool Settings::Set(std::string_view a_section, std::string_view a_key, const std::string& a_value)
	{
#define WFNG_SET(a_sec, a_name, a_default)                                  \
	if (IEquals(a_section, #a_sec) && IEquals(a_key, #a_name)) {            \
		Parse(a_value, a_name);                                             \
		return true;                                                        \
	}
		WFNG_FLOAT_SETTINGS(WFNG_SET)
		WFNG_BOOL_SETTINGS(WFNG_SET)
		WFNG_INT_SETTINGS(WFNG_SET)
#undef WFNG_SET
		return false;
	}

	void Settings::Load()
	{
		Settings fresh;
		const auto values = ReadIni(kSettings, fresh);
		Get() = fresh;
		spdlog::set_level(fresh.iLogLevel > 0 ? spdlog::level::debug : spdlog::level::info);
		logger::info("Settings loaded: {} values", values);
	}

	bool Settings::Save()
	{
		const auto path = std::filesystem::path(kSettings);
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream out(path, std::ios::trunc);
		if (!out) {
			logger::error("Could not save settings to {}", path.string());
			return false;
		}

		std::map<std::string, std::vector<std::pair<std::string, std::string>>> sections;
		const auto& settings = Get();
#define WFNG_COLLECT(a_section, a_name, a_default) sections[#a_section].emplace_back(#a_name, Value(settings.a_name));
		WFNG_FLOAT_SETTINGS(WFNG_COLLECT)
		WFNG_BOOL_SETTINGS(WFNG_COLLECT)
		WFNG_INT_SETTINGS(WFNG_COLLECT)
#undef WFNG_COLLECT

		out << "; WetFunction NG - changed in SKSE Menu Framework\n";
		for (const auto& [section, entries] : sections) {
			out << '\n' << '[' << section << "]\n";
			for (const auto& [key, value] : entries) {
				out << key << " = " << value << '\n';
			}
		}
		out.flush();
		if (!out) {
			logger::error("Could not finish writing settings to {}", path.string());
			return false;
		}
		spdlog::set_level(settings.iLogLevel > 0 ? spdlog::level::debug : spdlog::level::info);
		return true;
	}

	bool Settings::Reset()
	{
		Get() = Settings{};
		return Save();
	}
}
