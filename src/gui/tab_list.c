#include "backend.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  LinkedList list;
  GtkWidget *drawing_area;

  // Inputs
  GtkWidget *entry_val; // For manual add
  GtkWidget *entry_pos; // For manual insert pos
  GtkWidget *entry_del; // For delete value
  GtkWidget *entry_rand_n;

  // Config
  GtkWidget *combo_data_type;
  GtkWidget *radio_simple;
  GtkWidget *radio_double;

  // Saisie Mode
  GtkWidget *radio_manual;
  GtkWidget *radio_auto;

  // Containers to toggle
  GtkWidget *box_manual_ops;
  GtkWidget *box_auto_ops;

  GtkWidget *label_stats;

  // View Toggle
  gboolean show_chart;
  GtkWidget *btn_toggle_view;
} TabListWidgets;

static TabListWidgets *widgets_list;

// --- Helpers ---

static void style_button_color(GtkWidget *btn, const char *color_str) {
  GdkRGBA color;
  gdk_rgba_parse(&color, color_str);
  gtk_widget_override_background_color(btn, GTK_STATE_FLAG_NORMAL, &color);
}

static GtkWidget *create_card_frame(const char *title) {
  GtkWidget *frame = gtk_frame_new(title);
  GtkWidget *label = gtk_frame_get_label_widget(GTK_FRAME(frame));
  if (label) {
    char *markup = g_strdup_printf("<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
  }
  return frame;
}

static void update_stats() {
  char buf[64];
  snprintf(buf, sizeof(buf), "Taille: %zu | Type: %s", widgets_list->list.size,
           widgets_list->list.is_doubly ? "Double" : "Simple");
  gtk_label_set_text(GTK_LABEL(widgets_list->label_stats), buf);

  // Dynamic Resize (Only in List Mode)
  if (!widgets_list->show_chart) {
    int w = 100 + (widgets_list->list.size * 100);
    if (w < 800)
      w = 800;
    gtk_widget_set_size_request(widgets_list->drawing_area, w, 600);
  }
}

// --- Drawing Logic (Same as before) ---
static void draw_node_content(cairo_t *cr, double x, double y, void *data,
                              DataType type) {
  char buf[64] = "?";
  if (data) {
    switch (type) {
    case TYPE_INT:
      snprintf(buf, 64, "%d", *(int *)data);
      break;
    case TYPE_DOUBLE:
      snprintf(buf, 64, "%.1f", *(double *)data);
      break;
    case TYPE_CHAR:
      snprintf(buf, 64, "'%c'", *(char *)data);
      break;
    case TYPE_STRING:
      snprintf(buf, 64, "\"%s\"", (char *)data);
      break;
    default:
      strcpy(buf, "?");
    }
  }
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, buf, &ext);
  cairo_move_to(cr, x + 30 - ext.width / 2, y + 20);
  cairo_show_text(cr, buf);
}

// Forward declaration for chart drawing
static void draw_chart_popup(cairo_t *cr, double w, double h);

static gboolean on_draw_list(GtkWidget *widget, cairo_t *cr, gpointer data) {
  if (widgets_list->show_chart) {
    guint w = gtk_widget_get_allocated_width(widget);
    guint h = gtk_widget_get_allocated_height(widget);
    draw_chart_popup(cr, w, h);
    return FALSE;
  }

  // List Drawing Logic
  cairo_set_source_rgb(cr, 0.98, 0.98, 0.98);
  cairo_paint(cr);

  if (widgets_list->list.head == NULL) {
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_font_size(cr, 20);
    cairo_move_to(cr, 50, 50);
    cairo_show_text(cr, "Liste Vide");
    return FALSE;
  }

  Node *curr = widgets_list->list.head;
  double x = 50.0;
  double y = 80.0;
  double node_w = 60.0;
  double node_h = 30.0;
  double spacing = 40.0;

  int count = 0;
  int is_doubly = widgets_list->list.is_doubly;

  while (curr != NULL) {
    cairo_set_source_rgb(cr, 0.098, 0.463, 0.824); // #1976D2 Blue
    cairo_rectangle(cr, x, y, node_w, node_h);
    cairo_fill(cr);

    draw_node_content(cr, x, y, curr->data, widgets_list->list.type);

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 1.0);
    if (is_doubly) {
      cairo_move_to(cr, x + 10, y);
      cairo_line_to(cr, x + 10, y + node_h);
      cairo_move_to(cr, x + node_w - 10, y);
      cairo_line_to(cr, x + node_w - 10, y + node_h);
    } else {
      cairo_move_to(cr, x + node_w - 15, y);
      cairo_line_to(cr, x + node_w - 15, y + node_h);
    }
    cairo_stroke(cr);

    // Arrows
    if (curr->next) {
      double ax_start = x + node_w;
      double ax_end = x + node_w + spacing;
      double ay = y + node_h / 2;

      cairo_set_source_rgb(cr, 0, 0, 0);
      cairo_set_line_width(cr, 1.5);

      // Next Arrow
      double ky = is_doubly ? ay - 5 : ay;
      cairo_move_to(cr, ax_start, ky);
      cairo_line_to(cr, ax_end, ky);
      cairo_stroke(cr);
      cairo_move_to(cr, ax_end - 5, ky - 3);
      cairo_line_to(cr, ax_end, ky);
      cairo_line_to(cr, ax_end - 5, ky + 3);
      cairo_stroke(cr);

      // Prev Arrow
      if (is_doubly) {
        double py = ay + 5;
        cairo_move_to(cr, ax_end, py);
        cairo_line_to(cr, ax_start, py);
        cairo_stroke(cr);
        cairo_move_to(cr, ax_start + 5, py - 3);
        cairo_line_to(cr, ax_start, py);
        cairo_line_to(cr, ax_start + 5, py + 3);
        cairo_stroke(cr);
      }
    } else {
      cairo_set_source_rgb(cr, 0, 0, 0);
      cairo_set_font_size(cr, 10);
      cairo_move_to(cr, x + node_w + 5, y + node_h / 2 + 3);
      cairo_show_text(cr, "NULL");
    }

    x += node_w + spacing;
    // if (x > 750) {
    //   x = 50;
    //   y += 80;
    // }
    curr = curr->next;
    count++;
    // if (count > 100) break; // Limit removed
  }
  return FALSE;
}

