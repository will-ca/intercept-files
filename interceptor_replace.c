#ifndef INTERCEPTOR_REPLACE_C_INCL
#define INTERCEPTOR_REPLACE_C_INCL

#include "interceptor_pragmas.h"

#include <stdio.h>

#include <regex.h>
#include <string.h>

#include "interceptor_conf.c"
#include "interceptor_debug.c"
#include "interceptor_replace.h"


////// Simple regex replacement:

static char* regex_replace(const char* original_s, const char* match_regex_s, const char* replacement_s) {
	// Return an `original_s` string with one occurence of `match_regex_s` in it replaced with `replacement_s`, or NULL if the string does not match the `match_regex_s`.
	// The returned string is heap-allocated, and must be freed.

	static regex_t* _match_regex_compiled;
	static char* _match_regex_string;
	// Ooh. Race conditions? (I mean, I don't currently expect it to change anyway.) — Also, the code using this is currently all single-threaded.

	if (
		!_match_regex_compiled ||
		strcmp(match_regex_s, _match_regex_string) != 0
	) {
		LOG_PRINT("Compiling new path interceptor regex:\n\t%s\n", match_regex_s);

		if (_match_regex_compiled) {
			free(_match_regex_compiled);
			free(_match_regex_string);
		}

		_match_regex_compiled = (regex_t*)malloc(sizeof(regex_t));
		// Cast because Kate uses one ClangD LSP configuration for both C and C++. /Shrug./
		_match_regex_string = (char*)malloc(sizeof(char) *  (strlen(replacement_s) + 1));

		regcomp(_match_regex_compiled, match_regex_s, REG_EXTENDED);
		// TODO: The options flag could be exposed as a environment variable configuration.
		strcpy(_match_regex_string, match_regex_s);
	}

	regmatch_t regex_matches[1];

	int regex_return = regexec(
			_match_regex_compiled,
			original_s,
			1,
			regex_matches,
			0
	);

	if (!regex_return) {
		int original_len = strlen(original_s);
		int replacement_len = strlen(replacement_s);
		int replaced_len = original_len
			+ replacement_len
			- regex_matches[0].rm_eo
			+ regex_matches[0].rm_so;
		char* replaced_s = (char*)malloc(sizeof(char) * (replaced_len + 1));
		for (int i=0; i < regex_matches[0].rm_so; i++) {
			replaced_s[i] = original_s[i];
		}
		for (int i=0; i < replacement_len; i++) {
			replaced_s[regex_matches[0].rm_so + i] = replacement_s[i];
		}
		int i_offset = original_len - replaced_len;
		for (int i=regex_matches[0].rm_so+replacement_len; i < replaced_len; i++) {
			replaced_s[i] = original_s[i + i_offset];
		}
		replaced_s[replaced_len] = '\0';
		return replaced_s;
	}

	return NULL;
}

static char* intercept_path(const char* pathname) {
	// Also returns NULL for no match.
	// The returned string is heap-allocated, and must be freed.

	// const char* match_regex_s = getenv("_PATH_INTERCEPTOR_MATCH_REGEX");
	// const char* replacement_s = getenv("_PATH_INTERCEPTOR_REPLACEMENT_STRING");

	// Not sure how I feel about dynamic configuration mid-run. Env vars aren't meaningfully externally mutable anyway, but caching everything at launch feels a little weird.
	GET_AND_CACHE_ENV(match_regex_s, "_PATH_INTERCEPTOR_MATCH_REGEX");
	GET_AND_CACHE_ENV(replacement_s, "_PATH_INTERCEPTOR_REPLACEMENT_STRING");

	#define RETURN_DEFAULT \
		return NULL

	if (!match_regex_s || !replacement_s) {
		DEBUG_PRINT("No path replacer defined. Passing path through: %s\n", pathname);
		RETURN_DEFAULT;
	}

	DEBUG_PRINT("Path interception requested: %s\n", pathname);

	char* replaced_s = regex_replace(pathname, match_regex_s, replacement_s);

	if (replaced_s) {
		DEBUG_PRINT("Intercepted path: %s\n", pathname);
		return replaced_s;
	}

	DEBUG_PRINT("Not intercepting path: %s\n", pathname);

	RETURN_DEFAULT;

	#undef RETURN_DEFAULT
}

#endif
