#include "misc.h"

#include <iostream>
#include <cstdarg>
#include <Windows.h>

void SetTitle(char *format, ...)
{
	char title[256] = { 0 };

	va_list arg;

	va_start(arg, format);
	vsprintf_s(title, format, arg);
	va_end(arg);

	SetConsoleTitleA(title);
}

static HANDLE logfile = INVALID_HANDLE_VALUE;
static HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
static char log_code[] = "DFWIMA";
static int log_level_value = 4;

static BOOL log_writer(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite) {
	DWORD nNumberOfBytesWritten;

	// Write file
	if (hFile != INVALID_HANDLE_VALUE) {
		WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &nNumberOfBytesWritten, NULL);
		FlushFileBuffers(hFile);
	}

	// Write stdout
	if (std_out != INVALID_HANDLE_VALUE) {
		WriteFile(std_out, lpBuffer, nNumberOfBytesToWrite, &nNumberOfBytesWritten, NULL);
		FlushFileBuffers(std_out);
	}

	return TRUE;
}

void logger_set_level(int input) {
	log_level_value = input > 4 ? 4 : (input < 0 ? 0 : input);
}

static void log(int code, const char* module, const char* format, va_list ArgList)
{
	if (log_level_value >= code)
	{
		char log_data[2052] = { 0 }, * ptr_data;
		int sz = _snprintf_s(log_data, 0x800, "%c: %s: ", log_code[code], module == NULL ? "--" : module);
		sz = sz < 0 ? 0 : sz;

		ptr_data = &log_data[sz];
		_vsnprintf_s(ptr_data, 0x800 - sz, 0x800 - sz, format, ArgList);

		sz = strlen(log_data);
		log_data[sz] = '\n'; log_data[sz + 1] = 0;

		log_writer(logfile, log_data, sz + 1);
	}
}

void log_body_fatal(const char* module, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	log(1, module, format, args);
	va_end(args);
	exit(-1);
}

void log_body_warning(const char* module, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	log(2, module, format, args);
	va_end(args);
}

void log_body_info(const char* module, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	log(3, module, format, args);
	va_end(args);
}

void log_body_misc(const char* module, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	log(4, module, format, args);
	va_end(args);
}