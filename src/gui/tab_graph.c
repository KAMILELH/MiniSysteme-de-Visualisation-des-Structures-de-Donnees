#include "backend.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  Graph *graph;
  GtkWidget *drawing_area;
  GtkWidget *combo_algo;
  GtkWidget *combo_type;

  GtkWidget *entry_node_val;
  GtkWidget *entry_start_node;
  GtkWidget *entry_end_node;
  GtkWidget *label_info;
  GtkWidget *radio_move;
  GtkWidget *radio_add;
  GtkWidget *radio_link;

  DataType current_type;

  int selected_node_id;
  int edge_src_id;
  int tool_mode; // 0=Move, 1=Add, 2=Link

  // Dragging
  gboolean is_dragging;
  int drag_node_id;

  float *highlight_dist;
  int *highlight_prev_arr;
  int show_algo_results;
  int highlight_start_id;
  int highlight_end_id;
} TabGraphWidgets;

static TabGraphWidgets *widgets_graph;

// --- Helpers ---

static void *parse_node_input(const char *txt) {
  if (!txt)
    return NULL;
  void *data = NULL;
  switch (widgets_graph->current_type) {
  case TYPE_INT:
    data = malloc(sizeof(int));
    *(int *)data = atoi(txt);
    break;
  case TYPE_DOUBLE:
    data = malloc(sizeof(double));
    *(double *)data = atof(txt);
    break;
  case TYPE_CHAR:
    data = malloc(sizeof(char));
    if (strlen(txt) > 0)
      *(char *)data = txt[0];
    else
      *(char *)data = '?';
    break;
  case TYPE_STRING:
    data = strdup(txt);
    break;
  default:
    break;
  }
  return data;
}

static char *format_node_data(void *data) {
  char *buf = malloc(64);
  if (!data) {
    strcpy(buf, "?");
    return buf;
  }
  switch (widgets_graph->current_type) {
  case TYPE_INT:
    snprintf(buf, 64, "%d", *(int *)data);
    break;
  case TYPE_DOUBLE:
    snprintf(buf, 64, "%.1f", *(double *)data);
    break;
  case TYPE_CHAR:
    snprintf(buf, 64, "%c", *(char *)data);
    break;
  case TYPE_STRING:
    snprintf(buf, 64, "%s", (char *)data);
    break;
  default:
    strcpy(buf, "?");
    break;
  }
  return buf;
}

static GtkWidget *create_card_frame(const char *title) {
  GtkWidget *frame = gtk_frame_new(title);
  gtk_widget_set_margin_start(frame, 5);
  gtk_widget_set_margin_end(frame, 5);
  gtk_widget_set_margin_top(frame, 5);
  gtk_widget_set_margin_bottom(frame, 5);
  return frame;
}

static void style_button_color(GtkWidget *btn, const char *color_str) {
  GdkRGBA color;
  gdk_rgba_parse(&color, color_str);
  gtk_widget_override_background_color(btn, GTK_STATE_FLAG_NORMAL, &color);
}

static int get_node_at(int x, int y) {
  if (!widgets_graph->graph)
    return -1;
  for (int i = 0; i < widgets_graph->graph->count; i++) {
    GraphNode *node = widgets_graph->graph->nodes[i];
    if (!node)
      continue;
    int dx = x - node->x;
    int dy = y - node->y;
    if (dx * dx + dy * dy <= 20 * 20)
      return i; // Index in node pointer array
  }
  return -1;
}

