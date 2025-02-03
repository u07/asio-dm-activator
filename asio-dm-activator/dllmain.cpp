// ASIO-DM-ACTIVATOR
// 
// Plugin for Cubase and Studio One. Enables the ASIO Direct Monitoring feature
// on hardware that lacks it for some reason, namely Audient iD series.
// 
// Homepage: https://PetelinSasha.ru
// 2025.01.27
//
// Installation. Place the dll here:
//     Cubase: C:\Program Files\Steinberg\Cubase 12\Components
// Studio One: C:\Program Files\PreSonus\Studio One 7\Plugins

#define _WIN32_WINNT 0x0601
#include "pch.h" 

#define OK(x) ((x) == ERROR_SUCCESS)
#define NOT_OK(x) ((x) != ERROR_SUCCESS)

#define dbg(msg) OutputDebugStringW(std::format(L"[ASIO-DM-ACTIVATOR] {} :{}", msg, __LINE__).c_str())
#define err(msg) {dbg(msg); throw std::runtime_error("err");}

const int ASE_SUCCESS = 0x3f4847a0;
const int ASE_NotPresent = -1000;
const int kAsioSetInputMonitor = 3;
const int kAsioCanInputMonitor = 9;

struct ASIOInputMonitor {
	long input;		// input index (-1 = all)
	long output;	// output index
	long gain;		// gain 0..0x7fffffffL (-inf to +12 dB)
	long state;		// on/off
	long pan;		// pan, 0..0x7fffffff (left..right)
};
        
using AsioFutureFunction = long(*)(void* iasio, long selector, void* params);
using DllGetClassObjectFunction = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);


class AsioDriver {
public:

	enum class State {
		Unknown,
		PatchOk,
		PatchFail,
		Native
	};

	static const inline std::unordered_map<State, std::wstring> StateDescriptions = {
		{State::Unknown,	L"Not supported"},
		{State::PatchOk,	L"Patched OK"},
		{State::PatchFail,	L"Failed to patch"},
		{State::Native,		L"Native kAsioCanInputMonitor support"}
	};
		
	std::wstring name;		// Audient USB Audio ASIO Driver
	std::wstring clsid;		// {E7E92954-EA17-433A-9142-EE419BE4875C}
	IID iid{};				// same
	std::wstring asioPath;	// c:\program files\audient\usbaudiodriver\x64\audientusbaudioasio_x64.dll

	HMODULE asioDllHandle{};
	uintptr_t futureFunctionOriginal{};
	uintptr_t futureFunctionReplacementThunk{};
	uintptr_t asioDllPatchPlace{};

	std::wstring vendor;
	State state{};
	

	std::wstring Info() const {
		return std::format(L"{} = {} ({})",	name, StateDescriptions.at(state), vendor);
	}

	// Try to promote generic class to specific class
	virtual std::optional<std::unique_ptr<AsioDriver>> TryInit() {
		return std::nullopt;	
	}

	// Generate an adapter from function call to class function call
	uintptr_t FutureFunctionReplacementThunk() {
		const size_t funcSize = 31;
		uint8_t code[funcSize] = {	
			0x4d, 0x89, 0xc1,			 // mov r9, r8    ; shift 3 arguments right
			0x49, 0x89, 0xD0,            // mov r8, rdx   
			0x48, 0x89, 0xCA,            // mov rdx, rcx
			0x48, 0xB9, 0,0,0,0,0,0,0,0, // mov rcx, addr ; add driver instance pointer ("this" as the first argument)
			0x48, 0xB8, 0,0,0,0,0,0,0,0, // mov rax, addr ; the replacement function body address
			0xFF, 0xE0                   // jmp rax       ; continue to the replacement function
		};

		// Thunk already exists
		if (futureFunctionReplacementThunk)
			return futureFunctionReplacementThunk;

		// Insert the driver instance address
		*reinterpret_cast<uintptr_t*>(&code[11]) = reinterpret_cast<uintptr_t>(this);;

		// Insert the replacement function address
		auto replFunc = &AsioDriver::FutureFunctionReplacement;
		uintptr_t replAddr = reinterpret_cast<uintptr_t>(*(void**)&replFunc);
		*reinterpret_cast<uintptr_t*>(&code[21]) = replAddr;

		// Allocate executable memory and copy the machine code
		if (!(futureFunctionReplacementThunk = (uintptr_t)VirtualAlloc(nullptr, funcSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)))
			return 0;
		memcpy((void*)futureFunctionReplacementThunk, code, funcSize);

		return futureFunctionReplacementThunk;
	}


