/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: ThreadUtils.cpp //////////////////////////////////////////////////////
// GameSpy thread utils
// Author: Matthew D. Campbell, July 2002

#include "PreRTS.h"	// This must go first in EVERY cpp file int the GameEngine

//-------------------------------------------------------------------------

std::wstring MultiByteToWideCharSingleLine( const char *orig )
{
	if (orig == NULL) {
		return std::wstring();
	}

	const Int byteLen = (Int)strlen(orig);
	if (byteLen <= 0) {
		return std::wstring();
	}

	/*
	 * Use an explicit UTF-8 → UTF-16 length. Do NOT construct std::wstring from a
	 * C string: libstdc++ char_traits::length calls glibc wcslen, which assumes
	 * 4-byte wchar_t and truncates -fshort-wchar (UTF-16) strings (e.g. Cyrillic
	 * "Проверка" → "Пров").
	 */
	const Int need = MultiByteToWideChar(CP_UTF8, 0, orig, byteLen, NULL, 0);
	if (need <= 0) {
		return std::wstring();
	}

	WideChar *dest = NEW WideChar[need + 1];
	Int wrote = MultiByteToWideChar(CP_UTF8, 0, orig, byteLen, dest, need);
	if (wrote < 0) {
		wrote = 0;
	}
	dest[wrote] = 0;

	for (Int i = 0; i < wrote; ++i) {
		if (dest[i] == L'\n' || dest[i] == L'\r') {
			dest[i] = L' ';
		}
	}

	std::wstring ret(dest, dest + wrote);
	delete[] dest;
	return ret;
}

std::string WideCharStringToMultiByte( const WideChar *orig )
{
	std::string ret;
	if (orig == NULL) {
		return ret;
	}

	const Int wideLen = (Int)WW_WCSTRLEN(orig);
	if (wideLen <= 0) {
		return ret;
	}

	const Int need = WideCharToMultiByte(CP_UTF8, 0, orig, wideLen, NULL, 0, NULL, NULL);
	if (need <= 0) {
		return ret;
	}

	char *dest = NEW char[need + 1];
	Int wrote = WideCharToMultiByte(CP_UTF8, 0, orig, wideLen, dest, need, NULL, NULL);
	if (wrote < 0) {
		wrote = 0;
	}
	dest[wrote] = 0;
	ret.assign(dest, dest + wrote);
	delete[] dest;
	return ret;
}

//-------------------------------------------------------------------------
