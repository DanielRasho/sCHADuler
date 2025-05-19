#include "lib.c"
#include <gtk/gtk.h>

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
  GtkWidget *button = gtk_button_new_with_label("Hello World");

  g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_window_destroy),
                           window);

  return button;
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

  gtk_window_set_titlebar((GtkWindow *)window, tabSwitcher);

  GtkWidget *calendarView = buildCalendarView((GtkWindow *)window);
  GtkWidget *sincronizationView = buildSyncView((GtkWindow *)window);

  gtk_stack_add_titled((GtkStack *)tabStack, calendarView, "Calendarizacion",
                       "Calendarizacion");
  gtk_stack_add_titled((GtkStack *)tabStack, sincronizationView,
                       "Sincronizacion", "Sincronizacion");

  // gtk_box_append(GTK_BOX(box), button);

  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("uwu.uvgenios.schaduler",
                                            G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}