// --- DRAWING ---
static void draw_arrow(cairo_t *cr, double x1, double y1, double x2,
                       double y2) {
  double angle = atan2(y2 - y1, x2 - x1);
  double arrow_len = 10.0;

  double start_x = x1 + 20 * cos(angle);
  double start_y = y1 + 20 * sin(angle);
  double end_x = x2 - 20 * cos(angle);
  double end_y = y2 - 20 * sin(angle);

  cairo_move_to(cr, start_x, start_y);
  cairo_line_to(cr, end_x, end_y);
  cairo_stroke(cr);

  cairo_move_to(cr, end_x, end_y);
  cairo_line_to(cr, end_x - arrow_len * cos(angle - G_PI / 6),
                end_y - arrow_len * sin(angle - G_PI / 6));
  cairo_move_to(cr, end_x, end_y);
  cairo_line_to(cr, end_x - arrow_len * cos(angle + G_PI / 6),
                end_y - arrow_len * sin(angle + G_PI / 6));
  cairo_stroke(cr);
}

static gboolean on_draw_graph(GtkWidget *widget, cairo_t *cr, gpointer data) {
  if (!widgets_graph->graph)
    return FALSE;

  cairo_set_source_rgb(cr, 0.96, 0.96, 0.96);
  cairo_paint(cr);

  int n = widgets_graph->graph->count;

  // Edges
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      float w = widgets_graph->graph->matrix[i][j];

      if (w < 1e8) {
        GraphNode *u = widgets_graph->graph->nodes[i];
        GraphNode *v = widgets_graph->graph->nodes[j];

        int is_path = 0;
        if (widgets_graph->show_algo_results &&
            widgets_graph->highlight_prev_arr) {
          int curr = widgets_graph->highlight_end_id;
          while (curr != -1 && curr != widgets_graph->highlight_start_id) {
            int p = widgets_graph->highlight_prev_arr[curr];
            if (p == i && curr == j) {
              is_path = 1;
              break;
            }
            curr = p;
          }
        }

        if (is_path) {
          cairo_set_source_rgb(cr, 0.157, 0.655, 0.271);
          cairo_set_line_width(cr, 3.0);
        } else {
          cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
          cairo_set_line_width(cr, 1.5);
        }

        draw_arrow(cr, u->x, u->y, v->x, v->y);

        double mid_x = (u->x + v->x) / 2;
        double mid_y = (u->y + v->y) / 2 - 5;
        char buf[16];
        snprintf(buf, 16, "%.0f", w);
        cairo_set_source_rgb(cr, 0.098, 0.463, 0.824);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, mid_x, mid_y);
        cairo_show_text(cr, buf);
      }
    }
  }

  // Nodes
  for (int i = 0; i < n; i++) {
    GraphNode *node = widgets_graph->graph->nodes[i];

    if (i == widgets_graph->selected_node_id)
      cairo_set_source_rgb(cr, 1.0, 0.8, 0.2); // Gold
    else if (i == widgets_graph->edge_src_id)
      cairo_set_source_rgb(cr, 0.2, 0.8, 0.8); // Cyan
    else if (widgets_graph->show_algo_results &&
             (i == widgets_graph->highlight_start_id ||
              i == widgets_graph->highlight_end_id))
      cairo_set_source_rgb(cr, 0.5, 0.9, 0.5); // Green Start/End
    else
      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White

    cairo_arc(cr, node->x, node->y, 25.0, 0, 2 * G_PI);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
    cairo_arc(cr, node->x, node->y, 25.0, 0, 2 * G_PI);
    cairo_stroke(cr);

    // Draw Data (Generic)
    char *s = format_node_data(node->data);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, s, &ext);
    cairo_move_to(cr, node->x - ext.width / 2, node->y + ext.height / 2);
    cairo_show_text(cr, s);
    free(s);

    // ID (External Top-Right for Visibility)
    char idstats[8];
    snprintf(idstats, 8, "%d", node->id);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.5); // Navy Blue
    cairo_move_to(cr, node->x + 20, node->y - 20);
    cairo_show_text(cr, idstats);
  }

  return FALSE;
}

// --- INTERACTION ---

static void on_new_graph(GtkWidget *btn, gpointer data) {
  graph_free(widgets_graph->graph);
  widgets_graph->graph = graph_init(20);
  widgets_graph->selected_node_id = -1;
  widgets_graph->edge_src_id = -1;
  widgets_graph->show_algo_results = 0;
  gtk_widget_queue_draw(widgets_graph->drawing_area);
  gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                     "Nouveau graphe vide.");
}