// --- Logic ---

static void on_clear(GtkWidget *b, gpointer d) {
  list_clear(&widgets_list->list);
  gtk_widget_queue_draw(widgets_list->drawing_area);
  update_stats();
}

static void on_config_change(GtkWidget *w, gpointer d) {
  DataType dtype =
      gtk_combo_box_get_active(GTK_COMBO_BOX(widgets_list->combo_data_type));
  gboolean dbl = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(widgets_list->radio_double));

  if (widgets_list->list.type != dtype || widgets_list->list.is_doubly != dbl) {
    list_clear(&widgets_list->list);
    list_init(&widgets_list->list, dtype, dbl);
    gtk_widget_queue_draw(widgets_list->drawing_area);
    update_stats();
  }
}

static void on_mode_toggled(GtkToggleButton *btn, gpointer data) {
  gboolean manual = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(widgets_list->radio_manual));
  if (manual) {
    gtk_widget_show(widgets_list->box_manual_ops);
    gtk_widget_hide(widgets_list->box_auto_ops);
  } else {
    gtk_widget_hide(widgets_list->box_manual_ops);
    gtk_widget_show(widgets_list->box_auto_ops);
  }
}

// Add/Gen Logic
static void parse_and_add(int mode) { // 0=Head, 1=Tail, 2=Index
  const char *txt = gtk_entry_get_text(GTK_ENTRY(widgets_list->entry_val));
  if (!txt || strlen(txt) == 0)
    return;

  void *val = NULL;
  int ival;
  double dval;
  char cval;
  char *sval;

  switch (widgets_list->list.type) {
  case TYPE_INT:
    ival = atoi(txt);
    val = &ival;
    break;
  case TYPE_DOUBLE:
    dval = atof(txt);
    val = &dval;
    break;
  case TYPE_CHAR:
    cval = txt[0];
    val = &cval;
    break;
  case TYPE_STRING:
    sval = (char *)txt;
    val = sval;
    break;
  }

  if (mode == 0)
    list_prepend(&widgets_list->list, val);
  else if (mode == 1)
    list_append(&widgets_list->list, val);
  else if (mode == 2) {
    const char *pos_s = gtk_entry_get_text(GTK_ENTRY(widgets_list->entry_pos));
    int pos = atoi(pos_s);
    printf("DEBUG: Insert At Mode. Val: %s, Pos: %d\n", txt, pos);
    list_insert_at(&widgets_list->list, pos, val);
  }

  gtk_widget_queue_draw(widgets_list->drawing_area);
  update_stats();
}

static void on_add_head(GtkWidget *b, gpointer d) { parse_and_add(0); }
static void on_add_tail(GtkWidget *b, gpointer d) { parse_and_add(1); }
static void on_insert_at(GtkWidget *b, gpointer d) { parse_and_add(2); }

