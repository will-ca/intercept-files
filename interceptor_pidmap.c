#ifndef INTERCEPTOR_PIDMAP_C_INCL
#define INTERCEPTOR_PIDMAP_C_INCL

#include <stdlib.h>

#include "interceptor_debug.c"


// Just so we can remember which threads and child processes are on syscall-enter-stop and syscall-exit-stop.

// There shouldn't be *too* many concurrent threads and processes at a time in most sane applications anyway— I don't want to mess with hash tables/O(1) associative access.

typedef int pidmap_index_t;

typedef struct {
	int _valid;// = 0;
	int key;
	int value;
} PidMapEntry_t;

// I guess we should have an initialization function, but the only values that matter at allocation are zero anyway.

typedef struct {
	PidMapEntry_t* _entries;
	// Directly accessing the entries with an index would be better right now, but we want to be able to swap this out for something truly associative.
	int length;// = 0;
	int _cached_key;
	int _cached_key_valid;// = 0;
	int _cached_key_index;
} PidMap_t;

static void _pidMapResize(PidMap_t* pidmap, int newlength) {
	DEBUG_PRINT_L(5, "Resizing PID mapping: %i → %i\n", pidmap->length, newlength);
	PidMapEntry_t* new_entries = (PidMapEntry_t*)malloc(sizeof(PidMapEntry_t) * newlength);
	// TODO: Check failure.
	int i = 0;
	if (pidmap->_entries) {
		int copyable_l = newlength < pidmap->length ? newlength : pidmap->length;
		if (newlength < pidmap->length) {
			LOG_PRINT("ERROR: Shrinking PID mapping is not supported: %i → %i\n", pidmap->length, newlength);
			exit(1);
		}
		for (i = 0; i < copyable_l; i++) {
			new_entries[i] = pidmap->_entries[i];
		}
		free(pidmap->_entries);
	}
	for (; i < newlength; i++) {
		// Initialize new entries.
		new_entries[i]._valid = 0;
	}
	pidmap->_entries = new_entries;
	pidmap->length = newlength;
}

static void _pidMapGrow(PidMap_t* pidmap) {
	int orig_length = pidmap->length;
	_pidMapResize(pidmap, (pidmap->length * 1.3) + 5);
	// If this ever gets quite big, a big factor/exponent would be a little scary.
	DEBUG_PRINT("Automatically grew PID mapping: %i → %i\n", orig_length, pidmap->length);
}

static pidmap_index_t _pidMapGetIndex(PidMap_t* pidmap, int key) {
	DEBUG_PRINT_L(5, "Finding index in PID mapping for key: %i\n", key);
	if (pidmap->_cached_key_valid && pidmap->_cached_key == key)
		return pidmap->_cached_key_index;
		// Special-case consecutive accesses since this is meant to be used per-PID rather than cross-PID.
	for (int i = 0; i < pidmap->length; i++) {
		PidMapEntry_t entry = pidmap->_entries[i];
		if (entry._valid && entry.key == key) {
			pidmap->_cached_key = key;
			pidmap->_cached_key_index = i;
			pidmap->_cached_key_valid = 1;
			return i;
		}
	}
	return -1;
}

static pidmap_index_t _pidMapGetFreeIndex(PidMap_t* pidmap) {
	DEBUG_PRINT_L(5, "Finding free index in PID mapping.\n");
	int i;
	for (i = 0; i < pidmap->length; i++) {
		if (!pidmap->_entries[i]._valid)
			return i;
	}
	_pidMapGrow(pidmap);
	return i;
}

static void pidMapInit(PidMap_t* pidmap) {
	DEBUG_PRINT_L(4, "Initializing PID mapping.\n");
	pidmap->_entries = NULL;
	pidmap->length = 0;
	pidmap->_cached_key_valid = 0;
	_pidMapGrow(pidmap);
}

static void pidMapSet(PidMap_t* pidmap, int key, int value) {
	DEBUG_PRINT_L(4, "Setting key value in PID mapping: %i=%i\n", key, value);
	pidmap_index_t i = _pidMapGetIndex(pidmap, key);
	if (i < 0) {
		i = _pidMapGetFreeIndex(pidmap);
		pidmap->_entries[i].key = key;
		pidmap->_entries[i]._valid = 1;
	}
	pidmap->_entries[i].value = value;
}

static int pidMapHas(PidMap_t* pidmap, int key) {
	DEBUG_PRINT_L(4, "Checking key value presence in PID mapping: %i\n", key);
	if (_pidMapGetIndex(pidmap, key) < 0) {
		return 0;
	} else {
		return 1;
	}
}

static int pidMapGet(PidMap_t* pidmap, int key) {
	DEBUG_PRINT_L(4, "Retrieving key value in PID mapping: %i\n", key);
	pidmap_index_t i = _pidMapGetIndex(pidmap, key);
	if (i < 0) {
		LOG_PRINT("ERROR: Cannot access invalid key in PID mapping: %i\n", key);
		exit(1);
	}
	return pidmap->_entries[i].value;
}

static void pidMapRemove(PidMap_t* pidmap, int key) {
	DEBUG_PRINT_L(4, "Removing key value in PID mapping: %i\n", key);
	pidmap_index_t i = _pidMapGetIndex(pidmap, key);
	if (i < 0) {
		LOG_PRINT("ERROR: Cannot remove invalid key in PID mapping: %i\n", key);
		exit(1);
	}
	pidmap->_entries[i]._valid = 0;
}

#if 0

static void _testfunct() {
	PidMap_t pidmap;
	pidMapInit(&pidmap);
	for (int k = 0; k < 10; k++) {
		LOG_PRINT("Setting %i=%i\n", k, -k);
		pidMapSet(&pidmap, k, -k);
	}
	int test_keys[4] = { 0, 9, 3, 5 };
	for (int i = 0; i < 4; i++) {
		int k = test_keys[i];
		LOG_PRINT("Get %i: %i\n", k, pidMapGet(&pidmap, k));
	}
	LOG_PRINT("Setting 100=314.\n");
	pidMapSet(&pidmap, 100, 314);
	LOG_PRINT("Incrementing 9 by +5.\n");
	pidMapSet(&pidmap, 9, pidMapGet(&pidmap, 9) + 5);
	int test_keys2[5] = { 0, 9, 3, 5, 100 };
	for (int i = 0; i < 5; i++) {
		int _i = test_keys2[i];
		LOG_PRINT("Get %i: %i\n", _i, pidMapGet(&pidmap, _i));
	}
	for (int k = 0; k < 10; k += 2) {
		LOG_PRINT("Removing %i\n", k);
		pidMapRemove(&pidmap, k);
	}
	for (int k = 1000; k < 1010; k++) {
		LOG_PRINT("Setting %i=%i\n", k, -k);
		pidMapSet(&pidmap, k, -k);
	}
	for (int k = 1000; k < 1010; k += 3) {
		LOG_PRINT("Removing %i\n", k);
		pidMapRemove(&pidmap, k);
	}
	LOG_PRINT("Inspecting.\n");
	for (int i = 0; i < pidmap.length; i++) {
		PidMapEntry_t entry = pidmap._entries[i];
		LOG_PRINT("Inspect:\t\ti=%i\t\t_valid=%i\t\tkey=%i\t\tvalue=%i\n", i, entry._valid, entry.key, entry.value);
	}
	LOG_PRINT("Trying to get removed key %i.\n", 1006);
	pidMapGet(&pidmap, 1006);
}

#endif

#endif
