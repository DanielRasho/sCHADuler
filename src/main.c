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

// ################################
// ||                            ||
// ||          STRUCTS           ||
// ||                            ||
// ################################

typedef struct {
  GtkBox *canvas_container;
  GtkLabel *step_label;
  GListStore *info_store;
} SC_UpdateSimCanvasData;

typedef struct {
  GtkSpinButton *spin_button;
  GtkWindow *window;
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
} SC_SyncLoadedNewFileData;

typedef struct {
  SC_SyncLoadedNewFileData new_file_loaded;
} SC_SyncGlobalEventData;

// FIXME: Append to SC_GlobalEventData!
typedef struct {
  GtkWindow *window;
  GtkTextBuffer *buffer;
  GtkBox *button_container;
} SC_OpenFileEVData;

typedef int SC_Algorithm;
static SC_Algorithm SC_FirstInFirstOut = 1;
static SC_Algorithm SC_ShortestFirst = 2;
static SC_Algorithm SC_ShortestRemaining = 3;
static SC_Algorithm SC_RoundRobin = 4;
static SC_Algorithm SC_Priority = 5;

// ################################
// ||                            ||
// ||      CUSTOM GOBJECTS       ||
// ||                            ||
// ################################

#define CAPITAL_TYPE_ITEM (sc_process_gio_get_type())
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
  SCProcessGio *item = g_object_new(sc_process_gio_get_type(), NULL);
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

static SC_Simulation *SIM_STATE;
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

  char str[] = {'C', 'u', 'r', 'r', 'e', 'n', 't', ' ', 'S', 't',
                'e', 'p', ':', ' ', 0,   0,   0,   0,   0};
  sprintf(str + strlen(str), "%zu", SIM_STATE->current_step);
  gtk_label_set_label(params.step_label, str);

  SC_Arena_Reset(&SIM_BTN_LABELS_ARENA);
  for (int i = 0; i <= SIM_STATE->current_step; i++) {
    size_t current_process = SIM_STATE->steps[i].current_process;
    size_t pid_idx = SIM_STATE->steps[i].processes[current_process].pid_idx;
    SC_String pid_str = SC_StringList_GetAt(&PID_LIST, pid_idx, err);
    if (*err != NO_ERROR) {
      fprintf(stderr,
              "SIM_STEP_ERROR (%d): Failed to get pid for process (idx: %zu): "
              "Failed to get PID from stringlist with idx: %zu\n",
              i, current_process, pid_idx);
      return;
    }

    char css_class[] = {'p', 'i', 'd', '_', 0, 0, 0, 0, 0, 0, 0, 0};
    sprintf(css_class + strlen(css_class), "%zu", pid_idx);

    GtkWidget *label = gtk_label_new(pid_str.data);
    gtk_box_append(params.canvas_container, label);
    gtk_widget_add_css_class(label, css_class);
    gtk_widget_add_css_class(label, "pid_box");

    SC_Bool is_last_iteration = i == SIM_STATE->current_step;
    if (is_last_iteration) {
      g_list_store_remove_all(params.info_store);
      for (int j = 0; j < SIM_STATE->steps[i].process_length; j++) {
        SC_Process current = SIM_STATE->steps[i].processes[j];
        fprintf(stderr, "INFO: Appending value to store\n");
        g_list_store_append(
            params.info_store,
            sc_process_gio_new(current.pid_idx, current.burst_time,
                               current.arrival_time, current.priority));
      }
    }
  }
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

// ################################
// ||                            ||
// ||          HANDLERS          ||
// ||                            ||
// ################################

static void print_hello(GtkWidget *widget, gpointer data) {
  g_print("Hello World\n");
}

static void change_selected_algorithm(GtkCheckButton *self, gpointer *data) {
  SC_Algorithm algo = *(SC_Algorithm *)data;
  SELECTED_ALGORITHM = algo;
  fprintf(stderr, "Changing selected algorithm to: %d\n", SELECTED_ALGORITHM);
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

  SC_Arena_Reset(&SIM_ARENA);
  SIM_STATE = SC_Arena_Alloc(&SIM_ARENA, sizeof(SC_Simulation), &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "ERROR: %s\n", SC_Err_ToString(&err));
    exit(1);
  }

  SIM_STATE->steps =
      SC_Arena_Alloc(&SIM_ARENA, sizeof(SC_SimStepState) * total_steps, &err);
  if (err != NO_ERROR) {
    fprintf(stderr, "ERROR: %s\n", SC_Err_ToString(&err));
    exit(1);
  }

  for (size_t i = 0; i < total_steps; i++) {
    SIM_STATE->steps[i].processes =
        SC_Arena_Alloc(&SIM_ARENA, sizeof(SC_Process) * total_processes, &err);
    if (err != NO_ERROR) {
      fprintf(stderr, "ERROR: %s\n", SC_Err_ToString(&err));
      exit(1);
    }
    SIM_STATE->steps[i].process_length = total_processes;
  }
  SIM_STATE->step_length = total_steps;

  // TODO: Use the quantum variable depending on the algorithm...
  int quantum = gtk_spin_button_get_value_as_int(ev_data.spin_button);
  if (SELECTED_ALGORITHM == SC_FirstInFirstOut) {
    simulate_first_in_first_out(&PROCESS_LIST, SIM_STATE);
  }
  // TODO: Add another algorithms...

  // FIXME: The current step should be 0
  SIM_STATE->current_step = 0;

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
  SC_Bool has_next_step = SIM_STATE->current_step + 1 < SIM_STATE->step_length;
  if (has_next_step) {
    SIM_STATE->current_step += 1;
    size_t err = NO_ERROR;
    update_sim_canvas(ev_data->update_sim_canvas, &err);
    if (err != NO_ERROR) {
      return;
    }
  }
}

