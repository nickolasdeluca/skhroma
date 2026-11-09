#pragma once

// Simple linear RGB color in [0,1] used by all effect composition.
struct Color
{
	float r{ 0.0f };
	float g{ 0.0f };
	float b{ 0.0f };

	constexpr Color() = default;
	constexpr Color(float a_r, float a_g, float a_b) : r(a_r), g(a_g), b(a_b) {}

	static constexpr Color FromHex(std::uint32_t a_rgb)
	{
		return { ((a_rgb >> 16) & 0xFF) / 255.0f, ((a_rgb >> 8) & 0xFF) / 255.0f, (a_rgb & 0xFF) / 255.0f };
	}

	constexpr Color operator*(float a_s) const { return { r * a_s, g * a_s, b * a_s }; }
	constexpr Color operator+(const Color& o) const { return { r + o.r, g + o.g, b + o.b }; }

	[[nodiscard]] Color Clamped() const
	{
		return { std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f) };
	}

	// Razer COLORREF layout: 0x00BBGGRR
	[[nodiscard]] std::uint32_t ToColorRef(float a_brightness = 1.0f) const
	{
		const auto c = (*this * a_brightness).Clamped();
		const auto R = static_cast<std::uint32_t>(c.r * 255.0f + 0.5f);
		const auto G = static_cast<std::uint32_t>(c.g * 255.0f + 0.5f);
		const auto B = static_cast<std::uint32_t>(c.b * 255.0f + 0.5f);
		return R | (G << 8) | (B << 16);
	}
};

inline Color Lerp(const Color& a, const Color& b, float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

// Screen blend: brightens, good for flashes/glows on top of a base layer.
inline Color Screen(const Color& a, const Color& b, float t)
{
	const Color s{ 1.0f - (1.0f - a.r) * (1.0f - b.r), 1.0f - (1.0f - a.g) * (1.0f - b.g), 1.0f - (1.0f - a.b) * (1.0f - b.b) };
	return Lerp(a, s, t);
}

namespace Palette
{
	inline constexpr Color Black{ 0.0f, 0.0f, 0.0f };
	inline constexpr Color White{ 1.0f, 1.0f, 1.0f };
	inline constexpr Color Health = Color::FromHex(0xD01010);
	inline constexpr Color Magicka = Color::FromHex(0x1050FF);
	inline constexpr Color Stamina = Color::FromHex(0x20D040);
	inline constexpr Color Frost = Color::FromHex(0x9FE8FF);
	inline constexpr Color Gold = Color::FromHex(0xFFB020);
	inline constexpr Color Ember = Color::FromHex(0xFF5A10);
	inline constexpr Color Torch = Color::FromHex(0xFF8C30);
	inline constexpr Color Night = Color::FromHex(0x0A1440);
	inline constexpr Color Dawn = Color::FromHex(0xFF7040);
	inline constexpr Color Day = Color::FromHex(0x80C0FF);
	inline constexpr Color Rain = Color::FromHex(0x3050A0);
	inline constexpr Color Snow = Color::FromHex(0xC8E0FF);
	inline constexpr Color Cloud = Color::FromHex(0x606878);
	inline constexpr Color Water = Color::FromHex(0x0070C0);
	inline constexpr Color Sneak = Color::FromHex(0x40108A);
	inline constexpr Color Vampire = Color::FromHex(0x8A0020);
	inline constexpr Color Werewolf = Color::FromHex(0xB06010);
	inline constexpr Color Hunger = Color::FromHex(0xFF9020);
	inline constexpr Color Thirst = Color::FromHex(0x20A0FF);
	inline constexpr Color Fatigue = Color::FromHex(0xA060FF);
	inline constexpr Color Shout = Color::FromHex(0xFFD060);
}
