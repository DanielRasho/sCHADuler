#include "sys/types.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef size_t *SC_Err;
static const SC_Err NO_ERROR = (size_t *)0;
static const SC_Err NOT_FOUND = (size_t *)1;
static const SC_Err MALLOC_FAILED = (size_t *)2;
static const SC_Err ARENA_ALLOC_NO_SPACE = (size_t *)3;
static const SC_Err NO_SPACE_LEFT = (size_t *)4;
static const SC_Err HASHMAP_INITIALIZATION_ERROR = (size_t *)5;

typedef int SC_Bool;
static const SC_Bool SC_TRUE = 1;
static const SC_Bool SC_FALSE = 0;

// A panic represents an irrecoverable error.
//
// The program somehow got into an irrecoverable state and there's no other
// option other than to panic, because continuing would hide a bug!
//
// Common examples of appropriate places to panic include:
// * Accessing items out of bounds.
// * Trying to close a connection that has already been closed.
// * If your function can fail but it doesn't use the `SC_ERR` API then it
// should PANIC!
void SC_PANIC(const char *format, ...) {
  va_list args;

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  exit(1);
}

// ################################
// ||                            ||
// ||           ARENAS           ||
// ||                            ||
// ################################

// Holds and manages a chunk of fixed size memory.
// It can grow according to the needs.
struct SC_Arena {
  char *data;
  // The next free slot in the `data` array.
  size_t count;
  // The total capacity of the arena.
  size_t capacity;
  // If this arena reaches it's limit, it'll try to create another arena to
  // allocate all bytes before failing.
  struct SC_Arena *next;

  // Flag that checks whether or not this arena has been deinited.
  SC_Bool deinited;
};

/**
 * Initializes an arena.
 *
 * An Arena is a holder of a fixed amount of memory. This specific
 * implementation will try to create child arenas once it runs out of space!
 *
 * @param arena struct SC_Arena The Arena to initialize.
 * @param initial_capacity size_t The initial capacity of the arena.
 * @param err SC_Err The error parameter in case an allocation fails.
 *
 * @example
 *
 * SC_Err err = NO_ERROR;
 * struct SC_Arena arena;
 * SC_Arena_init(arena, 100, err);
 *
 * if (err != NO_ERROR) {
 *	// Do something in case of a failure!
 * } else {
 *	// Do something in case of success!
 * }
 */
void SC_Arena_init(struct SC_Arena *arena, size_t initial_capacity,
                   SC_Err err) {

  char *data = malloc(initial_capacity);
  if (NULL == data) {
    err = MALLOC_FAILED;
    return;
  }

  arena->data = data;
  arena->count = 0;
  arena->capacity = initial_capacity;
  arena->next = NULL;
  arena->deinited = SC_FALSE;
}

// Allocates the requested space on the arena.
// If too little or no space is available, creates a new child arena and tries
// to allocate in it!
char *SC_Arena_alloc(struct SC_Arena *arena, size_t requested_size,
                     SC_Err err) {

  if (arena->deinited) {
    SC_PANIC("Can't allocate data on an already deinited arena!");
    return NULL;
  }

  SC_Bool has_space = arena->count + requested_size < arena->capacity;
  if (has_space) {
    char *p = arena->data + arena->count;
    arena->count += requested_size;
    return p;
  } else if (NULL != arena->next) {
    return SC_Arena_alloc(arena->next, requested_size, err);
  } else {
    size_t next_capacity = arena->capacity;
    if (next_capacity < requested_size) {
      next_capacity = requested_size;
    }

    arena->next = malloc(sizeof(struct SC_Arena));
    if (NULL == arena) {
      err = MALLOC_FAILED;
      return NULL;
    }
    SC_Arena_init(arena->next, next_capacity, err);

    return SC_Arena_alloc(arena->next, requested_size, err);
  }
}

// Resets the arena so that it can be used again.
// All data is kept, you simply override it when writing to it.
//
// All child arenas are reset too!
void SC_Arena_reset(struct SC_Arena *arena) {
  if (arena->deinited) {
    SC_PANIC("Can reset an arena that is already deinited!");
    return;
  }

  if (NULL != arena->next) {
    SC_Arena_reset(arena->next);
  }

  arena->count = 0;
}

// Frees the memory associated with this arena.
void SC_Arena_deinit(struct SC_Arena *arena) {
  if (arena->deinited) {
    SC_PANIC("An arena should not be deinited more than 1 time!");
    return;
  }

  if (NULL != arena->next) {
    SC_Arena_deinit(arena->next);
  }

  free(arena->data);
  arena->deinited = SC_TRUE;
}

typedef struct {
  char *data;
  size_t length;
} SC_String;

typedef struct {
  SC_String pid;
  uint burst_time;
  uint arrival_time;
  uint priority;
} SC_Process;
