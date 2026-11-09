#include "GameState.h"

#include <unordered_set>

namespace
{
	std::mutex        g_lock;
	GameSnapshot      g_snapshot;
	GameEvents        g_events;
	std::atomic<bool> g_samplePending{ false };
	std::atomic<bool> g_inGame{ false };

	// -------------------------------------------------------------------------
	// Forms resolved once at data load
	// -------------------------------------------------------------------------
	struct SunHelmForms
	{
		static constexpr std::string_view PLUGIN = "SunHelmSurvival.esp";

		RE::TESGlobal*                 enabled{ nullptr };
		RE::TESGlobal*                 coldLevel{ nullptr };
		RE::TESGlobal*                 coldCap{ nullptr };
		RE::TESGlobal*                 coldActive{ nullptr };
		std::vector<RE::SpellItem*>    hunger;   // _SHHunger00..04
		std::vector<RE::SpellItem*>    thirst;   // _SHThirst00..05
		std::vector<RE::SpellItem*>    fatigue;  // _SHFatigue00..05
		std::vector<RE::SpellItem*>    cold;     // _SHColdSpell0..5
		bool                           present{ false };
	} g_sunhelm;

	RE::TESRace* g_werewolfRace{ nullptr };
	RE::TESRace* g_vampireLordRace{ nullptr };

