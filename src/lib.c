#include "sys/types.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ################################
// ||                            ||
// ||          UTILITIES         ||
// ||                            ||
// ################################

static int SC_Min(int a, int b) { return a < b ? a : b; }

// ################################
// ||                            ||
// ||           ERRORS           ||
// ||                            ||
// ################################

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
static const size_t RESOURCE_NOT_FOUND = 9;
static const size_t PROCESS_NOT_FOUND = 10;
static const size_t SLICE_EXPANSION_FAILED = 11;

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
  } else if (val == RESOURCE_NOT_FOUND) {
    return "Resource not found!";
  } else if (val == PROCESS_NOT_FOUND) {
    return "Process not found!";
  } else if (val == SLICE_EXPANSION_FAILED) {
    return "Slice expansion failed!";
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
      if (i != 0) {
        *err = INVALID_STRING;
        return 0;
      }

      current_factor *= -1;
    } break;

    default: {
      *err = INVALID_STRING;
      return 0;
    } break;
    }
  }

  return result;
}

/**
 * Removes all characters that match search from the start of the string.
 */
void SC_String_TrimStart(SC_String *str, char search) {
  for (int i = 0; i < str->length; i++) {
    if (str->data[i] != search) {
      break;
    }

    str->length -= 1;
    int next_pos = i + 1;
    memmove(str->data, str->data + next_pos, str->length);
    i--;
  }
}

int SC_String_LineCount(SC_String *str) {
  if (str == NULL || str->data == NULL || str->length == 0) {
    return 0;
  }

  int count = 1; // At least one line even if no '\n'
  for (size_t i = 0; i < str->length; ++i) {
    if (str->data[i] == '\n') {
      count++;
    }
  }

  return count;
}

SC_String SC_String_GetLine(SC_String *str, size_t line_number) {
  size_t start = 0;
  size_t current_line = 0;

  // Find the start index of the desired line
  for (size_t i = 0; i < str->length; ++i) {
    if (SC_String_CharAt(str, i) == '\n') {
      if (current_line == line_number) {
        break;
      }
      current_line++;
      start = i + 1;
    }
  }

  // If we never reached the desired line
  if (current_line < line_number) {
    return (SC_String){.data = NULL, .length = 0, .data_capacity = 0};
  }

  // Find the end index of the line
  size_t end = start;
  while (end < str->length && SC_String_CharAt(str, end) != '\n') {
    end++;
  }

  return (SC_String){
      .data = &str->data[start],
      .length = end - start,
      .data_capacity = 0 // capacity is not meaningful here
  };
}

