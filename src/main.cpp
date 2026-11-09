#include "Config.h"
#include "GameState.h"
#include "Renderer.h"

SKSEPluginInfo(
	.Version = REL::Version{ PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH, 0 },
	.Name = PLUGIN_NAME,
	.Author = "Nickolas de Luca Alberton",
	.StructCompatibility = SKSE::StructCompatibility::Independent,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary)

namespace
{
	void SetupLog()
	{
		auto dir = logger::log_directory();
		if (!dir) {
			return;
		}
		auto path = *dir / PLUGIN_NAME ".log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
		spdlog::set_default_logger(std::move(log));
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			GameState::OnDataLoaded();
			Renderer::Start();
			break;
		case SKSE::MessagingInterface::kNewGame:
		case SKSE::MessagingInterface::kPostLoadGame:
			GameState::SetInGame(true);
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();

	const auto runtime = REL::Module::get().version();
	logger::info("{} v{}.{}.{} loading (Skyrim {})", PLUGIN_NAME, PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH, runtime.string());

	Config::Get().Load("Data/SKSE/Plugins/" PLUGIN_NAME ".ini");

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		logger::critical("Failed to register SKSE messaging listener.");
		return false;
	}
	return true;
}
