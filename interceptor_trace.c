#ifndef INTERCEPTOR_TRACE_C_INCL
#define INTERCEPTOR_TRACE_C_INCL

#include "interceptor_pragmas.h"

// #include <stdlib.h>

#include <errno.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <linux/limits.h>

#include "interceptor_debug.c"
#include "interceptor_replace.h"

#include "interceptor_trace_types.h"
#include "interceptor_trace_calls.c"

#include "interceptor_pidmap.c"


/*
 * Based on `ptrace-redirect` by Alfonso Sánchez-Beato, originally released under BSD0:
 *
 * https://www.alfonsobeato.net/c/modifying-system-call-arguments-with-ptrace/
 * https://github.com/alfonsosanchezbeato/ptrace-redirect/blob/a2315cd10ff4f76ed71fcfef7a7cd6e7b34f0fe4/redirect.c
 * https://github.com/alfonsosanchezbeato/ptrace-redirect/blob/30b952765f4ac94cd37490594fb1c4e3164e2c74/COPYING
 */


////// PTRACE:

static void process_signals(pid_t child, StringReplacer_t);
static pid_t wait_for_stop(pid_t pid, int *wstatus, int options);
static void handle_syscall(rax_t rax, pid_t pid, StringReplacer_t replacer);
static int read_file(reg_t filearg_register, pid_t pid, char *file);
static void redirect_file(reg_t filearg_register, pid_t pid, const char *file);


static void process_signals(pid_t child, StringReplacer_t replacer) {

	LOG_PRINT("Starting main target:\n\t%i\n", child);

	PidMap_t pid_in_syscall;
	pidMapInit(&pid_in_syscall);
	// See section "Syscall-stops" in ptrace(2).
	// Each syscall causes one stop upon call entry, which must be continued with ptrace(PTRACE_SYSCALL), and another "indistinguishable" stop on call exit, which must also be continued.
	// We keep track of that oscillating state per thread to catch only syscall-enter-stops.
	// We can't just synchronously wait for the syscall-exit-stop each time, because then parent thread syscalls that require us to first handle child thread syscalls, like SYS_wait4 (61), have no way of completing.
	// This also means we only do wait_for_stop() once per loop. That in turn means we (1) can catch *every* potential event, such as exits and forks, and (2) we don't have to repeat (or worry as much about synchronizing) the logic for handling special events like that due to waiting multiple times.

	pidMapSet(&pid_in_syscall, child, 0);

	pid_t pid;

	ptrace(PTRACE_SYSCALL, child, 0, 0);

	while(1) {
		int status = 0;


		DEBUG_PRINT_L(3, "Awaiting stop.\n");

		pid = wait_for_stop(-1, &status, __WALL);

		DEBUG_PRINT_L(3, "Awaited stop: %i\n", pid);


		if (!pidMapHas(&pid_in_syscall, pid)) {
			// FIXME: This should never happen, but it does (wstatus: 4991).
			// It looks like sometimes we catch the first syscall from the child process before we catch the clone/fork event from the parent?
			// Since PTRACE_O_TRACE* is supposed to stop the new process, I suppose QtWebEngine/Chromium possibly has a third process that itself sends signals to the fork, prematurely continuing it?
			// `strace` doesn't have this issue, so it should be fixable.
			LOG_PRINT("ERROR: Unexpected PID %i.\n\tWhere did this come from?\n\tDetaching.\n", pid);
			// Attempts to gracefully handle this so far lead to invisible text in QtWebEngine and missing web views in Chromium, so just detach.
			ptrace(PTRACE_DETACH, pid, 0, 0);
			// pidMapSet(&pid_in_syscall, pid, 0);
		}


		int is_fork = 0;
		const char* fork_logverb;

		if (status >> 8 == (SIGTRAP | (PTRACE_EVENT_CLONE << 8))) {
			is_fork = 1;
			fork_logverb = "Cloned";
		} else if (status >> 8 == (SIGTRAP | (PTRACE_EVENT_FORK << 8))) {
			is_fork = 1;
			fork_logverb = "Forked";
		} else if (status >> 8 == (SIGTRAP | (PTRACE_EVENT_VFORK << 8))) {
			is_fork = 1;
			fork_logverb = "V-Forked";
		}

		if (is_fork) {
			unsigned long fork_pid;
			ptrace(PTRACE_GETEVENTMSG, pid, 0, &fork_pid);
			LOG_PRINT("%s new PID:\n\t%i → %li\n",
				fork_logverb,
				pid,
				fork_pid
			);
			ptrace(PTRACE_SYSCALL, pid, 0, 0);
			if (!pidMapHas(&pid_in_syscall, fork_pid)) {
				// Check because sometimes child stop is caught before parent clone, so we might have a fallback to already add it in that case. See above.
				ptrace(PTRACE_SYSCALL, fork_pid, 0, 0);
				pidMapSet(&pid_in_syscall, fork_pid, 0);
			} else {
				LOG_PRINT("ERROR: %s PID already recognized!\n\t%li\n",
					fork_logverb,
					fork_pid
				);
			}
			continue;
		}


		int is_exit = 0;
		int exit_code;
		const char* exit_logverb;

		if (WIFEXITED(status)) {
			is_exit = 1;
			exit_code = WEXITSTATUS(status);
			exit_logverb = "Exited";
		} else if (WIFSIGNALED(status)) {
			is_exit = 1;
			exit_code = WTERMSIG(status);
			exit_logverb = "Terminated";
		}

		int stop_sig = WSTOPSIG(status);

		#define _CHECK_STOPSIG(SIGNAME) \
			if (!is_exit && stop_sig == SIGNAME) { \
				is_exit = 1; \
				exit_code = stop_sig; \
				exit_logverb = "Killed (" #SIGNAME ")"; \
			}

		_CHECK_STOPSIG(SIGILL);
		_CHECK_STOPSIG(SIGABRT);
		_CHECK_STOPSIG(SIGFPE);
		_CHECK_STOPSIG(SIGSEGV);
		_CHECK_STOPSIG(SIGTERM);
		_CHECK_STOPSIG(SIGHUP);
		_CHECK_STOPSIG(SIGQUIT);
		_CHECK_STOPSIG(SIGKILL);
		_CHECK_STOPSIG(SIGPIPE);

		#undef _CHECK_STOPSIG

		if (is_exit) {
			LOG_PRINT("%s:\n\t%i %i\n",
				exit_logverb,
				pid,
				exit_code
			);
			if (pid == child || pid < 0) {
				LOG_PRINT("%s main thread.\n",
					exit_logverb
				);
				exit(exit_code);
			}
			pidMapRemove(&pid_in_syscall, pid);
			continue;
		}


		if ((stop_sig & (SIGTRAP | 0x80)) == (SIGTRAP | 0x80)) {
			// Manual says "WSTOPSIG(status) will give the value (SIGTRAP | 0x80)". Apparently other bits can still be set too though.
			int in_syscall = pidMapGet(&pid_in_syscall, pid);

			if (!in_syscall) {
				DEBUG_PRINT_L(3, "Entering syscall.\n");
				handle_syscall(
					ptrace(PTRACE_PEEKUSER, pid, sizeof(long)*ORIG_RAX, 0),
					pid,
					replacer
				);
			} else {
				DEBUG_PRINT_L(3, "Exiting syscall.\n");
			}

			pidMapSet(&pid_in_syscall, pid, !in_syscall);
		}


		ptrace(PTRACE_SYSCALL, pid, 0, 0);
	}
}


