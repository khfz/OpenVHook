/*
 * Modified from https://github.com/citizenfx/fivem/blob/master/code/client/shared/Hooking.h
 */
#pragma once

#include <stdint.h>

#include <memory>
#include <functional>

namespace hooking
{
	ptrdiff_t baseAddressDifference;

	// returns the adjusted address to the stated base
	template<typename T>
	inline uintptr_t get_adjusted(T address)
	{
		if ((uintptr_t)address >= 0x140000000 && (uintptr_t)address <= 0x146000000)
		{
			return (uintptr_t)address + baseAddressDifference;
		}

		return (uintptr_t)address;
	}

	template<typename T, typename TAddr>
	inline T get_address(TAddr address)
	{
		intptr_t target = *(int32_t*)(get_adjusted(address));
		target += (get_adjusted(address) + 4);

		return (T)target;
	}
}