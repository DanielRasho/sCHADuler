#include "lib.c"
#include "resources.c"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ################################
// ||                            ||
// ||         CONSTANTS          ||
// ||                            ||
// ################################

// The program preallocates enough space for this processes.
// If more than this quantity is required it allocates more.
const static size_t INITIAL_PROCESSES = 15;
const static size_t INITIAL_RESOURCES = 5;
const static size_t INITIAL_ACTIONS = 15;

typedef int SC_Algorithm;
const static SC_Algorithm SC_FirstInFirstOut = 0;
const static SC_Algorithm SC_ShortestFirst = 1;
const static SC_Algorithm SC_ShortestRemaining = 2;
const static SC_Algorithm SC_RoundRobin = 3;
const static SC_Algorithm SC_Priority = 4;

// ################################
// ||                            ||
// ||          STRUCTS           ||
// ||                            ||
// ################################

typedef struct {
  GtkWindow *window;
  GtkTextBuffer *buffer;
  GtkBox *button_container;
} SC_OpenFileEVData;

typedef struct {
  GtkBox *canvas_container;
  GtkLabel *step_label;
  GListStore *info_store;
} SC_UpdateSimCanvasData;

typedef struct {
  GtkSpinButton *spin_button;
  GtkWindow *window;
  GListStore *review_store;
} SC_LoadedNewFileData;

typedef struct {
  SC_LoadedNewFileData new_file_loaded;
  SC_UpdateSimCanvasData update_sim_canvas;
} SC_GlobalEventData;

// Syncronization

typedef struct {
  GtkWindow *window;
  GtkTextBuffer *processes_buffer;
  GtkTextBuffer *resources_buffer;
  GtkTextBuffer *actions_buffer;
  GtkSpinButton *semaphore_quantity;
  GtkWidget *syncronization_switch;
} SC_SyncLoadedNewFileData;

typedef struct {
  SC_SyncLoadedNewFileData new_file_loaded;
} SC_SyncGlobalEventData;

// ################################
// ||                            ||
// ||      CUSTOM GOBJECTS       ||
// ||                            ||
// ################################

#define SC_TYPE_PROCESS_GIO (sc_process_gio_get_type())
G_DECLARE_FINAL_TYPE(SCProcessGio, sc_process_gio, SC, PROCESS_GIO, GObject)

struct _SCProcessGio {
  GObject parent_instance;
  size_t pid_idx;
  uint burst_time;
  uint arrival_time;
  uint priority;
};
static void sc_process_gio_init(SCProcessGio *item) {}

struct _SCProcessGioClass {
  GObjectClass parent_class;
};
static void sc_process_gio_class_init(SCProcessGioClass *class) {}

G_DEFINE_TYPE(SCProcessGio, sc_process_gio, G_TYPE_OBJECT)

static SCProcessGio *sc_process_gio_new(size_t pid_idx, uint burst_time,
                                        uint arrival_time, uint priority) {
  SCProcessGio *item = g_object_new(SC_TYPE_PROCESS_GIO, NULL);
  item->pid_idx = pid_idx;
  item->burst_time = burst_time;
  item->arrival_time = arrival_time;
  item->priority = priority;

  return item;
}

static size_t sc_process_gio_get_pid_idx(SCProcessGio *self) {
  return self->pid_idx;
}
static uint sc_process_gio_get_burst_time(SCProcessGio *self) {
  return self->burst_time;
}
static uint sc_process_gio_get_arrival_time(SCProcessGio *self) {
  return self->arrival_time;
}
static uint sc_process_gio_get_priority(SCProcessGio *self) {
  return self->priority;
}

#define SC_TYPE_ALGORITHM_PERFORMANCE (sc_algorithm_performance_get_type())
G_DECLARE_FINAL_TYPE(SCAlgorithmPerformance, sc_algorithm_performance, SC,
                     ALGORITHM_PERFORMANCE, GObject)

struct _SCAlgorithmPerformance {
  GObject parent_instance;
  const char *name;
  float avg_waiting_time;
};
static void sc_algorithm_performance_init(SCAlgorithmPerformance *item) {}

struct _SCAlgorithmPerformanceClass {
  GObjectClass parent_class;
};
static void
sc_algorithm_performance_class_init(SCAlgorithmPerformanceClass *class) {}

G_DEFINE_TYPE(SCAlgorithmPerformance, sc_algorithm_performance, G_TYPE_OBJECT)

static SCAlgorithmPerformance *
sc_algorithm_performance_new(const char *name, float avg_waiting_time) {
  SCAlgorithmPerformance *item =
      g_object_new(SC_TYPE_ALGORITHM_PERFORMANCE, NULL);
  item->name = name;
  item->avg_waiting_time = avg_waiting_time;

  return item;
}

static const char *
sc_algorithm_performance_get_name(SCAlgorithmPerformance *self) {
  return self->name;
}
static float
sc_algorithm_performance_get_avg_waiting_time(SCAlgorithmPerformance *self) {
  return self->avg_waiting_time;
}

// ################################
// ||                            ||
// ||        GLOBAL STATE        ||
// ||                            ||
// ################################

// The currently selected algorithm.
static SC_Algorithm SELECTED_ALGORITHM;

// Arena used to store all data associated with an `SC_Simulation`.
static struct SC_Arena SIM_ARENA;

static struct SC_Arena PROCESS_LIST_ARENA;
static SC_ProcessList PROCESS_LIST;

static struct SC_Arena PIDS_ARENA;
static SC_StringList PID_LIST;

static SC_Simulation *SIM_STATES[5] = {0};
// static SC_Simulation *SIM_STATE;
static struct SC_Arena SIM_BTN_LABELS_ARENA;

// Syncronization

static struct SC_Arena SYNC_SIM_ARENA;

static SC_StringList SYNC_PROCESS_NAMES;
static SC_StringList SYNC_RESOURCES_NAMES;
static SC_StringList SYNC_ACTIONS_NAMES;

static SC_String PROCESS_FILE_CONTENT;
static SC_String RESOURCES_FILE_CONTENT;
static SC_String ACTIONS_FILE_CONTENT;

static SC_SyncSimulator *SYNC_SIM_STATE;

// ################################
// ||                            ||
// ||         UTILITIES          ||
// ||                            ||
// ################################

