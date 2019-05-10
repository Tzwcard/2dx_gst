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