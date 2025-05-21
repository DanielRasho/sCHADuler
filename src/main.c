#include "gio/gio.h"
#include "glib-object.h"
#include "lib.c"
#include "resources.c"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  GtkWindow *window;
  GtkTextBuffer *buffer;
} SC_OpenFileEVData;

static void print_hello(GtkWidget *widget, gpointer data) {
  g_print("Hello World\n");
}

static void file_dialog_finished(GObject *source_object, GAsyncResult *res,
                                 gpointer data) {
  GError **error = NULL;
  GFile *file =
      gtk_file_dialog_open_finish((GtkFileDialog *)source_object, res, error);
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

  fprintf(stderr, "The file contents are:\n%*s\n", (int)length, contents);

  SC_OpenFileEVData *ev_data = (SC_OpenFileEVData *)data;
  gtk_text_buffer_set_text(ev_data->buffer, contents, length);
}

static void handle_open_file_click(GtkWidget *widget, gpointer data) {
  SC_OpenFileEVData *ev_data = (SC_OpenFileEVData *)data;
  GtkFileDialog *dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Archivo de simulacion");
  gtk_file_dialog_set_modal(dialog, TRUE);
  gtk_file_dialog_open(dialog, ev_data->window, NULL, file_dialog_finished,
                       data);
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
static GtkWidget *buildCalendarView(GtkWindow *window) {
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

  GtkWidget *topSimBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(topSimBox, TRUE);
  gtk_widget_set_halign(topSimBox, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(simBox), topSimBox);

  GtkWidget *resetBtn = MainButton("RESET", NULL, NULL);
  gtk_box_append(GTK_BOX(topSimBox), resetBtn);

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
  gtk_box_append(GTK_BOX(processBox), p1Btn);
  GtkWidget *p2Btn = MainButton("P2 Button example", NULL, NULL);
  gtk_box_append(GTK_BOX(processBox), p2Btn);

  GtkWidget *simControlsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_append(GTK_BOX(simBox), simControlsBox);

  GtkWidget *backButton = MainButton("Back", NULL, NULL);
  gtk_box_append(GTK_BOX(simControlsBox), backButton);
  GtkWidget *ppButton = MainButton("Pause/Play", NULL, NULL);
  gtk_box_append(GTK_BOX(simControlsBox), ppButton);
  GtkWidget *nextButton = MainButton("Next", NULL, NULL);
  gtk_box_append(GTK_BOX(simControlsBox), nextButton);

  GtkWidget *processInfoBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_name(processInfoBox, "processBox");
  gtk_widget_set_vexpand(simBox, TRUE);
  gtk_widget_set_hexpand(simBox, TRUE);
  gtk_paned_set_end_child(GTK_PANED(simContainer), processInfoBox);

  const char *exampleFileContents =
      "P1, 8, 7, 1\nP2, 4, 15, 2\nP3, 16, 2, 3\nP4, 20, 0, 50";
  GtkTextBuffer *fileContentBuffer = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(fileContentBuffer, exampleFileContents,
                           strlen(exampleFileContents));

  GtkWidget *fileContentsTextView =
      gtk_text_view_new_with_buffer(fileContentBuffer);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(fileContentsTextView), FALSE);
  gtk_box_append(GTK_BOX(processInfoBox), fileContentsTextView);
  gtk_widget_set_vexpand(fileContentsTextView, TRUE);
  gtk_widget_set_hexpand(fileContentsTextView, TRUE);

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
  }

  GtkWidget *avgLabel = gtk_label_new("AVG: 0");
  gtk_box_append(GTK_BOX(algorithmSelectionContainer), avgLabel);

  GtkWidget *loadFileContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 30);
  gtk_widget_set_name(loadFileContainer, "loadFileContainer");
  gtk_widget_set_hexpand(algorithmSelectionContainer, TRUE);
  gtk_widget_set_vexpand(algorithmSelectionContainer, TRUE);
  gtk_box_append(GTK_BOX(controlsContainer), loadFileContainer);

  SC_OpenFileEVData *evData = malloc(sizeof(SC_OpenFileEVData));
  if (NULL == evData) {
    SC_PANIC("Failed to malloc enough space for the file loader event!\n");
    return NULL;
  }

  evData->window = window;
  evData->buffer = GTK_TEXT_BUFFER(fileContentBuffer);

  GtkWidget *loadFileBtn =
      MainButton("Load File", handle_open_file_click, evData);
  gtk_widget_set_valign(loadFileBtn, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(loadFileContainer), loadFileBtn);

  GtkWidget *quantumEntry = gtk_spin_button_new_with_range(0, 1000, 1);
  gtk_widget_set_valign(quantumEntry, GTK_ALIGN_CENTER);
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
static GtkWidget *buildSyncView(GtkWindow *window) {
  GtkWidget *button = gtk_button_new_with_label("Goodye world!");

  g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_window_destroy),
                           window);

  return button;
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

  GtkWidget *calendarView = buildCalendarView((GtkWindow *)window);
  gtk_stack_add_titled((GtkStack *)tabStack, calendarView, "Calendarizacion",
                       "Calendarizacion");

  GtkWidget *sincronizationView = buildSyncView((GtkWindow *)window);
  gtk_stack_add_titled((GtkStack *)tabStack, sincronizationView,
                       "Sincronizacion", "Sincronizacion");

  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  gtk_init();

  GtkApplication *app = gtk_application_new("uwu.uvgenios.schaduler",
                                            G_APPLICATION_DEFAULT_FLAGS);

  GdkDisplay *display = gdk_display_get_default();
  if (display == NULL) {
    fprintf(stderr, "FATAL: No GDK display found!");
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

  return status;
}