// Updated the simulation display
static void update_sim_canvas(SC_UpdateSimCanvasData params, SC_Err err) {
  GtkWidget *widget;
  while ((widget = gtk_widget_get_first_child(
              GTK_WIDGET(params.canvas_container))) != NULL) {
    gtk_box_remove(params.canvas_container, widget);
  }

  SC_Simulation *current_sim = SIM_STATES[SELECTED_ALGORITHM];
  if (current_sim == NULL) {
    fprintf(stderr, "ERROR: Simulation has not been initialized!\n");
    return;
  }

  char str[] = {'C', 'u', 'r', 'r', 'e', 'n', 't', ' ', 'S', 't',
                'e', 'p', ':', ' ', 0,   0,   0,   0,   0};
  sprintf(str + strlen(str), "%zu", current_sim->current_step);
  gtk_label_set_label(params.step_label, str);

  SC_Arena_Reset(&SIM_BTN_LABELS_ARENA);
  for (int i = 0; i <= current_sim->current_step; i++) {
    size_t current_process = current_sim->steps[i].current_process;
    size_t max = -1;

    size_t pid_idx = 49;
    char *data = "<N/A>";
    SC_String pid_str = {
        .data = data,
        .length = strlen(data),
        .data_capacity = strlen(data),
    };
    if (current_process != max) {
      pid_idx = current_sim->steps[i].processes[current_process].pid_idx;
      pid_str = SC_StringList_GetAt(&PID_LIST, pid_idx, err);
      if (*err != NO_ERROR) {
        fprintf(
            stderr,
            "SIM_STEP_ERROR (%d): Failed to get pid for process (idx: %zu): "
            "Failed to get PID from stringlist with idx: %zu\n",
            i, current_process, pid_idx);
        return;
      }
    }

    char css_class[] = {'p', 'i', 'd', '_', 0, 0, 0, 0, 0, 0, 0, 0};
    sprintf(css_class + strlen(css_class), "%zu", pid_idx);

    GtkWidget *label = gtk_label_new(pid_str.data);
    gtk_box_append(params.canvas_container, label);
    gtk_widget_add_css_class(label, css_class);
    gtk_widget_add_css_class(label, "pid_box");

    SC_Bool is_last_iteration = i == current_sim->current_step;
    if (is_last_iteration) {
      g_list_store_remove_all(params.info_store);
      for (int j = 0; j < current_sim->steps[i].process_length; j++) {
        SC_Process current = current_sim->steps[i].processes[j];
        fprintf(stderr, "INFO: Appending value to store\n");
        g_list_store_append(
            params.info_store,
            sc_process_gio_new(current.pid_idx, current.burst_time,
                               current.arrival_time, current.priority));
      }
    }
  }
}

void show_alert_dialog(GtkWidget *parent_widget, const char *title,
                       const char *message) {
  GtkWidget *parent_window = GTK_WIDGET(gtk_widget_get_root(parent_widget));

  // Create a new transient window (acts as a dialog)
  GtkWidget *dialog = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent_window));
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);
  gtk_widget_add_css_class(dialog, "alert_popup");

  // Container for dialog content
  GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_window_set_child(GTK_WINDOW(dialog), content_box);

  // Message label
  GtkWidget *label = gtk_label_new(message);
  gtk_label_set_wrap(GTK_LABEL(label), TRUE);
  gtk_box_append(GTK_BOX(content_box), label);

  // Close button
  GtkWidget *close_button = gtk_button_new_with_label("Close");
  g_signal_connect_swapped(close_button, "clicked",
                           G_CALLBACK(gtk_window_destroy), dialog);
  gtk_box_append(GTK_BOX(content_box), close_button);
  gtk_widget_add_css_class(close_button, "alert_popup_btn");

  gtk_window_present(GTK_WINDOW(dialog));
}

// ################################
// ||                            ||
// ||      COLUMN BUILDERS       ||
// ||                            ||
// ################################

static void setup_label_cb(GtkSignalListItemFactory *factory,
                           GObject *listitem) {
  GtkWidget *label = gtk_label_new(NULL);
  gtk_widget_add_css_class(label, "pid_box");
  gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void bind_pid_cb(GtkSignalListItemFactory *factory,
                        GtkListItem *listitem) {
  GtkWidget *label = gtk_list_item_get_child(listitem);
  gtk_widget_set_hexpand(label, TRUE);

  GObject *item = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
  size_t pid_idx = sc_process_gio_get_pid_idx(SC_PROCESS_GIO(item));

  for (int i = 0; i < 50; i++) {
    char class[] = {'p', 'i', 'd', '_', 0, 0, 0, 0};
    sprintf(class + strlen(class), "%d", i);
    gtk_widget_remove_css_class(label, class);
  }

  char class[] = {'p', 'i', 'd', '_', 0, 0, 0, 0};
  sprintf(class + strlen(class), "%zu", pid_idx);
  // gtk_widget_remove_css_class(label, class);
  gtk_widget_add_css_class(label, class);

  fprintf(stderr, "INFO: Binding to pid_idx %zu\n", pid_idx);

  size_t err = NO_ERROR;
  SC_String str = SC_StringList_GetAt(&PID_LIST, pid_idx, &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "ERROR: Failed to bind pid for tableview, pid_idx: %zu\n",
            pid_idx);
    return;
  }

  gtk_label_set_text(GTK_LABEL(label), str.data);
}

static void bind_burst_time_cb(GtkSignalListItemFactory *factory,
                               GtkListItem *listitem) {
  GtkWidget *label = gtk_list_item_get_child(listitem);
  GObject *item = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
  uint time = sc_process_gio_get_burst_time(SC_PROCESS_GIO(item));

  char buff[10] = {0};
  sprintf(buff, "%d", time);
  gtk_label_set_text(GTK_LABEL(label), buff);
}

static void bind_arrival_time_cb(GtkSignalListItemFactory *factory,
                                 GtkListItem *listitem) {
  GtkWidget *label = gtk_list_item_get_child(listitem);
  GObject *item = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
  uint time = sc_process_gio_get_arrival_time(SC_PROCESS_GIO(item));

  char buff[10] = {0};
  sprintf(buff, "%d", time);
  gtk_label_set_text(GTK_LABEL(label), buff);
}

