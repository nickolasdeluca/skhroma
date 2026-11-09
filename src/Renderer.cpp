#include "Renderer.h"

namespace
{
	constexpr float PI = 3.14159265f;

	float Hash(float a_x, float a_y)
	{
		auto h = static_cast<std::uint32_t>(a_x * 1009.0f) * 73856093u ^ static_cast<std::uint32_t>(a_y * 917.0f) * 19349663u;
		h ^= h >> 13;
		h *= 0x5bd1e995u;
		h ^= h >> 15;
		return static_cast<float>(h & 0xFFFFFF) / 16777216.0f;
	}

	float Fract(float a_v) { return a_v - std::floor(a_v); }

	float Smooth(float a_e0, float a_e1, float a_x)
	{
		const float t = std::clamp((a_x - a_e0) / (a_e1 - a_e0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// Two-beat "lub-dub" heartbeat waveform in [0,1].
	float Heartbeat(float a_t, float a_period)
	{
		const float p = Fract(a_t / a_period);
		return std::exp(-std::pow((p - 0.08f) / 0.05f, 2.0f)) + 0.6f * std::exp(-std::pow((p - 0.28f) / 0.06f, 2.0f));
	}

	Color TimeOfDay(float a_hour)
	{
		using namespace Palette;
		if (a_hour < 5.0f || a_hour >= 21.0f) return Night;
		if (a_hour < 6.5f) return Lerp(Night, Dawn, (a_hour - 5.0f) / 1.5f);
		if (a_hour < 8.0f) return Lerp(Dawn, Day, (a_hour - 6.5f) / 1.5f);
		if (a_hour < 17.5f) return Day;
		if (a_hour < 19.0f) return Lerp(Day, Dawn, (a_hour - 17.5f) / 1.5f);
		return Lerp(Dawn, Night, (a_hour - 19.0f) / 2.0f);
	}

	template <std::size_t R, std::size_t C>
	void Set(Chroma::COLORREF (&a_grid)[R][C], int a_row, int a_col, const Color& a_c)
	{
		if (a_row >= 0 && a_row < static_cast<int>(R) && a_col >= 0 && a_col < static_cast<int>(C)) {
			a_grid[a_row][a_col] = a_c.ToColorRef();
		}
	}
}

void Renderer::Start()
{
	static Renderer* instance = new Renderer();  // intentionally leaked: no teardown under loader lock
	std::thread([] { instance->Loop(); }).detach();
}

float Renderer::Pulse(float a_hz) const
{
	return 0.5f + 0.5f * std::sin(_t * a_hz * 2.0f * PI);
}

void Renderer::Loop()
{
	using clock = std::chrono::steady_clock;

	logger::info("Lighting thread started.");
	auto& sdk = Chroma::SDK::Get();

	auto lastFrame = clock::now();
	auto lastSample = clock::time_point{};
	auto nextInitTry = clock::time_point{};

	Chroma::Frame frame{};

	while (true) {
		const auto now = clock::now();

		if (!sdk.IsReady()) {
			if (now >= nextInitTry) {
				if (!sdk.Initialize()) {
					nextInitTry = now + 5s;
				}
			}
			std::this_thread::sleep_for(250ms);
			lastFrame = clock::now();
			continue;
		}

		if (now - lastSample >= std::chrono::milliseconds(_cfg.sampleIntervalMs)) {
			GameState::RequestSample();
			lastSample = now;
		}

		const float dt = std::min(std::chrono::duration<float>(now - lastFrame).count(), 0.25f);
		lastFrame = now;

		_prev = _s;
		_s = GameState::GetSnapshot();
		Update(dt);

		if (_cfg.enabled) {
			Compose(frame);
			sdk.Submit(frame, _cfg.devices);
		}

		std::this_thread::sleep_until(now + std::chrono::microseconds(1'000'000 / std::max(_cfg.fps, 1)));
	}
}

void Renderer::Update(float a_dt)
{
	_t += a_dt;

	auto decay = [a_dt](float& a_v, float a_rate) { a_v = std::max(0.0f, a_v - a_rate * a_dt); };
	decay(_damageFlash, 2.8f);
	decay(_blockFlash, 4.0f);
	decay(_lightning, 5.0f);
	decay(_shoutReadyFlash, 1.5f);

	auto& events = GameState::Events();
	const auto hits = events.hitsTaken.load(std::memory_order_relaxed);
	const auto blocks = events.hitsBlocked.load(std::memory_order_relaxed);
	const auto levels = events.levelUps.load(std::memory_order_relaxed);

	if (!_s.valid) {
		// Not in game: just resync counters and reset transient effects.
		_seenHits = hits;
		_seenBlocks = blocks;
		_seenLevelUps = levels;
		_levelUpT = _soulT = -1.0f;
		_deathT = 0.0f;
		return;
	}

	// --- Damage -------------------------------------------------------------
	if (_cfg.damageFlash) {
		if (hits != _seenHits) {
			_damageFlash = std::max(_damageFlash, 0.55f);
		}
		if (blocks != _seenBlocks) {
			_blockFlash = 1.0f;
		}
		if (_prev.valid && _prev.healthMax > 0.0f) {
			const float drop = _prev.HealthPct() - _s.HealthPct();
			if (drop > 0.02f) {
				_damageFlash = std::max(_damageFlash, std::clamp(0.3f + drop * 3.0f, 0.3f, 1.0f));
			}
		}
	}
	_seenHits = hits;
	_seenBlocks = blocks;

	// --- Level up / dragon soul ----------------------------------------------
	if (levels != _seenLevelUps && _cfg.levelUp) {
		_levelUpT = 0.0f;
	}
	_seenLevelUps = levels;
	if (_levelUpT >= 0.0f && (_levelUpT += a_dt) > 3.0f) {
		_levelUpT = -1.0f;
	}

	if (_cfg.dragonSoul && _prev.valid && _s.dragonSouls > _prev.dragonSouls + 0.5f) {
		_soulT = 0.0f;
	}
	if (_soulT >= 0.0f && (_soulT += a_dt) > 4.0f) {
		_soulT = -1.0f;
	}

	// --- Shout cooldown --------------------------------------------------------
	if (_s.voiceRecovery > _prev.voiceRecovery + 0.5f) {
		_shoutCooldownMax = _s.voiceRecovery;  // a new shout just started its cooldown
	}
	if (_prev.voiceRecovery > 0.0f && _s.voiceRecovery <= 0.0f && _cfg.shout) {
		_shoutReadyFlash = 1.0f;
	}

	// --- Lightning -------------------------------------------------------------
	if (_cfg.lightning && _s.raining && !_s.interior && _s.lightningFrequency > 0 && !_s.menuOpen) {
		// Frequency is 0..255 in the weather record; map to ~0.02..0.25 strikes per second.
		const float perSecond = 0.02f + 0.23f * (_s.lightningFrequency / 255.0f);
		std::uniform_real_distribution<float> roll(0.0f, 1.0f);
		if (roll(_rng) < perSecond * a_dt) {
			_lightning = 1.0f;
			_lightningSecond = _t + 0.12f + roll(_rng) * 0.15f;
		}
	}
	if (_lightningSecond > 0.0f && _t >= _lightningSecond) {
		_lightning = std::max(_lightning, 0.8f);
		_lightningSecond = -1.0f;
	}

	// --- Death -------------------------------------------------------------
	_deathT = (_s.dead && _cfg.death) ? _deathT + a_dt : 0.0f;
}

// -----------------------------------------------------------------------------
// Scene
// -----------------------------------------------------------------------------
Color Renderer::Ambient(float a_x, float a_y) const
{
	if (!_cfg.ambient) {
		return Palette::Black;
	}

	if (_s.interior) {
		// Warm torchlight with a per-LED flicker.
		const float h = Hash(a_x, a_y);
		const float flicker = 0.75f + 0.15f * std::sin(_t * 7.3f + h * 20.0f) + 0.10f * std::sin(_t * 13.1f + h * 7.0f);
		return Palette::Torch * flicker;
	}

	Color c = TimeOfDay(_s.hour);

	if (_cfg.weather) {
		if (_s.cloudy) {
			c = Lerp(c, Palette::Cloud, 0.45f);
		}
		if (_s.snowing) {
			c = Lerp(c, Palette::Snow, 0.5f);
			// Flakes drifting down: each column has its own flake.
			const float flakeY = Fract(_t * 0.18f + Hash(a_x, 0.37f));
			const float d = std::abs(flakeY - a_y);
			c = Screen(c, Palette::White, std::exp(-d * d / 0.004f) * 0.8f);
		} else if (_s.raining) {
			c = Lerp(c, Palette::Rain, 0.6f);
			// Raindrops: short random sparkles per LED.
			const float phase = Fract(_t * 0.9f + Hash(a_x, a_y));
			if (phase < 0.06f) {
				c = Screen(c, Palette::Snow, (0.06f - phase) / 0.06f * 0.7f);
			}
		}
	}

	return c;
}

Color Renderer::Scene(float a_x, float a_y) const
{
	using namespace Palette;

	// --- Outside of gameplay ---------------------------------------------------
	if (!_s.valid) {
		if (_s.loading || _s.mainMenu) {
			// Embers slowly breathing — a calm "Skyrim" idle.
			const float b = 0.25f + 0.2f * Pulse(0.15f) + 0.1f * std::sin(_t * 1.7f + Hash(a_x, a_y) * 12.0f);
			return Lerp(Ember, Gold, a_y) * b;
		}
		return Black;
	}

	if (_cfg.menus) {
		if (_s.lockpicking) {
			return Gold * (0.15f + 0.1f * Pulse(0.5f));
		}
		if (_s.sleepWait) {
			return Night * (0.6f + 0.4f * Pulse(0.1f));
		}
		if (_s.mapMenu) {
			return Color::FromHex(0xC8A060) * 0.3f;
		}
	}

	Color c = Ambient(a_x, a_y) * _cfg.ambientBrightness;

	// --- State tints -----------------------------------------------------------
	if (_cfg.transformations) {
		if (_s.werewolf) {
			c = Lerp(c, Werewolf * (0.5f + 0.5f * Heartbeat(_t, 0.9f)), 0.85f);
		} else if (_s.vampireLord) {
			const float swirl = 0.5f + 0.5f * std::sin(_t * 1.5f + (a_x + a_y) * 6.0f);
			c = Lerp(c, Vampire * (0.35f + 0.5f * swirl), 0.85f);
		} else if (_s.vampire) {
			c = Lerp(c, Vampire * 0.6f, 0.2f);
		}
	}

	if (_cfg.water && _s.swimming) {
		const float wave = 0.5f + 0.5f * std::sin(_t * 2.0f + a_x * 8.0f + std::sin(_t + a_y * 4.0f));
		c = Lerp(c, Water * (0.4f + 0.5f * wave), 0.8f);
	}

	if (_cfg.combat && _s.inCombat) {
		const float edge = 1.0f - std::min({ a_x, 1.0f - a_x, a_y, 1.0f - a_y }) * 2.0f;  // 1 at edges
		const float flame = 0.6f + 0.4f * std::sin(_t * 5.0f + Hash(a_x, a_y) * 10.0f);
		c = Lerp(c, _cfg.combatColor * flame, 0.35f + 0.35f * edge);
	}

	if (_cfg.sneak && _s.sneaking && !_s.inCombat) {
		c = Lerp(c * 0.4f, Sneak * 0.5f, 0.5f);
	}

	// --- SunHelm frost creeping in from the edges ---------------------------------
	if (_cfg.cold && _s.sunhelm) {
		float intensity = _s.coldLevel;
		if (_s.coldStage >= 0) {
			intensity = std::max(intensity, _s.coldStage / 5.0f);
		}
		if (intensity > 0.05f) {
			const float centerDist = std::min({ a_x, 1.0f - a_x, a_y, 1.0f - a_y }) * 2.0f;  // 0 edge, 1 center
			const float amount = Smooth(centerDist - 0.15f, centerDist + 0.15f, intensity * 1.15f);
			const float sparkle = 0.75f + 0.25f * std::sin(_t * 3.0f + Hash(a_x, a_y) * 30.0f);
			c = Lerp(c, _cfg.frost * (0.35f + 0.45f * sparkle), amount * 0.9f);
		}
	}

	if (_cfg.menus && (_s.menuOpen || _s.dialogue)) {
		c = c * (_s.dialogue ? 0.7f : 0.4f);
	}

	return c;
}

Color Renderer::Post(float a_x, float a_y, Color a_c) const
{
	using namespace Palette;
	if (!_s.valid) {
		return a_c;
	}

	// Low health heartbeat: faster and stronger as health drops.
	const float hp = _s.HealthPct();
	if (_cfg.lowHealthPulse && hp < _cfg.lowHealth && !_s.dead) {
		const float severity = 1.0f - hp / std::max(_cfg.lowHealth, 0.01f);
		const float beat = Heartbeat(_t, 1.2f - 0.6f * severity);
		a_c = Lerp(a_c, _cfg.health, std::clamp(beat, 0.0f, 1.0f) * (0.35f + 0.55f * severity));
	}

	if (_s.bleedingOut && !_s.dead) {
		a_c = Lerp(a_c, _cfg.health * (0.2f + 0.5f * Heartbeat(_t, 1.6f)), 0.8f);
	}

	if (_damageFlash > 0.0f) {
		a_c = Lerp(a_c, _cfg.health, std::min(_damageFlash, 1.0f));
	}
	if (_blockFlash > 0.0f) {
		a_c = Screen(a_c, Color(0.75f, 0.85f, 1.0f), _blockFlash * 0.7f);
	}

	if (_lightning > 0.0f) {
		a_c = Screen(a_c, Lerp(White, _s.lightningColor, 0.4f), _lightning);
	}

	if (_levelUpT >= 0.0f) {
		// A golden wave sweeping left to right, then a fading glow.
		const float front = _levelUpT / 1.4f;
		const float band = std::exp(-std::pow((a_x - front) / 0.12f, 2.0f));
		const float glow = std::max(0.0f, 1.0f - _levelUpT / 3.0f) * 0.35f;
		a_c = Screen(a_c, Gold, std::min(1.0f, band + glow));
	}

	if (_soulT >= 0.0f) {
		// Dragon soul: a ring of fire collapsing into the center, then a white burst.
		const float r = std::hypot(a_x - 0.5f, (a_y - 0.5f) * 0.6f) * 1.6f;
		const float front = 1.0f - _soulT / 2.8f;
		const float ring = std::exp(-std::pow((r - front) / 0.12f, 2.0f));
		const float burst = _soulT > 2.6f ? std::max(0.0f, 1.0f - (_soulT - 2.6f) / 1.2f) : 0.0f;
		a_c = Screen(a_c, Lerp(Ember, White, 1.0f - front), std::min(1.0f, ring + burst));
	}

	if (_deathT > 0.0f) {
		const float toRed = std::min(1.0f, _deathT * 2.0f);
		const float fade = std::clamp(1.0f - (_deathT - 0.8f) / 2.5f, 0.06f, 1.0f);
		a_c = Lerp(a_c, _cfg.health * 0.7f, toRed) * fade;
	}

	return a_c;
}

// -----------------------------------------------------------------------------
// Widgets
// -----------------------------------------------------------------------------
Color Renderer::BarCell(const Color& a_color, float a_value, int a_index, int a_count, bool a_low) const
{
	const float fill = std::clamp(a_value * a_count - a_index, 0.0f, 1.0f);
	Color c = Lerp(a_color * 0.05f, a_color, fill);
	if (a_low) {
		c = c * (0.35f + 0.65f * Pulse(2.0f));
	}
	return c;
}

Color Renderer::SurvivalCell(const Color& a_color, int a_stage, int a_maxStage, int a_cellFromBottom, int a_cells) const
{
	const float norm = std::clamp(static_cast<float>(a_stage) / static_cast<float>(a_maxStage), 0.0f, 1.0f);
	const int   lit = static_cast<int>(std::round(norm * a_cells));
	if (a_cellFromBottom >= lit) {
		return a_color * 0.05f;
	}
	// Shift towards red near the top of the meter.
	Color c = Lerp(a_color, Palette::Health, static_cast<float>(a_cellFromBottom) / std::max(a_cells - 1, 1));
	if (a_stage >= a_maxStage) {
		c = c * (0.3f + 0.7f * Pulse(1.0f));
	}
	return c;
}

void Renderer::ComposeKeyboard(Chroma::Keyboard::CUSTOM_EFFECT_TYPE& a_kb)
{
	using namespace Chroma::Keyboard;
	constexpr int NR = ROWS, NC = COLS;

	Color grid[NR][NC];
	for (int r = 0; r < NR; ++r) {
		for (int c = 0; c < NC; ++c) {
			grid[r][c] = Scene(c / float(NC - 1), r / float(NR - 1));
		}
	}

	auto at = [&grid](std::uint16_t a_rz) -> Color& {
		const auto k = FromRz(a_rz);
		return grid[k.row][k.col];
	};

	if (_s.valid) {
		// F1-F4 Magicka | F5-F8 Health | F9-F12 Stamina (same order as the HUD).
		if (_cfg.attributeBars) {
			const float mp = _s.MagickaPct(), hp = _s.HealthPct(), sp = _s.StaminaPct();
			for (int i = 0; i < 4; ++i) {
				at(static_cast<std::uint16_t>(F1 + i)) = BarCell(_cfg.magicka, mp, i, 4, mp < _cfg.lowMagicka);
				at(static_cast<std::uint16_t>(F1 + 4 + i)) = BarCell(_cfg.health, hp, i, 4, hp < _cfg.lowHealth);
				at(static_cast<std::uint16_t>(F1 + 8 + i)) = BarCell(_cfg.stamina, sp, i, 4, sp < _cfg.lowStamina);
			}
		}

		// Movement keys.
		const Color wasd = (_cfg.combat && _s.inCombat)       ? _cfg.combatColor :
		                   (_s.StaminaPct() < _cfg.lowStamina) ? _cfg.stamina * (0.3f + 0.7f * Pulse(2.0f)) :
		                                                         Lerp(at(W), Palette::White, 0.3f);
		at(W) = at(A) = at(S) = at(D) = wasd;

		// Shout key (Z): dim while recovering, bright gold when ready.
		if (_cfg.shout) {
			Color z = Palette::Shout;
			if (_s.voiceRecovery > 0.0f) {
				const float progress = _shoutCooldownMax > 0.0f ? 1.0f - _s.voiceRecovery / _shoutCooldownMax : 0.0f;
				z = Lerp(Palette::Health * 0.15f, Palette::Shout * 0.5f, progress);
			}
			at(Z) = Screen(z, Palette::White, _shoutReadyFlash);
		}

		// SunHelm meters on the numpad: columns = hunger, thirst, fatigue, cold; filled bottom-up.
		if (_cfg.survival && _s.sunhelm) {
			const struct
			{
				int   stage;
				int   max;
				Color color;
			} meters[4] = {
				{ _s.hungerStage, 4, Palette::Hunger },
				{ _s.thirstStage, 5, Palette::Thirst },
				{ _s.fatigueStage, 5, Palette::Fatigue },
				{ _s.coldStage, 5, _cfg.frost },
			};
			for (int m = 0; m < 4; ++m) {
				if (meters[m].stage < 0) {
					continue;
				}
				for (int row = 1; row <= 5; ++row) {
					grid[row][18 + m] = SurvivalCell(meters[m].color, meters[m].stage, meters[m].max, 5 - row, 5);
				}
			}
		}

		// ESC = general alarm.
		const bool alarm = _s.HealthPct() < _cfg.lowHealth ||
		                   (_s.sunhelm && (_s.hungerStage >= 4 || _s.thirstStage >= 5 || _s.fatigueStage >= 5 || _s.coldStage >= 5));
		if (alarm) {
			at(ESC) = Palette::Health * (Fract(_t * 2.0f) < 0.5f ? 1.0f : 0.1f);
		}
	}

	for (int r = 0; r < NR; ++r) {
		for (int c = 0; c < NC; ++c) {
			a_kb.Color[r][c] = Post(c / float(NC - 1), r / float(NR - 1), grid[r][c]).ToColorRef(_cfg.brightness);
		}
	}
}

void Renderer::ComposeMouse(Chroma::Mouse::CUSTOM_EFFECT_TYPE2& a_mouse)
{
	using namespace Chroma::Mouse;
	for (int r = 0; r < ROWS; ++r) {
		for (int c = 0; c < COLS; ++c) {
			const float x = c / float(COLS - 1), y = r / float(ROWS - 1);
			Color col = Scene(x, y);

			// Side strips (rows 1..7): left = health, right = stamina, filled bottom-up.
			if (_s.valid && _cfg.attributeBars && r >= 1 && r <= 7 && (c == 0 || c == COLS - 1)) {
				const int idx = 7 - r;  // 0 = bottom
				col = c == 0 ? BarCell(_cfg.health, _s.HealthPct(), idx, 7, _s.HealthPct() < _cfg.lowHealth) :
				               BarCell(_cfg.stamina, _s.StaminaPct(), idx, 7, _s.StaminaPct() < _cfg.lowStamina);
			}
			a_mouse.Color[r][c] = Post(x, y, col).ToColorRef(_cfg.brightness);
		}
	}
}

void Renderer::ComposeMousepad(Chroma::Mousepad::CUSTOM_EFFECT_TYPE& a_pad)
{
	using namespace Chroma::Mousepad;
	// Approximate LED ring: 0-4 right edge (top->bottom), 5-9 bottom (right->left), 10-14 left (bottom->top).
	for (int i = 0; i < LEDS; ++i) {
		float x, y;
		if (i < 5) {
			x = 1.0f, y = i / 4.0f;
		} else if (i < 10) {
			x = 1.0f - (i - 5) / 4.0f, y = 1.0f;
		} else {
			x = 0.0f, y = 1.0f - (i - 10) / 4.0f;
		}

		Color c = Scene(x, y);
		if (_s.valid && _cfg.attributeBars) {
			// Mirrors the HUD: magicka (left side), health (bottom), stamina (right side).
			if (i >= 10) {
				c = BarCell(_cfg.magicka, _s.MagickaPct(), i - 10, 5, _s.MagickaPct() < _cfg.lowMagicka);
			} else if (i >= 5) {
				c = BarCell(_cfg.health, _s.HealthPct(), 9 - i, 5, _s.HealthPct() < _cfg.lowHealth);
			} else {
				c = BarCell(_cfg.stamina, _s.StaminaPct(), 4 - i, 5, _s.StaminaPct() < _cfg.lowStamina);
			}
		}
		a_pad.Color[i] = Post(x, y, c).ToColorRef(_cfg.brightness);
	}
}

void Renderer::ComposeHeadset(Chroma::Headset::CUSTOM_EFFECT_TYPE& a_headset)
{
	using namespace Chroma::Headset;
	for (int i = 0; i < LEDS; ++i) {
		const float x = i / float(LEDS - 1);
		a_headset.Color[i] = Post(x, 0.5f, Scene(x, 0.5f)).ToColorRef(_cfg.brightness);
	}
}

void Renderer::ComposeKeypad(Chroma::Keypad::CUSTOM_EFFECT_TYPE& a_keypad)
{
	using namespace Chroma::Keypad;
	for (int r = 0; r < ROWS; ++r) {
		for (int c = 0; c < COLS; ++c) {
			const float x = c / float(COLS - 1), y = r / float(ROWS - 1);
			Color col = Scene(x, y);
			if (_s.valid) {
				const int fromBottom = ROWS - 1 - r;
				if (c == COLS - 1 && _cfg.attributeBars) {
					col = BarCell(_cfg.health, _s.HealthPct(), fromBottom, ROWS, _s.HealthPct() < _cfg.lowHealth);
				} else if (c < 4 && _cfg.survival && _s.sunhelm) {
					const int   stages[4] = { _s.hungerStage, _s.thirstStage, _s.fatigueStage, _s.coldStage };
					const int   maxes[4] = { 4, 5, 5, 5 };
					const Color colors[4] = { Palette::Hunger, Palette::Thirst, Palette::Fatigue, _cfg.frost };
					if (stages[c] >= 0) {
						col = SurvivalCell(colors[c], stages[c], maxes[c], fromBottom, ROWS);
					}
				}
			}
			a_keypad.Color[r][c] = Post(x, y, col).ToColorRef(_cfg.brightness);
		}
	}
}

void Renderer::ComposeChromaLink(Chroma::ChromaLink::CUSTOM_EFFECT_TYPE& a_link)
{
	using namespace Chroma::ChromaLink;
	for (int i = 0; i < LEDS; ++i) {
		const float x = i / float(LEDS - 1);
		a_link.Color[i] = Post(x, 0.5f, Scene(x, 0.5f)).ToColorRef(_cfg.brightness);
	}
}

void Renderer::Compose(Chroma::Frame& a_frame)
{
	if (_cfg.devices.keyboard) ComposeKeyboard(a_frame.keyboard);
	if (_cfg.devices.mouse) ComposeMouse(a_frame.mouse);
	if (_cfg.devices.mousepad) ComposeMousepad(a_frame.mousepad);
	if (_cfg.devices.headset) ComposeHeadset(a_frame.headset);
	if (_cfg.devices.keypad) ComposeKeypad(a_frame.keypad);
	if (_cfg.devices.chromaLink) ComposeChromaLink(a_frame.chromaLink);
}
