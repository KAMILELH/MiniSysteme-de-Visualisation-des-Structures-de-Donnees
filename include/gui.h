#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>

// Main Window
GtkWidget *create_full_window(void);
void load_css(void);

// Tabs
GtkWidget *create_tab_sort(void);
GtkWidget *create_tab_list(void);
GtkWidget *create_tab_tree(void);
GtkWidget *create_tab_graph(void);

#endif