static void bind_priority_cb(GtkSignalListItemFactory *factory,
                             GtkListItem *listitem) {
  GtkWidget *label = gtk_list_item_get_child(listitem);
  GObject *item = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
  uint priority = sc_process_gio_get_priority(SC_PROCESS_GIO(item));

  char buff[10] = {0};
  sprintf(buff, "%d", priority);
  gtk_label_set_text(GTK_LABEL(label), buff);
}

static void bind_algorithm_name_cb(GtkSignalListItemFactory *factory,
                                   GtkListItem *listitem) {
  GtkWidget *label = gtk_list_item_get_child(listitem);
  GObject *item = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
  const char *name =
      sc_algorithm_performance_get_name(SC_ALGORITHM_PERFORMANCE(item));

  gtk_label_set_text(GTK_LABEL(label), name);
}

static void bind_avg_waiting_time_cb(GtkSignalListItemFactory *factory,
                                     GtkListItem *listitem) {
  GtkWidget *label = gtk_list_item_get_child(listitem);
  GObject *item = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
  float waiting_time = sc_algorithm_performance_get_avg_waiting_time(
      SC_ALGORITHM_PERFORMANCE(item));

  char buff[10] = {0};
  sprintf(buff, "%.2f", waiting_time);
  gtk_label_set_text(GTK_LABEL(label), buff);
}

// ################################
// ||                            ||
// ||          HANDLERS          ||
// ||                            ||
// ################################

static void print_hello(GtkWidget *widget, gpointer data) {
  g_print("Hello World\n");
}

static void change_algorithm_to_first_in_first_out(GtkCheckButton *self,
                                                   gpointer *data) {
  size_t err = NO_ERROR;
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SELECTED_ALGORITHM = SC_FirstInFirstOut;
  update_sim_canvas(ev_data->update_sim_canvas, &err);
}

static void change_algorithm_to_shortest_first(GtkCheckButton *self,
                                               gpointer *data) {
  size_t err = NO_ERROR;
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SELECTED_ALGORITHM = SC_ShortestFirst;
  update_sim_canvas(ev_data->update_sim_canvas, &err);
}

static void change_algorithm_to_shortest_remaining(GtkCheckButton *self,
                                                   gpointer *data) {
  size_t err = NO_ERROR;
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SELECTED_ALGORITHM = SC_ShortestRemaining;
  update_sim_canvas(ev_data->update_sim_canvas, &err);
}

static void change_algorithm_to_round_robin(GtkCheckButton *self,
                                            gpointer *data) {
  size_t err = NO_ERROR;
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SELECTED_ALGORITHM = SC_RoundRobin;
  update_sim_canvas(ev_data->update_sim_canvas, &err);
}

static void change_algorithm_to_priority(GtkCheckButton *self, gpointer *data) {
  size_t err = NO_ERROR;
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SELECTED_ALGORITHM = SC_Priority;
  update_sim_canvas(ev_data->update_sim_canvas, &err);
}

static void handle_quantum_updated(GtkSpinButton *self, gpointer *data) {
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;

  if (NULL == SIM_STATES[SC_RoundRobin]) {
    fprintf(stderr, "INFO: Skipping rerendering because state is null\n");
    return;
  }

  fprintf(stderr, "INFO: Rerendering based on new quantum...\n");
  int quantum = gtk_spin_button_get_value_as_int(self);
  SIM_STATES[SC_RoundRobin]->current_step = 0;
  simulate_round_robin(&PROCESS_LIST, SIM_STATES[SC_RoundRobin], quantum);

  size_t err = NO_ERROR;
  update_sim_canvas(ev_data->update_sim_canvas, &err);
}

