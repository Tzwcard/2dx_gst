#pragma once

#define WINVER _WIN32_WINNT_WIN7

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include <mferror.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

int wma_helper_initialize(void);
int wma_to_waveform(unsigned char* wma_data, int size_wma_data, unsigned char* wave_data, unsigned char* ch);
void wma_helper_uninitialize(void);