	// This function extends the original one from the driver, adding DM support
	long FutureFunctionReplacement(void* iasio, long selector, void* params) {
		dbg(std::format(L"called {} future(iASIO={:016x}, selector={}, params={:016x})", vendor, (uintptr_t)iasio, selector, (uintptr_t)params));
		if (!futureFunctionOriginal) return ASE_NotPresent;
		if (selector == kAsioCanInputMonitor) return ASE_SUCCESS;
		if (selector == kAsioSetInputMonitor) return SetInputMonitor((ASIOInputMonitor*)params);
		dbg(L"passed to the original function");
		return ((AsioFutureFunction)futureFunctionOriginal)(iasio, selector, params);
	}

	// Actual work is done here
	virtual long SetInputMonitor(ASIOInputMonitor* params) {
		dbg(L"Generic SetInputMonitor called. This should not happen.");
		return ASE_NotPresent;
	}

};


// This will enable DM in generic Thesycon drivers v5 and probably v4
class AsioDriver_Thesycon: public AsioDriver {

	std::wstring apiPath;	// audientusbaudioapi_x64.dll
	HMODULE apiDllHandle{};
	long deviceIndex{};
	long deviceHandle{};
	std::wstring deviceName;
	uint64_t deviceModel{};
	struct VolPair { 
		short L{0}; 
		short R{0}; 
		bool operator==(const VolPair& x) const { return L == x.L && R == x.R; }
		bool operator!=(const VolPair& x) const { return !(*this == x); }
	};
	std::map<int, VolPair> lastVol;
	VolPair minusInf = {-32768, -32768};

	using TUSBAUDIO_EnumerateDevices = long(*)();
	using TUSBAUDIO_GetDeviceCount = long(*)();
	using TUSBAUDIO_OpenDeviceByIndex = long(*)(long deviceIndex, long* deviceHandle);
	using TUSBAUDIO_AudioControlRequestGet = long(*)(long deviceHandle, long entityID, long request,
		long controlSelector, char channelOrMixerControl, void* paramBlock, long paramBlockLength,
		long* bytesTransferred, long timeoutMillisecs);
	using TUSBAUDIO_AudioControlRequestSet = TUSBAUDIO_AudioControlRequestGet;
	using TUSBAUDIO_GetDeviceProperties = long(*)(long deviceHandle, void* properties); 

	// Trying to init the driver as Thesycon if possible
	// Returns false if the driver doesn't look like Thesycon
	std::optional<std::unique_ptr<AsioDriver>> TryInit() override {
		dbg(std::format(L"Trying to init {} as Thesycon", name));
		try {
			// Check asio driver naming convention: %VendorName%UsbAudioAsio_x64.dll
			std::wstring asioSuffix = L"usbaudioasio_x64.dll";
			if (!asioPath.ends_with(asioSuffix))
				err(L"Wrong asio driver name " + apiPath);

			// Check api library presence 
			std::wstring apiSuffix = L"usbaudioapi_x64.dll";
			std::wstring _apiPath = asioPath;
			_apiPath.replace(_apiPath.find(asioSuffix), asioSuffix.size(), apiSuffix);
			if (GetFileAttributesW(_apiPath.c_str()) == INVALID_FILE_ATTRIBUTES)
				err(L"Api dll does not exist " + _apiPath);
			apiPath = _apiPath;
			if (!(apiDllHandle = LoadLibraryW(apiPath.c_str())))
				err(L"Loading api dll failed " + apiPath);
			
			// Open device
			FARPROC func = 0; 
			if (!(func = GetProcAddress(apiDllHandle, "TUSBAUDIO_EnumerateDevices"))) err(L"func not found");
			dbg(std::format(L"EnumerateDevices={}", ((TUSBAUDIO_EnumerateDevices)func)()));

			if (!(func = GetProcAddress(apiDllHandle, "TUSBAUDIO_GetDeviceCount"))) err(L"func not found");
			dbg(std::format(L"GetDeviceCount={}", ((TUSBAUDIO_GetDeviceCount)func)()));

			if NOT_OK(ReopenDevice(0)) err(L"Failed to open device");

			if NOT_OK(GetDeviceProperties()) err(L"Failed to get device model");

			// Alright, it walks like a duck and quacks like a duck
			dbg(L"Init ok, this is Thesycon indeed.");
			vendor = L"Thesycon";

			// Promote generic class to Thesycon class
			auto specific = std::make_unique<AsioDriver_Thesycon>();
			*specific = *this;
			return specific;
		}
		catch (const std::exception& e) {
			dbg(L"Attempt unsuccessfull.");
			if (apiDllHandle) {
				FreeLibrary(apiDllHandle);
				apiDllHandle = 0;
			}
			return std::nullopt;
		}
	}