static void file_dialog_finished(GObject *source_object, GAsyncResult *res,
                                 gpointer data) {
  SC_GlobalEventData *global_ev_data = (SC_GlobalEventData *)data;
  SC_LoadedNewFileData ev_data = global_ev_data->new_file_loaded;

  GError **error = NULL;
  GFile *file =
      gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source_object), res, error);
  if (NULL != error) {
    SC_PANIC("An error occurred reading the file: `%s`!\n", (*error)->message);
    return;
  }

  if (NULL == file) {
    fprintf(stderr, "No file selected!\n");
    return;
  }

  const char *file_path = g_file_get_path(file);
  fprintf(stderr, "Loading file at: %s\n", file_path);

  char *contents;
  gsize length;
  if (!g_file_load_contents(file, NULL, &contents, &length, NULL, error)) {
    fprintf(stderr, "Failed to read file contents: `%s`!\n", (*error)->message);
    return;
  }

  SC_String file_contents = {
      .length = length,
      .data = contents,
      .data_capacity = length,
  };
  fprintf(stderr, "The file contents are:\n%*s\n", (int)file_contents.length,
          file_contents.data);

  SC_Arena_Reset(&PIDS_ARENA);
  SC_Arena_Reset(&PROCESS_LIST_ARENA);
  SC_StringList_Reset(&PID_LIST);
  SC_ProcessList_Reset(&PROCESS_LIST);

  size_t err = NO_ERROR;
  parse_scheduling_file(&file_contents, &PIDS_ARENA, &PROCESS_LIST_ARENA,
                        &PID_LIST, &PROCESS_LIST, &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "ERROR: %s\n", SC_Err_ToString(&err));
  } else {
    fprintf(stderr, "Correctly parsed the file!\n");
  }

  size_t total_processes = PROCESS_LIST.count;
  size_t total_steps = 0;
  struct SC_ProcessList_Node *current = PROCESS_LIST.head;
  for (size_t i = 0; i < total_processes; i++) {
    total_steps += current->value.burst_time;
    current = current->next;
  }

  g_list_store_remove_all(ev_data.review_store);
  int quantum = gtk_spin_button_get_value_as_int(ev_data.spin_button);
  SC_Arena_Reset(&SIM_ARENA);
  for (int i = 0; i < 5; i++) {
    SIM_STATES[i] = SC_Arena_Alloc(&SIM_ARENA, sizeof(SC_Simulation), &err);
    if (err != NO_ERROR) {
      fprintf(stderr, "ERROR: %s\n", SC_Err_ToString(&err));
      exit(1);
    }

    SIM_STATES[i]->steps =
        SC_Arena_Alloc(&SIM_ARENA, sizeof(SC_SimStepState) * total_steps, &err);
    if (err != NO_ERROR) {
      fprintf(stderr, "ERROR: %s\n", SC_Err_ToString(&err));
      exit(1);
    }

    for (size_t j = 0; j < total_steps; j++) {
      SIM_STATES[i]->steps[j].processes = SC_Arena_Alloc(
          &SIM_ARENA, sizeof(SC_Process) * total_processes, &err);
      if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: %s\n", SC_Err_ToString(&err));
        exit(1);
      }
      SIM_STATES[i]->steps[j].process_length = total_processes;
    }
    SIM_STATES[i]->step_length = total_steps;

    switch (i) {
    case SC_FirstInFirstOut: {
      simulate_first_in_first_out(&PROCESS_LIST, SIM_STATES[i]);
      g_list_store_append(
          ev_data.review_store,
          sc_algorithm_performance_new("First In First Out",
                                       SIM_STATES[i]->avg_waiting_time));
    } break;
    case SC_ShortestFirst: {
      simulate_shortest_first(&PROCESS_LIST, SIM_STATES[i]);
      g_list_store_append(
          ev_data.review_store,
          sc_algorithm_performance_new("Shortest First",
                                       SIM_STATES[i]->avg_waiting_time));
    } break;
    case SC_ShortestRemaining: {
      simulate_shortest_remaining(&PROCESS_LIST, SIM_STATES[i]);
      g_list_store_append(
          ev_data.review_store,
          sc_algorithm_performance_new("Shortest Remaining",
                                       SIM_STATES[i]->avg_waiting_time));
    } break;
    case SC_RoundRobin: {
      simulate_round_robin(&PROCESS_LIST, SIM_STATES[i], quantum);
      g_list_store_append(ev_data.review_store,
                          sc_algorithm_performance_new(
                              "Round Robin", SIM_STATES[i]->avg_waiting_time));
    } break;
    case SC_Priority: {
      simulate_priority(&PROCESS_LIST, SIM_STATES[i]);
      g_list_store_append(ev_data.review_store,
                          sc_algorithm_performance_new(
                              "Priority", SIM_STATES[i]->avg_waiting_time));
    } break;
    default: {
      SC_PANIC("FATAL: Unrecognized scheduling algorithm (%d)!", i);
    } break;
    }

    SIM_STATES[i]->current_step = 0;
  }

  update_sim_canvas(global_ev_data->update_sim_canvas, &err);
  if (err != NO_ERROR) {
    return;
  }
}

static void handle_open_file_click(GtkWidget *widget, gpointer data) {
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  GtkFileDialog *dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Archivo de simulacion");
  gtk_file_dialog_set_modal(dialog, TRUE);
  gtk_file_dialog_open(dialog, ev_data->new_file_loaded.window, NULL,
                       file_dialog_finished, data);
}

static void handle_next_click(GtkWidget *widget, gpointer data) {
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;

  SC_Simulation *current_sim = SIM_STATES[SELECTED_ALGORITHM];
  if (current_sim == NULL) {
    fprintf(stderr, "ERROR: Simulation has not been initialized!\n");
    return;
  }

  SC_Bool has_next_step =
      current_sim->current_step + 1 < current_sim->step_length;
  if (has_next_step) {
    current_sim->current_step += 1;
    size_t err = NO_ERROR;
    update_sim_canvas(ev_data->update_sim_canvas, &err);
    if (err != NO_ERROR) {
      return;
    }
  }
}

static void handle_previous_click(GtkWidget *widget, gpointer data) {
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SC_Bool has_previous_step = SIM_STATES[SELECTED_ALGORITHM]->current_step > 0;
  if (has_previous_step) {
    SIM_STATES[SELECTED_ALGORITHM]->current_step -= 1;
    size_t err = NO_ERROR;
    update_sim_canvas(ev_data->update_sim_canvas, &err);
    if (err != NO_ERROR) {
      return;
    }
  }
}

static void handle_reset_click(GtkWidget *widget, gpointer data) {
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;

  SC_Simulation *current_sim = SIM_STATES[SELECTED_ALGORITHM];
  if (current_sim == NULL) {
    fprintf(stderr, "ERROR: Simulation has not been initialized!\n");
    return;
  }

  SC_Bool should_reset = current_sim->current_step > 0;
  if (should_reset) {
    current_sim->current_step = 0;
    size_t err = NO_ERROR;
    update_sim_canvas(ev_data->update_sim_canvas, &err);
    if (err != NO_ERROR) {
      return;
    }
  }
}

// Syncronization

static void sync_file_dialog_finished(GObject *source_object, GAsyncResult *res,
                                      gpointer data, SC_String *file_contents,
                                      GtkTextBuffer *buffer) {
  SC_SyncGlobalEventData *global_ev_data = (SC_SyncGlobalEventData *)data;
  SC_SyncLoadedNewFileData ev_data = global_ev_data->new_file_loaded;

  GError **error = NULL;
  GFile *file =
      gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source_object), res, error);
  if (NULL != error) {
    SC_PANIC("An error occurred reading the file: `%s`!\n", (*error)->message);
    return;
  }

  if (NULL == file) {
    fprintf(stderr, "No file selected!\n");
    return;
  }

  const char *file_path = g_file_get_path(file);
  fprintf(stderr, "Loading file at: %s\n", file_path);

  char *contents;
  gsize length;
  if (!g_file_load_contents(file, NULL, &contents, &length, NULL, error)) {
    fprintf(stderr, "Failed to read file contents: `%s`!\n", (*error)->message);
    return;
  }

  // Free old data
  if (file_contents->data != NULL) {
    free(file_contents->data);
  }

  // Set New contents
  file_contents->length = length;
  file_contents->data = contents;
  file_contents->data_capacity = length;

  fprintf(stderr, "The file contents are:\n%*s\n", (int)file_contents->length,
          file_contents->data);

  gtk_text_buffer_set_text(buffer, contents, length);
}

