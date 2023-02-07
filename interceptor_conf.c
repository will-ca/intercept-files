#ifndef INTERCEPTOR_CONF_C_INCL
#define INTERCEPTOR_CONF_C_INCL

#include <stdlib.h>
#include <string.h>


#define GET_AND_CACHE_ENV(VARNAME, ENVNAME) \
	static char* VARNAME; \
	if (!VARNAME) { \
		VARNAME = getenv(ENVNAME); \
	}

static inline int do_trace_threads() {
	GET_AND_CACHE_ENV(threads_flag, "_PATH_INTERCEPTOR_THREADS");
	return (threads_flag && strcmp(threads_flag, "1") == 0);
}

#endif
