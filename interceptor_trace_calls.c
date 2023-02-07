#ifndef INTERCEPTOR_TRACE_CALLS_C_INCL
#define INTERCEPTOR_TRACE_CALLS_C_INCL

#include "interceptor_pragmas.h"

#include <sys/syscall.h>

#include <sys/reg.h>

#include "interceptor_trace_types.h"

#include "interceptor_trace_hooks.c"


////// PTRACE:

#define SYSCALL(PREHOOK, POSTHOOK, NAME, ...) { \
	.name = #NAME, \
	.call_rax = NAME, \
	.call_filearg_registers = { __VA_ARGS__ }, \
	.pre_hook = PREHOOK, \
	.post_hook = POSTHOOK \
}

const InterceptibleCall_t InterceptibleCalls[] = {
	// http://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/
	// https://chromium.googlesource.com/chromiumos/docs/+/HEAD/constants/syscalls.md
	// Above also includes tables of numbers and names for other CPU architectures.

	// Interestingly, there's no way to list directories here. SYS_readdir is superseded, and both it and SYS_getdents don't directly take path arguments anyway.

	// We rely on zero-initialization to detect early end of the register list. It's technically not part of the C standard until recently, but it seems pretty universal at least in GNU-compatible compilation.
	SYSCALL(NULL, NULL, SYS_open,
		RDI),
	SYSCALL(NULL, NULL, SYS_open,
		RDI),
	SYSCALL(NULL, NULL, SYS_stat,
		RDI),
	SYSCALL(NULL, NULL, SYS_lstat,
		RDI),
	SYSCALL(NULL, NULL, SYS_access,
		RDI),
	// SYSCALL(PREHOOK_clone, NULL, SYS_clone,
	// 	),
	// SYSCALL(PREHOOK_fork, NULL, SYS_fork,
	// 	),
	// SYSCALL(PREHOOK_vfork, NULL, SYS_vfork,
	// 	),
	SYSCALL(NULL/*PREHOOK_execve*/, NULL, SYS_execve,
		RDI),
	SYSCALL(NULL, NULL, SYS_truncate,
		RDI),
	SYSCALL(NULL, NULL, SYS_chdir,
		RDI),
	SYSCALL(NULL, NULL, SYS_rename,
		RDI,RSI),
	SYSCALL(NULL, NULL, SYS_mkdir,
		RDI),
	SYSCALL(NULL, NULL, SYS_rmdir,
		RDI), // Scary.
	SYSCALL(NULL, NULL, SYS_creat,
		RDI),
	SYSCALL(NULL, NULL, SYS_link,
		RDI,RSI),
	SYSCALL(NULL, NULL, SYS_unlink,
		RDI),
	SYSCALL(NULL, NULL, SYS_symlink,
		RDI,RSI),
	SYSCALL(NULL, NULL, SYS_readlink,
		RDI),
	SYSCALL(NULL, NULL, SYS_chmod,
		RDI),
	SYSCALL(NULL, NULL, SYS_chown,
		RDI),
	SYSCALL(NULL, NULL, SYS_lchown,
		RDI),
	SYSCALL(NULL, NULL, SYS_utime,
		RDI),
	SYSCALL(NULL, NULL, SYS_mknod,
		RDI),
	SYSCALL(NULL, NULL, SYS_statfs,
		RDI),
	SYSCALL(NULL, NULL, SYS_pivot_root,
		RDI,RSI), // This and the next couple are a bit low-level.
	SYSCALL(NULL, NULL, SYS_chroot,
		RDI),
	SYSCALL(NULL, NULL, SYS_mount,
		RDI,RSI),
	SYSCALL(NULL, NULL, SYS_umount2,
		RDI),
	SYSCALL(NULL, NULL, SYS_swapon,
		RDI),
	SYSCALL(NULL, NULL, SYS_swapoff,
		RDI),
	SYSCALL(NULL, NULL, SYS_setxattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_lsetxattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_getxattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_lgetxattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_listxattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_llistxattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_removexattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_lremovexattr,
		RDI),
	SYSCALL(NULL, NULL, SYS_utimes,
		RDI),
	SYSCALL(NULL, NULL, SYS_inotify_add_watch,
		RSI),
	SYSCALL(NULL, NULL, SYS_openat,
		RSI),
	SYSCALL(NULL, NULL, SYS_mkdirat,
		RSI),
	SYSCALL(NULL, NULL, SYS_mknodat,
		RSI),
	SYSCALL(NULL, NULL, SYS_fchownat,
		RSI),
	SYSCALL(NULL, NULL, SYS_futimesat,
		RSI),
	SYSCALL(NULL, NULL, SYS_newfstatat,
		RSI),
	SYSCALL(NULL, NULL, SYS_unlinkat,
		RSI),
	SYSCALL(NULL, NULL, SYS_renameat,
		RSI,R10),
	SYSCALL(NULL, NULL, SYS_linkat,
		RSI,R10),
	SYSCALL(NULL, NULL, SYS_symlinkat,
		RDI,RDX),
	SYSCALL(NULL, NULL, SYS_readlinkat,
		RSI),
	SYSCALL(NULL, NULL, SYS_fchmodat,
		RSI),
	SYSCALL(NULL, NULL, SYS_faccessat,
		RSI),
	SYSCALL(PREHOOK_utimesnat, NULL, SYS_utimensat,
		RSI),
	SYSCALL(NULL, NULL, SYS_name_to_handle_at,
		RSI),
	SYSCALL(NULL, NULL, SYS_open_by_handle_at,
		RSI),
	SYSCALL(NULL, NULL, SYS_renameat2,
		RSI,R10),
	SYSCALL(NULL, NULL, SYS_execveat,
		RSI), // const char __user *filename?
	SYSCALL(NULL, NULL, SYS_statx,
		RSI), // const char *restrict pathname?
};

const int InterceptibleCalls_l = sizeof(InterceptibleCalls) / sizeof(InterceptibleCalls[0]);

InterceptibleCall_t get_interceptible_call(rax_t rax) {
	for (int i = 0; i < InterceptibleCalls_l; i++) {
		if (InterceptibleCalls[i].call_rax == rax) {
			return InterceptibleCalls[i];
		}
	}
	InterceptibleCall_t not_found;
	not_found.call_rax = -1;
	return not_found;
}

#endif
