#include "sys/types.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int uint;
typedef size_t *SC_Err;
static const size_t NO_ERROR = 1;
static const size_t NOT_FOUND = 2;
static const size_t MALLOC_FAILED = 3;
static const size_t ARENA_ALLOC_NO_SPACE = 4;
static const size_t OUT_OF_BOUNDS = 5;
static const size_t EMPTY_STRING = 6;
static const size_t INVALID_STRING = 7;
static const size_t INVALID_TXT_FILE = 8;

static const char *SC_Err_ToString(SC_Err err) {
  size_t val = *err;
  if (val == NO_ERROR) {
    return "No error found!";
  } else if (val == NOT_FOUND) {
    return "Element was not found!";
  } else if (val == MALLOC_FAILED) {
    return "malloc failed! Maybe out of memory?";
  } else if (val == ARENA_ALLOC_NO_SPACE) {
    return "Arena is out of space!";
  } else if (val == OUT_OF_BOUNDS) {
    return "Tried to access an index out of bounds!";
  } else if (val == EMPTY_STRING) {
    return "Tried to do some operation on an empty string!";
  } else if (val == INVALID_STRING) {
    return "The supplied string is invalid for the operation!";
  } else if (val == INVALID_TXT_FILE) {
    return "The supplied TXT file is invalid!";
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
void SC_Arena_Init(struct SC_Arena *arena, size_t initial_capacity,
                   SC_Err err) {

  char *data = malloc(initial_capacity);
  if (NULL == data) {
    *err = MALLOC_FAILED;
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

  SC_Bool has_space = arena->count + requested_size <= arena->capacity;
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
      *err = MALLOC_FAILED;
      return NULL;
    }
    SC_Arena_Init(arena->next, next_capacity, err);

    return SC_Arena_Alloc(arena->next, requested_size, err);
  }
}

// Resets the arena so that it can be used again.
// All data is kept, you simply override it when writing to it.
//
// All child arenas are reset too!
void SC_Arena_Reset(struct SC_Arena *arena) {
  if (arena->deinited) {
    SC_PANIC("Can reset an arena that is already deinited!");
    return;
  }

  if (NULL != arena->next) {
    SC_Arena_Reset(arena->next);
  }

  arena->count = 0;
}

// Frees the memory associated with this arena.
void SC_Arena_Deinit(struct SC_Arena *arena) {
  if (arena->deinited) {
    SC_PANIC("An arena should not be deinited more than 1 time!");
    return;
  }

  if (NULL != arena->next) {
    SC_Arena_Deinit(arena->next);
  }

  free(arena->data);
  arena->deinited = SC_TRUE;
}

typedef struct {
  char *data;
  size_t length;
  size_t data_capacity;
} SC_String;

SC_String SC_String_FromCString(char *c_str) {
  SC_String str = {
      .length = strlen(c_str),
      .data = c_str,
      .data_capacity = strlen(c_str),
  };
  return str;
}

const char *SC_String_ToCString(SC_String *str, struct SC_Arena *arena,
                                SC_Err err) {
  char *space = SC_Arena_Alloc(arena, str->length + 1, err);
  if (*err != NO_ERROR) {
    return NULL;
  }

  memcpy(space, str->data, str->length);
  space[str->length] = 0;
  return space;
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
  if (*err != NO_ERROR) {
    return str;
  }

  memcpy(str.data, other->data, other->length);
  str.data_capacity = other->length;
  str.length = other->length;

  return str;
}

/**
 * Tries to append a character at the end of the string.
 *
 * This function fails if no capacity available to hold the value.
 */
void SC_String_AppendChar(SC_String *str, char value, SC_Err err) {
  if (str->length >= str->data_capacity) {
    *err = OUT_OF_BOUNDS;
  } else {
    str->data[str->length] = value;
    str->length++;
  }
}

/**
 * Tries to parse a SC_String into an integer.
 *
 * Ignores starting whitespace, it may or may not have a '-' sign at the
 * beginning of the number stream. If any other character is found that isn't
 * numbers on other positions, INVALID_STRING is set on the err.
 */
int SC_String_ParseInt(SC_String *str, SC_Err err) {
  if (str->length <= 0) {
    *err = EMPTY_STRING;
    return 0;
  }

  int result = 0;
  int current_factor = 1;
  for (int i = str->length - 1; i > -1; i--) {
    char digit = SC_String_CharAt(str, i);
    switch (digit) {
    case '0' ... '9': {
      result += (digit - '0') * current_factor;
      current_factor *= 10;
    } break;
    case '-': {
      if (result != 0) {
        *err = INVALID_STRING;
        return 0;
      }

      current_factor *= -1;
    } break;

    case ' ' | '\t' | '\n' | '\r': {
      if (result != 0) {
        *err = INVALID_STRING;
        return 0;
      }
    } break;

    default: {
      *err = INVALID_STRING;
      return 0;
    } break;
    }
  }

  return result;
}

struct SC_StringList_Node {
  SC_String value;
  struct SC_StringList_Node *next;
};

typedef struct {
  size_t count;
  struct SC_StringList_Node *head;
  struct SC_StringList_Node *tail;
} SC_StringList;

void SC_StringList_Init(SC_StringList *list) {
  list->count = 0;
  list->head = NULL;
  list->tail = NULL;
}

void SC_StringList_Reset(SC_StringList *list) {
  list->count = 0;
  list->head = NULL;
  list->tail = NULL;
}

/**
 * Appends the string at the end of the list.
 *
 * It copies the string to the supplied arena, make sure the string list lives
 * at least as long as the arena so no data corruption is done.
 */
void SC_StringList_Append(SC_StringList *list, struct SC_Arena *arena,
                          SC_String str, SC_Err err) {
  struct SC_StringList_Node *node =
      SC_Arena_Alloc(arena, sizeof(struct SC_StringList_Node), err);
  if (*err != NO_ERROR) {
    return;
  }

  SC_String new_str = SC_String_CopyOnArena(arena, &str, err);
  if (*err != NO_ERROR) {
    return;
  }

  node->value = new_str;
  node->next = NULL;

  if (list->count == 0) {
    list->head = node;
    list->tail = node;
  } else if (list->count == 1) {
    list->head->next = node;
    list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }

  list->count++;
}

SC_String SC_StringList_GetAt(SC_StringList *list, size_t idx, SC_Err err) {
  SC_String sb = {};

  struct SC_StringList_Node *current = list->head;
  for (int i = 0; current != NULL && i < idx; i++) {
    current = current->next;
  }

  if (current == NULL) {
    *err = NOT_FOUND;
  } else {
    sb = current->value;
  }

  return sb;
}

// ===========
//  SLICES
// ===========

/**
 * A generic dynamic array (slice) implementation.
 * Stores a list of elements of any type using `void*` and manages resizing
 * automatically.
 * How to iterate:
 *
 *  Resource* resources = (Resource*)slice.data;
 *  for (size_t i = 0; i < slice.length; i++) {
 *      Resource r = resources[i];
 *     printf("Resource ID: %d, Counter: %d\n", r.id, r.counter);
 *  }
 */
typedef struct {
  void *data;
  /** Number of elements currently in the slice. */
  size_t length;

  /** Maximum number of elements the slice can hold before reallocating. */
  size_t capacity;

  /** Size (in bytes) of each element. Required for generic memory operations.
   */
  size_t element_size;
} SC_Slice;

/**
 * Initializes a new slice with a given element size and initial capacity.
 *
 * @param s Pointer to the SC_Slice to initialize.
 * @param element_size Size of each element in bytes.
 * @param initial_capacity Initial number of elements the slice can hold.
 */
void SC_Slice_init(SC_Slice *s, size_t element_size, size_t initial_capacity) {
  s->data = malloc(element_size * initial_capacity);
  if (!s->data) {
    perror("malloc failed");
    exit(1);
  }
  s->length = 0;
  s->capacity = initial_capacity;
  s->element_size = element_size;
}

/**
 * Appends a new element to the slice. Automatically reallocates
 * if the current capacity is exceeded.
 *
 * @param s Pointer to the SC_Slice to append to.
 * @param element Pointer to the element to append.
 */
void SC_Slice_append(SC_Slice *s, void *element) {
  if (s->length == s->capacity) {
    size_t new_capacity = s->capacity * 2;
    void *new_data = malloc(s->element_size * new_capacity);
    if (!new_data) {
      perror("realloc failed");
      exit(1);
    }

    memcpy(new_data, s->data, s->element_size * s->length);
    free(s->data);
    s->data = new_data;
    s->capacity = new_capacity;
  }

  void *dest = (char *)s->data + (s->length * s->element_size);
  memcpy(dest, element, s->element_size);
  s->length++;
}

/**
 * Frees the memory used by the slice and resets its fields.
 *
 * @param s Pointer to the SC_Slice to deinitialize.
 */
void SC_Slice_deinit(SC_Slice *s) {
  free(s->data);
  s->data = NULL;
  s->length = 0;
  s->capacity = 0;
  s->element_size = 0;
}

// ##################################
// #                                #
// #       CALENDARIZER             #
// #                                #
// ##################################

typedef struct {
  size_t pid_idx;
  uint burst_time;
  uint arrival_time;
  uint priority;
} SC_Process;

typedef struct SC_ProcessList_Node {
  SC_Process value;
  struct SC_ProcessList_Node *next;
} SC_ProcessList_Node;

typedef struct {
  size_t count;
  struct SC_ProcessList_Node *head;
  struct SC_ProcessList_Node *tail;
} SC_ProcessList;

void SC_ProcessList_sort(SC_ProcessList *list,
                         int (*cmp)(SC_Process, SC_Process));
int compare_by_arrival_time(SC_Process a, SC_Process b);
int SC_Total_busrt_time(SC_ProcessList *list);

void SC_ProcessList_Init(SC_ProcessList *list) {
  list->count = 0;
  list->head = NULL;
  list->tail = NULL;
}

void SC_ProcessList_Reset(SC_ProcessList *list) {
  list->count = 0;
  list->head = NULL;
  list->tail = NULL;
}

void SC_ProcessList_Append(SC_ProcessList *list, struct SC_Arena *arena,
                           SC_Process process, SC_Err err) {
  struct SC_ProcessList_Node *node =
      SC_Arena_Alloc(arena, sizeof(struct SC_ProcessList_Node), err);
  if (*err != NO_ERROR) {
    return;
  }

  node->value = process;
  node->next = NULL;

  if (list->count == 0) {
    list->head = node;
    list->tail = node;
  } else if (list->count == 1) {
    list->head->next = node;
    list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }

  list->count++;
}

SC_Process SC_ProcessList_GetAt(SC_ProcessList *list, size_t idx, SC_Err err) {
  SC_Process sb = {};

  struct SC_ProcessList_Node *current = list->head;
  for (int i = 0; current != NULL && i < idx; i++) {
    current = current->next;
  }

  if (current == NULL) {
    *err = NOT_FOUND;
  } else {
    sb = current->value;
  }

  return sb;
}

/**
 * Saves all the state needed to render a single step in the animation.
 *
 * Each step should own it's memory! So we can free one step and not affect
 * all the others!
 */
typedef struct {
  size_t current_process;
  size_t process_length;
  SC_Process *processes;
} SC_SimStepState;

/**
 * Saves all the steps a simulation can have.
 */
typedef struct {
  size_t step_length;
  size_t current_step;
  float avg_waiting_time;
  SC_SimStepState *steps;
} SC_Simulation;

/**
 * Computes the FIFO scheduling simulation
 *
 * @param processes *SC_ProcessList The initial conditions of each
 * process.
 * @param sim *SC_Simulation The simulation state to fill with all the sim step
 * data.
 * @return SC_Simulation The simulation with all it's steps.
 */
void simulate_first_in_first_out(SC_ProcessList *processes,
                                 SC_Simulation *sim) {
  // TODO: Fill with real data and not this dummy data...
  SC_ProcessList_sort(processes, compare_by_arrival_time);

  int totalBurstTime = SC_Total_busrt_time(processes);

  sim->step_length = totalBurstTime;
  sim->current_step = 0;
  sim->steps = malloc(sizeof(SC_SimStepState) * totalBurstTime);

  SC_ProcessList_Node *current = processes->head;
  int time = 0;
  while (current != NULL) {
    SC_Process proc = current->value;

    for (int i = 0; i < (int)proc.burst_time; i++) {
      SC_SimStepState *step = &sim->steps[time];
      step->current_process = proc.pid_idx;

      step->process_length = processes->count;
      step->processes = malloc(sizeof(SC_Process) * processes->count);

      SC_ProcessList_Node *copy_node = processes->head;
      int j = 0;
      while (copy_node != NULL) {
        step->processes[j++] = copy_node->value;
        copy_node = copy_node->next;
      }

      time++;
    }

    current = current->next;
  }

  // for (int step_i = 0; step_i < 2; step_i++) {
  //   sim->steps[step_i].process_length = processes->count;

  //   int j = 0;
  //   for (struct SC_ProcessList_Node *current = processes->head; current !=
  //   NULL;
  //        current = current->next) {
  //     sim->steps[step_i].processes[j] = current->value;
  //     j++;
  //   }

  //   size_t current_process = 0;
  //   if (step_i == 1) {
  //     current_process = 2;
  //   }
  //   sim->steps[step_i].current_process = current_process;
  // }
}

void simulate_shortest_first(SC_ProcessList *processes, SC_Simulation *sim) {}
void simulate_shortest_remaining(SC_ProcessList *processes,
                                 SC_Simulation *sim) {}
void simulate_round_robin(SC_ProcessList *processes, SC_Simulation *sim,
                          int quantum) {}
void simulate_priority(SC_ProcessList *processes, SC_Simulation *sim) {}

void parse_scheduling_file(SC_String *file_contents,
                           struct SC_Arena *pids_arena,
                           struct SC_Arena *processes_arena,
                           SC_StringList *pid_list, SC_ProcessList *processes,
                           SC_Err err) {
  const int b_max_length = 255;
  char b_data[b_max_length]; // Max length of a column
  SC_String buffer = {
      .data = b_data,
      .length = 0,
      .data_capacity = b_max_length,
  };
  SC_Process current_process = {0};

  int current_column = 1;
  for (int i = 0; i < file_contents->length; i++) {
    char current_char = SC_String_CharAt(file_contents, i);
    switch (current_char) {
    case ' ': {
    } break;
    case '\n': {

      if (4 != current_column) {
        *err = INVALID_TXT_FILE;
        return;
      }

      int column_value = SC_String_ParseInt(&buffer, err);
      if (*err != NO_ERROR) {
        return;
      }
      current_process.priority = column_value;

      SC_ProcessList_Append(processes, processes_arena, current_process, err);
      if (*err != NO_ERROR) {
        return;
      }

      buffer.length = 0;
      current_column = 1;
      SC_Process zero_process = {0};
      current_process = zero_process;
    } break;

    case ',': {
      if (1 == current_column) {
        // Add null terminator to PIDs
        SC_String_AppendChar(&buffer, 0, err);
        if (*err != NO_ERROR) {
          return;
        }

        SC_StringList_Append(pid_list, pids_arena, buffer, err);
        if (*err != NO_ERROR) {
          return;
        }
        current_process.pid_idx = pid_list->count - 1;
      } else {
        int column_value = SC_String_ParseInt(&buffer, err);
        if (*err != NO_ERROR) {
          return;
        }

        if (2 == current_column) {
          current_process.burst_time = column_value;
        } else if (3 == current_column) {
          current_process.arrival_time = column_value;
        } else {
          *err = INVALID_TXT_FILE;
          return;
        }
      }

      buffer.length = 0;
      current_column++;
    } break;

    default: {
      SC_String_AppendChar(&buffer, current_char, err);
      if (*err != NO_ERROR) {
        return;
      }
    }
    }
  }
}

// ##################################
// #                                #
// #       MUTEX/SEMPAHORES         #
// #                                #
// ##################################

/**
 * Represents the possible states of a process in the simulation.
 */
typedef enum {
  STATE_READY,
  /** The process is currently using a resource. */
  STATE_ACCESSED,
  /** The process is waiting for a resource to become available. */
  STATE_WAITING,
  /** The process does not need any resource but still has burst time to
     consume. */
  STATE_COMPUTING,
  /** The process has completed all its burst time. */
  STATE_FINISHED
} SC_ProcessState;

/**
 * Defines the synchronization mechanism used by a resource.
 */
typedef enum {
  /** Only one process can access the resource per cycle, regardless of
     available instances. */
  SYNC_MUTEX,
  /** Up to "n" processes can access the resource concurrently. */
  SYNC_SEMAPHORE
} SC_SyncMechanism;

/**
 * Represents a process participating in the simulation.
 */
typedef struct {
  int id;
  /** Minimum number of cycles the process should run. */
  int burst_time;
  int arrival_time;
  /**
   * Determines execution order when multiple processes want the same resource.
   * Lower numbers indicate higher priority. */
  int priority;
  /** The current execution state of the process. */
  SC_ProcessState current_state;
  /** Remaining cycles to run. */
  int remaining_time;
  /** Indicates whether the process is currently active. */
  SC_Bool is_active;
} SC_SyncProcess;

/**
 * Represents an action a process wants to perform on a resource.
 */
typedef struct {
  int id;
  /** ID of the process performing the action. */
  int pid;
  /** ID of the resource to access. */
  int resource_id;
  /**
   * Cycle in which the action should be attempted.
   * If a process must wait for the resource, this value is updated to the next
   * cycle. */
  int cycle;
  /** Indicates whether this action has been executed. */
  SC_Bool executed;
  /** Process priority at the time of the action. */
  int priority;
} SC_Action;

/**
 * Represents a shared resource controlled by a synchronization mechanism.
 */
typedef struct {
  int id;
  /** For semaphores: available count. For mutexes: 1 or 0. */
  int counter;
  /** Maximum value the counter can hold (initial value). */
  int max_counter;
  /** Synchronization mechanism used (mutex or semaphore). */
  SC_SyncMechanism sync_type;
  /**
   * Unordered list of actions related to this resource.
   * Requires a full scan to process all actions.
   */
  SC_Action *actions;
  /** Total number of actions in the list. */
  int action_count;
} SC_Resource;

/**
 * Represents a single step in a process's execution timeline.
 * One entry must be generated per process for each simulation step
 * until the process finishes.
 */
typedef struct {
  /** Process state in this cycle. */
  SC_ProcessState state;
  /** ID of the action performed (if any). */
  int action_id;
  /** ID of the resource used, or -1 if none. */
  int resource_id;
} ProcessTimelineEntry;

/**
 * Represents the execution history of a process.
 */
typedef struct {
  int id;

  /**
   * Dynamic list of timeline entries representing each simulation step.
   * Automatically grows as new entries are appended.
   */
  SC_Slice entries;
} ProcessTimeline;

/**
 * Represents the full state of the simulation.
 * Updated on each step to reflect the new state and register timeline actions.
 */
typedef struct {
  /** List of all processes in the simulation. */
  SC_SyncProcess *processes;
  int process_count;

  /** List of all resources used in the simulation. */
  SC_Resource *resources;
  int resource_count;

  /** One timeline per process, tracking its execution history. */
  ProcessTimeline *process_timelines;
  int timeline_count;

  /** Current simulation cycle. */
  int current_cycle;

  /** Indicates whether the simulation is still running. */
  SC_Bool simulation_running;

  int semaphore_count;

  /** Total number of simulation cycles. */
  int total_cycles;
} SC_SyncSimulator;

void SC_SyncSimulator_init(SC_SyncSimulator *simu) {}

/**
 * Takes a Simulation objects and computes its next step inplace
 *
 * SIMULATION ==> f(x) ==> NEW SIMULATION
 *
 * The next step should be registered on
 */
void SC_SyncSimulator_next(SC_SyncSimulator *s) {
  // Pseudo code (just a suggestion)
  //
  // process_visited = []
  //
  // for each resources:
  //    actions = []
  //
  //    for a in actions:
  //      if a in this cycle => actions.append(a)
  //      else => continue
  //
  //    while len(consumed_actions) != len(actions):
  //      a = find_nex_action_with_highest_priority()
  //
  //      if simulation.mode = MUTEX and consumed_resource != 1:
  //        resources_count--
  //      if simulation.mode = SEMAPHORE and consumed_resource != n:
  //        resources_count--
  //
  //      p.status = STATE_ACCESSED
  //      a.states = finished
  //
  // for each process:
  //    if not action executed:
  //      p.STATUS = COMPUTING
  //
  // reset_resource_counters()
  //
}

void parse_syncProcess_file(SC_String *file_contents,
                            struct SC_Arena *pids_arena,
                            struct SC_Arena *processes_arena,
                            SC_StringList *pid_list, SC_ProcessList *processes,
                            SC_Err err) {}

// ##################################
// #                                #
// #         MISCELLANEOUS          #
// #                                #
// ##################################

// Comparison functions
int compare_by_arrival_time(SC_Process a, SC_Process b) {
  return (int)a.arrival_time - (int)b.arrival_time;
}

int compare_by_burst_time(SC_Process a, SC_Process b) {
  return (int)a.burst_time - (int)b.burst_time;
}

int compare_by_priority(SC_Process a, SC_Process b) {
  return (int)a.priority - (int)b.priority;
}

// Merge sort implementation
SC_ProcessList_Node *mergedSorted(SC_ProcessList_Node *a,
                                  SC_ProcessList_Node *b,
                                  int (*cmp)(SC_Process, SC_Process)) {
  if (!a)
    return b;
  if (!b)
    return a;

  if (cmp(a->value, b->value) <= 0) {
    a->next = mergedSorted(a->next, b, cmp);
    return a;
  } else {
    b->next = mergedSorted(a, b->next, cmp);
    return b;
  }
}

void splitList(SC_ProcessList_Node *source, SC_ProcessList_Node **front,
               SC_ProcessList_Node **back) {
  SC_ProcessList_Node *slow = source;
  SC_ProcessList_Node *fast = source->next;

  while (fast) {
    fast = fast->next;
    if (fast) {
      slow = slow->next;
      fast = fast->next;
    }
  }

  *front = source;
  *back = slow->next;
  slow->next = NULL;
}

void mergeSort(SC_ProcessList_Node **headRef,
               int (*cmp)(SC_Process, SC_Process)) {
  SC_ProcessList_Node *head = *headRef;
  if (!head || !head->next)
    return;

  SC_ProcessList_Node *a, *b;
  splitList(head, &a, &b);

  mergeSort(&a, cmp);
  mergeSort(&b, cmp);

  *headRef = mergedSorted(a, b, cmp);
}

void SC_ProcessList_sort(SC_ProcessList *list,
                         int (*cmp)(SC_Process, SC_Process)) {
  mergeSort(&(list->head), cmp);

  SC_ProcessList_Node *curr = list->head;
  while (curr && curr->next) {
    curr = curr->next;
  }
  list->tail = curr;
}

// Total burst time
// Calculates the total bust time of the scheduler process
int SC_Total_busrt_time(SC_ProcessList *list) {
  int totalTime = 0;
  SC_ProcessList_Node *current = list->head;
  while (current != NULL) {
    SC_Process proc = current->value;
    totalTime += proc.burst_time;
    current = current->next;
  }

  return totalTime;
}
