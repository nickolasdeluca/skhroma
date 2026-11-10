#pragma once

// Minimal, header-free binding to the Razer Chroma native SDK (RzChromaSDK64.dll).
// Works with Razer Synapse and with compatible re-implementations such as open-chroma.
// Only CUSTOM effects are used: every frame we push the full LED grid of each device.

namespace Chroma
{
	using COLORREF = std::uint32_t;  // 0x00BBGGRR

	namespace Keyboard
	{
		inline constexpr int ROWS = 6;
		inline constexpr int COLS = 22;
		inline constexpr int CHROMA_CUSTOM = 2;
		struct CUSTOM_EFFECT_TYPE
		{
			COLORREF Color[ROWS][COLS];
		};

		// Razer RZKEY encoding: high byte = row, low byte = column.
		struct Key
		{
			std::uint8_t row;
			std::uint8_t col;
		};
		constexpr Key FromRz(std::uint16_t a_rz) { return { static_cast<std::uint8_t>(a_rz >> 8), static_cast<std::uint8_t>(a_rz & 0xFF) }; }

		// A selection of RZKEY codes (from RzChromaSDKTypes.h).
		inline constexpr std::uint16_t ESC = 0x0001;
		inline constexpr std::uint16_t F1 = 0x0003;  // F1..F12 = 0x0003..0x000E
		inline constexpr std::uint16_t PRINTSCREEN = 0x000F;
		inline constexpr std::uint16_t SCROLL = 0x0010;
		inline constexpr std::uint16_t PAUSE = 0x0011;
		inline constexpr std::uint16_t OEM_1 = 0x0101;  // ` ~
		inline constexpr std::uint16_t K1 = 0x0102;     // 1..0 = 0x0102..0x010B
		inline constexpr std::uint16_t TAB = 0x0201;
		inline constexpr std::uint16_t Q = 0x0202;
		inline constexpr std::uint16_t W = 0x0203;
		inline constexpr std::uint16_t E = 0x0204;
		inline constexpr std::uint16_t R = 0x0205;
		inline constexpr std::uint16_t CAPSLOCK = 0x0301;
		inline constexpr std::uint16_t A = 0x0302;
		inline constexpr std::uint16_t S = 0x0303;
		inline constexpr std::uint16_t D = 0x0304;
		inline constexpr std::uint16_t LSHIFT = 0x0401;
		inline constexpr std::uint16_t Z = 0x0403;
		inline constexpr std::uint16_t LCTRL = 0x0501;
		inline constexpr std::uint16_t LALT = 0x0503;
		inline constexpr std::uint16_t SPACE = 0x0507;