static void on_gen_rand(GtkWidget *b, gpointer d) {
  list_clear(&widgets_list->list);
  int n = atoi(gtk_entry_get_text(GTK_ENTRY(widgets_list->entry_rand_n)));
  if (n <= 0)
    n = 5;
  // if (n > 50) n = 50; // Limit removed

  DataType type = widgets_list->list.type;
  for (int i = 0; i < n; i++) {
    if (type == TYPE_INT) {
      int v = rand() % 100;
      list_append(&widgets_list->list, &v);
    } else if (type == TYPE_DOUBLE) {
      double v = (rand() % 1000) / 10.0;
      list_append(&widgets_list->list, &v);
    } else if (type == TYPE_CHAR) {
      char v = 'A' + rand() % 26;
      list_append(&widgets_list->list, &v);
    } else if (type == TYPE_STRING) {
      char buf[5];
      for (int k = 0; k < 4; k++)
        buf[k] = 'a' + rand() % 26;
      buf[4] = 0;
      list_append(&widgets_list->list, buf);
    }
  }
  gtk_widget_queue_draw(widgets_list->drawing_area);
  update_stats();
}

static void on_delete_val(GtkWidget *b, gpointer d) {
  const char *txt = gtk_entry_get_text(GTK_ENTRY(widgets_list->entry_del));
  if (!txt || strlen(txt) == 0)
    return;

  Node *curr = widgets_list->list.head;
  Node *prev = NULL;
  int idx = 0;

  while (curr) {
    int match = 0;
    switch (widgets_list->list.type) {
    case TYPE_INT:
      if (*(int *)curr->data == atoi(txt))
        match = 1;
      break;
    case TYPE_DOUBLE:
      if (*(double *)curr->data == atof(txt))
        match = 1;
      break;
    case TYPE_CHAR:
      if (*(char *)curr->data == txt[0])
        match = 1;
      break;
    case TYPE_STRING:
      if (strcmp((char *)curr->data, txt) == 0)
        match = 1;
      break;
    }
    if (match) {
      list_remove_at(&widgets_list->list, idx); // Re-use remove_at
      gtk_widget_queue_draw(widgets_list->drawing_area);
      update_stats();
      return;
    }
    curr = curr->next;
    idx++;
  }
}

// Comparators
static int cmp_int(const void *a, const void *b) {
  return (*(int *)a - *(int *)b);
}
static int cmp_double(const void *a, const void *b) {
  return (*(double *)a > *(double *)b) - (*(double *)a < *(double *)b);
}
static int cmp_char(const void *a, const void *b) {
  return (*(char *)a - *(char *)b);
}
static int cmp_string(const void *a, const void *b) {
  return strcmp((char *)a, (char *)b);
}

static CompareFunc get_cmp_func() {
  switch (widgets_list->list.type) {
  case TYPE_INT:
    return cmp_int;
  case TYPE_DOUBLE:
    return cmp_double;
  case TYPE_CHAR:
    return cmp_char;
  case TYPE_STRING:
    return cmp_string;
  }
  return NULL;
}

static void on_sort_bubble(GtkWidget *b, gpointer d) {
  list_sort(&widgets_list->list, 0, get_cmp_func());
  gtk_widget_queue_draw(widgets_list->drawing_area);
}
static void on_sort_insert(GtkWidget *b, gpointer d) {
  list_sort(&widgets_list->list, 1, get_cmp_func());
  gtk_widget_queue_draw(widgets_list->drawing_area);
}
static void on_sort_shell(GtkWidget *b, gpointer d) {
  list_sort(&widgets_list->list, 2, get_cmp_func());
  gtk_widget_queue_draw(widgets_list->drawing_area);
}
static void on_sort_quick(GtkWidget *b, gpointer d) {
  list_sort(&widgets_list->list, 3, get_cmp_func());
  gtk_widget_queue_draw(widgets_list->drawing_area);
}

// --- Benchmark Structures ---
#define MAX_POINTS 5
static int LIST_BENCH_SIZES[MAX_POINTS];

typedef struct {
  double times[MAX_POINTS];
  GdkRGBA color;
  const char *name;
  int marker_type;
} AlgoBenchmark;

static AlgoBenchmark bench_ll_bulle;
static AlgoBenchmark bench_ll_insert;
static AlgoBenchmark bench_ll_shell;
static AlgoBenchmark bench_ll_quick;
static int has_ll_bench_data = 0;

// --- Chart Drawing Logic ---

