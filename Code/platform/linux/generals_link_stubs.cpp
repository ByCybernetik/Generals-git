/*
 * Generals Linux link stubs — symbols not provided by Renegade platform layer.
 */
#include "Common/GameEngine.h"
#include "Common/StackDump.h"
#include "Common/AsciiString.h"
#include "WWDownload/download.h"
#include "WWDownload/Registry.h"
#include "WWDownload/urlBuilder.h"
#include "sdl3_host.h"

#include <cstdio>
#include <stdint.h>
#include <string.h>
#include "linux_pointer.h"

/* Empty PE resource blob (Renegade chat.res not used by Generals). */

void (*Win32_Key_Notify_Callback_Ptr)(unsigned int message, unsigned int wParam, long lParam) = NULL;

bool DX8Wrapper_IsWindowed = false;
Int DX8Wrapper_PreserveFPU = 0;

AsciiString g_LastErrorDump;

extern "C" void Renegade_Stop_Main_Loop(int exitCode)
{
	if (TheGameEngine != NULL) {
		TheGameEngine->setQuitting(TRUE);
	}
	Platform_Pre_Shutdown();
	(void)exitCode;
}

void GetFunctionDetails(void *pointer, char *name, char *filename, unsigned int *linenumber,
	uintptr_t *address)
{
	if (name) {
		snprintf(name, _MAX_PATH, "0x%p", pointer);
	}
	if (filename) {
		filename[0] = '\0';
	}
	if (linenumber) {
		*linenumber = 0;
	}
	if (address) {
		*address = POINTER_TO_UINTPTR(pointer);
	}
}

void FillStackAddresses(void **addresses, unsigned int count, unsigned int skip)
{
	(void)skip;
	if (addresses == NULL) {
		return;
	}
	for (unsigned int i = 0; i < count; ++i) {
		addresses[i] = NULL;
	}
}

void StackDumpFromAddresses(void **addresses, unsigned int count, void (*callback)(const char *))
{
	(void)addresses;
	(void)count;
	if (callback) {
		callback("(stack dump unavailable on Linux)\n");
	}
}

extern "C" int getQR2HostingStatus(void)
{
	return 0;
}

/* --- WWDownload --- */
Cftp::Cftp() {}

Cftp::~Cftp() {}

HRESULT CDownload::PumpMessages() { return DOWNLOAD_STATUSERROR; }
HRESULT CDownload::Abort() { m_Status = DOWNLOADSTATUS_DONE; return S_OK; }
HRESULT CDownload::DownloadFile(LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, bool)
{
	return DOWNLOAD_PARAMERROR;
}
HRESULT CDownload::GetLastLocalFile(char *local_file, int maxlen)
{
	if (local_file != NULL && maxlen > 0) {
		local_file[0] = '\0';
	}
	return S_OK;
}

bool GetStringFromRegistry(std::string path, std::string key, std::string &val)
{
	(void)path;
	(void)key;
	val.clear();
	return false;
}

bool GetUnsignedIntFromRegistry(std::string path, std::string key, unsigned int &val)
{
	(void)path;
	(void)key;
	val = 0;
	return false;
}

bool SetStringInRegistry(std::string path, std::string key, std::string val)
{
	(void)path;
	(void)key;
	(void)val;
	return false;
}

bool SetUnsignedIntInRegistry(std::string path, std::string key, unsigned int val)
{
	(void)path;
	(void)key;
	(void)val;
	return false;
}

void FormatURLFromRegistry(std::string &gamePatchURL, std::string &mapPatchURL, std::string &configURL,
	std::string &motdURL)
{
	gamePatchURL.clear();
	mapPatchURL.clear();
	configURL.clear();
	motdURL.clear();
}
