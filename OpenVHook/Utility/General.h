#ifndef __GENERAL_H__
#define __GENERAL_H__

#include "..\OpenVHook.h"

namespace Utility {

	std::string	GetFilename(std::string filename);
	std::string	GetFilenameWithoutExtension(std::string filename);

	std::string	GetRunningExecutableFolder();
	std::string	GetOurModuleFolder();
	std::string	GetModuleName( const HMODULE Module );
	std::string	GetModuleFullName(const HMODULE Module);
	std::string	GetModuleNameWithoutExtension( const HMODULE Module );

	void		SetOurModuleHandle( const HMODULE Module );
	HMODULE		GetOurModuleHandle();
}

#endif // __GENERAL_H__