static void draw_chart_popup(cairo_t *cr, double w, double h) {
  double margin = 50.0;
  double graph_w = w - 2 * margin;
  double graph_h = h - 2 * margin;

  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_paint(cr);

  cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
  double dashes[] = {4.0};
  cairo_set_dash(cr, dashes, 1, 0);
  cairo_set_line_width(cr, 1.0);

  for (int i = 0; i <= 5; i++) {
    double y = margin + i * (graph_h / 5);
    cairo_move_to(cr, margin, y);
    cairo_line_to(cr, margin + graph_w, y);
  }
  cairo_stroke(cr);
  cairo_set_dash(cr, NULL, 0, 0);

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_set_line_width(cr, 2.0);
  cairo_move_to(cr, margin, margin);
  cairo_line_to(cr, margin, margin + graph_h);
  cairo_line_to(cr, margin + graph_w, margin + graph_h);
  cairo_stroke(cr);

  // Title & Labels
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 14);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, "Performance Listes Chainées", &ext);
  cairo_move_to(cr, w / 2 - ext.width / 2, margin / 2 + 10);
  cairo_show_text(cr, "Performance Listes Chainées");

  cairo_save(cr);
  cairo_translate(cr, 15, h / 2);
  cairo_rotate(cr, -G_PI / 2);
  cairo_set_font_size(cr, 12);
  cairo_show_text(cr, "Temps (ms)");
  cairo_restore(cr);

  // X Axis Labels
  cairo_set_font_size(cr, 10);
  for (int i = 0; i < MAX_POINTS; i++) {
    double x = margin + (i * (graph_w / (MAX_POINTS - 1)));
    char buf[16];
    snprintf(buf, 16, "%d", LIST_BENCH_SIZES[i]);
    cairo_text_extents(cr, buf, &ext);
    cairo_move_to(cr, x - ext.width / 2, margin + graph_h + 15);
    cairo_show_text(cr, buf);
  }

  if (!has_ll_bench_data)
    return;

  double max_time = 0;
  AlgoBenchmark *benches[] = {&bench_ll_bulle, &bench_ll_insert,
                              &bench_ll_shell, &bench_ll_quick};

  for (int b = 0; b < 4; b++)
    for (int i = 0; i < MAX_POINTS; i++)
      if (benches[b]->times[i] > max_time)
        max_time = benches[b]->times[i];

  if (max_time == 0)
    max_time = 1;

  // Y Axis Labels
  for (int i = 0; i <= 5; i++) {
    double val = max_time * (1.0 - i / 5.0);
    char buf[32];
    snprintf(buf, 32, "%.0f", val);
    cairo_text_extents(cr, buf, &ext);
    cairo_move_to(cr, margin - ext.width - 5,
                  margin + i * (graph_h / 5) + ext.height / 2);
    cairo_show_text(cr, buf);
  }

  // Draw Lines
  for (int b = 0; b < 4; b++) {
    gdk_cairo_set_source_rgba(cr, &benches[b]->color);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < MAX_POINTS; i++) {
      double x = margin + (i * (graph_w / (MAX_POINTS - 1)));
      double y =
          (margin + graph_h) - (benches[b]->times[i] / max_time * graph_h);
      if (i == 0)
        cairo_move_to(cr, x, y);
      else
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    // Draw Markers
    for (int i = 0; i < MAX_POINTS; i++) {
      double x = margin + (i * (graph_w / (MAX_POINTS - 1)));
      double y =
          (margin + graph_h) - (benches[b]->times[i] / max_time * graph_h);

      cairo_new_path(cr);
      if (benches[b]->marker_type == 0) // Circle
        cairo_arc(cr, x, y, 3, 0, 2 * G_PI);
      else if (benches[b]->marker_type == 1) // Square
        cairo_rectangle(cr, x - 3, y - 3, 6, 6);
      else if (benches[b]->marker_type == 2) { // Triangle
        cairo_move_to(cr, x, y - 4);
        cairo_line_to(cr, x + 3, y + 3);
        cairo_line_to(cr, x - 3, y + 3);
        cairo_close_path(cr);
      } else { // Diamond
        cairo_move_to(cr, x, y - 4);
        cairo_line_to(cr, x + 3, y);
        cairo_line_to(cr, x, y + 4);
        cairo_line_to(cr, x - 3, y);
        cairo_close_path(cr);
      }
      cairo_fill(cr);
    }
    cairo_stroke(cr);

    // Legend
    double lx = margin + 20;
    double ly = margin + 10 + b * 20;

    cairo_move_to(cr, lx, ly);
    cairo_line_to(cr, lx + 20, ly);
    cairo_stroke(cr);

    cairo_new_path(cr);
    double mx = lx + 10;
    if (benches[b]->marker_type == 0)
      cairo_arc(cr, mx, ly, 3, 0, 2 * G_PI);
    else if (benches[b]->marker_type == 1)
      cairo_rectangle(cr, mx - 3, ly - 3, 6, 6);
    else if (benches[b]->marker_type == 2) {
      cairo_move_to(cr, mx, ly - 4);
      cairo_line_to(cr, mx + 3, ly + 3);
      cairo_line_to(cr, mx - 3, ly + 3);
      cairo_close_path(cr);
    } else {
      cairo_move_to(cr, mx, ly - 4);
      cairo_line_to(cr, mx + 3, ly);
      cairo_line_to(cr, mx, ly + 4);
      cairo_line_to(cr, mx - 3, ly);
      cairo_close_path(cr);
    }
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, lx + 25, ly + 4);
    cairo_show_text(cr, benches[b]->name);
  }

  // X Axis Label
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  cairo_text_extents(cr, "N (Taille de la liste)", &ext);
  cairo_move_to(cr, w / 2 - ext.width / 2, h - 5);
  cairo_show_text(cr, "N (Taille de la liste)");
}