static void on_config_change(GtkWidget *w, gpointer data) {
  int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets_graph->combo_type));
  DataType new_type = TYPE_INT;
  if (idx == 1)
    new_type = TYPE_DOUBLE;
  if (idx == 2)
    new_type = TYPE_CHAR;
  if (idx == 3)
    new_type = TYPE_STRING;

  if (widgets_graph->current_type != new_type) {
    widgets_graph->current_type = new_type;
    on_new_graph(NULL, NULL); // Clear graph on type change logic
    gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                       "Type changé (Graphe effacé).");
  }
}

static gboolean on_click(GtkWidget *widget, GdkEventButton *event,
                         gpointer data) {
  int id = get_node_at(event->x, event->y);

  if (widgets_graph->tool_mode == 1) { // Add Node
    if (id == -1) {
      const char *txt =
          gtk_entry_get_text(GTK_ENTRY(widgets_graph->entry_node_val));
      void *val = parse_node_input(txt);
      if (val) {
        int new_id = graph_add_node(widgets_graph->graph, val);
        if (new_id != -1) {
          widgets_graph->graph->nodes[new_id]->x = event->x;
          widgets_graph->graph->nodes[new_id]->y = event->y;
          gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                             "Nœud ajouté.");
        }
      } else {
        gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                           "Valeur invalide.");
      }
    }
    gtk_widget_queue_draw(widgets_graph->drawing_area);
    return TRUE;
  }

  if (widgets_graph->tool_mode == 2) { // Link
    if (id != -1) {
      if (widgets_graph->edge_src_id == -1) {
        widgets_graph->edge_src_id = id;
        gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                           "Source choisie. Cliquez Destination.");
      } else {
        if (widgets_graph->edge_src_id != id) {
          GtkWidget *dialog = gtk_dialog_new_with_buttons(
              "Poids de l'arc", GTK_WINDOW(gtk_widget_get_toplevel(widget)),
              GTK_DIALOG_MODAL, "OK", GTK_RESPONSE_ACCEPT, NULL);
          GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
          GtkWidget *entry_w = gtk_entry_new();
          gtk_entry_set_text(GTK_ENTRY(entry_w), "10");
          gtk_container_add(GTK_CONTAINER(content), entry_w);
          gtk_widget_show_all(content);

          if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            float w = atof(gtk_entry_get_text(GTK_ENTRY(entry_w)));
            graph_add_edge(widgets_graph->graph, widgets_graph->edge_src_id, id,
                           w);
            gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                               "Lien créé.");
          }
          gtk_widget_destroy(dialog);
          widgets_graph->edge_src_id = -1;
        }
      }
    } else {
      widgets_graph->edge_src_id = -1;
    }
    gtk_widget_queue_draw(widgets_graph->drawing_area);
    return TRUE;
  }

  // Select / Move
  if (id != -1) {
    widgets_graph->selected_node_id = id;
    widgets_graph->drag_node_id = id;
    widgets_graph->is_dragging = TRUE;

    char buf[16];
    snprintf(buf, 16, "%d", id);
    if (gtk_entry_get_text_length(GTK_ENTRY(widgets_graph->entry_start_node)) ==
        0)
      gtk_entry_set_text(GTK_ENTRY(widgets_graph->entry_start_node), buf);
    else
      gtk_entry_set_text(GTK_ENTRY(widgets_graph->entry_end_node), buf);

    gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                       "Nœud sélectionné.");
  } else {
    widgets_graph->selected_node_id = -1;
  }
  gtk_widget_queue_draw(widgets_graph->drawing_area);
  return TRUE;
}

static gboolean on_release(GtkWidget *widget, GdkEventButton *event,
                           gpointer data) {
  widgets_graph->is_dragging = FALSE;
  widgets_graph->drag_node_id = -1;
  return TRUE;
}

