#ifndef INTERCEPTOR_TRACE_H_INCL
#define INTERCEPTOR_TRACE_H_INCL

#include "interceptor_pragmas.h"

#include <sys/types.h>

#include "interceptor_replace.h"


////// PTRACE:

typedef long rax_t;

typedef int reg_t; // There's a register_t in types.h. No idea what it is.
// <sys/syscall.h>/<asm/unistd_64.h> just has integer literals up to the mid hundreds.

typedef struct {
	const char* name;
	rax_t call_rax;
	reg_t call_filearg_registers[6];
	void (*pre_hook) ();
	void (*post_hook) ();
} InterceptibleCall_t;

const int InterceptibleCall_maxargs_l = 6;

#endif
