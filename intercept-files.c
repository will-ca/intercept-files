#include "interceptor_pragmas.h"

// #include <stdlib.h>
//
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>

#include "interceptor_conf.c"
// #include "interceptor_debug.c"

#include "interceptor_trace.c"

#include "interceptor_replace.c"

// TODO: If this gets much more complex, actually define headers and link instead of transcluding code.


static const char* HELP_TEXT = "\n"
"Intercept most Linux system calls that handle filepaths, and replace a REGEX match in all paths passed to them with a fixed string.\n"
"\n"
"Usage:\n"
"	$ intercept-files <COMMAND> [COMMAND ARGS...]\n"
"\n"
"Control with environment variables:\n"
"\n"
"	_PATH_INTERCEPTOR_THREADS\n"
"		\"1\" to apply interceptor to threads and child processes. Unless you know specifically that the program does not use threads or child processes, it is recommended to enable this.\n"
"\n"
"	_PATH_INTERCEPTOR_MATCH_REGEX\n"
"		A POSIX Extended Regular Expression string to match against intercepted pathnames.\n"
"	_PATH_INTERCEPTOR_REPLACEMENT_STRING\n"
"		A string with which to replace matched sections of intercepted pathnames.\n"
"\n"
"	_PATH_INTERCEPTOR_LOG_PREFIX\n"
"		Prefix to prepend to log messages. Default is \"STATUS: \".\n"
"	_PATH_INTERCEPTOR_DEBUG\n"
"		Integer specifying detail level of log messages. \"1\" by default, for status messsages. Higher values for increasingly detailed debug messages. \"0\" to disable.\n"
"	_PATH_INTERCEPTOR_LOG_FILE\n"
"		Filepath to which to append log messages. If unset, log messages are sent to STDERR instead.\n"
"\n";


// Using environment variables for control feels a bit messy, but is both easier and also makes some sense since you usually probably want potentially multiple instances of `intercept-files` to be started with the same configuration.

// I originally wrote the replacement portion intending it to be used with LD_PRELOAD.
// But it turns out QT uses a lot of dynamic loading that wasn't affected by it.


// Basic test cases:

// _PATH_INTERCEPTOR_DEBUG=1 _PATH_INTERCEPTOR_LOG_FILE='' _PATH_INTERCEPTOR_THREADS=1 _PATH_INTERCEPTOR_LOG_PREFIX="INTERCEPT: " _PATH_INTERCEPTOR_MATCH_REGEX=^A _PATH_INTERCEPTOR_REPLACEMENT_STRING=B ./intercept-files bash -c '(stat Abc); (true); (false); stat ABD; exec stat ABE;'
// Basic multithreaded. Disable threads too for single.

// _PATH_INTERCEPTOR_DEBUG=4 _PATH_INTERCEPTOR_LOG_FILE='' _PATH_INTERCEPTOR_THREADS=1 _PATH_INTERCEPTOR_LOG_PREFIX="INTERCEPT: " _PATH_INTERCEPTOR_MATCH_REGEX=^A _PATH_INTERCEPTOR_REPLACEMENT_STRING=B ./intercept-files bash -c 'for i in $(seq 100); do (stat Abc); done'
// Stability. (At one point I mixed up the RSP register in redirect_file().)

// _PATH_INTERCEPTOR_DEBUG=1 _PATH_INTERCEPTOR_LOG_FILE='' _PATH_INTERCEPTOR_THREADS=1 _PATH_INTERCEPTOR_LOG_PREFIX="INTERCEPT: " _PATH_INTERCEPTOR_MATCH_REGEX=^A _PATH_INTERCEPTOR_REPLACEMENT_STRING=B ./intercept-files parallel stat ::: ABF ABG ABH
// Possibly different type of forking. Recursive forking too, I think.


int main(int argc, char **argv)
{
	pid_t pid;
	int status;

	if (argc < 2) {
		fprintf(stderr, HELP_TEXT, argv[0]);
		return 1;
	}

	if ((pid = fork()) == 0) {
		ptrace(PTRACE_TRACEME, 0, 0, 0);
		kill(getpid(), SIGSTOP);
		return execvp(argv[1], argv + 1);
	} else {
		long ptrace_options = PTRACE_O_TRACESYSGOOD;
		if (do_trace_threads()) {
			ptrace_options |=
				PTRACE_O_TRACECLONE |
				// PTRACE_O_TRACEEXEC |
				PTRACE_O_TRACEFORK |
				PTRACE_O_TRACEVFORK
			;
		}
		waitpid(pid, &status, 0);
		ptrace(PTRACE_SETOPTIONS, pid, 0, ptrace_options);
		process_signals(pid, intercept_path);
		return 0;
	}
}
