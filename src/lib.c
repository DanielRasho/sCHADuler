#include "sys/types.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef size_t *SC_Err;
static const SC_Err NO_ERROR = (size_t *)0;
static const SC_Err NOT_FOUND = (size_t *)1;
static const SC_Err MALLOC_FAILED = (size_t *)2;
static const SC_Err ARENA_ALLOC_NO_SPACE = (size_t *)3;
static const SC_Err OUT_OF_BOUNDS = (size_t *)4;
static const SC_Err EMPTY_STRING = (size_t *)5;
static const SC_Err INVALID_STRING = (size_t *)6;

static const char *SC_Err_ToString(SC_Err err) {
  if (err == NO_ERROR) {
    return "No error found!";
  } else if (err == NOT_FOUND) {
    return "Element was not found!";
  } else if (err == MALLOC_FAILED) {
    return "malloc failed! Maybe out of memory?";
  } else if (err == ARENA_ALLOC_NO_SPACE) {
    return "Arena is out of space!";
  } else if (err == OUT_OF_BOUNDS) {
    return "Tried to access an index out of bounds!";
  } else if (err == EMPTY_STRING) {
    return "Tried to do some operation on an empty string!";
  } else if (err == INVALID_STRING) {
    return "The supplied string is invalid for the operation!";
  } else {
    return "INVALID ERROR VALUE RECEIVED!";
  }
}

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
void *SC_Arena_Alloc(struct SC_Arena *arena, size_t requested_size,
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
    return SC_Arena_Alloc(arena->next, requested_size, err);
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

    return SC_Arena_Alloc(arena->next, requested_size, err);
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

SC_String SC_String_from_c_string(char *c_str) {
  SC_String str = {.length = strlen(c_str), .data = c_str};
  return str;
}

char SC_String_CharAt(SC_String *str, size_t idx) {
  if (idx < 0 || idx >= str->length) {
    SC_PANIC("Trying to access an invalid Index! %d from %*s", idx, str->length,
             str->data);
    return 0;
  } else {
    return str->data[idx];
  }
}

SC_String SC_String_CopyOnArena(struct SC_Arena *arena, SC_String *other,
                                SC_Err err) {
  SC_String str = {};
  str.data = SC_Arena_Alloc(arena, other->length, err);
  if (err != NO_ERROR) {
    return str;
  }

  memcpy(str.data, other->data, other->length);

  return str;
}

void SC_String_TrySetCharAt(SC_String *str, size_t idx, char value,
                            SC_Err err) {
  if (idx < 0 || idx >= str->length) {
    err = OUT_OF_BOUNDS;
  } else {
    str->data[idx] = value;
  }
}

int SC_String_ToInt(SC_String *str, SC_Err err) {
  if (str->length <= 0) {
    err = EMPTY_STRING;
    return 0;
  }

  int result = 0;
  int current_factor = 1;
  for (int i = str->length - 1; i > -1; i--) {
    char digit = SC_String_CharAt(str, i);
    switch (digit) {
    case '1' ... '9': {
      result += (digit - '0') * current_factor;
      current_factor *= 10;
    } break;
    case '-': {
      if (result != 0) {
        err = INVALID_STRING;
        return 0;
      }

      current_factor *= -1;
    } break;

    case ' ' | '\t' | '\n' | '\r': {
      if (result != 0) {
        err = INVALID_STRING;
        return 0;
      }
    } break;

    default: {
      err = INVALID_STRING;
      return 0;
    } break;
    }
  }

  return result;
}

typedef struct {
  size_t capacity;
  size_t count;
  SC_String data[];
} SC_StringList;

SC_StringList *SC_StringList_NewWithArena(struct SC_Arena *arena,
                                          size_t capacity) {
  SC_Err err = NO_ERROR;
  SC_StringList *list = SC_Arena_Alloc(
      arena, sizeof(SC_StringList) + sizeof(SC_String) * capacity, err);
  return list;
}

void SC_StringList_Append(SC_StringList *list, SC_String str, SC_Err err) {
  if (list->count >= list->capacity) {
    err = OUT_OF_BOUNDS;
    return;
  }

  list->data[list->count] = str;
  list->count++;
}

typedef struct {
  size_t pid_idx;
  uint burst_time;
  uint arrival_time;
  uint priority;
} SC_Process;

typedef struct {
  size_t count;
  size_t capacity;
  SC_Process processes[];
} SC_ProcessList;

/**
 * Saves all the state needed to render a single step in the animation.
 *
 * Each step should own it's memory! So we can free one step and not affect
 * all the others!
 */
typedef struct {
  size_t current_process;
  SC_ProcessList processes;
} SC_SimStepState;

/**
 * Saves all the steps a simulation can have.
 */
typedef struct {
  size_t step_max;
  size_t current_step;
  SC_SimStepState steps[];
} SC_Simulation;

/**
 * Computes the FIFO scheduling simulation
 *
 * This method must compute the complete list of steps the simulation must
 * render. EACH STEP MUST OWN it's data!
 *
 * @param arena SC_Arena The Arena to allocate everything that you need to
 * copy over.
 * @param processes SC_FixedProcessList The initial conditions of each
 * process.
 * @return SC_Simulation The simulation with all it's steps.
 */
void simulate_first_in_first_out(SC_ProcessList *processes,
                                 SC_Simulation *sim) {
  // Guide to initialize arrays and use the arrays:
  // https://en.wikipedia.org/wiki/Flexible_array_member
}

void parse_scheduling_file(SC_String *file_contents, struct SC_Arena *arena,
                           SC_StringList *pid_list, SC_ProcessList *processes,
                           SC_Err err) {
  char buff[255]; // Max length of a column
  SC_String buffer = {
      .data = buff,
      .length = 0,
  };
  SC_Process current_process;

  int current_column = 1;
  for (int i = 0; i < file_contents->length; i++) {
    char current_byte = SC_String_CharAt(file_contents, i);
    switch (current_byte) {
    case '\n':
      current_column = 1;

    case ',': {
      if (1 == current_column) {
        SC_String pid_str = SC_String_CopyOnArena(arena, &buffer, err);
        if (err != NO_ERROR) {
          return;
        }

        SC_StringList_Append(pid_list, pid_str, err);
        if (err != NO_ERROR) {
          return;
        }
        current_process.pid_idx = pid_list->count - 1;
      } else {
        // TODO: Transform digit into string...
      }
      current_column++;
    } break;

    default: {
      SC_String_TrySetCharAt(&buffer, buffer.length, current_byte, err);
      if (err != NO_ERROR) {
        return;
      }
    }
    }
  }
}
