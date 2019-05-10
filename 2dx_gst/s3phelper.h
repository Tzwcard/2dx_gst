#pragma once

#include <stdint.h>
#include <Windows.h>

int read_s3p(char *s3p_path, FARPROC wma_read_callback);