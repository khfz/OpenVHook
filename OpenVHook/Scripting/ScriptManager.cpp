#include "ScriptManager.h"
#include "ScriptEngine.h"
#include "Hook.h"
#include "WICTextureLoader.h"
#include "CommonStates.h"
#include "SpriteBatch.h"
#include "..\Utility\Log.h"
#include "..\Utility\General.h"
#include "..\ASI Loader\ASILoader.h"
#include "Types.h"
#include <stdio.h>
#include <cstdlib>
#include <chrono>
#include <wrl\wrappers\corewrappers.h>
#include <wrl\client.h>

using namespace Utility;
using namespace DirectX;
using namespace Hook;
using namespace Microsoft::WRL;

#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#define DLL_EXPORT __declspec( dllexport )

enum eGameVersion;

ScriptManagerThread g_ScriptManagerThread;

static HANDLE		mainFiber;
static Script *		currentScript;

ID3D11Device* Hook::pDevice = NULL;
ID3D11DeviceContext* Hook::pContext = NULL;
IDXGISwapChain* Hook::pSwapChain = NULL;
ID3D11RenderTargetView* Hook::pRenderTargetView = NULL;

static ComPtr<ID3D11ShaderResourceView> m_texture;
static ComPtr<ID3D11Resource> resource;
static std::unique_ptr<CommonStates> m_states;

static int textureId = 0; 

std::mutex mutex;

int ExceptionHandler(int type, PEXCEPTION_POINTERS ex) {
	LOG_ERROR("Caught exception 0x%X", type);
	LOG_ERROR("Exception address 0x%p", ex->ExceptionRecord->ExceptionAddress);

	return EXCEPTION_EXECUTE_HANDLER;
}

void Script::Tick() {

	if ( mainFiber == nullptr ) {
		mainFiber = ConvertThreadToFiber( nullptr );
	}

	if ( timeGetTime() < wakedAt ) {
		return;
	}

	if ( scriptFiber ) {

		currentScript = this;
		SwitchToFiber( scriptFiber );
		currentScript = nullptr;
	}

	else if (ScriptEngine::GetGameState() == GameStatePlaying) {

		scriptFiber = CreateFiber(NULL, [](LPVOID handler) {
			__try {
				LOG_PRINT("Launching script %s", reinterpret_cast<Script*>(handler)->name.c_str());
				reinterpret_cast<Script*>( handler )->Run();
			} __except (ExceptionHandler(GetExceptionCode(), GetExceptionInformation())) {
				LOG_ERROR("Error in script->Run");
			}
		}, this );
	}

}

void Script::Run() {

	callbackFunction();
}

void Script::Yield( uint32_t time ) {

	wakedAt = timeGetTime() + time;
	SwitchToFiber( mainFiber );
}

void ScriptManagerThread::DoRun() {

	std::unique_lock<std::mutex> lock(mutex);

	scriptMap thisIterScripts( m_scripts );

	for ( auto & pair : thisIterScripts ) {
		for ( auto & script : pair.second ) {
			script->Tick();
		}	
	}
}

eThreadState ScriptManagerThread::Reset( uint32_t scriptHash, void * pArgs, uint32_t argCount ) {

	// Collect all scripts
	scriptMap tempScripts;

	for ( auto && pair : m_scripts ) {
		tempScripts[pair.first] = pair.second;
	}

	// Clear the scripts
	m_scripts.clear();

	// Start all scripts
	for ( auto && pair : tempScripts ) {
		for ( auto & script : pair.second ) {
			AddScript( pair.first, script->GetCallbackFunction() );
		}
	}

	return ScriptThread::Reset( scriptHash, pArgs, argCount );
}

bool ScriptManagerThread::LoadScripts() {

	if (!m_scripts.empty()) return false;

	// load known scripts
	for (auto && scriptName : m_scriptNames)
	{
		LOG_PRINT("Loading \"%s\"", scriptName.c_str());
		HMODULE module = LoadLibraryA(scriptName.c_str());
		if (module) {
			LOG_PRINT("\tLoaded \"%s\" => 0x%p", scriptName.c_str(), module);
		} else {
			LOG_DEBUG("\tSkip \"%s\"", scriptName.c_str());
		}
	}

	// ASILoader::Initialize only load new DLLs if called multiple times.
	ASILoader::Initialize();

	return true;
}