static gboolean on_draw_popup(GtkWidget *widget, cairo_t *cr, gpointer data) {
  guint w = gtk_widget_get_allocated_width(widget);
  guint h = gtk_widget_get_allocated_height(widget);
  draw_chart_popup(cr, w, h);
  return FALSE;
}

static void perform_benchmarks() {
  srand(time(NULL));
  FILE *log = fopen("bench_log.txt", "w");
  if (log)
    fprintf(log, "Starting Benchmark\n");

  // Dynamic Size Calculation
  const char *s_n = gtk_entry_get_text(GTK_ENTRY(widgets_list->entry_rand_n));
  int max_n = atoi(s_n);
  if (max_n <= 0)
    max_n = 1000;

  // Create 5 steps
  for (int i = 0; i < MAX_POINTS; i++) {
    LIST_BENCH_SIZES[i] = (max_n * (i + 1)) / MAX_POINTS;
    if (LIST_BENCH_SIZES[i] == 0)
      LIST_BENCH_SIZES[i] = 1;
  }

  // Generate data and run sorts
  AlgoBenchmark *benches[] = {&bench_ll_bulle, &bench_ll_insert,
                              &bench_ll_shell, &bench_ll_quick};

  // Init colors if not done
  if (bench_ll_bulle.name == NULL) {
    bench_ll_bulle.name = "Bulle";
    bench_ll_bulle.marker_type = 0;
    gdk_rgba_parse(&bench_ll_bulle.color, "#D9534F");

    bench_ll_insert.name = "Insertion";
    bench_ll_insert.marker_type = 1;
    gdk_rgba_parse(&bench_ll_insert.color, "#F0AD4E");

    bench_ll_shell.name = "Shell";
    bench_ll_shell.marker_type = 2;
    gdk_rgba_parse(&bench_ll_shell.color, "#A569BD");

    bench_ll_quick.name = "Rapide";
    bench_ll_quick.marker_type = 3;
    gdk_rgba_parse(&bench_ll_quick.color, "#5CB85C");
  }

  DataType type = widgets_list->list.type;
  CompareFunc cmp = get_cmp_func();

  for (int i = 0; i < MAX_POINTS; i++) {
    int n = LIST_BENCH_SIZES[i];

    int *rand_ints = NULL;
    double *rand_doubles = NULL;

    if (type == TYPE_INT) {
      rand_ints = malloc(n * sizeof(int));
      for (int k = 0; k < n; k++)
        rand_ints[k] = rand() % 10000;
    } else if (type == TYPE_DOUBLE) {
      rand_doubles = malloc(n * sizeof(double));
      for (int k = 0; k < n; k++)
        rand_doubles[k] = (double)rand() / RAND_MAX * 1000.0;
    }

    // Benchmark Loop
    for (int b = 0; b < 4; b++) {
      LinkedList temp_list;
      list_init(&temp_list, type, 0);

      // Build list
      for (int k = 0; k < n; k++) {
        if (type == TYPE_INT)
          list_append(&temp_list, &rand_ints[k]);
        else if (type == TYPE_DOUBLE)
          list_append(&temp_list, &rand_doubles[k]);
        else {
          int v = rand();
          list_append(&temp_list, &v);
        }
      }

      clock_t start = clock();
      list_sort(&temp_list, b, cmp);
      clock_t end = clock();

      benches[b]->times[i] = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
      if (log)
        fprintf(log, "Size: %d, Algo: %d, Time: %.2f ms\n", n, b,
                benches[b]->times[i]);

      list_clear(&temp_list);
    }

    if (rand_ints)
      free(rand_ints);
    if (rand_doubles)
      free(rand_doubles);
  }
  if (log)
    fclose(log);
  has_ll_bench_data = 1;
}

static void on_toggle_view(GtkWidget *b, gpointer d) {
  widgets_list->show_chart = !widgets_list->show_chart;

  if (widgets_list->show_chart) {
    gtk_button_set_label(GTK_BUTTON(widgets_list->btn_toggle_view),
                         "Voir Liste");
    gtk_widget_set_size_request(widgets_list->drawing_area, 800, 600);
  } else {
    gtk_button_set_label(GTK_BUTTON(widgets_list->btn_toggle_view),
                         "Voir Courbe");
    update_stats();
  }
  gtk_widget_queue_draw(widgets_list->drawing_area);
}