	// Every external integration goes through these helpers, so each one is optional:
	// if the plugin is missing or disabled in the load order, lookups return nullptr and
	// the dependent effects simply never trigger. Skhroma has no masters and no ESP.
	bool IsPluginLoaded(std::string_view a_plugin)
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		return dh && dh->LookupLoadedModByName(a_plugin) != nullptr;
	}

	template <class T>
	T* Lookup(RE::FormID a_localID, std::string_view a_plugin)
	{
		if (!IsPluginLoaded(a_plugin)) {
			return nullptr;
		}
		// LookupForm<T> also checks the form type, so a mismatched ID yields nullptr instead of a wrong form.
		return RE::TESDataHandler::GetSingleton()->LookupForm<T>(a_localID, a_plugin);
	}

	std::vector<RE::SpellItem*> LookupSpells(std::initializer_list<RE::FormID> a_ids)
	{
		std::vector<RE::SpellItem*> out;
		for (auto id : a_ids) {
			out.push_back(Lookup<RE::SpellItem>(id, SunHelmForms::PLUGIN));
		}
		return out;
	}

	void ResolveForms()
	{
		if (IsPluginLoaded(SunHelmForms::PLUGIN)) {
			// Local FormIDs taken from SunHelmSurvival.esp (SunHelm 3.x as shipped in Nolvus Awakening).
			g_sunhelm.enabled = Lookup<RE::TESGlobal>(0x02EB63, SunHelmForms::PLUGIN);     // _SHEnabled
			g_sunhelm.coldLevel = Lookup<RE::TESGlobal>(0x6A13C5, SunHelmForms::PLUGIN);   // _SHCurrentColdLevel
			g_sunhelm.coldCap = Lookup<RE::TESGlobal>(0xA08085, SunHelmForms::PLUGIN);     // _SHColdLevelCap
			g_sunhelm.coldActive = Lookup<RE::TESGlobal>(0x79441D, SunHelmForms::PLUGIN);  // _SHColdActive

			g_sunhelm.hunger = LookupSpells({ 0x001825, 0x001827, 0x001829, 0x00182B, 0x00182D });
			g_sunhelm.thirst = LookupSpells({ 0x01E850, 0x05C47E, 0x05C47F, 0x05C481, 0x05C483, 0x05C485 });
			g_sunhelm.fatigue = LookupSpells({ 0x01E846, 0x01E845, 0x01E848, 0x01E84A, 0x01E84C, 0x01E84E });
			g_sunhelm.cold = LookupSpells({ 0x6E81DA, 0x6E81DB, 0x6E81DE, 0x6E81E0, 0x6E81E2, 0x6E81E4 });

			auto count = [](const std::vector<RE::SpellItem*>& v) { return std::ranges::count_if(v, [](auto* s) { return s != nullptr; }); };
			g_sunhelm.present = g_sunhelm.enabled != nullptr;
			logger::info("SunHelm detected: globals(enabled={}, cold={}, cap={}, coldActive={}) spells(hunger={}/5, thirst={}/6, fatigue={}/6, cold={}/6)",
				g_sunhelm.enabled != nullptr, g_sunhelm.coldLevel != nullptr, g_sunhelm.coldCap != nullptr, g_sunhelm.coldActive != nullptr,
				count(g_sunhelm.hunger), count(g_sunhelm.thirst), count(g_sunhelm.fatigue), count(g_sunhelm.cold));
		} else {
			logger::info("SunHelm not found; survival effects disabled.");
		}

		g_werewolfRace = Lookup<RE::TESRace>(0x0CDD84, "Skyrim.esm");        // WerewolfBeastRace
		g_vampireLordRace = Lookup<RE::TESRace>(0x00283A, "Dawnguard.esm");  // DLC1VampireBeastRace (optional DLC)
		logger::info("Integrations: Dawnguard (Vampire Lord) {}.", g_vampireLordRace ? "found" : "not found, effect disabled");
	}

	// -------------------------------------------------------------------------
	// Event sinks
	// -------------------------------------------------------------------------
	class HitSink final : public RE::BSTEventSink<RE::TESHitEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
		{
			if (a_event && a_event->target && a_event->target.get() == RE::PlayerCharacter::GetSingleton()) {
				if (a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked)) {
					g_events.hitsBlocked.fetch_add(1, std::memory_order_relaxed);
				} else {
					g_events.hitsTaken.fetch_add(1, std::memory_order_relaxed);
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	class LevelSink final : public RE::BSTEventSink<RE::LevelIncrease::Event>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::LevelIncrease::Event*, RE::BSTEventSource<RE::LevelIncrease::Event>*) override
		{
			g_events.levelUps.fetch_add(1, std::memory_order_relaxed);
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	HitSink   g_hitSink;
	LevelSink g_levelSink;

	// -------------------------------------------------------------------------
	// Sampling (main thread only)
	// -------------------------------------------------------------------------
	int StageOf(RE::Actor* a_actor, const std::vector<RE::SpellItem*>& a_stages, const std::unordered_set<RE::MagicItem*>& a_active)
	{
		for (int i = static_cast<int>(a_stages.size()) - 1; i >= 0; --i) {
			auto* spell = a_stages[static_cast<std::size_t>(i)];
			if (spell && (a_active.contains(spell) || a_actor->HasSpell(spell))) {
				return i;
			}
		}
		return -1;
	}

	float MaxAV(RE::Actor* a_actor, RE::ActorValue a_av)
	{
		auto* owner = a_actor->AsActorValueOwner();
		return owner->GetPermanentActorValue(a_av) +
		       a_actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIERS::kTemporary, a_av);
	}

	void Sample()
	{
		GameSnapshot s;

		auto* ui = RE::UI::GetSingleton();
		if (ui) {
			s.mainMenu = ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
			s.loading = ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
			s.mapMenu = ui->IsMenuOpen(RE::MapMenu::MENU_NAME);
			s.lockpicking = ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME);
			s.sleepWait = ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME);
			s.dialogue = ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
			s.menuOpen = ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::MagicMenu::MENU_NAME) ||
			             ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) || ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) ||
			             ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) || ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
			             ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME) || ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) ||
			             ui->IsMenuOpen(RE::LevelUpMenu::MENU_NAME) || s.mapMenu;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (g_inGame && player && player->GetParentCell() && !s.mainMenu) {
			s.valid = true;

			auto* av = player->AsActorValueOwner();
			s.health = av->GetActorValue(RE::ActorValue::kHealth);
			s.magicka = av->GetActorValue(RE::ActorValue::kMagicka);
			s.stamina = av->GetActorValue(RE::ActorValue::kStamina);
			s.healthMax = MaxAV(player, RE::ActorValue::kHealth);
			s.magickaMax = MaxAV(player, RE::ActorValue::kMagicka);
			s.staminaMax = MaxAV(player, RE::ActorValue::kStamina);
			s.dragonSouls = av->GetActorValue(RE::ActorValue::kDragonSouls);
			s.level = player->GetLevel();

			s.inCombat = player->IsInCombat();
			s.dead = player->IsDead();
			if (auto* state = player->AsActorState()) {
				s.sneaking = state->IsSneaking();
				s.swimming = state->IsSwimming();
				s.weaponDrawn = state->IsWeaponDrawn();
				s.bleedingOut = state->IsBleedingOut();
			}

			if (auto* process = player->GetActorRuntimeData().currentProcess; process && process->high) {
				s.voiceRecovery = std::max(0.0f, process->high->voiceRecoveryTime);
			}

			if (auto* race = player->GetRace()) {
				s.werewolf = g_werewolfRace && race == g_werewolfRace;
				s.vampireLord = g_vampireLordRace && race == g_vampireLordRace;
				s.vampire = race->HasKeywordString("Vampire");
			}

			auto* cell = player->GetParentCell();
			s.interior = cell->IsInteriorCell();

			if (auto* calendar = RE::Calendar::GetSingleton()) {
				s.hour = calendar->GetHour();
			}

			if (auto* sky = RE::Sky::GetSingleton(); sky && !s.interior) {
				s.raining = sky->IsRaining();
				s.snowing = sky->IsSnowing();
				if (auto* weather = sky->currentWeather) {
					s.cloudy = weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kCloudy);
					s.lightningFrequency = static_cast<std::uint8_t>(weather->data.thunderLightningFrequency);
					const auto& lc = weather->data.lightningColor;
					s.lightningColor = Color(lc.red / 255.0f, lc.green / 255.0f, lc.blue / 255.0f);
				}
			}

			if (g_sunhelm.present && g_sunhelm.enabled->value != 0.0f) {
				s.sunhelm = true;

				std::unordered_set<RE::MagicItem*> active;
				if (auto* effects = player->AsMagicTarget()->GetActiveEffectList()) {
					for (auto* effect : *effects) {
						if (effect && effect->spell &&
							effect->flags.none(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
							active.insert(effect->spell);
						}
					}
				}

				s.hungerStage = StageOf(player, g_sunhelm.hunger, active);
				s.thirstStage = StageOf(player, g_sunhelm.thirst, active);
				s.fatigueStage = StageOf(player, g_sunhelm.fatigue, active);
				s.coldStage = StageOf(player, g_sunhelm.cold, active);
				s.coldActive = g_sunhelm.coldActive && g_sunhelm.coldActive->value != 0.0f;

				if (g_sunhelm.coldLevel) {
					const float cap = (g_sunhelm.coldCap && g_sunhelm.coldCap->value > 0.0f) ? g_sunhelm.coldCap->value : 900.0f;
					s.coldLevel = std::clamp(g_sunhelm.coldLevel->value / cap, 0.0f, 1.0f);
				}
			}
		}

		std::scoped_lock lock(g_lock);
		g_snapshot = s;
	}
}

namespace GameState
{
	void OnDataLoaded()
	{
		ResolveForms();

		if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESHitEvent>(&g_hitSink);
		}
		if (auto* source = RE::LevelIncrease::GetEventSource()) {
			source->AddEventSink(&g_levelSink);
		}
		logger::info("Event sinks registered.");
	}

	void RequestSample()
	{
		bool expected = false;
		if (!g_samplePending.compare_exchange_strong(expected, true)) {
			return;  // previous request still queued
		}
		SKSE::GetTaskInterface()->AddTask([] {
			Sample();
			g_samplePending = false;
		});
	}

	GameSnapshot GetSnapshot()
	{
		std::scoped_lock lock(g_lock);
		return g_snapshot;
	}

	GameEvents& Events() { return g_events; }

	void SetInGame(bool a_inGame) { g_inGame = a_inGame; }
}
