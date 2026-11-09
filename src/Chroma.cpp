#include "Chroma.h"

#include <Windows.h>

namespace Chroma
{
	SDK& SDK::Get()
	{
		static SDK instance;
		return instance;
	}

	bool SDK::Initialize()
	{
		if (_ready) {
			return true;
		}

		if (!_module) {
			HMODULE mod = ::LoadLibraryW(L"RzChromaSDK64.dll");
			if (!mod) {
				if (!_loggedLoadFailure) {
					logger::warn("RzChromaSDK64.dll not found (error {}). Chroma output disabled until it appears.", ::GetLastError());
					_loggedLoadFailure = true;
				}
				return false;
			}
			_module = mod;

			auto get = [mod](const char* a_name) { return reinterpret_cast<void*>(::GetProcAddress(mod, a_name)); };
			_init = reinterpret_cast<InitFn>(get("Init"));
			_uninit = reinterpret_cast<UnInitFn>(get("UnInit"));
			_keyboard = reinterpret_cast<CreateFn>(get("CreateKeyboardEffect"));
			_mouse = reinterpret_cast<CreateFn>(get("CreateMouseEffect"));
			_mousepad = reinterpret_cast<CreateFn>(get("CreateMousepadEffect"));
			_headset = reinterpret_cast<CreateFn>(get("CreateHeadsetEffect"));
			_keypad = reinterpret_cast<CreateFn>(get("CreateKeypadEffect"));
			_chromaLink = reinterpret_cast<CreateFn>(get("CreateChromaLinkEffect"));

			logger::info("Loaded RzChromaSDK64.dll (Init={}, Keyboard={}, Mouse={}, Mousepad={}, Headset={}, Keypad={}, ChromaLink={})",
				_init != nullptr, _keyboard != nullptr, _mouse != nullptr, _mousepad != nullptr,
				_headset != nullptr, _keypad != nullptr, _chromaLink != nullptr);

			if (!_init) {
				logger::error("RzChromaSDK64.dll has no Init export; Chroma output disabled.");
				return false;
			}
		}

		if (!_init) {
			return false;
		}

		const auto result = _init();
		if (result != 0) {
			logger::warn("Chroma Init() failed with code {} (will retry).", result);
			return false;
		}

		logger::info("Chroma SDK initialized.");
		_ready = true;
		_hasLast = false;
		return true;
	}

	void SDK::Shutdown()
	{
		if (_ready && _uninit) {
			_uninit();
		}
		_ready = false;
		_hasLast = false;
	}

	void SDK::Submit(const Frame& a_frame, const DeviceMask& a_mask)
	{
		if (!_ready) {
			return;
		}

		auto push = [this](CreateFn a_fn, int a_effect, const auto& a_data, const auto& a_prev, bool a_enabled) {
			if (!a_enabled || !a_fn) {
				return;
			}
			if (_hasLast && std::memcmp(&a_data, &a_prev, sizeof(a_data)) == 0) {
				return;
			}
			auto copy = a_data;  // the SDK takes a non-const pointer
			a_fn(a_effect, &copy, nullptr);
		};

		push(_keyboard, Keyboard::CHROMA_CUSTOM, a_frame.keyboard, _last.keyboard, a_mask.keyboard);
		push(_mouse, Mouse::CHROMA_CUSTOM2, a_frame.mouse, _last.mouse, a_mask.mouse);
		push(_mousepad, Mousepad::CHROMA_CUSTOM, a_frame.mousepad, _last.mousepad, a_mask.mousepad);
		push(_headset, Headset::CHROMA_CUSTOM, a_frame.headset, _last.headset, a_mask.headset);
		push(_keypad, Keypad::CHROMA_CUSTOM, a_frame.keypad, _last.keypad, a_mask.keypad);
		push(_chromaLink, ChromaLink::CHROMA_CUSTOM, a_frame.chromaLink, _last.chromaLink, a_mask.chromaLink);

		_last = a_frame;
		_hasLast = true;
	}
}