static gboolean on_motion(GtkWidget *widget, GdkEventMotion *event,
                          gpointer data) {
  if (widgets_graph->is_dragging && widgets_graph->drag_node_id != -1 &&
      widgets_graph->tool_mode == 0) {
    GraphNode *n = widgets_graph->graph->nodes[widgets_graph->drag_node_id];
    n->x = event->x;
    n->y = event->y;
    gtk_widget_queue_draw(widgets_graph->drawing_area);
  }
  return TRUE;
}

static void on_tool_toggled(GtkToggleButton *btn, gpointer data) {
  if (!gtk_toggle_button_get_active(btn))
    return;

  if (btn == GTK_TOGGLE_BUTTON(widgets_graph->radio_move))
    widgets_graph->tool_mode = 0;
  if (btn == GTK_TOGGLE_BUTTON(widgets_graph->radio_add))
    widgets_graph->tool_mode = 1;
  if (btn == GTK_TOGGLE_BUTTON(widgets_graph->radio_link))
    widgets_graph->tool_mode = 2;

  widgets_graph->edge_src_id = -1;
  gtk_label_set_text(GTK_LABEL(widgets_graph->label_info), "Mode changé.");
  gtk_widget_queue_draw(widgets_graph->drawing_area);
}

static void on_delete_sel(GtkWidget *btn, gpointer data) {
  if (widgets_graph->selected_node_id == -1)
    return;

  graph_remove_node(widgets_graph->graph, widgets_graph->selected_node_id);
  widgets_graph->selected_node_id = -1;
  widgets_graph->edge_src_id = -1;

  gtk_widget_queue_draw(widgets_graph->drawing_area);
  gtk_label_set_text(GTK_LABEL(widgets_graph->label_info), "Nœud supprimé.");
}

static void on_rand_graph(GtkWidget *btn, gpointer data) {
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Générer Graphe", GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn))),
      GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL, "Générer",
      GTK_RESPONSE_ACCEPT, NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *entry_n = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_n), "5");
  gtk_box_pack_start(GTK_BOX(content), gtk_label_new("Nombre de Nœuds:"), FALSE,
                     FALSE, 5);
  gtk_box_pack_start(GTK_BOX(content), entry_n, FALSE, FALSE, 5);
  gtk_widget_show_all(content);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    int n = atoi(gtk_entry_get_text(GTK_ENTRY(entry_n)));
    if (n > 20)
      n = 20;
    if (n < 2)
      n = 2;

    on_new_graph(NULL, NULL);

    if (n < 12) {
      // Circle layout for small N
      double cx = 400, cy = 300, r = 250;
      for (int i = 0; i < n; i++) {
        void *val = NULL;
        int rnds = rand();

        switch (widgets_graph->current_type) {
        case TYPE_INT:
          val = malloc(sizeof(int));
          *(int *)val = rnds % 100;
          break;
        case TYPE_DOUBLE:
          val = malloc(sizeof(double));
          *(double *)val = (rnds % 1000) / 10.0;
          break;
        case TYPE_CHAR:
          val = malloc(sizeof(char));
          *(char *)val = 'A' + (rnds % 26);
          break;
        case TYPE_STRING: {
          char buf[16];
          snprintf(buf, 16, "S-%d", rnds % 100);
          val = strdup(buf);
        } break;
        default:
          break;
        }

        graph_add_node(widgets_graph->graph, val);

        double angle = (2 * G_PI * i) / n;
        widgets_graph->graph->nodes[i]->x = cx + r * cos(angle);
        widgets_graph->graph->nodes[i]->y = cy + r * sin(angle);
      }
    } else {
      // Grid Layout for Large N
      int cols = (int)ceil(sqrt(n));
      int rows = (int)ceil((double)n / cols);
      double w = 750.0 / cols;
      double h = 550.0 / rows;
      double start_x = 25.0 + w / 2;
      double start_y = 25.0 + h / 2;

      for (int i = 0; i < n; i++) {
        void *val = NULL;
        int rnds = rand();
        // ... (Data Gen Code Duplicated or Helper? Let's just dupe for now to
        // fit in replacement block or extract)
        switch (widgets_graph->current_type) {
        case TYPE_INT:
          val = malloc(sizeof(int));
          *(int *)val = rnds % 100;
          break;
        case TYPE_DOUBLE:
          val = malloc(sizeof(double));
          *(double *)val = (rnds % 1000) / 10.0;
          break;
        case TYPE_CHAR:
          val = malloc(sizeof(char));
          *(char *)val = 'A' + (rnds % 26);
          break;
        case TYPE_STRING: {
          char buf[16];
          snprintf(buf, 16, "S-%d", rnds % 100);
          val = strdup(buf);
        } break;
        default:
          break;
        }

        graph_add_node(widgets_graph->graph, val);

        int r = i / cols;
        int c = i % cols;
        widgets_graph->graph->nodes[i]->x = start_x + c * w;
        widgets_graph->graph->nodes[i]->y = start_y + r * h;
      }
    }

    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        if (i == j)
          continue;
        if ((rand() % 100) < 30) {
          float w = (rand() % 20) + 1;
          graph_add_edge(widgets_graph->graph, i, j, w);
        }
      }
    }

    gtk_widget_queue_draw(widgets_graph->drawing_area);
    gtk_label_set_text(GTK_LABEL(widgets_graph->label_info),
                       "Graphe aléatoire généré.");
  }
  gtk_widget_destroy(dialog);
}

