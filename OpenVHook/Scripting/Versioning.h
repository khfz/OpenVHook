#include "NativeHashMap.h"
#include "..\Utility\Log.h"

#include <iostream>
#include <string>

static std::vector<std::string> GameVersionString = {
    "VER_1_0_335_2_STEAM",
    "VER_1_0_335_2_NOSTEAM",

    "VER_1_0_350_1_STEAM",
    "VER_1_0_350_2_NOSTEAM",

    "VER_1_0_372_2_STEAM",
    "VER_1_0_372_2_NOSTEAM",

    "VER_1_0_393_2_STEAM",
    "VER_1_0_393_2_NOSTEAM",
    "VER_1_0_393_4_STEAM",
    "VER_1_0_393_4_NOSTEAM",

    "VER_1_0_463_1_STEAM",
    "VER_1_0_463_1_NOSTEAM",

    "VER_1_0_505_2_STEAM",
    "VER_1_0_505_2_NOSTEAM",

    "VER_1_0_573_1_STEAM",
    "VER_1_0_573_1_NOSTEAM",

    "VER_1_0_617_1_STEAM",
    "VER_1_0_617_1_NOSTEAM",

    "VER_1_0_678_1_STEAM",
    "VER_1_0_678_1_NOSTEAM",

    "VER_1_0_757_2_STEAM",
    "VER_1_0_757_2_NOSTEAM",

    "VER_1_0_757_4_STEAM",
    "VER_1_0_757_4_NOSTEAM",

    "VER_1_0_791_2_STEAM",
    "VER_1_0_791_2_NOSTEAM",

    "VER_1_0_877_1_STEAM",
    "VER_1_0_877_1_NOSTEAM",

    "VER_1_0_944_2_STEAM",
    "VER_1_0_944_2_NOSTEAM",

    "VER_1_0_1011_1_STEAM",
    "VER_1_0_1011_1_NOSTEAM",

    "VER_1_0_1032_1_STEAM",
    "VER_1_0_1032_1_NOSTEAM",

    "VER_1_0_1103_2_STEAM",
    "VER_1_0_1103_2_NOSTEAM",

    "VER_1_0_1180_2_STEAM",
    "VER_1_0_1180_2_NOSTEAM",

    "VER_1_0_1290_1_STEAM",
    "VER_1_0_1290_1_NOSTEAM",

    "VER_1_0_1365_1_STEAM",
    "VER_1_0_1365_1_NOSTEAM",

    "VER_1_0_1493_0_STEAM",
    "VER_1_0_1493_0_NOSTEAM",

    "VER_1_0_1493_1_STEAM",
    "VER_1_0_1493_1_NOSTEAM",

    "VER_1_0_1604_0_STEAM",
    "VER_1_0_1604_0_NOSTEAM",

    "VER_1_0_1604_1_STEAM",
    "VER_1_0_1604_1_NOSTEAM",

    "VER_1_0_1737_0_STEAM",
    "VER_1_0_1737_0_NOSTEAM",

    "VER_1_0_1737_6_STEAM",
    "VER_1_0_1737_6_NOSTEAM",

    "VER_1_0_1868_0_STEAM",
    "VER_1_0_1868_0_NOSTEAM",

    "VER_1_0_1868_1_STEAM",
    "VER_1_0_1868_1_NOSTEAM",

    "VER_1_0_1868_4_EGS",

	"VER_1_0_2060_0_STEAM",
	"VER_1_0_2060_0_NOSTEAM",

	"VER_1_0_2060_1_STEAM",
	"VER_1_0_2060_1_NOSTEAM",

	"VER_1_0_2189_0_STEAM",
	"VER_1_0_2189_0_NOSTEAM",
};

int ScriptEngine::GetGameVersion()
{
	LPVOID pModule = GetModuleHandleA(NULL);

	DWORD codeSig = *(DWORD*)((DWORD64)pModule + 0x870000);

	switch (codeSig)
	{
	case 0xE8012024:
		return 0;
	case 0xA29410:
		return 1;
	case 0x7D2205FF:
		return 2;
	case 0x1:
		return 3;
	case 0x1ECB9:
		return 4;
	case 0x100FF360:
		return 5;
	case 0x8B48FF79:
		return 7;
	case 0xC4834800:
		return 9;
	case 0xF000001:
		return 10;
	case 0xC86E0F66:
		return 11;
	case 0x57085889:
		return 12;
	case 0x28C48348:
		return 13;
	case 0x4DE2E800:
		return 14;
	case 0x8948C88B:
		return 15;
	case 0xF4397715:
		return 16;
	case 0x48FFF41E:
		return 17;
	case 0x36CB0305:
		return 18;
	case 0xB95A0589:
		return 19;
	case 0x8B48C88B:
		return 20;
	case 0xE80C75D2:
		return 21;
	case 0x137978C:
		return 23;
	case 0xB86AE800:
		return 24;
	case 0x75C68441:
		return 27;
	case 0x828B1C74:
		return 28;
	case 0xD8B4800:
		return 29;
	case 0x3C244C10:
		return 30;
	case 0xB2F4E30D:
		return 31;
	case 0x89587500:
		return 35;
	case 0xC4834801:
		return 36;
	case 0xF36C5010:
		return 37;
	case 0x83483024:
		return 38;
	case 0x2C0EB25:
		return 40;
	case 0x8B484874:	// 1.0.1868.0 STEAM
		return 54;
	case 0xA0C18148:    // 1.0.1868.4 EPIC
		return 58;
	case 0x02:			// Steam Placeholder
		return 59;
	case 0x0:			// 1.0.2060.0 NONSTEAM
		return 62;
	case 59:			// 1.0.2060.1 STEAM Placeholder
		return 61;
	case 60:			// 1.0.2060.1 NONSTEAM
		return 62;
	case 61:			// 1.0.2189.0 STEAM Placeholder
		return 63;
	case 0xC1000000:	// 1.0.2189.0 NONSTEAM
		return 64;
#ifdef _DEBUG
	default:
		return codeSig;
#else
	default:
		return -1;
#endif
	}
}

int ScriptEngine::GameVersionToSearchDepth(int version)
{
	switch (version) {
	case 0:
	case 1:
		return 0;
	case 2:
	case 3:
		return 1;
	case 4:
	case 5:
		return 2;
	case 6:
	case 7:
	case 8:
	case 9:
		return 3;
	case 10:
	case 11:
		return 4;
	case 12:
	case 13:
		return 5;
	case 14:
	case 15:
		return 6;
	case 16:
	case 17:
		return 7;
	case 18:
	case 19:
		return 8;
	case 20:
	case 21:
	case 22:
	case 23:
		return 9;
	case 24:
	case 25:
		return 10;
	case 26:
	case 27:
		return 11;
	case 28:
	case 29:
		return 12;
	case 30:
	case 31:
	case 32:
	case 33:
		return 13;
	case 34:
	case 35:
		return 14;
	case 36:
	case 37:
		return 15;
	case 38:
	case 39:
		return 16;
	case 40:
	case 41:
		return 17;
	case 42:
	case 43:
	case 44:
	case 45:
		return 18;
	case 46:
	case 47:
	case 48:
	case 49:
		return 19;
	case 50:
	case 51:
	case 52:
	case 53:
		return 20;
	case 54:
	case 55:
	case 58:
		return 21;
	case 59:
	case 60:
	case 61:
	case 62:
		return 22;
	case 63:
	case 64:
		return 23;

	default:
		return fullHashMapDepth - 1;
	}
}