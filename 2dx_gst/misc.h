#pragma once

void SetTitle(char *format, ...);

void log_body_misc(const char* module, const char* format, ...);
void log_body_info(const char* module, const char* format, ...);
void log_body_warning(const char* module, const char* format, ...);
void log_body_fatal(const char* module, const char* format, ...);
void logger_set_level(int input);

#define _logm(...) log_body_misc(__MODULE__, __VA_ARGS__)
#define _logi(...) log_body_info(__MODULE__, __VA_ARGS__)
#define _logw(...) log_body_warning(__MODULE__, __VA_ARGS__)
#define _logf(...) log_body_fatal(__MODULE__, __VA_ARGS__)