static void on_calc_path(GtkWidget *btn, gpointer data) {
  const char *s_start =
      gtk_entry_get_text(GTK_ENTRY(widgets_graph->entry_start_node));
  const char *s_end =
      gtk_entry_get_text(GTK_ENTRY(widgets_graph->entry_end_node));
  int start_id = atoi(s_start);
  int end_id = atoi(s_end);

  if (start_id < 0 || start_id >= widgets_graph->graph->count)
    return;

  int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets_graph->combo_algo));
  float *dist = NULL;
  int *prev = NULL;

  if (idx == 0)
    algo_dijkstra(widgets_graph->graph, start_id, &dist, &prev);
  else if (idx == 1)
    algo_bellman(widgets_graph->graph, start_id, &dist, &prev);
  else if (idx == 2) { // Floyd-Warshall
    float *d_mat = NULL;
    int *n_mat = NULL;
    algo_floyd(widgets_graph->graph, &d_mat, &n_mat);

    int n = widgets_graph->graph->count;
    dist = malloc(n * sizeof(float));
    prev = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) {
      dist[i] = INF;
      prev[i] = -1;
    }

    if (d_mat[start_id * n + end_id] < 1e8) {
      dist[end_id] = d_mat[start_id * n + end_id];
      int curr = start_id;
      while (curr != end_id) {
        int nxt = n_mat[curr * n + end_id];
        if (nxt == -1)
          break;
        prev[nxt] = curr;
        curr = nxt;
      }
    }
    free(d_mat);
    free(n_mat);
  }

  widgets_graph->highlight_prev_arr = prev;
  widgets_graph->highlight_dist = dist;
  widgets_graph->highlight_start_id = start_id;
  widgets_graph->highlight_end_id = end_id;
  widgets_graph->show_algo_results = 1;

  char msg[64];
  if (dist && dist[end_id] < 1e8)
    snprintf(msg, 64, "Coût total: %.0f", dist[end_id]);
  else
    snprintf(msg, 64, "Pas de chemin.");
  gtk_label_set_text(GTK_LABEL(widgets_graph->label_info), msg);

  gtk_widget_queue_draw(widgets_graph->drawing_area);
}

