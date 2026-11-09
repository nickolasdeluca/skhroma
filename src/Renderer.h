#pragma once

#include "Chroma.h"
#include "Color.h"
#include "Config.h"
#include "GameState.h"

// Owns the lighting thread: samples game state, animates effects and pushes frames to Chroma.
//
// Composition pipeline (per LED, using its normalized position on the device):
//   Scene  : ambient (time of day / weather / interior torchlight) + state tints
//            (combat, sneak, swimming, transformations) + SunHelm frost + menu dimming
//   Widgets: device-specific HUD elements (attribute bars, survival meters, shout key...)
//   Post   : low-health heartbeat, damage/block flashes, lightning, level up, dragon soul, death
class Renderer
{
public:
	static void Start();

private:
	Renderer() = default;

	void Loop();
	void Update(float a_dt);
	void Compose(Chroma::Frame& a_frame);

	[[nodiscard]] Color Ambient(float a_x, float a_y) const;
	[[nodiscard]] Color Scene(float a_x, float a_y) const;
	[[nodiscard]] Color Post(float a_x, float a_y, Color a_c) const;

	void ComposeKeyboard(Chroma::Keyboard::CUSTOM_EFFECT_TYPE& a_kb);
	void ComposeMouse(Chroma::Mouse::CUSTOM_EFFECT_TYPE2& a_mouse);
	void ComposeMousepad(Chroma::Mousepad::CUSTOM_EFFECT_TYPE& a_pad);
	void ComposeHeadset(Chroma::Headset::CUSTOM_EFFECT_TYPE& a_headset);
	void ComposeKeypad(Chroma::Keypad::CUSTOM_EFFECT_TYPE& a_keypad);
	void ComposeChromaLink(Chroma::ChromaLink::CUSTOM_EFFECT_TYPE& a_link);

	// Helpers
	[[nodiscard]] Color BarCell(const Color& a_color, float a_value, int a_index, int a_count, bool a_low) const;
	[[nodiscard]] Color SurvivalCell(const Color& a_color, int a_stage, int a_maxStage, int a_cellFromBottom, int a_cells) const;
	[[nodiscard]] float Pulse(float a_hz) const;

	const Config& _cfg{ Config::Get() };
	GameSnapshot  _s{};
	GameSnapshot  _prev{};
	std::mt19937  _rng{ std::random_device{}() };

	// Animation state (seconds)
	float _t{ 0.0f };
	float _damageFlash{ 0.0f };
	float _blockFlash{ 0.0f };
	float _lightning{ 0.0f };
	float _lightningSecond{ -1.0f };  // time of the follow-up strike
	float _levelUpT{ -1.0f };
	float _soulT{ -1.0f };
	float _shoutReadyFlash{ 0.0f };
	float _shoutCooldownMax{ 0.0f };
	float _deathT{ 0.0f };

	std::uint32_t _seenHits{ 0 };
	std::uint32_t _seenBlocks{ 0 };
	std::uint32_t _seenLevelUps{ 0 };
};
