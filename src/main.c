#include "gui.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>

// The original activate function is now integrated into main.
// static void activate(GtkApplication *app, gpointer user_data) {
//   GtkWidget *window = create_full_window(app);

//   // Create Notebook (Tabs)
//   GtkWidget *notebook = gtk_notebook_new();
//   gtk_container_add(GTK_CONTAINER(window), notebook);

//   // Tab 1: Sort
//   GtkWidget *tab_sort = create_tab_sort();
//   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_sort,
//                            gtk_label_new("Tableaux & Tri"));

//   // Tab 2: List
//   GtkWidget *tab_list = create_tab_list();
//   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_list,
//                            gtk_label_new("Listes Chainees"));

//   // Tab 3: Tree
//   GtkWidget *tab_tree = create_tab_tree();
//   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_tree,
//                            gtk_label_new("Arbres"));

//   // Tab 4: Graph
//   GtkWidget *tab_graph = create_tab_graph();
//   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_graph,
//                            gtk_label_new("Graphes"));

//   gtk_widget_show_all(window);
// }

int main(int argc, char **argv) {
  // Initialize random seed
  srand(time(NULL));

  gtk_init(&argc, &argv);

  load_css();

  GtkWidget *window = create_full_window();

  // Create Notebook (Tabs)
  GtkWidget *notebook = gtk_notebook_new();
  gtk_container_add(GTK_CONTAINER(window), notebook);

  // Tab 1: Sort
  GtkWidget *tab_sort = create_tab_sort();
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_sort,
                           gtk_label_new("Tableaux & Tri"));

  // Tab 2: List
  GtkWidget *tab_list = create_tab_list();
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_list,
                           gtk_label_new("Listes Chainees"));

  // Tab 3: Tree
  GtkWidget *tab_tree = create_tab_tree();
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_tree,
                           gtk_label_new("Arbres"));

  // Tab 4: Graph
  GtkWidget *tab_graph = create_tab_graph();
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_graph,
                           gtk_label_new("Graphes"));

  gtk_widget_show_all(window);

  gtk_main();

  return 0;
}