GtkWidget *create_tab_graph(void) {
  widgets_graph = malloc(sizeof(TabGraphWidgets));
  widgets_graph->graph = graph_init(20);
  widgets_graph->selected_node_id = -1;
  widgets_graph->edge_src_id = -1;
  widgets_graph->tool_mode = 0;
  widgets_graph->highlight_prev_arr = NULL;
  widgets_graph->show_algo_results = 0;
  widgets_graph->is_dragging = FALSE;
  widgets_graph->current_type = TYPE_INT;

  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_container_set_border_width(GTK_CONTAINER(main_box), 20);

  // --- SIDEBAR ---
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_size_request(sidebar, 280, -1);
  gtk_box_pack_start(GTK_BOX(main_box), sidebar, FALSE, FALSE, 0);

  // 1. Config
  GtkWidget *frame_conf = create_card_frame("1. Configuration");
  GtkWidget *box_conf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_set_border_width(GTK_CONTAINER(box_conf), 10);
  gtk_container_add(GTK_CONTAINER(frame_conf), box_conf);
  gtk_box_pack_start(GTK_BOX(sidebar), frame_conf, FALSE, FALSE, 0);

  // Type Selector
  gtk_box_pack_start(GTK_BOX(box_conf), gtk_label_new("Type de Données:"),
                     FALSE, FALSE, 0);
  widgets_graph->combo_type = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_graph->combo_type),
                                 "Entiers");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_graph->combo_type),
                                 "Réels");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_graph->combo_type),
                                 "Caractères");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_graph->combo_type),
                                 "Chaînes");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widgets_graph->combo_type), 0);
  g_signal_connect(widgets_graph->combo_type, "changed",
                   G_CALLBACK(on_config_change), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), widgets_graph->combo_type, FALSE, FALSE,
                     0);

  GtkWidget *btn_new = gtk_button_new_with_label("Nouveau Graphe");
  style_button_color(btn_new, "#007BFF");
  g_signal_connect(btn_new, "clicked", G_CALLBACK(on_new_graph), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), btn_new, FALSE, FALSE, 0);

  GtkWidget *btn_rand = gtk_button_new_with_label("Générer Aléatoire");
  style_button_color(btn_rand, "#6F42C1");
  g_signal_connect(btn_rand, "clicked", G_CALLBACK(on_rand_graph), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), btn_rand, FALSE, FALSE, 0);

  // 2. Tools
  GtkWidget *frame_tools = create_card_frame("2. Outils");
  GtkWidget *box_tools = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_set_border_width(GTK_CONTAINER(box_tools), 10);
  gtk_container_add(GTK_CONTAINER(frame_tools), box_tools);
  gtk_box_pack_start(GTK_BOX(sidebar), frame_tools, FALSE, FALSE, 0);

  widgets_graph->radio_move =
      gtk_radio_button_new_with_label(NULL, "Déplacer / Sélection");
  widgets_graph->radio_add = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(widgets_graph->radio_move), "Ajouter Nœud");
  widgets_graph->radio_link = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(widgets_graph->radio_move), "Lier (Arête)");

  g_signal_connect(widgets_graph->radio_move, "toggled",
                   G_CALLBACK(on_tool_toggled), NULL);
  g_signal_connect(widgets_graph->radio_add, "toggled",
                   G_CALLBACK(on_tool_toggled), NULL);
  g_signal_connect(widgets_graph->radio_link, "toggled",
                   G_CALLBACK(on_tool_toggled), NULL);

  gtk_box_pack_start(GTK_BOX(box_tools), widgets_graph->radio_move, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_tools), widgets_graph->radio_add, FALSE, FALSE,
                     0);

  // Value Entry for Add Node
  GtkWidget *hbox_add = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(hbox_add), gtk_label_new("      Valeur: "), FALSE,
                     FALSE, 0);
  widgets_graph->entry_node_val = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(widgets_graph->entry_node_val), "0");
  gtk_entry_set_width_chars(GTK_ENTRY(widgets_graph->entry_node_val), 8);
  gtk_box_pack_start(GTK_BOX(hbox_add), widgets_graph->entry_node_val, TRUE,
                     TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_tools), hbox_add, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box_tools), widgets_graph->radio_link, FALSE,
                     FALSE, 0);

  GtkWidget *btn_del_sel = gtk_button_new_with_label("Supprimer Sélection");
  style_button_color(btn_del_sel, "#DC3545");
  g_signal_connect(btn_del_sel, "clicked", G_CALLBACK(on_delete_sel), NULL);
  gtk_box_pack_start(GTK_BOX(box_tools), btn_del_sel, FALSE, FALSE, 5);

  // 3. Paths
  GtkWidget *frame_path = create_card_frame("3. Algorithmes");
  GtkWidget *box_path = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_set_border_width(GTK_CONTAINER(box_path), 10);
  gtk_container_add(GTK_CONTAINER(frame_path), box_path);
  gtk_box_pack_start(GTK_BOX(sidebar), frame_path, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box_path), gtk_label_new("Méthode :"), FALSE,
                     FALSE, 0);
  widgets_graph->combo_algo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_graph->combo_algo),
                                 "Dijkstra");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_graph->combo_algo),
                                 "Bellman-Ford");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_graph->combo_algo),
                                 "Floyd-Warshall");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widgets_graph->combo_algo), 0);
  gtk_box_pack_start(GTK_BOX(box_path), widgets_graph->combo_algo, FALSE, FALSE,
                     0);

  GtkWidget *hbox_ids = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  widgets_graph->entry_start_node = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_graph->entry_start_node),
                                 "Début");
  widgets_graph->entry_end_node = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_graph->entry_end_node),
                                 "Fin");
  gtk_entry_set_width_chars(GTK_ENTRY(widgets_graph->entry_start_node), 6);
  gtk_entry_set_width_chars(GTK_ENTRY(widgets_graph->entry_end_node), 6);
  gtk_box_pack_start(GTK_BOX(hbox_ids), widgets_graph->entry_start_node, TRUE,
                     TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_ids), widgets_graph->entry_end_node, TRUE,
                     TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_path), hbox_ids, FALSE, FALSE, 0);

  GtkWidget *btn_calc = gtk_button_new_with_label("Calculer le Chemin");
  style_button_color(btn_calc, "#28A745");
  g_signal_connect(btn_calc, "clicked", G_CALLBACK(on_calc_path), NULL);
  gtk_box_pack_start(GTK_BOX(box_path), btn_calc, FALSE, FALSE, 0);

  widgets_graph->label_info = gtk_label_new("Prêt.");
  gtk_widget_set_margin_top(widgets_graph->label_info, 10);
  gtk_box_pack_start(GTK_BOX(sidebar), widgets_graph->label_info, FALSE, FALSE,
                     0);

  // --- CANVAS ---
  GtkWidget *frame_canvas = gtk_frame_new("Visualisation");
  gtk_widget_set_margin_start(frame_canvas, 5);

  widgets_graph->drawing_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(widgets_graph->drawing_area, 800, 600);
  gtk_widget_add_events(widgets_graph->drawing_area,
                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                            GDK_POINTER_MOTION_MASK);
  g_signal_connect(widgets_graph->drawing_area, "draw",
                   G_CALLBACK(on_draw_graph), NULL);
  g_signal_connect(widgets_graph->drawing_area, "button-press-event",
                   G_CALLBACK(on_click), NULL);
  g_signal_connect(widgets_graph->drawing_area, "button-release-event",
                   G_CALLBACK(on_release), NULL);
  g_signal_connect(widgets_graph->drawing_area, "motion-notify-event",
                   G_CALLBACK(on_motion), NULL);

  gtk_container_add(GTK_CONTAINER(frame_canvas), widgets_graph->drawing_area);
  gtk_box_pack_start(GTK_BOX(main_box), frame_canvas, TRUE, TRUE, 0);

  return main_box;
}