	long ReopenDevice(long index) {
		auto func = GetProcAddress(apiDllHandle, "TUSBAUDIO_OpenDeviceByIndex");
		if (!func) return -1;
		deviceIndex = index;
		auto result = ((TUSBAUDIO_OpenDeviceByIndex)func)(deviceIndex, &deviceHandle);
		dbg(std::format(L"ReopenDevice result={} handle={}", result, deviceHandle));
		return result;
	}


	long GetDeviceProperties() {
		auto func = GetProcAddress(apiDllHandle, "TUSBAUDIO_GetDeviceProperties");
		if (!func) return -1;
		char buf[2048]{};
		auto result = ((TUSBAUDIO_GetDeviceProperties)func)(deviceHandle, (void*)buf);
		deviceModel = *(uint64_t*)buf; // VID & PID
		deviceName = (WCHAR*)(buf + 524); // product "Audient iD14"
		dbg(std::format(L"Device info result={} model={:016x} name={}", result, deviceModel, deviceName));
		return result;
	}


	bool DeviceAlive() {
		auto func = GetProcAddress(apiDllHandle, "TUSBAUDIO_AudioControlRequestGet");
		if (!func) return false;
		short buf{};
		auto result = ((TUSBAUDIO_AudioControlRequestGet)func)(deviceHandle, 0x3C, 0x1, 0x1, 0, (void*)&buf, 2, NULL, 2000);
		dbg(std::format(L"DeviceCheck = {}", (result==0)));
		return (result == 0);
	}

	byte GetVirtualChannelIndex(byte channel) {

		std::unordered_map<uint64_t, int> mixSupport = {
			{ 0x0100002708, 3 },   //=Audient iD22 mk1 (from video) 
			{ 0x0200002708, 3 },   //?Audient iD14 mk1 (from video, but somewhere 2)
			{ 0x0300002708, 1 },   // Audient iD4 mk1
			{ 0x0400002708, 2 },   //=Audient Sono (from video)                             todo! 
			{ 0x0500002708, 3 },   // Audient iD44 mk1                            Check mix numbers with real HW.
			{ 0x0600002708, 1 },   // Audient EVO4                                 Not sure if they are correct.
			{ 0x0700002708, 2 },   //?Audient EVO8 (from video)
			{ 0x0900002708, 1 },   // Audient iD4 mk2                                    1 = MainMix
			{ 0x0800002708, 3 },   //+Audient iD14 mk2 (real hw tested)                  2 = MainMix + Cue
			{ 0x0A00002708, 5 },   //=Audient EVO16 (from video)                         3 = MainMix + CueA + CueB
			{ 0x0B00002708, 5 },   //=Audient iD44 mk2 (from video)
			{ 0x0D00002708, 3 },   //=Audient iD24 mk2 (from video)
			{ 0x0E00002708, 1 },   // Audient ORIA
			{ 0x0F00002708, 1 },   // Audient iD4 Stream OTG 
			{ 0x1000002708, 3 },   // Audient iD14 Stream OTG
//			{ 0x__00002708, 5 },   //=Audient iD48 (from video presentation)
		};
		int mixCount = mixSupport.contains(deviceModel) ? mixSupport[deviceModel] : 1;
		return channel * (mixCount * 2); // L+R
	}