void ScriptManagerThread::FreeScripts() {

	scriptMap tempScripts;

	for (auto && pair : m_scripts) {
		tempScripts[pair.first] = pair.second;
	}

	for (auto && pair : tempScripts) {
		FreeLibrary( pair.first );
	}

	m_scripts.clear();
}

void ScriptManagerThread::AddScript( HMODULE module, void( *fn )( ) ) {

	const std::string moduleName = GetModuleFullName( module );
	const std::string shortName = GetFilename(moduleName);

	if (m_scripts.find( module ) == m_scripts.end())	
		LOG_PRINT("Registering script '%s' (0x%p)", shortName.c_str(), fn);
	else 
		LOG_PRINT("Registering additional script thread '%s' (0x%p)", shortName.c_str(), fn);

	if ( find(m_scriptNames.begin(), m_scriptNames.end(), 
		moduleName ) == m_scriptNames.end() )
	{
		m_scriptNames.push_back( moduleName );
	}

	m_scripts[module].push_back(std::make_shared<Script>( fn, shortName ));
}

void ScriptManagerThread::RemoveScript( void( *fn )( ) ) {

	for (auto & pair : m_scripts) {
		for (auto script : pair.second) {
			if (script->GetCallbackFunction() == fn) {

				RemoveScript(pair.first);

				break;
			}
		}
	}
}

void ScriptManagerThread::RemoveScript( HMODULE module ) {

	std::unique_lock<std::mutex> lock(mutex);

	auto pair = m_scripts.find( module );
	if ( pair == m_scripts.end() ) {

		LOG_ERROR( "Could not find script for module 0x%p", module );
		return;
	}

	LOG_PRINT( "Unregistered script '%s'", GetModuleNameWithoutExtension( module ).c_str() );
	m_scripts.erase( pair );
}

void DLL_EXPORT scriptWait(DWORD Time) {

	currentScript->Yield( Time );
}

void DLL_EXPORT scriptRegister( HMODULE module, void( *function )( ) ) {

	g_ScriptManagerThread.AddScript( module, function );
}

void DLL_EXPORT scriptRegisterAdditionalThread(HMODULE module, void(*function)()) {

	g_ScriptManagerThread.AddScript(module, function);
}

void DLL_EXPORT scriptUnregister( void( *function )( ) ) {

	g_ScriptManagerThread.RemoveScript( function );
}

void DLL_EXPORT scriptUnregister( HMODULE module ) {
	
	g_ScriptManagerThread.RemoveScript( module );
}

eGameVersion DLL_EXPORT getGameVersion() {

	return (eGameVersion)gameVersion;
}

static ScriptManagerContext g_context;
static uint64_t g_hash;

void DLL_EXPORT nativeInit(UINT64 hash ) {

	g_context.Reset();
	g_hash = hash;
}

void DLL_EXPORT nativePush64(UINT64 value ) {

	g_context.Push( value );
}

DLL_EXPORT uint64_t * nativeCall() {

	auto fn = ScriptEngine::GetNativeHandler( g_hash );

	if ( fn != 0 ) {

		__try {

			fn( &g_context );
            scrNativeCallContext::SetVectorResults(&g_context);
		} __except ( EXCEPTION_EXECUTE_HANDLER ) {

			LOG_ERROR( "Error in nativeCall" );
		}
	}

	return reinterpret_cast<uint64_t*>( g_context.GetResultPointer() );
}

typedef void(*KeyboardHandler)(DWORD, WORD, BYTE, BOOL, BOOL, BOOL, BOOL);

static std::set<KeyboardHandler> g_keyboardFunctions;

void DLL_EXPORT keyboardHandlerRegister(KeyboardHandler function ) {

	g_keyboardFunctions.insert( function );
}