static pid_t wait_for_stop(pid_t pid, int *wstatus, int options) {
	pid_t changed_pid;
	while (1) {
		changed_pid = waitpid(pid, wstatus, options);
		#define _CHECK_EVENT(EVENTNAME) (*wstatus >> 8 == (SIGTRAP | (EVENTNAME << 8)))
		DEBUG_PRINT_L(4, "State change: %i %i %i %i    %i %i %i %i    %i %i %i %i    %i %i %i %i    %i %i %i %i    %i\n",
			changed_pid,
			*wstatus,
			WIFEXITED(*wstatus),
			WEXITSTATUS(*wstatus),

			WIFSIGNALED(*wstatus),
			WTERMSIG(*wstatus),
			WCOREDUMP(*wstatus),
			WIFSTOPPED(*wstatus),

			WSTOPSIG(*wstatus),
			WIFCONTINUED(*wstatus),
			WSTOPSIG(*wstatus) & SIGTRAP,
			WSTOPSIG(*wstatus) & 0x80,

			_CHECK_EVENT(PTRACE_EVENT_CLONE),
			_CHECK_EVENT(PTRACE_EVENT_EXEC),
			_CHECK_EVENT(PTRACE_EVENT_EXIT),
			_CHECK_EVENT(PTRACE_EVENT_FORK),

			_CHECK_EVENT(PTRACE_EVENT_VFORK),
			_CHECK_EVENT(PTRACE_EVENT_VFORK_DONE),
			_CHECK_EVENT(PTRACE_EVENT_SECCOMP),
			_CHECK_EVENT(PTRACE_EVENT_STOP),

			(*wstatus >> 16 == PTRACE_EVENT_STOP)
		);
		#undef _CHECK_EVENT
		if (WIFSTOPPED(*wstatus)) {
			return changed_pid;
		}
		if (WIFEXITED(*wstatus)) {
			return changed_pid;
		}
	}
}


