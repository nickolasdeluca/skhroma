// Standalone smoke test for the Chroma layer (no Skyrim needed).
// Plays a few seconds of a draining health bar + heartbeat on every device.
#include "Chroma.h"
#include "Color.h"

#include <spdlog/sinks/stdout_color_sinks.h>

int main()
{
	spdlog::set_default_logger(spdlog::stdout_color_mt("chroma_test"));

	auto& sdk = Chroma::SDK::Get();
	for (int attempt = 0; attempt < 5 && !sdk.Initialize(); ++attempt) {
		std::this_thread::sleep_for(1s);
	}
	if (!sdk.IsReady()) {
		spdlog::error("Chroma SDK not available.");
		return 1;
	}
	std::this_thread::sleep_for(1s);  // the SDK needs a moment before devices accept effects

	Chroma::Frame       frame{};
	Chroma::DeviceMask  all{};
	constexpr int       FPS = 30;
	constexpr float     DURATION = 8.0f;

	for (int i = 0; i <= static_cast<int>(DURATION * FPS); ++i) {
		const float t = i / static_cast<float>(FPS);
		const float hp = 1.0f - t / DURATION;
		const float beat = hp < 0.3f ? 0.5f + 0.5f * std::sin(t * 12.0f) : 0.0f;
		const Color bg = Lerp(Palette::Torch * 0.25f, Palette::Health, beat * 0.8f);

		for (int r = 0; r < Chroma::Keyboard::ROWS; ++r) {
			for (int c = 0; c < Chroma::Keyboard::COLS; ++c) {
				frame.keyboard.Color[r][c] = bg.ToColorRef();
			}
		}
		for (int k = 0; k < 12; ++k) {  // F1..F12 health bar
			const float fill = std::clamp(hp * 12.0f - k, 0.0f, 1.0f);
			frame.keyboard.Color[0][3 + k] = Lerp(Palette::Health * 0.05f, Palette::Health, fill).ToColorRef();
		}
		for (auto& row : frame.mouse.Color) for (auto& led : row) led = bg.ToColorRef();
		for (auto& led : frame.mousepad.Color) led = bg.ToColorRef();
		for (auto& led : frame.headset.Color) led = bg.ToColorRef();
		for (auto& row : frame.keypad.Color) for (auto& led : row) led = bg.ToColorRef();
		for (auto& led : frame.chromaLink.Color) led = bg.ToColorRef();

		sdk.Submit(frame, all);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 / FPS));
	}

	spdlog::info("Done.");
	sdk.Shutdown();
	return 0;
}
