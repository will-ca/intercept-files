**This code is an unmaintained single-purpose workaround for Krashboard AppImage packaging, that didn't actually end up working for its single purpose.**

# **You probably want to use [`proot`](https://proot-me.github.io/), [`unshare`](https://man7.org/linux/man-pages/man1/unshare.1.html), [`bwrap`](https://github.com/containers/bubblewrap), or [`faketree`](https://github.com/enfabrica/enkit/tree/master/faketree) instead.**

---

`intercept-files` is a simple command-line tool to emulate an altered virtual filesystem structure for a specific program, without root privileges.

It works by using the [`ptrace`](https://man7.org/linux/man-pages/man2/ptrace.2.html) feature of the Linux kernel to intercept any system calls that include filepaths as arguments, and running a REGEX string replacement on the filepaths.

**Is it stable?** Who knows! This is my first time writing any C that's more complicated than a micro-benchmark, and even so I prioritized getting it running and keeping it simple over accounting for edge cases. PRs are welcome, if you notice anything wrong, although I tried to not do anything too insane.

**Does it work?** Yes! No! Not really? I guess so??? Nah. I tried to use it to package the AppImage for Krashboard, which includes binaries from [Nix](https://nixos.org/) that have hardcoded paths, hoping to trick that into looking in the AppDir instead of `/nix/store`. The Krashboard AppImage is a QT app that uses multiple Chromium processes which have hardcoded filepaths that must be redirected, so that's probably a pretty robust test. Unfortunately, it didn't really work— It did its job, but also broke some other stuff. For simple shell scripts it should be fine, however, and I have successfully used it to redirect specific files on a full Chromium instance. The basic concept has also been [explored](https://www.alfonsobeato.net/c/modifying-system-call-arguments-with-ptrace/) by Alfonso Sánchez-Beato at Canonical for `snap`, on whose code much of this is based.

Compared to other approaches, this does of course have some drawbacks, such as not easily accounting for relative paths, and having degraded performance due to stopping the running program twice to mutate every system call.

But it also has some benefits, such as not relying on special permissions, newer kernel features, or non-standard external tools.

---

```

Intercept most Linux system calls that handle filepaths, and replace a REGEX match in all paths passed to them with a fixed string.

Usage:
		$ intercept-files <COMMAND> [COMMAND ARGS...]

Control with environment variables:

		_PATH_INTERCEPTOR_THREADS
				"1" to apply interceptor to threads and child processes. Unless you know specifically that the program does not use threads or child processes, it is recommended to enable this.

		_PATH_INTERCEPTOR_MATCH_REGEX
				A POSIX Extended Regular Expression string to match against intercepted pathnames.
		_PATH_INTERCEPTOR_REPLACEMENT_STRING
				A string with which to replace matched sections of intercepted pathnames.

		_PATH_INTERCEPTOR_LOG_PREFIX
				Prefix to prepend to log messages. Default is "STATUS: ".
		_PATH_INTERCEPTOR_DEBUG
				Integer specifying detail level of log messages. "1" by default, for status messsages. Higher values for increasingly detailed debug messages. "0" to disable.
		_PATH_INTERCEPTOR_LOG_FILE
				Filepath to which to append log messages. If unset, log messages are sent to STDERR instead.

```
