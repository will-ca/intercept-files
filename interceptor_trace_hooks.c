#ifndef INTERCEPTOR_TRACE_HOOKS_C_INCL
#define INTERCEPTOR_TRACE_HOOKS_C_INCL

#include "interceptor_pragmas.h"

#include "interceptor_debug.c"


////// PTRACE:

#define TEST_LOG_PREHOOK(NAME) \
	static void NAME() { \
		LOG_PRINT("Running " #NAME ".\n"); \
	}

TEST_LOG_PREHOOK(PREHOOK_clone)
TEST_LOG_PREHOOK(PREHOOK_fork)
TEST_LOG_PREHOOK(PREHOOK_vfork)
TEST_LOG_PREHOOK(PREHOOK_execve)

static void PREHOOK_utimesnat() {
	DEBUG_PRINT("INFO: SYS_utimensat is known to sometimes cause errors with E.G. `touch ABC`.\n");
}

TEST_LOG_PREHOOK(TESTHOOK_statx)

#endif
