#pragma once

#include "Chroma.h"
#include "Color.h"

struct Config
{
	// [General]
	bool  enabled{ true };
	int   fps{ 30 };
	int   sampleIntervalMs{ 66 };
	float brightness{ 1.0f };
	float ambientBrightness{ 0.35f };

	// [Devices]
	Chroma::DeviceMask devices{};

	// [Thresholds]
	float lowHealth{ 0.30f };
	float lowMagicka{ 0.20f };
	float lowStamina{ 0.20f };

	// [Effects]
	bool ambient{ true };
	bool attributeBars{ true };
	bool lowHealthPulse{ true };
	bool damageFlash{ true };
	bool combat{ true };
	bool survival{ true };
	bool cold{ true };
	bool weather{ true };
	bool lightning{ true };
	bool shout{ true };
	bool sneak{ true };
	bool water{ true };
	bool transformations{ true };
	bool levelUp{ true };
	bool dragonSoul{ true };
	bool menus{ true };
	bool death{ true };

	// [Debug]
	bool verboseLog{ false };

	// [Colors]
	Color health{ Palette::Health };
	Color magicka{ Palette::Magicka };
	Color stamina{ Palette::Stamina };
	Color frost{ Palette::Frost };
	Color combatColor{ Palette::Ember };

	static Config& Get();
	void           Load(const std::filesystem::path& a_path);
};