static void on_compare_all(GtkWidget *b, gpointer d) {
  perform_benchmarks();
  widgets_list->show_chart = TRUE;
  gtk_button_set_label(GTK_BUTTON(widgets_list->btn_toggle_view), "Voir Liste");
  gtk_widget_set_size_request(widgets_list->drawing_area, 800, 600);
  gtk_widget_queue_draw(widgets_list->drawing_area);
}

// --- Persistence ---

static void on_save_list(GtkWidget *btn, gpointer data) {
  if (!widgets_list->list.head)
    return;

  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Sauvegarder Liste", NULL, GTK_FILE_CHOOSER_ACTION_SAVE, "Annuler",
      GTK_RESPONSE_CANCEL, "Sauvegarder", GTK_RESPONSE_ACCEPT, NULL);
  GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
  gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
  gtk_file_chooser_set_current_name(chooser, "list_data.csv");

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename = gtk_file_chooser_get_filename(chooser);
    FILE *f = fopen(filename, "w");
    if (f) {
      // Header: TYPE, IS_DOUBLY
      fprintf(f, "%d,%d\n", widgets_list->list.type,
              widgets_list->list.is_doubly);

      Node *curr = widgets_list->list.head;
      while (curr) {
        switch (widgets_list->list.type) {
        case TYPE_INT:
          fprintf(f, "%d", *(int *)curr->data);
          break;
        case TYPE_DOUBLE:
          fprintf(f, "%.4f", *(double *)curr->data);
          break;
        case TYPE_CHAR:
          fprintf(f, "%c", *(char *)curr->data);
          break;
        case TYPE_STRING:
          fprintf(f, "%s", (char *)curr->data);
          break;
        }
        if (curr->next)
          fprintf(f, ",");
        curr = curr->next;
      }
      fclose(f);
    }
    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