SC_String SC_String_GetCSVColumn(SC_String *str, size_t column_index) {
  size_t start = 0;
  size_t current_col = 0;

  for (size_t i = 0; i <= str->length; ++i) {
    // Either end of string or comma
    if (i == str->length || SC_String_CharAt(str, i) == ',') {
      if (current_col == column_index) {
        return (SC_String){
            .data = &str->data[start],
            .length = i - start,
            .data_capacity = 0 // meaningless in a slice
        };
      }
      current_col++;
      start = i + 1;
    }
  }

  // Column index out of bounds
  return (SC_String){.data = NULL, .length = 0, .data_capacity = 0};
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
void SC_Slice_init(SC_Slice *s, size_t element_size, size_t initial_capacity,
                   SC_Err err) {
  s->data = malloc(element_size * initial_capacity);
  if (!s->data) {
    *err = MALLOC_FAILED;
    return;
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
void SC_Slice_append(SC_Slice *s, void *element, SC_Err err) {
  if (s->length == s->capacity) {
    size_t new_capacity = s->capacity * 2;
    void *new_data = malloc(s->element_size * new_capacity);
    if (new_data == NULL) {
      *err = SLICE_EXPANSION_FAILED;
      return;
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
  uint waiting_time;
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
int compare_by_burst_time(SC_Process a, SC_Process b);
int compare_by_priority(SC_Process a, SC_Process b);
int SC_Total_busrt_time(SC_ProcessList *list);
int compare_proc_ptrAT(const void *a, const void *b);
int compare_proc_ptrBT(const void *a, const void *b);
int compare_proc_ptrP(const void *a, const void *b);

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

SC_Process SC_Verify_AT_BT(SC_ProcessList *list, int step);
static int compare_proc_ptr(const void *a, const void *b) {
  const SC_Process *pa = *(const SC_Process **)a;
  const SC_Process *pb = *(const SC_Process **)b;
  return compare_by_arrival_time(*pa, *pb);
}
/**
 * Computes the FIFO scheduling simulation
 *
 * @param processes *SC_ProcessList The initial conditions of each
 * process.
 * @param sim *SC_Simulation The simulation state to fill with all the sim
 * step data.
 * @return SC_Simulation The simulation with all it's steps.
 */
void simulate_first_in_first_out(SC_ProcessList *processes,
                                 SC_Simulation *sim) {
  int n = processes->count;

  SC_Process **proc_array = malloc(sizeof(SC_Process *) * n);
  SC_ProcessList_Node *node = processes->head;
  for (int i = 0; i < n && node != NULL; i++, node = node->next) {
    proc_array[i] = &node->value;
  }

  qsort(proc_array, n, sizeof(SC_Process *), compare_proc_ptrAT);

  int totalBurstTime = 0;
  for (int i = 0; i < n; i++) {
    totalBurstTime += proc_array[i]->burst_time;
  }

  sim->step_length = totalBurstTime;
  sim->current_step = 0;
  sim->steps = malloc(sizeof(SC_SimStepState) * totalBurstTime);

  int *remaining_bursts = malloc(sizeof(int) * n);
  for (int i = 0; i < n; i++) {
    remaining_bursts[i] = proc_array[i]->burst_time;
  }

  int time = 0;
  int current_time = 0;

  for (int pidx = 0; pidx < n; pidx++) {
    SC_Process *proc = proc_array[pidx];

    if (current_time < proc->arrival_time) {
      current_time = proc->arrival_time;
    }

    int waiting_time = current_time;
    proc->waiting_time = waiting_time;

    for (int b = 0; b < proc->burst_time; b++) {
      SC_SimStepState *step = &sim->steps[time];
      step->current_process = proc->pid_idx;
      step->process_length = n;
      step->processes = malloc(sizeof(SC_Process) * n);

      SC_ProcessList_Node *orig = processes->head;
      int j = 0;
      while (orig != NULL) {
        SC_Process copy = orig->value;

        for (int k = 0; k < n; k++) {
          if (copy.pid_idx == proc_array[k]->pid_idx) {
            int adjusted_burst = remaining_bursts[k] - (k == pidx ? 1 : 0);
            copy.burst_time = adjusted_burst < 0 ? 0 : adjusted_burst;
            if (k == pidx) {
              copy.waiting_time = waiting_time;
            } else {
              copy.waiting_time = 0;
            }
            break;
          }
        }

        step->processes[j++] = copy;
        orig = orig->next;
      }

      remaining_bursts[pidx]--;
      time++;
      current_time++;
    }
  }

  float total_waiting_time = 0;
  SC_ProcessList_Node *nnode = processes->head;
  while (nnode != NULL) {
    total_waiting_time += nnode->value.waiting_time;
    nnode = nnode->next;
  }
  sim->avg_waiting_time = total_waiting_time / (float)n;

  free(remaining_bursts);
  free(proc_array);
}

void simulate_shortest_first(SC_ProcessList *processes, SC_Simulation *sim) {
  int n = processes->count;

  SC_Process **proc_array = malloc(sizeof(SC_Process *) * n);
  SC_ProcessList_Node *node = processes->head;
  for (int i = 0; i < n && node != NULL; i++, node = node->next) {
    proc_array[i] = &node->value;
  }

  qsort(proc_array, n, sizeof(SC_Process *), compare_proc_ptrBT);

  int totalBurstTime = 0;
  for (int i = 0; i < n; i++) {
    totalBurstTime += proc_array[i]->burst_time;
  }

  sim->step_length = totalBurstTime;
  sim->current_step = 0;
  sim->steps = malloc(sizeof(SC_SimStepState) * totalBurstTime);

  int *remaining_bursts = malloc(sizeof(int) * n);
  for (int i = 0; i < n; i++) {
    remaining_bursts[i] = proc_array[i]->burst_time;
  }

  int time = 0;
  int elapsed_time = 0;

  for (int pidx = 0; pidx < n; pidx++) {
    SC_Process *proc = proc_array[pidx];

    int waiting_time = elapsed_time;
    if (waiting_time < 0)
      waiting_time = 0;

    proc->waiting_time = waiting_time;

    for (int b = 0; b < proc->burst_time; b++) {
      SC_SimStepState *step = &sim->steps[time];
      step->current_process = proc->pid_idx;

      step->process_length = n;
      step->processes = malloc(sizeof(SC_Process) * n);

      SC_ProcessList_Node *orig = processes->head;
      int j = 0;
      while (orig != NULL) {
        SC_Process copy = orig->value;

        for (int k = 0; k < n; k++) {
          if (copy.pid_idx == proc_array[k]->pid_idx) {
            int adjusted_burst = remaining_bursts[k] - (k == pidx ? 1 : 0);
            copy.burst_time = adjusted_burst < 0 ? 0 : adjusted_burst;
            if (k == pidx) {
              copy.waiting_time = waiting_time;
            } else {
              copy.waiting_time = 0;
            }
            break;
          }
        }

        step->processes[j++] = copy;
        orig = orig->next;
      }

      remaining_bursts[pidx]--;
      time++;
    }

    elapsed_time += proc->burst_time;
  }

  float total_waiting_time = 0;
  SC_ProcessList_Node *ptr = processes->head;
  while (ptr != NULL) {
    total_waiting_time += ptr->value.waiting_time;
    ptr = ptr->next;
  }

  sim->avg_waiting_time = (total_waiting_time) / (float)n;

  free(remaining_bursts);
  free(proc_array);
}

void simulate_shortest_remaining(SC_ProcessList *processes,
                                 SC_Simulation *sim) {
  if (!processes || !sim || processes->count == 0)
    return;

  int totalBurstTime = SC_Total_busrt_time(processes) + 1;
  if (totalBurstTime <= 0)
    return;

  sim->steps = calloc(totalBurstTime, sizeof(SC_SimStepState));
  sim->step_length = totalBurstTime;
  sim->current_step = 0;

  int n = processes->count;

  SC_Process **proc_array = malloc(sizeof(SC_Process *) * n);
  SC_ProcessList_Node *node = processes->head;
  for (int i = 0; i < n && node != NULL; i++, node = node->next) {
    proc_array[i] = &node->value;
  }

  int *remaining_time = malloc(n * sizeof(int));
  int *start_time = malloc(n * sizeof(int));
  int *finish_time = malloc(n * sizeof(int));

  if (!remaining_time || !start_time || !finish_time || !proc_array) {
    free(proc_array);
    free(remaining_time);
    free(start_time);
    free(finish_time);
    return;
  }

  for (int i = 0; i < n; i++) {
    remaining_time[i] = proc_array[i]->burst_time;
    start_time[i] = -1;
    finish_time[i] = -1;
  }

  int time = 0;
  int completed = 0;

  while (completed < n && time < totalBurstTime - 1) {
    int shortest = -1;
    for (int j = 0; j < n; j++) {
      if (proc_array[j]->arrival_time <= time && remaining_time[j] > 0) {
        if (shortest == -1 || remaining_time[j] < remaining_time[shortest]) {
          shortest = j;
        }
      }
    }

    SC_SimStepState *step = &sim->steps[time];
    step->process_length = n;
    step->current_process =
        (shortest != -1) ? proc_array[shortest]->pid_idx : -1;
    step->processes = malloc(sizeof(SC_Process) * n);

    SC_ProcessList_Node *temp = processes->head;
    for (int k = 0; k < n && temp != NULL; k++, temp = temp->next) {
      step->processes[k] = temp->value;
      step->processes[k].burst_time = remaining_time[k];
    }

    if (shortest != -1) {
      if (start_time[shortest] == -1)
        start_time[shortest] = time;
      remaining_time[shortest]--;
      if (remaining_time[shortest] == 0) {
        finish_time[shortest] = time + 1;
        completed++;
      }
    }
    time++;
  }

  if (time < totalBurstTime) {
    SC_SimStepState *step = &sim->steps[time];
    step->process_length = n;
    step->current_process = -1;
    step->processes = malloc(sizeof(SC_Process) * n);

    SC_ProcessList_Node *temp = processes->head;
    for (int k = 0; k < n && temp != NULL; k++, temp = temp->next) {
      step->processes[k] = temp->value;
      step->processes[k].burst_time = 0;
    }
    sim->step_length = time + 1;
  }

  float total_waiting = 0.0f;
  for (int j = 0; j < n; j++) {
    int turnaround = finish_time[j] - proc_array[j]->arrival_time;
    int waiting = turnaround - proc_array[j]->burst_time;

    if (waiting < 0)
      waiting = 0;

    proc_array[j]->waiting_time = waiting;
    total_waiting += waiting;
  }
  sim->avg_waiting_time = total_waiting / (float)n;

  free(proc_array);
  free(remaining_time);
  free(start_time);
  free(finish_time);
}

void simulate_round_robin(SC_ProcessList *processes, SC_Simulation *sim,
                          int quantum) {
  if (!processes || !sim || processes->count == 0 || quantum <= 0)
    return;

  SC_ProcessList_sort(processes, compare_by_arrival_time);
  int totalBurstTime = SC_Total_busrt_time(processes) + 1;
  int n = processes->count;

  sim->steps = calloc(totalBurstTime * 2, sizeof(SC_SimStepState));
  sim->step_length = 0;
  sim->current_step = 0;

  SC_Process **proc_array = malloc(sizeof(SC_Process *) * n);
  SC_ProcessList_Node *node = processes->head;
  for (int i = 0; i < n && node != NULL; i++, node = node->next) {
    proc_array[i] = &node->value;
  }

  int *remaining_time = malloc(sizeof(int) * n);
  int *start_time = malloc(sizeof(int) * n);
  int *finish_time = malloc(sizeof(int) * n);
  int *visited = calloc(n, sizeof(int));

  if (!proc_array || !remaining_time || !start_time || !finish_time ||
      !visited) {
    free(proc_array);
    free(remaining_time);
    free(start_time);
    free(finish_time);
    free(visited);
    return;
  }

  for (int i = 0; i < n; i++) {
    remaining_time[i] = proc_array[i]->burst_time;
    start_time[i] = -1;
    finish_time[i] = -1;
  }

  int *queue = malloc(sizeof(int) * totalBurstTime * 2);
  if (!queue) {
    free(proc_array);
    free(remaining_time);
    free(start_time);
    free(finish_time);
    free(visited);
    return;
  }

  int front = 0, rear = 0;
  int time = 0;
  int completed = 0;
  int current_process = -1;
  int time_slice = 0;

  while (completed < n || current_process != -1) {
    for (int j = 0; j < n; j++) {
      if (proc_array[j]->arrival_time == time && remaining_time[j] > 0 &&
          !visited[j]) {
        queue[rear++] = j;
        visited[j] = 1;
      }
    }

    if (current_process == -1 && front < rear) {
      current_process = queue[front++];
      time_slice = 0;
    }

    SC_SimStepState *step = &sim->steps[sim->step_length++];
    step->current_process =
        (current_process != -1) ? proc_array[current_process]->pid_idx : -1;
    step->process_length = n;
    step->processes = malloc(sizeof(SC_Process) * n);

    SC_ProcessList_Node *temp = processes->head;
    for (int k = 0; k < n && temp; k++, temp = temp->next) {
      step->processes[k] = temp->value;
      step->processes[k].burst_time = remaining_time[k];
    }

    if (current_process != -1) {
      if (start_time[current_process] == -1)
        start_time[current_process] = time;

      remaining_time[current_process]--;
      time_slice++;

      if (remaining_time[current_process] == 0) {
        finish_time[current_process] = time + 1;
        completed++;
        current_process = -1;
      } else if (time_slice == quantum) {
        queue[rear++] = current_process;
        current_process = -1;
      }
    }
    time++;
  }

  float total_waiting = 0.0f;
  for (int j = 0; j < n; j++) {
    int turnaround = finish_time[j] - proc_array[j]->arrival_time;
    int waiting = turnaround - proc_array[j]->burst_time;

    if (waiting < 0)
      waiting = 0;

    proc_array[j]->waiting_time = waiting;
    total_waiting += waiting;
  }

  sim->avg_waiting_time = total_waiting / (float)n;

  free(proc_array);
  free(remaining_time);
  free(start_time);
  free(finish_time);
  free(queue);
  free(visited);
}

void simulate_priority(SC_ProcessList *processes, SC_Simulation *sim) {
  int n = processes->count;

  SC_Process **proc_array = malloc(sizeof(SC_Process *) * n);
  SC_ProcessList_Node *node = processes->head;
  for (int i = 0; i < n && node != NULL; i++, node = node->next) {
    proc_array[i] = &node->value;
  }

  qsort(proc_array, n, sizeof(SC_Process *), compare_proc_ptrP);

  int totalBurstTime = 0;
  for (int i = 0; i < n; i++) {
    totalBurstTime += proc_array[i]->burst_time;
  }

  sim->step_length = totalBurstTime;
  sim->current_step = 0;
  sim->steps = malloc(sizeof(SC_SimStepState) * totalBurstTime);

  int *remaining_bursts = malloc(sizeof(int) * n);
  for (int i = 0; i < n; i++) {
    remaining_bursts[i] = proc_array[i]->burst_time;
  }

  int time = 0;
  int elapsed_time = 0;
  float total_waiting_time = 0;

  for (int pidx = 0; pidx < n; pidx++) {
    SC_Process *proc = proc_array[pidx];

    int waiting_time = elapsed_time - proc->arrival_time;
    if (waiting_time < 0)
      waiting_time = 0;

    total_waiting_time += waiting_time;

    for (int b = 0; b < proc->burst_time; b++) {
      SC_SimStepState *step = &sim->steps[time];
      step->current_process = proc->pid_idx;

      step->process_length = n;
      step->processes = malloc(sizeof(SC_Process) * n);

      SC_ProcessList_Node *orig = processes->head;
      int j = 0;
      while (orig != NULL) {
        SC_Process copy = orig->value;

        for (int k = 0; k < n; k++) {
          if (copy.pid_idx == proc_array[k]->pid_idx) {
            int adjusted_burst = remaining_bursts[k] - (k == pidx ? 1 : 0);
            copy.burst_time = adjusted_burst < 0 ? 0 : adjusted_burst;
            if (k == pidx) {
              copy.waiting_time = waiting_time;
            } else {
              copy.waiting_time = 0;
            }
            break;
          }
        }

        step->processes[j++] = copy;
        orig = orig->next;
      }

      remaining_bursts[pidx]--;
      time++;
    }

    elapsed_time += proc->burst_time;
  }

  sim->avg_waiting_time = total_waiting_time / (float)n;

  free(remaining_bursts);
  free(proc_array);
}

void parse_scheduling_file(SC_String *file_contents,
                           struct SC_Arena *pids_arena,
                           struct SC_Arena *processes_arena,
                           SC_StringList *pid_list, SC_ProcessList *processes,
                           SC_Err err) {
  const int b_max_length = 255;
  char b_data[b_max_length];
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
    case '\n': {

      if (4 != current_column) {
        *err = INVALID_TXT_FILE;
        return;
      }

      SC_String_TrimStart(&buffer, ' ');
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
        SC_String_TrimStart(&buffer, ' ');
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
} SC_ProcessTimelineEntry;

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
} SC_ProcessTimeline;

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
  SC_ProcessTimeline *process_timelines;
  int timeline_count;

  /** Current simulation cycle. */
  int current_cycle;

  /** Indicates whether the simulation is still running. */
  SC_Bool simulation_running;

  /** Synchronization mechanism used (mutex or semaphore). */
  SC_SyncMechanism sync_type;

  int semaphore_count;

  /** Total number of simulation cycles. */
  int total_cycles;
} SC_SyncSimulator;

// You can adjust this enum name/values based on your actual definition
const char *process_state_to_string(SC_ProcessState state) {
  switch (state) {
  case STATE_READY:
    return "READY";
  case STATE_ACCESSED:
    return "ACCESING";
  case STATE_COMPUTING:
    return "RUNNING";
  case STATE_WAITING:
    return "WAITING";
  case STATE_FINISHED:
    return "FINISHED";
  default:
    return "UNKNOWN";
  }
}

void print_sync_process(const SC_SyncProcess *process) {
  if (process == NULL) {
    fprintf(stderr, "SC_SyncProcess is NULL\n");
    return;
  }

  fprintf(stderr, "SC_SyncProcess:\n");
  fprintf(stderr, "  ID: %d\n", process->id);
  fprintf(stderr, "  Arrival Time: %d\n", process->arrival_time);
  fprintf(stderr, "  Burst Time: %d\n", process->burst_time);
  fprintf(stderr, "  Priority: %d\n", process->priority);
  fprintf(stderr, "  Remaining Time: %d\n", process->remaining_time);
  fprintf(stderr, "  Current State: %s\n",
          process_state_to_string(process->current_state));
}

void print_sc_action(const SC_Action *action) {
  if (!action) {
    fprintf(stderr, "SC_Action_Print: NULL pointer provided.\n");
    return;
  }

  fprintf(stderr,
          "SC_Action { id: %d, pid: %d, resource_id: %d, cycle: %d, executed: "
          "%s, priority: %d }\n",
          action->id, action->pid, action->resource_id, action->cycle,
          action->executed ? "true" : "false", action->priority);
}

void print_sc_resource(const SC_Resource *res) {
  fprintf(stderr, "Resource ID: %d\n", res->id);
  fprintf(stderr, "  Counter: %d\n", res->counter);
  fprintf(stderr, "  Max Counter: %d\n", res->max_counter);
  fprintf(stderr, "  Action Count: %d\n", res->action_count);

  for (int i = 0; i < res->action_count; ++i) {
    fprintf(stderr, "    Action #%d:\n", i);
    print_sc_action(&res->actions[i]);
  }
}

void SC_SyncSimulator_init(SC_SyncSimulator *simu) {}

void sort_int_by_action_priority(int *indices, size_t length,
                                 const SC_Action *actions) {
  for (size_t i = 1; i < length; i++) {
    int key = indices[i];
    int key_priority = actions[key].priority;
    size_t j = i;

    while (j > 0 && actions[indices[j - 1]].priority > key_priority) {
      indices[j] = indices[j - 1];
      j--;
    }
    indices[j] = key;
  }
}

/**
 * Takes a Simulation objects and computes its next step inplace
 *
 * SIMULATION ==> f(x) ==> NEW SIMULATION
 *
 * The next step should be registered on
 */
void SC_SyncSimulator_next(SC_SyncSimulator *s, SC_Err err) {

  int next_cycle = s->current_cycle + 1;

  int visited_processes[s->process_count];
  for (int i = 0; i < s->process_count; i++) {
    visited_processes[i] = 0;
  }

  // CHECK IF SIMULATION ENDED
  int proc_finished = 0;
  for (int i = 0; i < s->process_count; i++) {
    SC_SyncProcess *process = &s->processes[i];
    if (process->current_state == STATE_FINISHED) {
      proc_finished++;
      visited_processes[i] = 1;
    }
  }
  if (proc_finished == s->process_count) {
    s->simulation_running = SC_FALSE;
    return;
  }

  // SET FINISHED PROCESSES
  for (int i = 0; i < s->process_count; i++) {
    SC_SyncProcess *process = &s->processes[i];
    if (process->burst_time == 0 && visited_processes[i] == 0) {
      process->current_state = STATE_FINISHED;

      SC_Slice *entries = &s->process_timelines[i].entries;
      SC_ProcessTimelineEntry entry = {
          .state = STATE_FINISHED, .action_id = -1, .resource_id = -1};
      SC_Slice_append(entries, &entry, err);
      if (*err != NO_ERROR) {
        return;
      }

      visited_processes[i] = 1;
    }
  }

  // SET WAITING AND PROCCESSED STATES

  int max_syncronization_count =
      s->sync_type == SYNC_MUTEX ? 1 : s->semaphore_count;

  for (int r = 0; r < s->resource_count; r++) {
    SC_Resource *resource = &s->resources[r];
    SC_Slice actions_in_cycle;
    SC_Slice_init(&actions_in_cycle, sizeof(int), s->process_count, err);
    if (*err != NO_ERROR) {
      return;
    }

    for (int i = 0; i < resource->action_count; i++) {
      SC_Action *action = &resource->actions[i];
      if (action->cycle == next_cycle) {
        SC_Slice_append(&actions_in_cycle, &i, err);
      }
    }

    sort_int_by_action_priority(actions_in_cycle.data, actions_in_cycle.length,
                                resource->actions);

    for (int i = 0; i < actions_in_cycle.length; i++) {
      int *index = (int *)actions_in_cycle.data + i;
      fprintf(stderr, "SORTED %d\n", resource->actions[*index].pid);
    }
    fprintf(stderr, "=============\n");

    int limit = SC_Min(max_syncronization_count, resource->max_counter);

    for (int i = 0; i < actions_in_cycle.length; i++) {
      int *action_index = (int *)actions_in_cycle.data + i;

      SC_Action *action = &resource->actions[*action_index];
      SC_Slice *entries = &s->process_timelines[action->pid].entries;

      //  if are resources available
      if (resource->max_counter - resource->counter < limit) {
        resource->counter--;
        s->processes[action->pid].current_state = STATE_ACCESSED;
        s->processes[action->pid].burst_time--;
        SC_ProcessTimelineEntry entry = {.state = STATE_ACCESSED,
                                         .action_id = action->id,
                                         .resource_id = resource->id};
        SC_Slice_append(entries, &entry, err);

        visited_processes[action->pid] = 1;
      } else {
        s->processes[action->pid].current_state = STATE_WAITING;
        action->cycle++;
        SC_ProcessTimelineEntry entry = {.state = STATE_WAITING,
                                         .action_id = action->id,
                                         .resource_id = resource->id};
        SC_Slice_append(entries, &entry, err);

        visited_processes[action->pid] = 1;
      }
    }

    resource->counter = resource->max_counter;

    SC_Slice_deinit(&actions_in_cycle);
  }

  // SET STARTING PROCESSES
  for (int i = 0; i < s->process_count; i++) {
    SC_SyncProcess *process = &s->processes[i];
    if (next_cycle < process->arrival_time && visited_processes[i] == 0) {
      process->current_state = STATE_READY;
      process->burst_time--;

      SC_Slice *entries = &s->process_timelines[i].entries;
      SC_ProcessTimelineEntry entry = {
          .state = STATE_READY, .action_id = -1, .resource_id = -1};
      SC_Slice_append(entries, &entry, err);
      if (*err != NO_ERROR) {
        return;
      }

      visited_processes[i] = 1;
    }
  }

  // SET UNVISITED PROCESS TO 'COMPUTING' STATE;
  for (int i = 0; i < s->process_count; i++) {
    SC_SyncProcess *process = &s->processes[i];
    if (visited_processes[i] == 0) {
      process->current_state = STATE_COMPUTING;
      process->burst_time--;

      SC_Slice *entries = &s->process_timelines[i].entries;
      SC_ProcessTimelineEntry entry = {
          .state = STATE_COMPUTING, .action_id = -1, .resource_id = -1};
      SC_Slice_append(entries, &entry, err);
      if (*err != NO_ERROR) {
        return;
      }

      visited_processes[i] = 1;
    }
  }

  // for (int i = 0; i < s->process_count; i++) {
  //   SC_Slice *entries = &s->process_timelines[i].entries;

  //   SC_ProcessTimelineEntry entry = {
  //       .state = STATE_COMPUTING, .action_id = 1, .resource_id = 1};
  //   SC_Slice_append(entries, &entry, err);
  //   if (*err != NO_ERROR) {
  //     return;
  //   }
  // }

  s->current_cycle += 1;
  s->total_cycles += 1;

  //
}

void fill_name_list_helper(SC_String *content, struct SC_Arena *arena,
                           SC_StringList *list, int column, SC_Err err);

void fill_processes(SC_String *file_contents, SC_StringList *process_names,
                    SC_SyncSimulator *simulator, SC_Err err);

void fill_resources(SC_String *file_contents, SC_StringList *resources_names,
                    SC_SyncSimulator *simulator, SC_Err err);

void fill_actions(SC_String *file_contents, struct SC_Arena *sync_arena,
                  SC_StringList *resources_names, SC_StringList *process_names,
                  SC_StringList *actions_names, SC_SyncSimulator *simulator,
                  SC_Err err);

void parse_syncProcess_file(SC_String *process_file, SC_String *resource_file,
                            SC_String *actions_file,
                            struct SC_Arena *sync_arena,
                            SC_SyncSimulator **simulator_ptr,
                            SC_StringList *process_names,
                            SC_StringList *resources_names,
                            SC_StringList *actions_names, SC_Err err) {

  // GET PROCESS NAMES
  fill_name_list_helper(process_file, sync_arena, process_names, 1, err);
  if (*err != NO_ERROR) {
    return;
  }

  fill_name_list_helper(resource_file, sync_arena, resources_names, 1, err);
  if (*err != NO_ERROR) {
    return;
  }

  fill_name_list_helper(actions_file, sync_arena, actions_names, 2, err);
  if (*err != NO_ERROR) {
    return;
  }

  struct SC_StringList_Node *current = process_names->head;
  while (current != NULL) {
    if (current->value.data != NULL) {
      fprintf(stderr, "%s\n", current->value.data);
    }
    current = current->next;
  }

  current = resources_names->head;
  while (current != NULL) {
    if (current->value.data != NULL) {
      fprintf(stderr, "%s\n", current->value.data);
    }
    current = current->next;
  }

  current = actions_names->head;
  while (current != NULL) {
    if (current->value.data != NULL) {
      fprintf(stderr, "%s\n", current->value.data);
    }
    current = current->next;
  }

  // ALLOCATE SIMULATION

  SC_SyncSimulator *simulator = *simulator_ptr;
  simulator = SC_Arena_Alloc(sync_arena, sizeof(SC_SyncSimulator), err);
  if (*err != NO_ERROR) {
    return;
  }
  *simulator_ptr = simulator;

  // PROCCESSES
  SC_SyncProcess *proccesses = SC_Arena_Alloc(
      sync_arena, sizeof(SC_SyncProcess) * process_names->count, err);
  simulator->process_count = process_names->count;
  simulator->processes = proccesses;

  fill_processes(process_file, process_names, simulator, err);
  if (*err != NO_ERROR) {
    return;
  }

  for (int i = 0; i < simulator->process_count; ++i) {
    SC_SyncProcess *proc = &simulator->processes[i];
    print_sync_process(
        proc); // or access fields like proc->id, proc->burst_time, etc.
  }

  // RESOURCES
  SC_Resource *resources = SC_Arena_Alloc(
      sync_arena, sizeof(SC_Resource) * resources_names->count, err);
  simulator->resource_count = resources_names->count;
  simulator->resources = resources;

  fill_resources(resource_file, resources_names, simulator, err);
  if (*err != NO_ERROR) {
    return;
  }

  for (int i = 0; i < simulator->resource_count; ++i) {
    SC_Resource *res = &simulator->resources[i];
    print_sc_resource(res);
  }

  // ACTIONS
  fill_actions(actions_file, sync_arena, resources_names, process_names,
               actions_names, simulator, err);

  for (int i = 0; i < simulator->resource_count; ++i) {
    SC_Resource *res = &simulator->resources[i];
    print_sc_resource(res);
  }

  // TIMELINES
  SC_ProcessTimeline *timelines = SC_Arena_Alloc(
      sync_arena, sizeof(SC_ProcessTimeline) * simulator->process_count, err);
  simulator->process_timelines = timelines;
  simulator->timeline_count = simulator->process_count;

  // TIMELINES ENTRIES
  for (int i = 0; i < simulator->timeline_count; ++i) {
    SC_Slice_init(&simulator->process_timelines[i].entries,
                  sizeof(SC_ProcessTimelineEntry), 8, err);
    if (*err != NO_ERROR) {
      return;
    }
  }
}

void fill_name_list_helper(SC_String *content, struct SC_Arena *arena,
                           SC_StringList *list, int column, SC_Err err) {

  const int b_max_length = 255;
  char b_data[b_max_length]; // Max length of a column
  SC_String buffer = {
      .data = b_data,
      .length = 0,
      .data_capacity = b_max_length,
  };

  int current_column = 1;

  for (int i = 0; i < content->length; i++) {
    char current_char = SC_String_CharAt(content, i);

    if (current_column == column) {
      if (current_char == ',' || current_char == '\n') {

        SC_String_AppendChar(&buffer, 0, err);
        if (*err != NO_ERROR) {
          return;
        }

        SC_StringList_Append(list, arena, buffer, err);
        if (*err != NO_ERROR) {
          return;
        }

        // Save current
        buffer.length = 0;
        current_column++;
        continue;
      }

      SC_String_AppendChar(&buffer, current_char, err);
      if (*err != NO_ERROR) {
        return;
      }
    } else {

      if (current_char == ',') {
        current_column++;
        continue;
      }

      if (current_char == '\n') {
        current_column = 1;
        continue;
      }
    }
  }
}

void fill_processes(SC_String *file_contents, SC_StringList *process_names,
                    SC_SyncSimulator *simulator, SC_Err err) {

  const int b_max_length = 255;
  char b_data[b_max_length]; // Max length of a column
  SC_String buffer = {
      .data = b_data,
      .length = 0,
      .data_capacity = b_max_length,
  };

  int current_column = 1;
  int current_row = 0;

  for (int i = 0; i < file_contents->length; i++) {

    char current_char = SC_String_CharAt(file_contents, i);
    switch (current_char) {

    case '\n': {

      if (4 != current_column) {
        *err = INVALID_TXT_FILE;
        return;
      }

      SC_String_TrimStart(&buffer, ' ');
      int column_value = SC_String_ParseInt(&buffer, err);
      if (*err != NO_ERROR) {
        return;
      }

      simulator->processes[current_row].priority = column_value;
      simulator->processes[current_row].id = current_row;
      simulator->processes[current_row].current_state = STATE_READY;

      buffer.length = 0;
      current_column = 1;
      current_row++;

    } break;

    case ',': {
      if (1 != current_column) {
        SC_String_TrimStart(&buffer, ' ');
        int column_value = SC_String_ParseInt(&buffer, err);
        if (*err != NO_ERROR) {
          return;
        }

        if (2 == current_column) {
          simulator->processes[current_row].burst_time = column_value;
          simulator->processes[current_row].remaining_time = column_value;

        } else if (3 == current_column) {
          simulator->processes[current_row].arrival_time = column_value;
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

void fill_resources(SC_String *file_contents, SC_StringList *resources_names,
                    SC_SyncSimulator *simulator, SC_Err err) {

  const int b_max_length = 255;
  char b_data[b_max_length]; // Max length of a column
  SC_String buffer = {
      .data = b_data,
      .length = 0,
      .data_capacity = b_max_length,
  };

  int current_column = 1;
  int current_row = 0;

  for (int i = 0; i < file_contents->length; i++) {

    char current_char = SC_String_CharAt(file_contents, i);
    switch (current_char) {

    case '\n': {

      if (2 != current_column) {
        *err = INVALID_TXT_FILE;
        return;
      }

      SC_String_TrimStart(&buffer, ' ');
      int column_value = SC_String_ParseInt(&buffer, err);
      if (*err != NO_ERROR) {
        return;
      }

      simulator->resources[current_row].id = current_row;
      simulator->resources[current_row].counter = column_value;
      simulator->resources[current_row].max_counter = column_value;
      simulator->resources[current_row].action_count = 0;

      buffer.length = 0;
      current_column = 1;
      current_row++;

    } break;

    case ',': {
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

/** Ignores the last charactef of the b string */
SC_Bool SC_String_Equals(SC_String *a, SC_String *b) {
  if (a->length != b->length - 1) {
    return SC_FALSE;
  }

  for (size_t i = 0; i < a->length; ++i) {
    if (a->data[i] != b->data[i]) {
      return SC_FALSE;
    }
  }

  return SC_TRUE;
}

int SC_StringList_IndexOf(SC_StringList *list, SC_String *target) {
  if (!list || !target)
    return -1;

  struct SC_StringList_Node *current = list->head;
  int index = 0;

  while (current != NULL) {
    if (SC_String_Equals(target, &current->value) == SC_TRUE) {
      return index;
    }
    current = current->next;
    index++;
  }

  return -1; // not found
}

void fill_actions(SC_String *file_contents, struct SC_Arena *sync_arena,
                  SC_StringList *resources_names, SC_StringList *process_names,
                  SC_StringList *actions_names, SC_SyncSimulator *simulator,
                  SC_Err err) {

  int actions_resource_index[actions_names->count];
  int actions_index_in_resource[actions_names->count];

  if (file_contents->length == 0) {
    return;
  }

  // COUNT ACTIONS PER RESOURCE

  // The files always have an extra line jump, hence -1
  int num_lines = SC_String_LineCount(file_contents) - 1;

  for (int line = 0; line < num_lines; line++) {
    SC_String lineValue = SC_String_GetLine(file_contents, line);

    SC_String resource = SC_String_GetCSVColumn(&lineValue, 2);
    int index = SC_StringList_IndexOf(resources_names, &resource);
    if (index == -1) {
      *err = RESOURCE_NOT_FOUND;
      return;
    }
    fprintf(stderr, "%.*s\n", (int)resource.length, resource.data);

    actions_index_in_resource[line] = simulator->resources[index].action_count;
    simulator->resources[index].action_count += 1;
    actions_resource_index[line] = index;
  }

  // ALLOCATE SPACE FOR ACTIONS IN EACH RESOURCE;

  for (int i = 0; i < simulator->resource_count; ++i) {
    SC_Action *actions = SC_Arena_Alloc(
        sync_arena, sizeof(SC_Action) * simulator->resources[i].action_count,
        err);
    simulator->resources[i].actions = actions;
  }

  // PARSE EACH ACTION;

  const int b_max_length = 255;
  char b_data[b_max_length]; // Max length of a column
  SC_String buffer = {
      .data = b_data,
      .length = 0,
      .data_capacity = b_max_length,
  };

  int current_column = 1;
  int current_row = 0;

  for (int i = 0; i < file_contents->length; i++) {

    char current_char = SC_String_CharAt(file_contents, i);
    switch (current_char) {

    case '\n': {

      if (4 != current_column) {
        *err = INVALID_TXT_FILE;
        return;
      }

      SC_String_TrimStart(&buffer, ' ');
      int column_value = SC_String_ParseInt(&buffer, err);
      if (*err != NO_ERROR) {
        return;
      }

      int resourceIndex = actions_resource_index[current_row];
      int actionIndex = actions_index_in_resource[current_row];

      simulator->resources[resourceIndex].actions[actionIndex].id = current_row;
      simulator->resources[resourceIndex].actions[actionIndex].cycle =
          column_value;
      simulator->resources[resourceIndex].actions[actionIndex].executed =
          SC_FALSE;
      simulator->resources[resourceIndex].actions[actionIndex].resource_id =
          resourceIndex;

      buffer.length = 0;
      current_column = 1;
      current_row++;

    } break;

    case ',': {
      if (1 == current_column) {

        int procIndex = SC_StringList_IndexOf(process_names, &buffer);
        if (procIndex == -1) {
          *err = PROCESS_NOT_FOUND;
          return;
        }

        int resourceIndex = actions_resource_index[current_row];
        int actionIndex = actions_index_in_resource[current_row];

        simulator->resources[resourceIndex].actions[actionIndex].priority =
            simulator->processes[procIndex].priority;

        simulator->resources[resourceIndex].actions[actionIndex].pid =
            simulator->processes[procIndex].id;

      } else if (current_column > 4) {
        *err = INVALID_TXT_FILE;
        return;
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

// Returns the process with minimun burst time in a step
SC_Process SC_Verify_AT_BT(SC_ProcessList *list, int step) {
  SC_ProcessList_sort(list, compare_by_burst_time);

  SC_ProcessList_Node *current = list->head;

  while (current != NULL) {
    if (current->value.arrival_time <= step && current->value.burst_time != 0) {
      return current->value;
    }
    current = current->next;
  }

  return current->value;
}

int compare_proc_ptrAT(const void *a, const void *b) {
  const SC_Process *proc_a = *(const SC_Process **)a;
  const SC_Process *proc_b = *(const SC_Process **)b;

  if (proc_a->arrival_time != proc_b->arrival_time)
    return proc_a->arrival_time - proc_b->arrival_time;
  return proc_a->pid_idx - proc_b->pid_idx;
}

int compare_proc_ptrBT(const void *a, const void *b) {
  const SC_Process *proc_a = *(const SC_Process **)a;
  const SC_Process *proc_b = *(const SC_Process **)b;

  if (proc_a->burst_time != proc_b->burst_time)
    return proc_a->burst_time - proc_b->burst_time;
  return proc_a->pid_idx - proc_b->pid_idx;
}

int compare_proc_ptrP(const void *a, const void *b) {
  const SC_Process *proc_a = *(const SC_Process **)a;
  const SC_Process *proc_b = *(const SC_Process **)b;

  if (proc_a->priority != proc_b->priority)
    return proc_a->priority - proc_b->priority;
  return proc_a->pid_idx - proc_b->pid_idx;
}