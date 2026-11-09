#include "Config.h"

namespace
{
	std::string Trim(std::string_view a_str)
	{
		const auto first = a_str.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		const auto last = a_str.find_last_not_of(" \t\r\n");
		return std::string(a_str.substr(first, last - first + 1));
	}

	std::string Lower(std::string a_str)
	{
		std::ranges::transform(a_str, a_str.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_str;
	}

	using IniMap = std::unordered_map<std::string, std::string>;  // "section.key" (lowercase) -> value

	IniMap Parse(const std::filesystem::path& a_path)
	{
		IniMap        map;
		std::ifstream file(a_path);
		if (!file) {
			return map;
		}

		std::string section;
		std::string line;
		while (std::getline(file, line)) {
			auto text = Trim(line);
			if (text.empty() || text[0] == ';' || text[0] == '#') {
				continue;
			}
			if (text.front() == '[' && text.back() == ']') {
				section = Lower(Trim(std::string_view(text).substr(1, text.size() - 2)));
				continue;
			}
			const auto eq = text.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto key = Lower(Trim(std::string_view(text).substr(0, eq)));
			auto value = Trim(std::string_view(text).substr(eq + 1));
			if (const auto comment = value.find(';'); comment != std::string::npos) {
				value = Trim(std::string_view(value).substr(0, comment));
			}
			map[section + "." + key] = value;
		}
		return map;
	}

	class Reader
	{
	public:
		explicit Reader(IniMap a_map) : _map(std::move(a_map)) {}

		void Bool(std::string_view a_key, bool& a_out) const
		{
			if (auto v = Find(a_key)) {
				const auto s = Lower(*v);
				a_out = (s == "1" || s == "true" || s == "on" || s == "yes");
			}
		}

		void Int(std::string_view a_key, int& a_out) const
		{
			if (auto v = Find(a_key)) {
				try {
					a_out = std::stoi(*v);
				} catch (...) {
					logger::warn("Invalid integer for {}: '{}'", a_key, *v);
				}
			}
		}

		void Float(std::string_view a_key, float& a_out) const
		{
			if (auto v = Find(a_key)) {
				try {
					a_out = std::stof(*v);
				} catch (...) {
					logger::warn("Invalid float for {}: '{}'", a_key, *v);
				}
			}
		}

		void Hex(std::string_view a_key, Color& a_out) const
		{
			if (auto v = Find(a_key)) {
				auto s = *v;
				if (!s.empty() && s[0] == '#') {
					s.erase(0, 1);
				} else if (s.size() > 2 && (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)) {
					s.erase(0, 2);
				}
				try {
					a_out = Color::FromHex(static_cast<std::uint32_t>(std::stoul(s, nullptr, 16)));
				} catch (...) {
					logger::warn("Invalid color for {}: '{}'", a_key, *v);
				}
			}
		}

	private:
		[[nodiscard]] std::optional<std::string> Find(std::string_view a_key) const
		{
			if (auto it = _map.find(Lower(std::string(a_key))); it != _map.end()) {
				return it->second;
			}
			return std::nullopt;
		}

		IniMap _map;
	};
}

Config& Config::Get()
{
	static Config instance;
	return instance;
}

void Config::Load(const std::filesystem::path& a_path)
{
	auto map = Parse(a_path);
	if (map.empty()) {
		logger::warn("Config {} not found or empty; using defaults.", a_path.string());
		return;
	}

	const Reader ini(std::move(map));

	ini.Bool("General.bEnabled", enabled);
	ini.Int("General.iFPS", fps);
	ini.Int("General.iSampleIntervalMs", sampleIntervalMs);
	ini.Float("General.fBrightness", brightness);
	ini.Float("General.fAmbientBrightness", ambientBrightness);

	ini.Bool("Devices.bKeyboard", devices.keyboard);
	ini.Bool("Devices.bMouse", devices.mouse);
	ini.Bool("Devices.bMousepad", devices.mousepad);
	ini.Bool("Devices.bHeadset", devices.headset);
	ini.Bool("Devices.bKeypad", devices.keypad);
	ini.Bool("Devices.bChromaLink", devices.chromaLink);

	ini.Float("Thresholds.fLowHealth", lowHealth);
	ini.Float("Thresholds.fLowMagicka", lowMagicka);
	ini.Float("Thresholds.fLowStamina", lowStamina);

	ini.Bool("Effects.bAmbient", ambient);
	ini.Bool("Effects.bAttributeBars", attributeBars);
	ini.Bool("Effects.bLowHealthPulse", lowHealthPulse);
	ini.Bool("Effects.bDamageFlash", damageFlash);
	ini.Bool("Effects.bCombat", combat);
	ini.Bool("Effects.bSurvival", survival);
	ini.Bool("Effects.bCold", cold);
	ini.Bool("Effects.bWeather", weather);
	ini.Bool("Effects.bLightning", lightning);
	ini.Bool("Effects.bShout", shout);
	ini.Bool("Effects.bSneak", sneak);
	ini.Bool("Effects.bWater", water);
	ini.Bool("Effects.bTransformations", transformations);
	ini.Bool("Effects.bLevelUp", levelUp);
	ini.Bool("Effects.bDragonSoul", dragonSoul);
	ini.Bool("Effects.bMenus", menus);
	ini.Bool("Effects.bDeath", death);

	ini.Hex("Colors.sHealth", health);
	ini.Hex("Colors.sMagicka", magicka);
	ini.Hex("Colors.sStamina", stamina);
	ini.Hex("Colors.sFrost", frost);
	ini.Hex("Colors.sCombat", combatColor);

	fps = std::clamp(fps, 5, 60);
	sampleIntervalMs = std::clamp(sampleIntervalMs, 16, 1000);
	brightness = std::clamp(brightness, 0.0f, 1.0f);
	ambientBrightness = std::clamp(ambientBrightness, 0.0f, 1.0f);

	logger::info("Config loaded from {} (fps={}, sample={}ms, brightness={:.2f})", a_path.string(), fps, sampleIntervalMs, brightness);
}