		// DirectInput scan code (what Skyrim's ControlMap stores) -> RZKEY. 0 = no LED for that key.
		constexpr std::uint16_t FromDIK(std::uint32_t a_dik)
		{
			if (a_dik >= 0x02 && a_dik <= 0x0B) return static_cast<std::uint16_t>(0x0102 + (a_dik - 0x02));  // 1..0
			if (a_dik >= 0x10 && a_dik <= 0x19) return static_cast<std::uint16_t>(0x0202 + (a_dik - 0x10));  // Q..P
			if (a_dik >= 0x1E && a_dik <= 0x26) return static_cast<std::uint16_t>(0x0302 + (a_dik - 0x1E));  // A..L
			if (a_dik >= 0x2C && a_dik <= 0x32) return static_cast<std::uint16_t>(0x0403 + (a_dik - 0x2C));  // Z..M
			if (a_dik >= 0x3B && a_dik <= 0x44) return static_cast<std::uint16_t>(0x0003 + (a_dik - 0x3B));  // F1..F10
			switch (a_dik) {
			case 0x01: return 0x0001;  // Esc
			case 0x0C: return 0x010C;  // -
			case 0x0D: return 0x010D;  // =
			case 0x0E: return 0x010E;  // Backspace
			case 0x0F: return 0x0201;  // Tab
			case 0x1A: return 0x020C;  // [
			case 0x1B: return 0x020D;  // ]
			case 0x1C: return 0x030E;  // Enter
			case 0x1D: return 0x0501;  // LCtrl
			case 0x27: return 0x030B;  // ;
			case 0x28: return 0x030C;  // '
			case 0x29: return 0x0101;  // `
			case 0x2A: return 0x0401;  // LShift
			case 0x2B: return 0x020E;  // backslash
			case 0x33: return 0x040A;  // ,
			case 0x34: return 0x040B;  // .
			case 0x35: return 0x040C;  // /
			case 0x36: return 0x040E;  // RShift
			case 0x37: return 0x0114;  // Num *
			case 0x38: return 0x0503;  // LAlt
			case 0x39: return 0x0507;  // Space
			case 0x3A: return 0x0301;  // CapsLock
			case 0x45: return 0x0112;  // NumLock
			case 0x46: return 0x0010;  // ScrollLock
			case 0x47: return 0x0212;  // Num 7
			case 0x48: return 0x0213;  // Num 8
			case 0x49: return 0x0214;  // Num 9
			case 0x4A: return 0x0115;  // Num -
			case 0x4B: return 0x0312;  // Num 4
			case 0x4C: return 0x0313;  // Num 5
			case 0x4D: return 0x0314;  // Num 6
			case 0x4E: return 0x0215;  // Num +
			case 0x4F: return 0x0412;  // Num 1
			case 0x50: return 0x0413;  // Num 2
			case 0x51: return 0x0414;  // Num 3
			case 0x52: return 0x0513;  // Num 0
			case 0x53: return 0x0514;  // Num .
			case 0x57: return 0x000D;  // F11
			case 0x58: return 0x000E;  // F12
			case 0x9C: return 0x0415;  // Num Enter
			case 0x9D: return 0x050E;  // RCtrl
			case 0xB5: return 0x0113;  // Num /
			case 0xB7: return 0x000F;  // PrintScreen
			case 0xB8: return 0x050B;  // RAlt
			case 0xC5: return 0x0011;  // Pause
			case 0xC7: return 0x0110;  // Home
			case 0xC8: return 0x0410;  // Up
			case 0xC9: return 0x0111;  // PageUp
			case 0xCB: return 0x050F;  // Left
			case 0xCD: return 0x0511;  // Right
			case 0xCF: return 0x0210;  // End
			case 0xD0: return 0x0510;  // Down
			case 0xD1: return 0x0211;  // PageDown
			case 0xD2: return 0x010F;  // Insert
			case 0xD3: return 0x020F;  // Delete
			case 0xDB: return 0x0502;  // LWin
			case 0xDD: return 0x050D;  // Apps/Menu
			default: return 0;
			}
		}
	}

	namespace Mouse
	{
		inline constexpr int ROWS = 9;
		inline constexpr int COLS = 7;
		inline constexpr int CHROMA_CUSTOM2 = 8;
		struct CUSTOM_EFFECT_TYPE2
		{
			COLORREF Color[ROWS][COLS];
		};
	}

	namespace Mousepad
	{
		inline constexpr int LEDS = 15;
		inline constexpr int CHROMA_CUSTOM = 2;
		struct CUSTOM_EFFECT_TYPE
		{
			COLORREF Color[LEDS];
		};
	}

	namespace Headset
	{
		inline constexpr int LEDS = 5;
		inline constexpr int CHROMA_CUSTOM = 4;
		struct CUSTOM_EFFECT_TYPE
		{
			COLORREF Color[LEDS];
		};
	}

	namespace Keypad
	{
		inline constexpr int ROWS = 4;
		inline constexpr int COLS = 5;
		inline constexpr int CHROMA_CUSTOM = 2;
		struct CUSTOM_EFFECT_TYPE
		{
			COLORREF Color[ROWS][COLS];
		};
	}