static void sync_handle_open_file_click(GtkWidget *widget, gpointer data,
                                        GAsyncReadyCallback callback) {
  SC_SyncGlobalEventData *ev_data = (SC_SyncGlobalEventData *)data;
  GtkFileDialog *dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Archivo de simulacion");
  gtk_file_dialog_set_modal(dialog, TRUE);
  gtk_file_dialog_open(dialog, ev_data->new_file_loaded.window, NULL, callback,
                       data);
}

static void sync_handle_finish_load_process(GObject *source_object,
                                            GAsyncResult *res, gpointer data) {
  SC_SyncGlobalEventData *ev_data = (SC_SyncGlobalEventData *)data;
  sync_file_dialog_finished(source_object, res, data, &PROCESS_FILE_CONTENT,
                            ev_data->new_file_loaded.processes_buffer);
}

static void sync_handle_load_process(GtkWidget *widget, gpointer data) {
  sync_handle_open_file_click(widget, data, sync_handle_finish_load_process);
}

static void sync_handle_finish_load_resources(GObject *source_object,
                                              GAsyncResult *res,
                                              gpointer data) {
  SC_SyncGlobalEventData *ev_data = (SC_SyncGlobalEventData *)data;
  sync_file_dialog_finished(source_object, res, data, &RESOURCES_FILE_CONTENT,
                            ev_data->new_file_loaded.resources_buffer);
}

static void sync_handle_load_resources(GtkWidget *widget, gpointer data) {
  sync_handle_open_file_click(widget, data, sync_handle_finish_load_resources);
}

static void sync_handle_finish_load_actions(GObject *source_object,
                                            GAsyncResult *res, gpointer data) {
  SC_SyncGlobalEventData *ev_data = (SC_SyncGlobalEventData *)data;
  sync_file_dialog_finished(source_object, res, data, &ACTIONS_FILE_CONTENT,
                            ev_data->new_file_loaded.actions_buffer);
}

static void sync_handle_load_actions(GtkWidget *widget, gpointer data) {
  sync_handle_open_file_click(widget, data, sync_handle_finish_load_actions);
}

static void load_sync_files(GtkWidget *widget, gpointer data) {

  if (PROCESS_FILE_CONTENT.length == 0 || RESOURCES_FILE_CONTENT.length == 0 ||
      ACTIONS_FILE_CONTENT.length == 0) {
    // Create and show an alert dialog
    show_alert_dialog(widget, "Missing Files",
                      "One or more required files are missing.");
    return;
  }

  size_t err = NO_ERROR;

  // TODO: FREE TIMELINES STEP DATA
  SC_Arena_Reset(&SYNC_SIM_ARENA);
  SC_StringList_Reset(&SYNC_PROCESS_NAMES);
  SC_StringList_Reset(&SYNC_RESOURCES_NAMES);
  SC_StringList_Reset(&SYNC_ACTIONS_NAMES);

  parse_syncProcess_file(&PROCESS_FILE_CONTENT, &RESOURCES_FILE_CONTENT,
                         &ACTIONS_FILE_CONTENT, &SYNC_SIM_ARENA, SYNC_SIM_STATE,
                         &SYNC_PROCESS_NAMES, &SYNC_RESOURCES_NAMES,
                         &SYNC_ACTIONS_NAMES, &err);

  if (err != NO_ERROR) {
    fprintf(stderr, "Something went wrong during data parsing.\n");
    return;
  }
}

// ################################
// ||                            ||
// ||         UI BLOCKS          ||
// ||                            ||
// ################################

GtkWidget *TitleLabel(const char *content) {
  GtkWidget *label = gtk_label_new(content);
  gtk_widget_add_css_class(label, "h1_title");
  return label;
}

GtkWidget *MainButton(const char *label,
                      void (*onClick)(GtkWidget *widget, gpointer data),
                      void *ev_data) {
  GtkWidget *btn = gtk_button_new_with_label(label);
  gtk_widget_add_css_class(btn, "btn_main");
  gtk_widget_remove_css_class(btn, "text-button");
  if (NULL != onClick) {
    g_signal_connect(btn, "clicked", G_CALLBACK(onClick), ev_data);
  }
  return btn;
}

/**
 * Creates the scheduling view.
 *
 * @param window GtkWindow* The window where this component will be displayed,
 * it's not the parent container!
 * @return GtkWidget* The container of this view.
 */