	long SetVol(byte channel, VolPair vol) {
		byte channel_l = GetVirtualChannelIndex(channel);
		byte channel_r = GetVirtualChannelIndex(channel) + 1;
		auto func = GetProcAddress(apiDllHandle, "TUSBAUDIO_AudioControlRequestSet");
		if (!func) return -1;
		auto result = ((TUSBAUDIO_AudioControlRequestSet)func)(deviceHandle, 0x3C, 0x1, 0x1, channel_l, (void*)&vol.L, 2, NULL, 10000);
		              ((TUSBAUDIO_AudioControlRequestSet)func)(deviceHandle, 0x3C, 0x1, 0x1, channel_r, (void*)&vol.R, 2, NULL, 10000);
		dbg(std::format(L"SetVol ch{}={}/{} result={}", channel, vol.L, vol.R, result));
		return result;
	}


	long GetVol(byte channel, VolPair& vol) {
		byte channel_l = GetVirtualChannelIndex(channel);
		byte channel_r = GetVirtualChannelIndex(channel) + 1;
		auto func = GetProcAddress(apiDllHandle, "TUSBAUDIO_AudioControlRequestGet");
		if (!func) return -1;
		auto result = ((TUSBAUDIO_AudioControlRequestGet)func)(deviceHandle, 0x3C, 0x1, 0x1, channel_l, (void*)&vol.L, 2, NULL, 10000);
		              ((TUSBAUDIO_AudioControlRequestGet)func)(deviceHandle, 0x3C, 0x1, 0x1, channel_r, (void*)&vol.R, 2, NULL, 10000);
		dbg(std::format(L"GetVol ch{}={}/{} result={}", channel, vol.L, vol.R, result));
		return result;
	}


	// Actual work is done here
	long SetInputMonitor(ASIOInputMonitor* params) override {
		dbg(std::format(L"Thesycon SetInputMonitor in={} out={} gain={} pan={} state={}", params->input, params->output, params->gain, params->pan, params->state));

		if (!DeviceAlive()) ReopenDevice(deviceIndex);
		VolPair v{};
		GetVol(params->input, v);
		if (v != minusInf) lastVol[params->input] = v;

		if (params->state && (params->gain > 100)) 
			SetVol(params->input, lastVol[params->input]);
		else 
			SetVol(params->input, minusInf);

		return ASE_SUCCESS;
	}
};


// This will "enable" DM in Asio4All (not really, debug purposes)
class AsioDriver_Asio4All : public AsioDriver {

	std::optional<std::unique_ptr<AsioDriver>> TryInit() override {
		dbg(L"Trying to init '" + name + L"' as Asio4All");
		#ifndef _DEBUG
			dbg(L"Available in debug builds only");
			return std::nullopt;
		#endif
		if (asioPath.ends_with(L"asio4all64.dll")) {
			dbg(L"Init ok, this is Asio4All.");
			vendor = L"Asio4All";

			// Promote generic class to Asio4All class
			auto specific = std::make_unique<AsioDriver_Asio4All>();
			*specific = *this; 
			return specific;
		}
		else {
			dbg(L"Nah, no way.");
			return std::nullopt;
		}
	}

	// Actual work is done here
	long SetInputMonitor(ASIOInputMonitor* params) override {
		dbg(std::format(L"Asio4All SetInputMonitor in={} out={} gain={} pan={} state={}", params->input, params->output, params->gain, params->pan, params->state));
		return ASE_NotPresent;
	}

};


class AsioDriver_Antelope : public AsioDriver {
	// A placeholder for other audio interfaces. 
	// Don't forget to register class in AsioDriverManager.
};