	namespace ChromaLink
	{
		inline constexpr int LEDS = 5;
		inline constexpr int CHROMA_CUSTOM = 1;
		struct CUSTOM_EFFECT_TYPE
		{
			COLORREF Color[LEDS];
		};
	}

	// One full lighting frame for every device type.
	struct Frame
	{
		Keyboard::CUSTOM_EFFECT_TYPE   keyboard{};
		Mouse::CUSTOM_EFFECT_TYPE2     mouse{};
		Mousepad::CUSTOM_EFFECT_TYPE   mousepad{};
		Headset::CUSTOM_EFFECT_TYPE    headset{};
		Keypad::CUSTOM_EFFECT_TYPE     keypad{};
		ChromaLink::CUSTOM_EFFECT_TYPE chromaLink{};
	};

	struct DeviceMask
	{
		bool keyboard{ true };
		bool mouse{ true };
		bool mousepad{ true };
		bool headset{ true };
		bool keypad{ true };
		bool chromaLink{ true };
	};

	class SDK
	{
	public:
		static SDK& Get();

		// Loads the DLL and calls Init(). Safe to call repeatedly (retries after failure).
		bool Initialize();
		void Shutdown();
		[[nodiscard]] bool IsReady() const { return _ready; }

		// Pushes devices whose content changed since the previous call. Devices whose last
		// push failed are retried every frame, and unchanged ones are refreshed once per
		// KEEPALIVE in case the SDK accepted a frame but dropped it. If every device keeps
		// failing, the SDK is shut down so the caller's Initialize() retry re-registers it.
		void Submit(const Frame& a_frame, const DeviceMask& a_mask);

		// Verbose diagnostics: logs every change in a device's SDK result code and
		// periodic per-device send statistics (see LogStats).
		void SetVerbose(bool a_verbose) { _verbose = a_verbose; }
		void LogStats();

		using RZRESULT = long;
		static const char* ResultName(RZRESULT a_result);

	private:
		SDK() = default;

		using Clock = std::chrono::steady_clock;
		static constexpr auto KEEPALIVE = 1s;
		static constexpr auto REINIT_AFTER = 5s;    // all devices failing this long -> re-init
		static constexpr auto REINIT_MAX = 60s;     // cap for the doubling re-init delay

		enum Device : std::size_t
		{
			kKeyboard,
			kMouse,
			kMousepad,
			kHeadset,
			kKeypad,
			kChromaLink,
			kDeviceCount
		};

		struct DeviceStats
		{
			std::uint64_t sent{ 0 };
			std::uint64_t failed{ 0 };
			std::uint64_t skipped{ 0 };  // unchanged frames not re-sent
			RZRESULT      lastResult{ 0 };
			bool          hasResult{ false };
			bool          lastOk{ false };
			Clock::time_point lastSend{};
		};

		void Record(Device a_device, RZRESULT a_result);
		[[nodiscard]] bool Quiet() const { return _reinits > 3; }

		using InitFn = RZRESULT (*)();
		using UnInitFn = RZRESULT (*)();
		using CreateFn = RZRESULT (*)(int a_effect, void* a_param, void* a_effectId);

		void*    _module{ nullptr };
		InitFn   _init{ nullptr };
		UnInitFn _uninit{ nullptr };
		CreateFn _keyboard{ nullptr };
		CreateFn _mouse{ nullptr };
		CreateFn _mousepad{ nullptr };
		CreateFn _headset{ nullptr };
		CreateFn _keypad{ nullptr };
		CreateFn _chromaLink{ nullptr };
		bool     _ready{ false };
		bool     _loggedLoadFailure{ false };
		bool     _verbose{ false };

		std::array<DeviceStats, kDeviceCount> _stats{};

		std::optional<Clock::time_point> _allFailingSince;
		int                              _reinits{ 0 };  // consecutive re-inits without a successful push

		Frame _last{};
		bool  _hasLast{ false };
	};
}