void DLL_EXPORT keyboardHandlerUnregister(KeyboardHandler function ) {

	g_keyboardFunctions.erase( function );
}

void ScriptManager::HandleKeyEvent(DWORD key, WORD repeats, BYTE scanCode, BOOL isExtended, BOOL isWithAlt, BOOL wasDownBefore, BOOL isUpNow) {

	auto functions = g_keyboardFunctions;

	for (auto & function : functions) {
		function(key, repeats, scanCode, isExtended, isWithAlt, wasDownBefore, isUpNow);
	}
}

DLL_EXPORT uint64_t* getGlobalPtr(int index)
{
	return (uint64_t*)globalTable.AddressOf(index);
}

BYTE DLL_EXPORT *getScriptHandleBaseAddress(int handle) {

	if (handle != -1)
	{
		int index = handle >> 8;

		auto entityPool = pools.GetEntityPool();

		if (index < entityPool->m_count && entityPool->m_bitMap[index] == (handle & 0xFF))
		{
			auto result = entityPool->m_pData[index];

			return (BYTE*)result.m_pEntity;
		}
	}

	return NULL;
}

int DLL_EXPORT worldGetAllVehicles(int* array, int arraySize) {

	int index = 0;

	auto vehiclePool = pools.GetVehiclePool();

	for (auto i = 0; i < vehiclePool->m_count; i++)
	{
		if (i >= arraySize) break;

		if (vehiclePool->m_bitMap[i] >= 0)
		{
			array[index++] = (i << 8) + vehiclePool->m_bitMap[i];
		}
	}

	return index;
}

int DLL_EXPORT worldGetAllPeds(int* array, int arraySize) {

	int index = 0;

	auto pedPool = pools.GetPedPool();

	for (auto i = 0; i < pedPool->m_count; i++)
	{
		if (i >= arraySize) break;

		if (pedPool->m_bitMap[i] >= 0)
		{
			array[index++] = pedPool->getHandle(i);
		}
	}

	return index;
}

int DLL_EXPORT worldGetAllObjects(int* array, int arraySize) {

	int index = 0;

	auto objectPool = pools.GetObjectsPool();

	for (auto i = 0; i < objectPool->m_count; i++)
	{
		if (i >= arraySize) break;

		if (objectPool->m_bitMap[i] >= 0)
		{
			array[index++] = objectPool->getHandle(i);
		}
	}

	return index;
}

int DLL_EXPORT worldGetAllPickups(int* array, int arraySize) {

	int index = 0;

	auto pickupPool = pools.GetPickupsPool();

	for (auto i = 0; i < pickupPool->m_count; i++)
	{
		if (i >= arraySize) break;

		if (pickupPool->m_bitMap[i] >= 0)
		{
			array[index++] = pickupPool->getHandle(i);
		}
	}

	return index;
}

void Script::Start()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) {
		LOG_ERROR("Failure to Intialize COM Libary");
	}

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

	HWND hWindow = FindWindowA(NULL, "Window"); // TODO: Modify this.

#pragma region Initialise DXGI_SWAP_CHAIN_DESC
	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(scd));

	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // sets color formatting, we are using RGBA
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // says what we are doing with the buffer
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // msdn explains better than i can: https://msdn.microsoft.com/en-us/library/windows/desktop/bb173076(v=vs.85).aspx
	scd.OutputWindow = hWindow; // our gamewindow, obviously
	scd.SampleDesc.Count = 1; // Set to 1 to disable multisampling
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // D3D related stuff, cant really describe what it does
	scd.Windowed = ((GetWindowLongPtr(hWindow, GWL_STYLE) & WS_POPUP) != 0) ? false : true; // check if our game is windowed
	scd.BufferDesc.Width = 1920; // temporary width
	scd.BufferDesc.Height = 1080; // temporary height
	scd.BufferDesc.RefreshRate.Numerator = 144; // refreshrate in Hz
	scd.BufferDesc.RefreshRate.Denominator = 1; // no clue, lol