static GtkWidget *CalendarView(GtkWindow *window) {
  SC_GlobalEventData *evData = malloc(sizeof(SC_GlobalEventData));
  if (NULL == evData) {
    SC_PANIC("Failed to malloc enough space for the file loader event!\n");
    return NULL;
  }

  evData->new_file_loaded.window = window;

  GtkWidget *container = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_vexpand(container, TRUE);
  gtk_widget_set_hexpand(container, TRUE);

  // Simulation half
  GtkWidget *simContainer = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_name(simContainer, "simContainer");
  gtk_widget_set_vexpand(simContainer, TRUE);
  gtk_widget_set_hexpand(simContainer, TRUE);
  gtk_paned_set_start_child(GTK_PANED(container), simContainer);

  GtkWidget *simBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_name(simBox, "simBox");
  gtk_widget_set_vexpand(simBox, TRUE);
  gtk_widget_set_hexpand(simBox, TRUE);
  gtk_paned_set_start_child(GTK_PANED(simContainer), simBox);

  GtkWidget *topSimBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_hexpand(topSimBox, TRUE);
  gtk_widget_set_halign(topSimBox, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(simBox), topSimBox);

  GtkWidget *resetBtn = MainButton("RESET", NULL, NULL);
  gtk_box_append(GTK_BOX(topSimBox), resetBtn);

  GtkWidget *tickLabel = gtk_label_new("Current Step: XX");
  evData->update_sim_canvas.step_label = GTK_LABEL(tickLabel);
  gtk_box_append(GTK_BOX(topSimBox), tickLabel);

  GtkWidget *processBoxScroller = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(processBoxScroller, TRUE);
  gtk_widget_set_hexpand(processBoxScroller, TRUE);
  gtk_box_append(GTK_BOX(simBox), processBoxScroller);

  GtkWidget *processBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  evData->update_sim_canvas.canvas_container = GTK_BOX(processBox);

  gtk_widget_set_hexpand(processBox, TRUE);
  gtk_widget_set_valign(processBox, GTK_ALIGN_CENTER);
  gtk_widget_set_halign(processBox, GTK_ALIGN_CENTER);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(processBoxScroller),
                                processBox);

  GtkWidget *p1Btn = MainButton("P1 Button example", NULL, NULL);
  gtk_widget_set_name(p1Btn, "ExampleButton");
  gtk_box_append(GTK_BOX(processBox), p1Btn);
  GtkWidget *p2Btn = MainButton("P2 Button example", NULL, NULL);
  gtk_box_append(GTK_BOX(processBox), p2Btn);

  GtkWidget *simControlsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_append(GTK_BOX(simBox), simControlsBox);

  GtkWidget *processInfoBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_name(processInfoBox, "processBox");
  gtk_widget_set_vexpand(simBox, TRUE);
  gtk_widget_set_hexpand(simBox, TRUE);
  gtk_paned_set_end_child(GTK_PANED(simContainer), processInfoBox);

  GListStore *processStore = g_list_store_new(G_TYPE_OBJECT);
  evData->update_sim_canvas.info_store = processStore;

  g_list_store_append(processStore, sc_process_gio_new(0, 0, 0, 0));
  fprintf(stderr, "INFO: Creating selection model...\n");
  GtkNoSelection *selectionModel =
      gtk_no_selection_new(G_LIST_MODEL(processStore));
  GtkWidget *tableView =
      gtk_column_view_new(GTK_SELECTION_MODEL(selectionModel));
  gtk_box_append(GTK_BOX(processInfoBox), tableView);
  gtk_widget_set_vexpand(tableView, TRUE);
  gtk_widget_set_hexpand(tableView, TRUE);
  gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(tableView), TRUE);
  gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(tableView), TRUE);

  // PID column setup
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_label_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_pid_cb), NULL);
  GtkColumnViewColumn *col = gtk_column_view_column_new("PID", factory);
  gtk_column_view_column_set_expand(col, TRUE);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(tableView), col);

  // Burst time column setup
  factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_label_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_burst_time_cb), NULL);
  col = gtk_column_view_column_new("Burst Time", factory);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(tableView), col);

  // Arrival time column setup
  factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_label_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_arrival_time_cb), NULL);
  col = gtk_column_view_column_new("Arrival Time", factory);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(tableView), col);

  // Priority column setup
  factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_label_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_priority_cb), NULL);
  col = gtk_column_view_column_new("Priority", factory);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(tableView), col);

  // Algorithm selection and load new file half
  GtkWidget *controlsContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_name(controlsContainer, "controlsContainer");
  gtk_widget_set_hexpand(controlsContainer, TRUE);
  gtk_widget_set_vexpand(controlsContainer, TRUE);
  gtk_paned_set_end_child(GTK_PANED(container), controlsContainer);

  GtkWidget *algorithmSelectionContainer =
      gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_name(algorithmSelectionContainer,
                      "algorithmSelectionContainer");
  gtk_widget_set_hexpand(algorithmSelectionContainer, TRUE);
  gtk_widget_set_vexpand(algorithmSelectionContainer, TRUE);
  gtk_box_append(GTK_BOX(controlsContainer), algorithmSelectionContainer);

  GListStore *reviewStore = g_list_store_new(G_TYPE_OBJECT);
  evData->new_file_loaded.review_store = reviewStore;

  const char *algoNames[] = {
      "First In First Out", "Shortest Job First", "Shortest Remaining Time",
      "Round Robin",        "Priority",
  };
  GtkWidget *group = gtk_check_button_new();
  for (int i = 0; i < 5; i++) {
    GtkWidget *checkBox = gtk_check_button_new_with_label(algoNames[i]);
    gtk_box_append(GTK_BOX(algorithmSelectionContainer), checkBox);
    gtk_check_button_set_group(GTK_CHECK_BUTTON(checkBox),
                               GTK_CHECK_BUTTON(group));
    if (i == 0) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_algorithm_to_first_in_first_out),
                       evData);
      gtk_check_button_set_active(GTK_CHECK_BUTTON(checkBox), TRUE);
    } else if (i == 1) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_algorithm_to_shortest_first), evData);
    } else if (i == 2) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_algorithm_to_shortest_remaining),
                       evData);
    } else if (i == 3) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_algorithm_to_round_robin), evData);
    } else if (i == 4) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_algorithm_to_priority), evData);
    }
  }

  g_list_store_append(reviewStore, sc_algorithm_performance_new("TEST", 5.0));
  GtkNoSelection *reviewStoreSelectionModel =
      gtk_no_selection_new(G_LIST_MODEL(reviewStore));
  GtkWidget *reviewTable =
      gtk_column_view_new(GTK_SELECTION_MODEL(reviewStoreSelectionModel));
  gtk_box_append(GTK_BOX(algorithmSelectionContainer), reviewTable);
  gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(reviewTable), TRUE);

  // Name column setup
  factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_label_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_algorithm_name_cb), NULL);
  col = gtk_column_view_column_new("Algorithm Name", factory);
  gtk_column_view_column_set_expand(col, TRUE);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(reviewTable), col);

  // AVG Waiting time setup
  factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_label_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_avg_waiting_time_cb), NULL);
  col = gtk_column_view_column_new("AVG Waiting Time", factory);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(reviewTable), col);

  GtkWidget *loadFileContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 30);
  gtk_widget_set_name(loadFileContainer, "loadFileContainer");
  gtk_widget_set_hexpand(algorithmSelectionContainer, TRUE);
  gtk_widget_set_vexpand(algorithmSelectionContainer, TRUE);
  gtk_box_append(GTK_BOX(controlsContainer), loadFileContainer);

  GtkWidget *quantumEntry = gtk_spin_button_new_with_range(0, 1000, 1);
  gtk_widget_set_valign(quantumEntry, GTK_ALIGN_CENTER);
  evData->new_file_loaded.spin_button = GTK_SPIN_BUTTON(quantumEntry);
  g_signal_connect(quantumEntry, "value-changed",
                   G_CALLBACK(handle_quantum_updated), evData);

  GtkWidget *backButton = MainButton("Back", handle_previous_click, evData);
  gtk_box_append(GTK_BOX(simControlsBox), backButton);
  // GtkWidget *ppButton = MainButton("Pause/Play", NULL, NULL);
  // gtk_box_append(GTK_BOX(simControlsBox), ppButton);
  GtkWidget *nextButton = MainButton("Next", handle_next_click, evData);
  gtk_box_append(GTK_BOX(simControlsBox), nextButton);

  g_signal_connect(resetBtn, "clicked", G_CALLBACK(handle_reset_click), evData);

  GtkWidget *loadFileBtn =
      MainButton("Load File", handle_open_file_click, evData);
  gtk_widget_set_valign(loadFileBtn, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(loadFileContainer), loadFileBtn);

  gtk_box_append(GTK_BOX(loadFileContainer), quantumEntry);

  return container;
}

