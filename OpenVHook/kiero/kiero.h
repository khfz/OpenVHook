#ifndef __KIERO_H__
#define __KIERO_H__

#include <stdint.h>
#include <d3d11.h>
#include <string>

#define KIERO_INCLUDE_D3D11  1 // 1 if you need D3D11 hook
#define KIERO_USE_MINHOOK    1 // 1 if you will use kiero::bind function

#define KIERO_ARCH_X64 1

typedef uint64_t uint150_t;

typedef HRESULT(__stdcall* tD3D11Present)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

namespace kiero
{
	extern ID3D11Device* device;
	extern ID3D11DeviceContext* context;
	extern IDXGISwapChain* swapChain;
	extern ID3D11RenderTargetView* renderTargetView;

	extern tD3D11Present oPresent;

	extern bool g_kieroInitialized;

	HRESULT hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);


	struct Status
	{
		enum Enum
		{
			UnknownError = -1,
			NotSupportedError = -2,
			ModuleNotFoundError = -3,

			AlreadyInitializedError = -4,
			NotInitializedError = -5,

			Success = 0,
		};
	};

	Status::Enum init();
	void shutdown();

	Status::Enum bind(uint16_t index, void** original, void* function);
	void unbind(uint16_t index);

	uint150_t* getMethodsTable();
}

#endif // __KIERO_H__