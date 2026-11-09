#pragma once

#include "Color.h"

// Everything the renderer needs to know about the game, captured on the main thread.
struct GameSnapshot
{
	bool valid{ false };  // player exists and is in the world

	// UI
	bool mainMenu{ false };
	bool loading{ false };
	bool menuOpen{ false };  // inventory/magic/map/journal/etc.
	bool mapMenu{ false };
	bool lockpicking{ false };
	bool sleepWait{ false };
	bool dialogue{ false };

	// Attributes
	float health{ 1.0f }, healthMax{ 1.0f };
	float magicka{ 1.0f }, magickaMax{ 1.0f };
	float stamina{ 1.0f }, staminaMax{ 1.0f };
	float dragonSouls{ 0.0f };
	std::uint16_t level{ 0 };

	// Actor state
	bool  inCombat{ false };
	bool  sneaking{ false };
	bool  swimming{ false };
	bool  weaponDrawn{ false };
	bool  dead{ false };
	bool  bleedingOut{ false };
	float voiceRecovery{ 0.0f };  // seconds remaining on shout cooldown

	// Transformations / races
	bool vampire{ false };
	bool werewolf{ false };
	bool vampireLord{ false };

	// World
	bool          interior{ true };
	float         hour{ 12.0f };
	bool          raining{ false };
	bool          snowing{ false };
	bool          cloudy{ false };
	std::uint8_t  lightningFrequency{ 0 };
	Color         lightningColor{ Palette::White };

	// SunHelm (stage = -1 when unknown / mod absent)
	bool  sunhelm{ false };
	int   hungerStage{ -1 };
	int   thirstStage{ -1 };
	int   fatigueStage{ -1 };
	int   coldStage{ -1 };
	float coldLevel{ 0.0f };  // 0..1 (normalized by _SHColdLevelCap)
	bool  coldActive{ false };

	[[nodiscard]] float HealthPct() const { return healthMax > 0.0f ? std::clamp(health / healthMax, 0.0f, 1.0f) : 0.0f; }
	[[nodiscard]] float MagickaPct() const { return magickaMax > 0.0f ? std::clamp(magicka / magickaMax, 0.0f, 1.0f) : 0.0f; }
	[[nodiscard]] float StaminaPct() const { return staminaMax > 0.0f ? std::clamp(stamina / staminaMax, 0.0f, 1.0f) : 0.0f; }
};

// Counters bumped by game event sinks; the renderer compares them to detect new events.
struct GameEvents
{
	std::atomic<std::uint32_t> hitsTaken{ 0 };
	std::atomic<std::uint32_t> hitsBlocked{ 0 };
	std::atomic<std::uint32_t> levelUps{ 0 };
};

namespace GameState
{
	// Called on kDataLoaded (main thread): resolves forms and registers event sinks.
	void OnDataLoaded();

	// Asks the main thread for a fresh snapshot (non-blocking, coalesced).
	void RequestSample();

	[[nodiscard]] GameSnapshot GetSnapshot();
	[[nodiscard]] GameEvents&  Events();

	// Set from SKSE messages: true once a save is loaded or a new game started.
	void SetInGame(bool a_inGame);
}
