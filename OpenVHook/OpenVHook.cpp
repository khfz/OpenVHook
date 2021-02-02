#include "Input\InputHook.h"
#include "ASI Loader\ASILoader.h"
#include "Scripting\ScriptEngine.h"
#include "Scripting\ScriptManager.h"
#include "Utility\Console.h"
#include "Utility\General.h"
#include "Utility\Pattern.h"
#include "Utility\Thread.h"
#include "kiero/kiero.h"
#include "minhook/include/MinHook.h"

using namespace Utility;

#pragma comment(lib, "libMinHook.x64.lib")

std::string KieroErrorString(int ErrorNum) {
	switch (ErrorNum) {
	case 0:
		return "Kiero: Success";
	case -1:
		return  "[Error] Kiero: Unknown Error";
	case -2:
		return  "[Error] Kiero: Not Supported";
	case -3:
		return  "[Error] Kiero: Module Not Found";
	case -4:
		return  "[Error] Kiero: Already Initialized";
	case -5:
		return  "[Error] Kiero: Not Initialized";
	default:
		return  "[Error] Kiero: Unknown Error";
	}
};


static Thread mainThread = Thread([](ThreadState*) {

	if (GetTickCount() - keyboardState[VK_CONTROL].lastUpTime < 5000 && !keyboardState[VK_CONTROL].isUpNow &&
		GetTickCount() - keyboardState[0x52].lastUpTime < 100 && keyboardState[0x52].isUpNow)
	{
		keyboardState[0x52].lastUpTime = 0;

		if (g_ScriptManagerThread.LoadScripts())
		{
			int count = 3;
			while (count > 0)
			{
				MessageBeep(0);
				Sleep(200);
				--count;
			}
		}

		else
		{
			LOG_PRINT("Unloading .asi plugins...");

			g_ScriptManagerThread.FreeScripts();

			MessageBeep(0);
		}
	}
});

static Thread initThread = Thread([](ThreadState* ts) {
	ts->shouldExit = 1;

#ifdef _DEBUG
	GetConsole()->Allocate();
#endif

	MH_Initialize();

	LOG_PRINT( "Initializing..." );

	if ( !InputHook::Initialize() ) {

		LOG_ERROR( "Failed to initialize InputHook" );
		return;
	}

	if ( !ScriptEngine::Initialize() ) {

		LOG_ERROR( "Failed to initialize ScriptEngine" );
		return;
	}

	ScriptEngine::CreateThread( &g_ScriptManagerThread );

#ifdef _DEBUG
	ASILoader::Initialize();
#endif
	LOG_PRINT(KieroErrorString(kiero::init()).c_str());

	LOG_PRINT(KieroErrorString(kiero::bind(8, (void**)&kiero::oPresent, kiero::hkPresent)).c_str());

	// If you just need to get the function address you can use the kiero::getMethodsTable function
	kiero::oPresent = (tD3D11Present)kiero::getMethodsTable()[8];
	if (kiero::oPresent == NULL) {
		LOG_ERROR("Failed to attach Present hook");
	}
	else {
		LOG_PRINT("Present hook attached: Present 0x%p", (DWORD_PTR)kiero::oPresent);
	}

	LOG_PRINT( "Initialization finished" );

	mainThread.Run();
});

void Cleanup() {

	LOG_PRINT( "Cleanup" );

	initThread.Exit();

	mainThread.Exit();

	kiero::shutdown();

	InputHook::Remove();

	if ( GetConsole()->IsAllocated() ) {
		GetConsole()->DeAllocate();
	}
}

BOOL APIENTRY DllMain( HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved ) {

	switch ( dwReason ) {
		case DLL_PROCESS_ATTACH: {
			SetOurModuleHandle(hModule);
			initThread.Run();
			DisableThreadLibraryCalls(hModule);
			break;
		}
		case DLL_PROCESS_DETACH: {

			Cleanup();
			break;
		}
	}

	return TRUE;
}