static void on_load_list(GtkWidget *btn, gpointer data) {
  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Charger Liste", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, "Annuler",
      GTK_RESPONSE_CANCEL, "Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    FILE *f = fopen(filename, "r");
    if (f) {
      int type_int, is_doubly;
      // Read Header
      if (fscanf(f, "%d,%d\n", &type_int, &is_doubly) == 2) {
        list_clear(&widgets_list->list);
        list_init(&widgets_list->list, (DataType)type_int, is_doubly);

        // Update UI to match loaded config
        gtk_combo_box_set_active(GTK_COMBO_BOX(widgets_list->combo_data_type),
                                 type_int);
        if (is_doubly)
          gtk_toggle_button_set_active(
              GTK_TOGGLE_BUTTON(widgets_list->radio_double), TRUE);
        else
          gtk_toggle_button_set_active(
              GTK_TOGGLE_BUTTON(widgets_list->radio_simple), TRUE);

        // Read Content
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        // Skip header line... rough approximation or just read line
        char line[256];
        fgets(line, sizeof(line), f);

        long data_pos = ftell(f);
        long data_len = fsize - data_pos;

        if (data_len > 0) {
          char *buf = malloc(data_len + 1);
          fread(buf, 1, data_len, f);
          buf[data_len] = 0;

          char *dup = strdup(buf);
          char *p = strtok(dup, ",");
          while (p) {
            void *val = NULL;
            int ival;
            double dval;
            char cval;
            switch (widgets_list->list.type) {
            case TYPE_INT:
              ival = atoi(p);
              val = &ival;
              break;
            case TYPE_DOUBLE:
              dval = atof(p);
              val = &dval;
              break;
            case TYPE_CHAR:
              cval = p[0];
              val = &cval;
              break;
            case TYPE_STRING:
              val = p;
              break;
            }
            list_append(&widgets_list->list, val);
            p = strtok(NULL, ",");
          }
          free(dup);
          free(buf);
        }
      }
      fclose(f);
      gtk_widget_queue_draw(widgets_list->drawing_area);
      update_stats();
    }
    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

// --- Initialization ---

GtkWidget *create_tab_list(void) {
  // ...
  // (Find btn_cmp creation and connect it)

  widgets_list = malloc(sizeof(TabListWidgets));
  list_init(&widgets_list->list, TYPE_INT, 0);

  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned), 350);

  // Sidebar
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_size_request(sidebar, 330, -1);
  gtk_widget_set_margin_start(sidebar, 10);
  gtk_widget_set_margin_top(sidebar, 10);

  // --- 1. CONFIGURATION ---
  GtkWidget *fr_conf = create_card_frame("1. Configuration");
  GtkWidget *box_conf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(fr_conf), box_conf);

  // Type
  gtk_box_pack_start(GTK_BOX(box_conf), gtk_label_new("Type :"), FALSE, FALSE,
                     0);
  widgets_list->combo_data_type = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(
      GTK_COMBO_BOX_TEXT(widgets_list->combo_data_type), "Entiers");
  gtk_combo_box_text_append_text(
      GTK_COMBO_BOX_TEXT(widgets_list->combo_data_type), "Réels");
  gtk_combo_box_text_append_text(
      GTK_COMBO_BOX_TEXT(widgets_list->combo_data_type), "Caractères");
  gtk_combo_box_text_append_text(
      GTK_COMBO_BOX_TEXT(widgets_list->combo_data_type), "Chaînes");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widgets_list->combo_data_type), 0);
  g_signal_connect(widgets_list->combo_data_type, "changed",
                   G_CALLBACK(on_config_change), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), widgets_list->combo_data_type, FALSE,
                     FALSE, 0);

  // Structure
  gtk_box_pack_start(GTK_BOX(box_conf), gtk_label_new("Structure :"), FALSE,
                     FALSE, 0);
  widgets_list->radio_simple =
      gtk_radio_button_new_with_label(NULL, "Simple Chaînage");
  widgets_list->radio_double = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(widgets_list->radio_simple), "Double Chaînage");
  g_signal_connect(widgets_list->radio_simple, "toggled",
                   G_CALLBACK(on_config_change), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), widgets_list->radio_simple, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_conf), widgets_list->radio_double, FALSE,
                     FALSE, 0);

  // Persistence
  gtk_box_pack_start(GTK_BOX(box_conf), gtk_label_new("Persistance :"), FALSE,
                     FALSE, 0);
  GtkWidget *box_pers = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *btn_save = gtk_button_new_with_label("Sauvegarder");
  GtkWidget *btn_load = gtk_button_new_with_label("Charger");

  g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_list), NULL);
  g_signal_connect(btn_load, "clicked", G_CALLBACK(on_load_list), NULL);

  gtk_box_pack_start(GTK_BOX(box_pers), btn_save, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_pers), btn_load, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_conf), box_pers, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(sidebar), fr_conf, FALSE, FALSE, 0);

  // --- 2. SAISIE ---
  GtkWidget *fr_saisie = create_card_frame("2. Saisie");
  GtkWidget *box_saisie = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(fr_saisie), box_saisie);

  widgets_list->radio_manual = gtk_radio_button_new_with_label(NULL, "Manuel");
  widgets_list->radio_auto = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(widgets_list->radio_manual), "Aléatoire");
  // Default Auto
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets_list->radio_auto),
                               TRUE);
  g_signal_connect(widgets_list->radio_manual, "toggled",
                   G_CALLBACK(on_mode_toggled), NULL);

  gtk_box_pack_start(GTK_BOX(box_saisie), widgets_list->radio_manual, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_saisie), widgets_list->radio_auto, FALSE,
                     FALSE, 0);

  gtk_box_pack_start(GTK_BOX(sidebar), fr_saisie, FALSE, FALSE, 0);

  // --- 3. OPERATIONS ---
  GtkWidget *fr_ops = create_card_frame("3. Opérations");
  GtkWidget *box_ops = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(fr_ops), box_ops);

  // AUTO OPS
  widgets_list->box_auto_ops = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  widgets_list->entry_rand_n = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(widgets_list->entry_rand_n), "10");
  GtkWidget *btn_gen = gtk_button_new_with_label("Générer");
  style_button_color(btn_gen, "#28A745"); // Green
  g_signal_connect(btn_gen, "clicked", G_CALLBACK(on_gen_rand), NULL);

  gtk_box_pack_start(GTK_BOX(widgets_list->box_auto_ops),
                     gtk_label_new("Taille :"), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(widgets_list->box_auto_ops),
                     widgets_list->entry_rand_n, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(widgets_list->box_auto_ops), btn_gen, FALSE, FALSE,
                     5);
  gtk_box_pack_start(GTK_BOX(box_ops), widgets_list->box_auto_ops, FALSE, FALSE,
                     0);

  // MANUAL OPS (Hidden by default)
  widgets_list->box_manual_ops = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  widgets_list->entry_val = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_list->entry_val),
                                 "Valeur...");
  GtkWidget *box_m_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *b_head = gtk_button_new_with_label("Tête");
  GtkWidget *b_tail = gtk_button_new_with_label("Queue");
  g_signal_connect(b_head, "clicked", G_CALLBACK(on_add_head), NULL);
  g_signal_connect(b_tail, "clicked", G_CALLBACK(on_add_tail), NULL);
  gtk_box_pack_start(GTK_BOX(box_m_btns), b_head, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_m_btns), b_tail, TRUE, TRUE, 0);

  widgets_list->entry_pos = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_list->entry_pos),
                                 "Index...");
  GtkWidget *b_ins = gtk_button_new_with_label("Insérer Pos");
  g_signal_connect(b_ins, "clicked", G_CALLBACK(on_insert_at), NULL);

  gtk_box_pack_start(GTK_BOX(widgets_list->box_manual_ops),
                     widgets_list->entry_val, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(widgets_list->box_manual_ops), box_m_btns, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(widgets_list->box_manual_ops),
                     widgets_list->entry_pos, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(widgets_list->box_manual_ops), b_ins, FALSE, FALSE,
                     0);
  gtk_box_pack_start(GTK_BOX(box_ops), widgets_list->box_manual_ops, FALSE,
                     FALSE, 0);

  // Common Ops
  gtk_box_pack_start(GTK_BOX(box_ops),
                     gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
                     FALSE, 5);
  gtk_box_pack_start(GTK_BOX(box_ops), gtk_label_new("Suppression :"), FALSE,
                     FALSE, 0);
  widgets_list->entry_del = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_list->entry_del),
                                 "Valeur à supprimer...");
  GtkWidget *btn_del = gtk_button_new_with_label("Supprimer");
  style_button_color(btn_del, "#DC3545"); // Red
  g_signal_connect(btn_del, "clicked", G_CALLBACK(on_delete_val), NULL);
  gtk_box_pack_start(GTK_BOX(box_ops), widgets_list->entry_del, FALSE, FALSE,
                     0);
  gtk_box_pack_start(GTK_BOX(box_ops), btn_del, FALSE, FALSE, 5);

  gtk_box_pack_start(GTK_BOX(box_ops), gtk_label_new("Méthode de Tri :"), FALSE,
                     FALSE, 0);
  GtkWidget *grid_sort = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid_sort), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid_sort), 5);
  GtkWidget *bs1 = gtk_button_new_with_label("Bulle");
  GtkWidget *bs2 = gtk_button_new_with_label("Insertion");
  GtkWidget *bs3 = gtk_button_new_with_label("Shell");
  GtkWidget *bs4 = gtk_button_new_with_label("Rapide");
  g_signal_connect(bs1, "clicked", G_CALLBACK(on_sort_bubble), NULL);
  g_signal_connect(bs2, "clicked", G_CALLBACK(on_sort_insert), NULL);
  g_signal_connect(bs3, "clicked", G_CALLBACK(on_sort_shell), NULL);
  g_signal_connect(bs4, "clicked", G_CALLBACK(on_sort_quick), NULL);
  gtk_grid_attach(GTK_GRID(grid_sort), bs1, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid_sort), bs2, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid_sort), bs3, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid_sort), bs4, 1, 1, 1, 1);
  gtk_box_pack_start(GTK_BOX(box_ops), grid_sort, FALSE, FALSE, 5);

  GtkWidget *btn_cmp = gtk_button_new_with_label("Comparer Tout");
  style_button_color(btn_cmp, "#28A745");
  g_signal_connect(btn_cmp, "clicked", G_CALLBACK(on_compare_all), NULL);
  gtk_box_pack_start(GTK_BOX(box_ops), btn_cmp, FALSE, FALSE, 0);

  widgets_list->show_chart = FALSE; // Default List View

  widgets_list->btn_toggle_view = gtk_button_new_with_label("Voir Courbe");
  style_button_color(widgets_list->btn_toggle_view, "#007BFF"); // Blue
  g_signal_connect(widgets_list->btn_toggle_view, "clicked",
                   G_CALLBACK(on_toggle_view), NULL);
  gtk_box_pack_start(GTK_BOX(box_ops), widgets_list->btn_toggle_view, FALSE,
                     FALSE, 5);

  GtkWidget *btn_reset = gtk_button_new_with_label("Tout Recommencer");
  style_button_color(btn_reset, "#DC3545");
  g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_clear), NULL);
  gtk_box_pack_start(GTK_BOX(box_ops), btn_reset, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(sidebar), fr_ops, FALSE, FALSE, 0);

  widgets_list->label_stats = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(sidebar), widgets_list->label_stats, FALSE, FALSE,
                     5);

  // Sidebar Scroll
  GtkWidget *sidebar_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(sidebar_scroll), sidebar);

  gtk_paned_add1(GTK_PANED(paned), sidebar_scroll);

  // Canvas
  widgets_list->drawing_area = gtk_drawing_area_new();
  g_signal_connect(widgets_list->drawing_area, "draw", G_CALLBACK(on_draw_list),
                   NULL);
  gtk_widget_set_size_request(widgets_list->drawing_area, 800, 600);
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(scroll), widgets_list->drawing_area);
  gtk_paned_add2(GTK_PANED(paned), scroll);

  // Trigger Initial State
  on_mode_toggled(NULL, NULL);
  update_stats();

  return paned;
}