#pragma endregion

	if (FAILED(D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
		NULL, &featureLevel, 1, D3D11_SDK_VERSION,
		&scd, &Hook::pSwapChain,
		&Hook::pDevice, NULL, &Hook::pContext
	)))
	{// failed to create D3D11 device
		return;
	}

	//Get VTable Pointers
	DWORD_PTR* pSwapChainVT = reinterpret_cast<DWORD_PTR*>(Hook::pSwapChain);
	DWORD_PTR* pDeviceVT = reinterpret_cast<DWORD_PTR*>(Hook::pDevice); // Device not needed, but prolly need it to draw stuff in Present, so it is included
	DWORD_PTR* pContextVT = reinterpret_cast<DWORD_PTR*>(Hook::pContext);

	//Pointer->Table
	pSwapChainVT = reinterpret_cast<DWORD_PTR*>(pSwapChainVT[0]);
	pDeviceVT = reinterpret_cast<DWORD_PTR*>(pDeviceVT[0]);
	pContextVT = reinterpret_cast<DWORD_PTR*>(pContextVT[0]);

	Hook::oPresent = reinterpret_cast<tD3D11Present>(pSwapChainVT[8]); // Present Function

	//Hook using Detour
	Hook::HookFunction(reinterpret_cast<PVOID*>(&Hook::oPresent), Hook::D3D11Present);
}

typedef void(*PresentCallback)(void*);

DLL_EXPORT void presentCallbackRegister(PresentCallback cb) {
	static bool flag_warn_presentCallbackRegister = true;
	if (flag_warn_presentCallbackRegister)
		LOG_WARNING("plugin is trying to use presentCallbackRegister");
	flag_warn_presentCallbackRegister = false;
	
}

DLL_EXPORT void presentCallbackUnregister(PresentCallback cb) {
	static bool flag_warn_presentCallbackUnregister = true;
	if (flag_warn_presentCallbackUnregister)
		LOG_WARNING("plugin is trying to use presentCallbackUnregister");
	flag_warn_presentCallbackUnregister = false;
}

DLL_EXPORT int createTexture(const char* fileName) {

	//convert fileName to a wchar_t
	size_t size = strlen(fileName) + 1;
	size_t convertedChars;
	wchar_t* temp = new wchar_t[size];
	mbstowcs_s(&convertedChars, temp, size, fileName, _TRUNCATE);

	//Create texture using WIC
	HRESULT Status = CreateWICTextureFromFile(pDevice, pContext, temp, resource.GetAddressOf(), m_texture.ReleaseAndGetAddressOf());
	if (FAILED(Status)) {
		LOG_ERROR("Failed to create texture");
	}

	ComPtr<ID3D11Texture2D> texture;
	DX::ThrowIfFailed(resource.As(&texture));

	CD3D11_TEXTURE2D_DESC textureDesc;
	texture->GetDesc(&textureDesc);

	if (textureId != 0) {
		textureId++;
	}
	LOG_PRINT("Creating Texture", fileName, ", id", textureId); 

	return textureId;
}

DLL_EXPORT void drawTexture(int id, int index, int level, int time,
	float sizeX, float sizeY, float centerX, float centerY,
	float posX, float posY, float rotation, float screenHeightScaleFactor,
	float r, float g, float b, float a) {

	m_states = std::make_unique<CommonStates>(pDevice);
	std::unique_ptr<SpriteBatch> spriteBatch;
	spriteBatch = std::make_unique<SpriteBatch>(pContext);

	spriteBatch->Begin(SpriteSortMode_Deferred, m_states->NonPremultiplied());
	spriteBatch->Draw(m_texture.Get(), XMFLOAT2(posX, posY), nullptr, SimpleMath::Color(r, g, b, a), rotation, XMFLOAT2(centerX, centerY), screenHeightScaleFactor);
	spriteBatch->End();

	//count time since drawn using chrono
	bool elapsedtime = false; 
	while (elapsedtime != true) {
		std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		if (time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) {
			elapsedtime = true;
		}
	}
}
