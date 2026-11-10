#include "Chroma.h"

#include <Windows.h>

namespace Chroma
{
	namespace
	{
		constexpr const char* DEVICE_NAMES[] = { "Keyboard", "Mouse", "Mousepad", "Headset", "Keypad", "ChromaLink" };

		std::string ModulePath(HMODULE a_module)
		{
			wchar_t buffer[MAX_PATH]{};
			const auto len = ::GetModuleFileNameW(a_module, buffer, MAX_PATH);
			if (len == 0) {
				return "<unknown>";
			}
			// UTF-8 by hand: path::string() throws on characters outside the ANSI code page.
			const int size = ::WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
			std::string out(static_cast<std::size_t>(size), '\0');
			::WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(len), out.data(), size, nullptr, nullptr);
			return out;
		}
	}

	SDK& SDK::Get()
	{
		static SDK instance;
		return instance;
	}

	const char* SDK::ResultName(RZRESULT a_result)
	{
		// Codes from Razer's RzErrors.h.
		switch (a_result) {
		case 0: return "SUCCESS";
		case -1: return "INVALID";
		case 5: return "ACCESS_DENIED";
		case 6: return "INVALID_HANDLE";
		case 50: return "NOT_SUPPORTED";
		case 87: return "INVALID_PARAMETER";
		case 259: return "NO_MORE_ITEMS";
		case 1062: return "SERVICE_NOT_ACTIVE";
		case 1152: return "SINGLE_INSTANCE_APP";
		case 1167: return "DEVICE_NOT_CONNECTED";
		case 1168: return "NOT_FOUND";
		case 1235: return "REQUEST_ABORTED";
		case 1247: return "ALREADY_INITIALIZED";
		case 4309: return "RESOURCE_DISABLED";
		case 4319: return "DEVICE_NOT_AVAILABLE";
		case 5023: return "NOT_VALID_STATE";
		case static_cast<RZRESULT>(0x80004005L): return "FAILED";
		default: return "UNKNOWN";
		}
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

			// The path tells a stray copy (e.g. next to SkyrimSE.exe) apart from Synapse's own DLL.
			logger::info("Loaded {} (Init={}, Keyboard={}, Mouse={}, Mousepad={}, Headset={}, Keypad={}, ChromaLink={})",
				ModulePath(mod), _init != nullptr, _keyboard != nullptr, _mouse != nullptr, _mousepad != nullptr,
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
			logger::warn("Chroma Init() failed with code {} ({}) (will retry).", result, ResultName(result));
			return false;
		}

		logger::info("Chroma SDK initialized.");
		if (_verbose) {
			logger::debug("Verbose logging on: SDK result changes are logged per device, with statistics every 10 s. "
						  "DEVICE_NOT_CONNECTED is normal for device types you do not own.");
		}
		_ready = true;
		_hasLast = false;
		_stats = {};
		_allFailingSince.reset();
		return true;
	}

	void SDK::Shutdown()
	{
		if (_ready && _uninit) {
			_uninit();
		}
		_ready = false;
		_hasLast = false;
		_allFailingSince.reset();
	}

	void SDK::Record(Device a_device, RZRESULT a_result)
	{
		auto& st = _stats[a_device];
		++st.sent;
		if (a_result != 0) {
			++st.failed;
		}
		// The first result per device (after each Init) is always logged; later changes only when verbose.
		// After a few failed re-inits the first results drop to debug so a dead service can't flood the log.
		if (!st.hasResult && !Quiet()) {
			if (a_result == 0) {
				logger::info("{}: first effect accepted by the SDK.", DEVICE_NAMES[a_device]);
			} else {
				logger::warn("{}: first effect rejected by the SDK with code {} ({}).", DEVICE_NAMES[a_device], a_result, ResultName(a_result));
			}
		} else if (_verbose && (!st.hasResult || st.lastResult != a_result)) {
			if (a_result == 0) {
				logger::debug("{}: effect accepted by the SDK.", DEVICE_NAMES[a_device]);
			} else {
				logger::debug("{}: SDK rejected effect with code {} ({}).", DEVICE_NAMES[a_device], a_result, ResultName(a_result));
			}
		}
		st.lastResult = a_result;
		st.hasResult = true;
		st.lastOk = a_result == 0;
	}

	void SDK::LogStats()
	{
		if (!_verbose) {
			return;
		}
		if (!_ready) {
			logger::debug("Stats: SDK not initialized.");
			return;
		}
		for (std::size_t i = 0; i < kDeviceCount; ++i) {
			const auto& st = _stats[i];
			if (!st.hasResult) {
				logger::debug("Stats: {} - nothing sent (disabled in the ini or not exported by the DLL).", DEVICE_NAMES[i]);
				continue;
			}
			logger::debug("Stats: {} - sent={} failed={} unchanged={} last={} ({})",
				DEVICE_NAMES[i], st.sent, st.failed, st.skipped, st.lastResult, ResultName(st.lastResult));
		}
	}

	void SDK::Submit(const Frame& a_frame, const DeviceMask& a_mask)
	{
		if (!_ready) {
			return;
		}

		const auto now = Clock::now();
		bool       anyOk = false;
		bool       anyFailed = false;

		auto push = [&, this](Device a_device, CreateFn a_fn, int a_effect, const auto& a_data, const auto& a_prev, bool a_enabled) {
			if (!a_enabled || !a_fn) {
				return;
			}
			auto& st = _stats[a_device];
			// Skip an unchanged frame only if the previous push went through and is recent;
			// failed pushes are retried every frame and good ones refreshed once per KEEPALIVE.
			if (_hasLast && st.lastOk && now - st.lastSend < KEEPALIVE && std::memcmp(&a_data, &a_prev, sizeof(a_data)) == 0) {
				++st.skipped;
				anyOk = true;
				return;
			}
			auto copy = a_data;  // the SDK takes a non-const pointer
			Record(a_device, a_fn(a_effect, &copy, nullptr));
			st.lastSend = now;
			(st.lastOk ? anyOk : anyFailed) = true;
		};

		push(kKeyboard, _keyboard, Keyboard::CHROMA_CUSTOM, a_frame.keyboard, _last.keyboard, a_mask.keyboard);
		push(kMouse, _mouse, Mouse::CHROMA_CUSTOM2, a_frame.mouse, _last.mouse, a_mask.mouse);
		push(kMousepad, _mousepad, Mousepad::CHROMA_CUSTOM, a_frame.mousepad, _last.mousepad, a_mask.mousepad);
		push(kHeadset, _headset, Headset::CHROMA_CUSTOM, a_frame.headset, _last.headset, a_mask.headset);
		push(kKeypad, _keypad, Keypad::CHROMA_CUSTOM, a_frame.keypad, _last.keypad, a_mask.keypad);
		push(kChromaLink, _chromaLink, ChromaLink::CHROMA_CUSTOM, a_frame.chromaLink, _last.chromaLink, a_mask.chromaLink);

		_last = a_frame;
		_hasLast = true;

		if (anyOk || !anyFailed) {
			_allFailingSince.reset();
			if (anyOk) {
				_reinits = 0;
			}
			return;
		}

		// Every enabled device is failing (e.g. Synapse or its Chroma service restarted).
		// Re-register after REINIT_AFTER, doubling the wait on each consecutive attempt.
		if (!_allFailingSince) {
			_allFailingSince = now;
			return;
		}
		const auto wait = std::min<Clock::duration>(REINIT_AFTER * (1LL << std::min(_reinits, 4)), REINIT_MAX);
		if (now - *_allFailingSince < wait) {
			return;
		}
		++_reinits;
		const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(wait).count();
		if (Quiet()) {
			logger::debug("Every Chroma device has been failing for {} s; reinitializing the SDK (attempt {}).", seconds, _reinits);
		} else {
			logger::warn("Every Chroma device has been failing for {} s; reinitializing the SDK (attempt {}).", seconds, _reinits);
		}
		Shutdown();
	}
}