class AsioDriver_SomethingElse : public AsioDriver {
	// A placeholder for other audio interfaces. 
	// Don't forget to register class in AsioDriverManager.
};



class AsioDriverManager {
public:

	std::vector<std::unique_ptr<AsioDriver>> drivers;

	// All known driver types (class fabric)
	const std::vector<std::function<std::unique_ptr<AsioDriver>()>> knownDrivers = {
		[]() { return std::make_unique<AsioDriver_Thesycon>(); },
		[]() { return std::make_unique<AsioDriver_Asio4All>(); },
		[]() { return std::make_unique<AsioDriver_Antelope>(); },
		[]() { return std::make_unique<AsioDriver_SomethingElse>(); }
	};

	// Wakey-wakey, eggs and bakey
	AsioDriverManager() {
		ListDrivers();
		InitDrivers();
		PatchDrivers();
	}

private:

	void ListDrivers() {
		dbg(L"Getting installed drivers");
		drivers.clear();
		HKEY asioRoot;
		if NOT_OK(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\ASIO", 0, KEY_READ, &asioRoot)) return;

		HKEY subkey;
		DWORD index = 0;
		WCHAR buf[512]{};
		DWORD bufSize;
		while (true) {                 // Loop through all the registered drivers

			// read name
			auto driver = std::make_unique<AsioDriver>();
			bufSize = sizeof(buf) / sizeof(buf[0]);
			if NOT_OK(RegEnumKeyExW(asioRoot, index++, buf, &bufSize, NULL, NULL, NULL, NULL)) break;
			driver->name = buf;

			// read clsid
			bufSize = sizeof(buf) / sizeof(buf[0]);
			if NOT_OK(RegOpenKeyExW(asioRoot, driver->name.c_str(), 0, KEY_READ, &subkey)) continue;
			if NOT_OK(RegQueryValueExW(subkey, L"CLSID", NULL, NULL, (LPBYTE)buf, &bufSize)) { RegCloseKey(subkey); continue; }
			driver->clsid = buf;
			CLSIDFromString(driver->clsid.c_str(), &driver->iid);
			RegCloseKey(subkey);

			// read asio dll path (lowercase)
			bufSize = sizeof(buf) / sizeof(buf[0]);
			if NOT_OK(RegOpenKeyExW(HKEY_CLASSES_ROOT, (L"CLSID\\" + driver->clsid + L"\\InProcServer32").c_str(), 0, KEY_READ, &subkey)) continue;
			if NOT_OK(RegQueryValueExW(subkey, L"", NULL, NULL, (LPBYTE)buf, &bufSize)) { RegCloseKey(subkey); continue; }
			driver->asioPath = buf;
			std::transform(driver->asioPath.begin(), driver->asioPath.end(), driver->asioPath.begin(), ::towlower);
			if (!PathFileExistsW(driver->asioPath.c_str())) continue;
			RegCloseKey(subkey);
			
			dbg(L"Found " + driver->name);
			drivers.push_back((std::move(driver)));
		}
		RegCloseKey(asioRoot);
	}


	// Init and promote generic classes to specific classes
	void InitDrivers() {
		dbg(L"Initializing drivers");

		for (auto& driver : drivers) {
			for (const auto& createDriver : knownDrivers) {
				auto specificDriver = createDriver();
				*specificDriver = *driver;
				if (auto result = specificDriver->TryInit(); result) {
					driver = std::move(*result); // Replace generic asio driver with a specific one					
					break;
				}
			}
		}
	}


	// Load Asio drivers and patch them in-memory
	void PatchDrivers() {
		dbg(L"Patching drivers");

        for (auto& driver : drivers) {
			HMODULE hModule = 0;
            try {
				dbg(L"Patching " + driver->name);
				if (typeid(*driver) == typeid(AsioDriver)) 
					err(L"Unknown or unsupported driver.");

                // Load ASIO driver
                hModule = LoadLibraryW(driver->asioPath.c_str());
				if (!hModule) 
					err(L"Can not load dll " + driver->asioPath);
    
                auto pDllGetClassObject = reinterpret_cast<DllGetClassObjectFunction>(GetProcAddress(hModule, "DllGetClassObject"));
				if (!pDllGetClassObject)
					err(L"Function DllGetClassObject not found in " + driver->asioPath);
        
				// Get class factory
                IClassFactory* pClassFactory = nullptr;  
				if NOT_OK(pDllGetClassObject(driver->iid, IID_IClassFactory, (void**)&pClassFactory))
					err(L"Unable to obtain class factory from " + driver->asioPath);
        
				// Get IASIO interface from factory
                IUnknown* pAsio = nullptr;  
				if NOT_OK(pClassFactory->CreateInstance(nullptr, driver->iid, (void**)&pAsio)) 
					err(L"Unable to obtain iASIO " + driver->clsid + L" from " + driver->asioPath);
        
				// Find future() is the 23'rd method (0-based)
                uintptr_t* vtable = *(uintptr_t**)(pAsio);  
                if (!vtable || !vtable[22])  
					err(std::format(L"iASIO vtable @ {:016x} looks corrupted", (uintptr_t)vtable));
                driver->asioDllPatchPlace = (uintptr_t) &vtable[22];
                driver->futureFunctionOriginal = vtable[22];
                
                // Is native dm control supported? then no need to patch
				if (((AsioFutureFunction)driver->futureFunctionOriginal)(pAsio, kAsioCanInputMonitor, nullptr) == ASE_SUCCESS) {
					driver->state = AsioDriver::State::Native;
					err(L"Native DM support detected.");
				}      
				
                pClassFactory->Release();
                pAsio->Release();      

                // Replace future() with our own implementation
				if (!driver->FutureFunctionReplacementThunk()) 
					err(L"The replacement function is not ready (yet)");

				DWORD oldProtect;
				VirtualProtect((void*)driver->asioDllPatchPlace, sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);
				*(uintptr_t*)driver->asioDllPatchPlace = driver->FutureFunctionReplacementThunk();
				dbg(std::format(L"Patched @{:016x} old:{:016x}, new:{:016x}", driver->asioDllPatchPlace, driver->futureFunctionOriginal, driver->FutureFunctionReplacementThunk()));

				driver->asioDllHandle = hModule;
                driver->state = AsioDriver::State::PatchOk;
            } catch (const std::exception& e) {
				driver->state = AsioDriver::State::PatchFail;
				dbg(L"Not patched.");
				if (hModule) FreeLibrary(hModule);
                continue;
            }
		}
	}
};






// ===========================================

HMODULE g_hSelf = nullptr;
std::unique_ptr<AsioDriverManager> g_driverManager = nullptr;


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) 
		g_hSelf = hModule;
	return TRUE;
}


