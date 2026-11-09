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

		// Pushes only devices whose content changed since the previous call.
		void Submit(const Frame& a_frame, const DeviceMask& a_mask);

	private:
		SDK() = default;

		using RZRESULT = long;
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

		Frame _last{};
		bool  _hasLast{ false };
	};
}
