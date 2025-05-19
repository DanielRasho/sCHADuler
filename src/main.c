#include "lib.c"
#include "resources.c"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdio.h>

static void print_hello(GtkWidget *widget, gpointer data) {
  g_print("Hello World\n");
}

/**
 * Creates the scheduling view.
 *
 * @param window GtkWindow* The window where this component will be displayed,
 * it's not the parent container!
 * @return GtkWidget* The container of this view.
 */
static GtkWidget *buildCalendarView(GtkWindow *window) {
  GtkWidget *container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_halign(container, GTK_ALIGN_FILL);
  gtk_widget_set_vexpand(container, TRUE);
  gtk_widget_set_hexpand(container, TRUE);
  gtk_widget_set_size_request(container, -1, -1);

  GtkWidget *simContainer = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_name(simContainer, "simContainer");
  gtk_widget_set_halign(simContainer, GTK_ALIGN_FILL);
  gtk_widget_set_vexpand(simContainer, TRUE);
  gtk_widget_set_hexpand(simContainer, TRUE);

  GtkWidget *controlsContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_name(controlsContainer, "controlsContainer");
  gtk_widget_set_halign(controlsContainer, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(controlsContainer, TRUE);
  gtk_widget_set_vexpand(controlsContainer, TRUE);

  gtk_box_append((GtkBox *)container, simContainer);
  gtk_box_append((GtkBox *)container, controlsContainer);

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
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
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
  GtkWidget *sincronizationView = buildSyncView((GtkWindow *)window);

  gtk_stack_add_titled((GtkStack *)tabStack, calendarView, "Calendarizacion",
                       "Calendarizacion");
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