int MyInit() {
	dbg(L"Hello");
	try {	
		// Prevent being unloaded by host
		wchar_t selfName[MAX_PATH];
		GetModuleFileNameW(g_hSelf, selfName, MAX_PATH);
		LoadLibrary(selfName); 
		// Do our job
		g_driverManager = std::make_unique<AsioDriverManager>();
		for (auto &driver : g_driverManager->drivers) 
			dbg(driver->Info());
		return 1;
	}
	catch (const std::exception& e) {
		dbg(L"Aaargh!");
		//dbg(e.what()); 
		return 0;
	}
}


int MyCleanup() {
	// No cleanup. Stay alive till the very end
	dbg(L"Bye"); 
	return 1;
}


// Cubase initialization
extern "C" __declspec(dllexport) int InitDll() {
	dbg(L"Cubase :: init");
	return MyInit();
}


// Cubase cleanup
extern "C" __declspec(dllexport) int ExitDll() {
	dbg(L"Cubase :: cleanup.");
	return MyCleanup();
}


// Studio One initialization and cleanup
extern "C" __declspec(dllexport) int CCLModuleMain(HMODULE hModule, int reason, void* pContext, void* pExtra) {
	dbg(L"Studio One :: main.");
	switch (reason) {
		case 1: return MyInit();
		case 2: return MyCleanup();
		default: return 0;
	}
}
