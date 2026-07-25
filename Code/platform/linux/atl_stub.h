#ifndef RENEGADE_ATL_STUB_H
#define RENEGADE_ATL_STUB_H

#include <windows.h>

class CComModule
{
public:
	void Init(void *, HINSTANCE) {}
	void Term(void) {}
};

#endif
