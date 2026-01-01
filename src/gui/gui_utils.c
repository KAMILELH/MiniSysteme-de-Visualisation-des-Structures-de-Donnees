#include "gui.h"
#include <gtk/gtk.h>
#include <stdio.h>


void load_css(void) {
  GtkCssProvider *provider = gtk_css_provider_new();
  GdkDisplay *display = gdk_display_get_default();
  GdkScreen *screen = gdk_display_get_default_screen(display);

  gtk_style_context_add_provider_for_screen(
      screen, GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  GError *error = NULL;
  // Try to load style.css from current directory
  gtk_css_provider_load_from_path(provider, "style.css", &error);

  if (error) {
    // Just print warning, don't crash
    g_printerr("CSS loading failed: %s\n", error->message);
    g_error_free(error);
  } else {
    g_print("CSS loaded successfully.\n");
  }

  g_object_unref(provider);
}
