#ifndef INTERCEPTOR_DEBUG_C_INCL
#define INTERCEPTOR_DEBUG_C_INCL

#include "interceptor_pragmas.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "interceptor_conf.c"


////// Debug messages:

// TODO: I guess these are all reserved names.

#define _DEFAULT_LOG_FILE stderr

#define _DEBUG_PRINT_L_unique(ID, LEVEL, ...) \
	if (debug_level() >= LEVEL) { \
		FILE* _log_file##ID = open_log_file(); \
		fprintf(_log_file##ID, "%s", log_prefix()); \
		fprintf(_log_file##ID, "DEBUG: "); \
		fprintf(_log_file##ID, __VA_ARGS__); \
		if (_log_file##ID != _DEFAULT_LOG_FILE) { \
			fclose(_log_file##ID); \
		} \
	}

#define _DEBUG_PRINT_L_unique_expander(ID, LEVEL, ...) \
	_DEBUG_PRINT_L_unique(ID, LEVEL, __VA_ARGS__)

#define DEBUG_PRINT_L(LEVEL, ...) \
	_DEBUG_PRINT_L_unique_expander(__LINE__, LEVEL, __VA_ARGS__)

#define DEBUG_PRINT(...) \
	DEBUG_PRINT_L(2, __VA_ARGS__)

#define _LOG_PRINT_unique(ID, ...) \
	if (debug_level() >= 1) { \
		FILE* _log_file##ID = open_log_file(); \
		fprintf(_log_file##ID, "%s", log_prefix()); \
		fprintf(_log_file##ID, __VA_ARGS__); \
		if (_log_file##ID != _DEFAULT_LOG_FILE) { \
			fclose(_log_file##ID); \
		} \
	}

#define _LOG_PRINT_unique_expander(ID, ...) \
	_LOG_PRINT_unique(ID, __VA_ARGS__)

#define LOG_PRINT(...) \
	_LOG_PRINT_unique_expander(__LINE__, __VA_ARGS__)

static inline int debug_level() {
	GET_AND_CACHE_ENV(debug_print_flag, "_PATH_INTERCEPTOR_DEBUG");
	if (debug_print_flag && strlen(debug_print_flag)) {
		return strtol(debug_print_flag, NULL, 0);
	}
	return 1;
}

static inline FILE* open_log_file() {
	GET_AND_CACHE_ENV(log_filepath, "_PATH_INTERCEPTOR_LOG_FILE");
	if (log_filepath && strlen(log_filepath)) {
		return fopen(log_filepath, "a");
	}
	return _DEFAULT_LOG_FILE;
}

static inline const char* log_prefix() {
	GET_AND_CACHE_ENV(_log_prefix, "_PATH_INTERCEPTOR_LOG_PREFIX");
	if (!_log_prefix) {
		return "STATUS: ";
	}
	return _log_prefix;
}

#endif
