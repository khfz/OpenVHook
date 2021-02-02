#include "ScriptManager.h"
#include "ScriptEngine.h"
#include "WICTextureLoader.h"
#include "CommonStates.h"
#include "SpriteBatch.h"
#include "..\Utility\Log.h"
#include "..\Utility\General.h"
#include "..\ASI Loader\ASILoader.h"
#include "kiero/kiero.h"
#include "Types.h"
#include <wrl\wrappers\corewrappers.h>
#include <wrl\client.h>

using namespace Utility;
using namespace DirectX;
using namespace Microsoft::WRL;

#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#define DLL_EXPORT __declspec( dllexport )

enum eGameVersion;

ScriptManagerThread g_ScriptManagerThread;

static HANDLE		mainFiber;
static Script *		currentScript;

tD3D11Present kiero::oPresent = NULL;
ID3D11RenderTargetView* kiero::renderTargetView = NULL;
bool bOnce = false;

ComPtr<ID3D11ShaderResourceView> m_texture;
std::unique_ptr<CommonStates> m_states;
ComPtr<ID3D11ShaderResourceView> texture; 

int drawtime;
XMFLOAT2 pos;
SimpleMath::Color color;
float Rotation; 
XMFLOAT2 center;
float scaleFactor;
bool draw = false;
float drawlevel = 0.0f;

unsigned int last_call_time;
bool elapsedtime = false;

std::unique_ptr<SpriteBatch> spriteBatch;
std::map<int, ComPtr<ID3D11ShaderResourceView>> idmap;
int textureId = -1; 

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
		HMODULE Module = LoadLibraryA(scriptName.c_str());
		if (Module) {
			LOG_PRINT("\tLoaded \"%s\" => 0x%p", scriptName.c_str(), Module);
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

void ScriptManagerThread::AddScript( HMODULE Module, void( *fn )( ) ) {

	const std::string moduleName = GetModuleFullName( Module );
	const std::string shortName = GetFilename(moduleName);

	if (m_scripts.find( Module ) == m_scripts.end())	
		LOG_PRINT("Registering script '%s' (0x%p)", shortName.c_str(), fn);
	else 
		LOG_PRINT("Registering additional script thread '%s' (0x%p)", shortName.c_str(), fn);

	if ( find(m_scriptNames.begin(), m_scriptNames.end(), 
		moduleName ) == m_scriptNames.end() )
	{
		m_scriptNames.push_back( moduleName );
	}

	m_scripts[Module].push_back(std::make_shared<Script>( fn, shortName ));
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

void ScriptManagerThread::RemoveScript( HMODULE Module ) {

	std::unique_lock<std::mutex> lock(mutex);

	auto pair = m_scripts.find( Module );
	if ( pair == m_scripts.end() ) {

		LOG_ERROR( "Could not find script for module 0x%p", Module );
		return;
	}

	LOG_PRINT( "Unregistered script '%s'", GetModuleNameWithoutExtension( Module ).c_str() );
	m_scripts.erase( pair );
}

void DLL_EXPORT scriptWait(DWORD Time) {

	currentScript->Yield( Time );
}

void DLL_EXPORT scriptRegister( HMODULE Module, void( *function )( ) ) {

	g_ScriptManagerThread.AddScript( Module, function );
}

void DLL_EXPORT scriptRegisterAdditionalThread(HMODULE Module, void(*function)()) {

	g_ScriptManagerThread.AddScript(Module, function);
}

void DLL_EXPORT scriptUnregister( void( *function )( ) ) {

	g_ScriptManagerThread.RemoveScript( function );
}

void DLL_EXPORT scriptUnregister( HMODULE Module ) {
	
	g_ScriptManagerThread.RemoveScript( Module );
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

			LOG_ERROR( "Error in nativeCall");
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

typedef void(*PresentCallback)(void*);

static std::set<PresentCallback> g_PresentCallback;

DLL_EXPORT void presentCallbackRegister(PresentCallback cb) {
	g_PresentCallback.insert(cb);
}

DLL_EXPORT void presentCallbackUnregister(PresentCallback cb) {
	g_PresentCallback.erase(cb);
}

DLL_EXPORT int createTexture(const char* fileName) {
	//convert fileName to a wchar_t
	size_t size = strlen(fileName) + 1;
	size_t convertedChars;
	wchar_t* temp = new wchar_t[size];
	mbstowcs_s(&convertedChars, temp, size, fileName, _TRUNCATE);

	//Create texture using WIC
	HRESULT Status = CreateWICTextureFromFile(kiero::device, kiero::context, temp, nullptr, m_texture.ReleaseAndGetAddressOf());
	if (FAILED(Status)) {
		LOG_ERROR("Failed to create texture");
	}

	textureId++;
	idmap.insert(std::pair<int, ComPtr<ID3D11ShaderResourceView>>(textureId, m_texture));
	LOG_PRINT("Creating Texture %s, id %i", fileName, textureId);

	return textureId;
}

HRESULT __stdcall kiero::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if(kiero::g_kieroInitialized == true) {
	m_states = std::make_unique<CommonStates>(kiero::device);
	if (!bOnce)
	{
		HWND hWindow = NULL;
		while (!hWindow)
		{
			hWindow = FindWindow("grcWindow", NULL);
			Sleep(100);
		}

		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&kiero::device)))
		{
			pSwapChain->GetDevice(__uuidof(kiero::device), (void**)&kiero::device);
			kiero::device->GetImmediateContext(&kiero::context);
		}

		ID3D11Texture2D* renderTargetTexture = nullptr;
		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&renderTargetTexture))))
		{
			kiero::device->CreateRenderTargetView(renderTargetTexture, NULL, &kiero::renderTargetView);
			renderTargetTexture->Release();
		}

		bOnce = true;


	}
	if (draw == true) {
		spriteBatch->Begin(SpriteSortMode_BackToFront, m_states->NonPremultiplied());
		spriteBatch->Draw(texture.Get(), pos, nullptr, color, Rotation, center, scaleFactor, SpriteEffects_None, 1.0F);
		spriteBatch->End();
		kiero::context->OMSetRenderTargets(1, &kiero::renderTargetView, NULL);
		//draw = false;
	}
}
	return oPresent(pSwapChain, SyncInterval, Flags);
}

DLL_EXPORT void drawTexture(int id, int index, int level, int time,
	float sizeX, float sizeY, float centerX, float centerY,
	float posX, float posY, float rotation, float screenHeightScaleFactor,
	float r, float g, float b, float a) {

	texture = idmap[id];
	drawtime = time;
	pos = XMFLOAT2(posX, posY);
	color = SimpleMath::Color(r, g, b, a);
	Rotation = rotation;
	center = XMFLOAT2(centerX, centerY);
	scaleFactor = screenHeightScaleFactor;
	last_call_time = timeGetTime();
	bool draw = true;

}
