#pragma once

#include <d3d12.h>
#include "../Application/Types.h"
#include "../Utils/Source/Log.h"

#define KILOBYTE 1024
#define MEGABYTE 1024*KILOBYTE
#define GIGABYTE 1024*MEGABYTE


template<class T>
T AlignOffset(const T& uOffset, const T& uAlign) { return ((uOffset + (uAlign - 1)) & ~(uAlign - 1)); }

template<class... Args>
void SetName(ID3D12Object* pObj, const char* format, Args&&... args)
{
	char bufName[240];
	sprintf_s(bufName, format, args...);
	std::string Name = bufName;
	std::wstring wName(Name.begin(), Name.end());
	pObj->SetName(wName.c_str());
}