static void handle_previous_click(GtkWidget *widget, gpointer data) {
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SC_Bool has_previous_step = SIM_STATE->current_step > 0;
  if (has_previous_step) {
    SIM_STATE->current_step -= 1;
    size_t err = NO_ERROR;
    update_sim_canvas(ev_data->update_sim_canvas, &err);
    if (err != NO_ERROR) {
      return;
    }
  }
}

static void handle_reset_click(GtkWidget *widget, gpointer data) {
  SC_GlobalEventData *ev_data = (SC_GlobalEventData *)data;
  SC_Bool should_reset = SIM_STATE->current_step > 0;
  if (should_reset) {
    SIM_STATE->current_step = 0;
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
  gtk_box_append(GTK_BOX(topSimBox), tickLabel);

  GtkWidget *processBoxScroller = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(processBoxScroller, TRUE);
  gtk_widget_set_hexpand(processBoxScroller, TRUE);
  gtk_box_append(GTK_BOX(simBox), processBoxScroller);

  GtkWidget *processBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
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

  // PID column setup
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup_label_cb), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(bind_pid_cb), NULL);
  GtkColumnViewColumn *col = gtk_column_view_column_new("PID", factory);
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
                       G_CALLBACK(change_selected_algorithm),
                       &SC_FirstInFirstOut);
      gtk_check_button_set_active(GTK_CHECK_BUTTON(checkBox), TRUE);
    } else if (i == 1) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_selected_algorithm),
                       &SC_ShortestFirst);
    } else if (i == 2) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_selected_algorithm),
                       &SC_ShortestRemaining);
    } else if (i == 3) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_selected_algorithm), &SC_RoundRobin);
    } else if (i == 4) {
      g_signal_connect(checkBox, "toggled",
                       G_CALLBACK(change_selected_algorithm), &SC_Priority);
    }
  }

  GtkWidget *avgLabel = gtk_label_new("AVG: 0");
  gtk_box_append(GTK_BOX(algorithmSelectionContainer), avgLabel);

  GtkWidget *loadFileContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 30);
  gtk_widget_set_name(loadFileContainer, "loadFileContainer");
  gtk_widget_set_hexpand(algorithmSelectionContainer, TRUE);
  gtk_widget_set_vexpand(algorithmSelectionContainer, TRUE);
  gtk_box_append(GTK_BOX(controlsContainer), loadFileContainer);

  GtkWidget *quantumEntry = gtk_spin_button_new_with_range(0, 1000, 1);
  gtk_widget_set_valign(quantumEntry, GTK_ALIGN_CENTER);

  SC_GlobalEventData *evData = malloc(sizeof(SC_GlobalEventData));
  if (NULL == evData) {
    SC_PANIC("Failed to malloc enough space for the file loader event!\n");
    return NULL;
  }

  evData->new_file_loaded.window = window;
  evData->new_file_loaded.spin_button = GTK_SPIN_BUTTON(quantumEntry);
  evData->update_sim_canvas.canvas_container = GTK_BOX(processBox);
  evData->update_sim_canvas.step_label = GTK_LABEL(tickLabel);
  evData->update_sim_canvas.info_store = processStore;

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

  // === MAIN CONTENT ===
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);

  GtkWidget *main_label = gtk_label_new("Main Content");

  GtkWidget *exit_button = gtk_button_new_with_label("Goodbye world!");
  g_signal_connect(exit_button, "clicked", G_CALLBACK(print_hello), NULL);
  g_signal_connect_swapped(exit_button, "clicked",
                           G_CALLBACK(gtk_window_destroy), window);

  gtk_box_append(GTK_BOX(main_box), main_label);
  gtk_box_append(GTK_BOX(main_box), exit_button);

  // === SPLIT VIEW ===
  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_hexpand(paned, TRUE);
  gtk_widget_set_vexpand(paned, TRUE);

  // Assign children to the paned container
  gtk_paned_set_start_child(GTK_PANED(paned), sidebar);
  gtk_paned_set_end_child(GTK_PANED(paned), main_box);

  // Optional: set initial position (in pixels) for sidebar width
  gtk_paned_set_position(GTK_PANED(paned), 400);

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