static void handle_syscall(rax_t rax, pid_t pid, StringReplacer_t replacer) {
	InterceptibleCall_t interceptible_call = get_interceptible_call(rax);

	if (interceptible_call.call_rax < 0) {
		DEBUG_PRINT("Skipping unknown syscall (%i %li).\n",
			pid,
			rax
		);
		return;
	}

	DEBUG_PRINT("Handling syscall '%s' (%i %li).\n",
		interceptible_call.name,
		pid,
		interceptible_call.call_rax
	);

	if (interceptible_call.pre_hook)
		interceptible_call.pre_hook();

	for (int i = 0; i < InterceptibleCall_maxargs_l; i++) {

		reg_t filearg_reg = interceptible_call.call_filearg_registers[i];

		if (!filearg_reg)
			break;

		/* Find out file and re-direct if appropriate */

		char orig_file[PATH_MAX];

		if (debug_level())
			for (int i = 0; i < PATH_MAX; i++) {
				orig_file[i] = '\0';
			}

		DEBUG_PRINT("Reading file argument from syscall '%s' (%i %li %i).\n",
			interceptible_call.name,
			pid,
			interceptible_call.call_rax,
			filearg_reg
		);

		int _errno = read_file(filearg_reg, pid, orig_file);

		if (_errno != 0) {
			LOG_PRINT(
				"ERROR: PTRACE_PEEKTEXT ERROR! (PID %i %s REG %i):\n\t%s\n\tEnable _PATH_INTERCEPTER_DEBUG=1 for more information.\n\tPlease consider reporting this if it looks like a bug.\n\tMax read out (only accurate if _PATH_INTERCEPTER_DEBUG=1): %s\n",
				pid,
				interceptible_call.name,
				filearg_reg,
				strerror(_errno),
				orig_file
			);
			continue;
		}

		char *new_file = replacer(orig_file);

		if (new_file) {
			LOG_PRINT(
				"Intercepted and substituted path (PID %i %s REG %i):\n\t%s\n\t→\t%s\n",
				pid,
				interceptible_call.name,
				filearg_reg,
				orig_file,
				new_file
			);
			DEBUG_PRINT("Writing file argument to syscall '%s' (%i %li %i).\n",
				interceptible_call.name,
				pid,
				interceptible_call.call_rax,
				filearg_reg
			);

			redirect_file(filearg_reg, pid, new_file);

			free(new_file);
		}
	}

	if (interceptible_call.post_hook)
		interceptible_call.post_hook();
}


static int read_file(reg_t filearg_register, pid_t pid, char *file)
{
	// Returns `errno` on failure, 0 otherwise.

	char *child_addr;
	int i;

	child_addr = (char *) ptrace(PTRACE_PEEKUSER,
		pid,
		sizeof(long)*filearg_register,
		0
	);

	int debug_loop_i = 0;

	do {
		long val;
		char *p;

		val = ptrace(PTRACE_PEEKTEXT,
			pid,
			child_addr,
			NULL
		);
		if (val == -1) {
			// ERROR.
			DEBUG_PRINT("PTRACE_PEEKTEXT error at word %i (%i %i).\n",
				debug_loop_i,
				pid,
				filearg_register
			);
			return errno;
		}
		child_addr += sizeof (long);

		p = (char *) &val;
		for (i = 0; i < sizeof (long); ++i, ++file) {
			*file = *p++;
			if (*file == '\0') break;
		}

		debug_loop_i++;
	} while (i == sizeof (long));
	return 0;
}


static void redirect_file(reg_t filearg_register, pid_t pid, const char *file)
{

	char *stack_addr, *file_addr;

	stack_addr = (char *) ptrace(PTRACE_PEEKUSER,
		pid,
		sizeof(long)*RSP,
	0);
	/* Move further of red zone and make sure we have space for the file name */
	stack_addr -= 128 + PATH_MAX;
	file_addr = stack_addr;

	/* Write new file in lower part of the stack */
	do {
		int i;
		char val[sizeof (long)];

		for (i = 0; i < sizeof (long); ++i, ++file) {
			val[i] = *file;
			if (*file == '\0') break;
		}

		ptrace(PTRACE_POKETEXT,
			pid,
			stack_addr,
			*(long *) val
		);
		stack_addr += sizeof (long);
	} while (*file);

	/* Change argument to open */
	ptrace(PTRACE_POKEUSER,
		pid,
		sizeof(long)*filearg_register,
		file_addr
	);
}

#endif