/**
 * Creates the sync view.
 *
 * @param window GtkWindow* The window where this component will be displayed,
 * it's not the parent container!
 * @return GtkWidget* The container of this view.
 */
static GtkWidget *SyncView(GtkWindow *window) {
  // === SIDEBAR LAYOUT ===
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_hexpand(sidebar, FALSE);
  gtk_widget_set_vexpand(sidebar, TRUE);

  // === ALLOCATE EVENT ===

  SC_SyncGlobalEventData *evData = malloc(sizeof(SC_SyncGlobalEventData));
  if (NULL == evData) {
    SC_PANIC("Failed to malloc enough space for the file loader event!\n");
    return NULL;
  }

  // === LOAD PROCESSES ===
  GtkTextBuffer *process_buffer = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(process_buffer, "NO FILE LOADED!", -1);

  evData->new_file_loaded.window = window;
  evData->new_file_loaded.processes_buffer = GTK_TEXT_BUFFER(process_buffer);

  GtkWidget *load_process_btn =
      MainButton("Load processes", sync_handle_load_process, evData);
  gtk_box_append(GTK_BOX(sidebar), load_process_btn);

  GtkWidget *process_text_view = gtk_text_view_new_with_buffer(process_buffer);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(process_text_view), FALSE);

  // Wrap in scrolled window
  GtkWidget *process_scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(process_scroll, TRUE);
  gtk_widget_set_hexpand(process_scroll, TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(process_scroll),
                                process_text_view);
  gtk_box_append(GTK_BOX(sidebar), process_scroll);

  // === LOAD RESOURCES ===
  GtkTextBuffer *resources_buffer = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(resources_buffer, "NO FILE LOADED!", -1);

  evData->new_file_loaded.resources_buffer = GTK_TEXT_BUFFER(resources_buffer);

  GtkWidget *load_resources_btn =
      MainButton("Load resources", sync_handle_load_resources, evData);
  gtk_box_append(GTK_BOX(sidebar), load_resources_btn);

  GtkWidget *resources_text_view =
      gtk_text_view_new_with_buffer(resources_buffer);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(resources_text_view), FALSE);

  GtkWidget *resources_scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(resources_scroll, TRUE);
  gtk_widget_set_hexpand(resources_scroll, TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(resources_scroll),
                                resources_text_view);
  gtk_box_append(GTK_BOX(sidebar), resources_scroll);

  // === LOAD ACTIONS ===

  GtkTextBuffer *actions_buffer = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(actions_buffer, "NO FILE LOADED!", -1);

  evData->new_file_loaded.actions_buffer = GTK_TEXT_BUFFER(actions_buffer);

  GtkWidget *load_actions_btn =
      MainButton("Load actions", sync_handle_load_actions, evData);
  gtk_box_append(GTK_BOX(sidebar), load_actions_btn);

  GtkWidget *actions_text_view = gtk_text_view_new_with_buffer(actions_buffer);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(actions_text_view), FALSE);

  GtkWidget *actions_scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(actions_scroll, TRUE);
  gtk_widget_set_hexpand(actions_scroll, TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(actions_scroll),
                                actions_text_view);
  gtk_box_append(GTK_BOX(sidebar), actions_scroll);

  // === TOPBAR ===
  GtkWidget *topbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign(topbar, GTK_ALIGN_FILL);
  gtk_widget_set_valign(topbar, GTK_ALIGN_START);
  gtk_widget_add_css_class(topbar, "top_bar");

  // Right-side widgets
  GtkWidget *mutex_label = gtk_label_new("Mutex");
  gtk_box_append(GTK_BOX(topbar), mutex_label);

  GtkWidget *switch_widget = gtk_switch_new();
  gtk_box_append(GTK_BOX(topbar), switch_widget);
  evData->new_file_loaded.syncronization_switch = switch_widget;

  GtkWidget *semaphore_label = gtk_label_new("Semaphore");
  gtk_box_append(GTK_BOX(topbar), semaphore_label);

  // === Add number input (spin button) ===
  // initial=1, min=0, max=100, step=1
  GtkAdjustment *adjustment = gtk_adjustment_new(1, 1, 100, 1, 10, 0);
  // step=1, digits=0
  GtkWidget *spin_button = gtk_spin_button_new(adjustment, 1, 0);
  gtk_box_append(GTK_BOX(topbar), spin_button);
  evData->new_file_loaded.semaphore_quantity = GTK_SPIN_BUTTON(spin_button);

  // === Spacer to push next widgets to the right ===
  GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(spacer, TRUE);
  gtk_widget_set_vexpand(spacer, FALSE);
  gtk_box_append(GTK_BOX(topbar), spacer);

  // "Load" Button (left)
  GtkWidget *load_button = gtk_button_new_with_label("Load Data");
  gtk_box_append(GTK_BOX(topbar), load_button);
  g_signal_connect(load_button, "clicked", G_CALLBACK(load_sync_files), evData);

  // === SIMULATION ===
  GtkWidget *simulation = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_halign(simulation, GTK_ALIGN_FILL);
  gtk_widget_set_valign(simulation, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand(simulation, TRUE);
  gtk_widget_set_vexpand(simulation, TRUE);
  gtk_widget_add_css_class(
      simulation, "simulation"); // was incorrectly added to `topbar` earlier

  GtkWidget *main_label = gtk_label_new("Main Content");
  GtkWidget *exit_button = gtk_button_new_with_label("Goodbye world!");
  g_signal_connect(exit_button, "clicked", G_CALLBACK(print_hello), NULL);

  gtk_box_append(GTK_BOX(simulation), main_label);
  gtk_box_append(GTK_BOX(simulation), exit_button);

  // === MAIN CONTENT ===
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_halign(main_box, GTK_ALIGN_FILL);
  gtk_widget_set_valign(main_box, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(main_box, TRUE);
  gtk_widget_set_vexpand(main_box, TRUE);
  gtk_widget_add_css_class(main_box, "main_content");

  // Insert topbar at the top of the vertical box
  gtk_box_append(GTK_BOX(main_box), topbar);
  gtk_box_append(GTK_BOX(main_box), simulation);

  // === SPLIT VIEW ===
  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_hexpand(paned, TRUE);
  gtk_widget_set_vexpand(paned, TRUE);

  // Assign children to the paned container
  gtk_paned_set_start_child(GTK_PANED(paned), sidebar);
  gtk_paned_set_end_child(GTK_PANED(paned), main_box);

  // Optional: set initial position (in pixels) for sidebar width
  gtk_paned_set_position(GTK_PANED(paned), 200);

  return paned;
}

static void activate(GtkApplication *app, gpointer user_data) {

  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "SCHADuler");
  gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(box, GTK_ALIGN_FILL);
  gtk_widget_set_valign(box, GTK_ALIGN_FILL);
  gtk_window_set_child(GTK_WINDOW(window), box);

  GtkWidget *tabStack = gtk_stack_new();
  gtk_box_append((GtkBox *)box, tabStack);

  GtkWidget *tabSwitcher = gtk_stack_switcher_new();
  gtk_stack_switcher_set_stack((GtkStackSwitcher *)tabSwitcher,
                               (GtkStack *)tabStack);

  GtkWidget *headerBar = gtk_header_bar_new();
  gtk_header_bar_set_title_widget((GtkHeaderBar *)headerBar, tabSwitcher);
  gtk_window_set_titlebar((GtkWindow *)window, headerBar);

  GtkWidget *calendarView = CalendarView((GtkWindow *)window);
  gtk_stack_add_titled((GtkStack *)tabStack, calendarView, "Calendarizacion",
                       "Calendarizacion");

  GtkWidget *sincronizationView = SyncView((GtkWindow *)window);
  gtk_stack_add_titled((GtkStack *)tabStack, sincronizationView,
                       "Sincronizacion", "Sincronizacion");

  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  SC_ProcessList_Init(&PROCESS_LIST);
  SC_StringList_Init(&PID_LIST);

  size_t err = NO_ERROR;
  SC_Arena_Init(&PROCESS_LIST_ARENA, sizeof(SC_Process) * INITIAL_PROCESSES,
                &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "FATAL: Failed to initialize process arena!\n");
    return 1;
  }

  SC_Arena_Init(&PIDS_ARENA,
                (sizeof(SC_String) + sizeof(char) * 10) * INITIAL_PROCESSES,
                &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "FATAL: Failed to initialize pids arena!\n");
    SC_Arena_Deinit(&PROCESS_LIST_ARENA);
    return 1;
  }

  SC_Arena_Init(
      &SIM_ARENA,
      sizeof(SC_Simulation) +
          (sizeof(SC_SimStepState) + sizeof(SC_Process) * INITIAL_PROCESSES) *
              INITIAL_PROCESSES * 5,
      &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "FATAL: Failed to initialize simulation arena!\n");
    SC_Arena_Deinit(&PROCESS_LIST_ARENA);
    SC_Arena_Deinit(&PIDS_ARENA);
    return 1;
  }

  SC_Arena_Init(&SIM_BTN_LABELS_ARENA, sizeof(char) * 10 * INITIAL_PROCESSES,
                &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "FATAL: Failed to initialize btn labels arena!\n");
    SC_Arena_Deinit(&PROCESS_LIST_ARENA);
    SC_Arena_Deinit(&PIDS_ARENA);
    SC_Arena_Deinit(&SIM_ARENA);
    return 1;
  }

  // Syncronization

  SC_StringList_Init(&SYNC_PROCESS_NAMES);
  SC_StringList_Init(&SYNC_RESOURCES_NAMES);
  SC_StringList_Init(&SYNC_ACTIONS_NAMES);

  static SC_String PROCESS_FILE_CONTENT = {
      .length = 0,
      .data = NULL,
      .data_capacity = 0,
  };
  static SC_String RESOURCES_FILE_CONTENT = {
      .length = 0,
      .data = NULL,
      .data_capacity = 0,
  };
  static SC_String ACTIONS_FILE_CONTENT = {
      .length = 0,
      .data = NULL,
      .data_capacity = 0,
  };

  SC_Arena_Init(&SYNC_SIM_ARENA,
                sizeof(SC_SyncSimulator) +
                    sizeof(SC_SyncProcess) * INITIAL_PROCESSES +
                    sizeof(SC_Action) * INITIAL_ACTIONS +
                    sizeof(SC_Resource) * INITIAL_RESOURCES,
                &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "FATAL: Failed to initialize pids arena!\n");
    SC_Arena_Deinit(&PROCESS_LIST_ARENA);
    return 1;
  }

  gtk_init();

  GtkApplication *app = gtk_application_new("uwu.uvgenios.schaduler",
                                            G_APPLICATION_DEFAULT_FLAGS);

  GdkDisplay *display = gdk_display_get_default();
  if (display == NULL) {
    fprintf(stderr, "FATAL: No GDK display found!\n");
    SC_Arena_Deinit(&PROCESS_LIST_ARENA);
    SC_Arena_Deinit(&PIDS_ARENA);
    SC_Arena_Deinit(&SIM_ARENA);
    SC_Arena_Deinit(&SIM_BTN_LABELS_ARENA);
    return 1;
  }

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(provider,
                                      "/uwu/uvgenios/schaduler/app.css");
  gtk_style_context_add_provider_for_display(
      display, GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  fprintf(stderr, "INFO: deiniting all arenas\n");
  SC_Arena_Deinit(&PROCESS_LIST_ARENA);
  SC_Arena_Deinit(&PIDS_ARENA);
  SC_Arena_Deinit(&SIM_ARENA);
  SC_Arena_Deinit(&SIM_BTN_LABELS_ARENA);
  SC_Arena_Deinit(&SYNC_SIM_ARENA);

  return status;
}
