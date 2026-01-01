#include "backend.h"
#include "gui.h"
#include <ctype.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- Data Structures ---

#define MAX_POINTS 5
static int BENCH_SIZES[MAX_POINTS];

// ... (typedefs)

// typedef enum { TYPE_INT, TYPE_DOUBLE, TYPE_CHAR, TYPE_STRING } DataType; //
// Now in backend.h

typedef struct {
  double times[MAX_POINTS];
  GdkRGBA color;
  const char *name;
  int marker_type; // 0=Circle, 1=Square, 2=Triangle, 3=Diamond
} AlgoBenchmark;

typedef struct {
  // Data Storage
  void *array;
  size_t size;
  DataType current_type;

  // UI Elements
  GtkWidget *entry_size;
  GtkWidget *combo_type;
  GtkWidget *radio_auto;
  GtkWidget *radio_manual;

  // New Manual Input UI
  GtkWidget *entry_manual_val; // For single value input
  GtkWidget *box_manual_input;

  // Text Views for Data
  GtkWidget *text_raw;
  GtkWidget *text_sorted;

  // Chart
  GtkWidget *drawing_area;

  // Benchmark Data
  AlgoBenchmark bench_bulle;
  AlgoBenchmark bench_insertion;
  AlgoBenchmark bench_shell;
  AlgoBenchmark bench_quick;
  int has_bench_data;

} TabSortWidgets;

static TabSortWidgets *widgets_sort;

// --- Generic Sorting Utils (UNCHANGED) ---

typedef int (*CompareFunc)(const void *, const void *);

int cmp_int(const void *a, const void *b) { return (*(int *)a - *(int *)b); }
int cmp_double(const void *a, const void *b) {
  return (*(double *)a > *(double *)b) - (*(double *)a < *(double *)b);
}
int cmp_char(const void *a, const void *b) { return (*(char *)a - *(char *)b); }
int cmp_str(const void *a, const void *b) {
  return strcmp(*(char **)a, *(char **)b);
}

static void swap_gen(void *a, void *b, size_t size) {
  char temp[size];
  memcpy(temp, a, size);
  memcpy(a, b, size);
  memcpy(b, temp, size);
}

// 1. Bubble Sort
void sort_bubble_gen(void *base, size_t n, size_t size, CompareFunc cmp,
                     SortStats *stats) {
  if (n < 2)
    return;
  char *arr = (char *)base;
  for (size_t i = 0; i < n - 1; i++) {
    for (size_t j = 0; j < n - i - 1; j++) {
      stats->comparisons++;
      if (cmp(arr + j * size, arr + (j + 1) * size) > 0) {
        swap_gen(arr + j * size, arr + (j + 1) * size, size);
        stats->assignments += 3;
      }
    }
  }
}

// 2. Insertion Sort
void sort_insertion_gen(void *base, size_t n, size_t size, CompareFunc cmp,
                        SortStats *stats) {
  char *arr = (char *)base;
  char key[size];
  for (size_t i = 1; i < n; i++) {
    memcpy(key, arr + i * size, size);
    long j = i - 1;
    while (j >= 0) {
      stats->comparisons++;
      if (cmp(arr + j * size, key) > 0) {
        memcpy(arr + (j + 1) * size, arr + j * size, size);
        stats->assignments++;
        j--;
      } else {
        break;
      }
    }
    memcpy(arr + (j + 1) * size, key, size);
    stats->assignments++;
  }
}

// 3. Shell Sort
void sort_shell_gen(void *base, size_t n, size_t size, CompareFunc cmp,
                    SortStats *stats) {
  char *arr = (char *)base;
  for (size_t gap = n / 2; gap > 0; gap /= 2) {
    for (size_t i = gap; i < n; i++) {
      char temp[size];
      memcpy(temp, arr + i * size, size);
      size_t j;
      for (j = i; j >= gap; j -= gap) {
        stats->comparisons++;
        if (cmp(arr + (j - gap) * size, temp) > 0) {
          memcpy(arr + j * size, arr + (j - gap) * size, size);
          stats->assignments++;
        } else
          break;
      }
      memcpy(arr + j * size, temp, size);
    }
  }
}

// 4. Quick Sort
void qs_rec(char *arr, long low, long high, size_t size, CompareFunc cmp,
            SortStats *stats) {
  if (low < high) {
    char pivot[size];
    memcpy(pivot, arr + high * size, size);
    long i = low - 1;
    for (long j = low; j < high; j++) {
      stats->comparisons++;
      if (cmp(arr + j * size, pivot) < 0) {
        i++;
        swap_gen(arr + i * size, arr + j * size, size);
        stats->assignments += 3;
      }
    }
    swap_gen(arr + (i + 1) * size, arr + high * size, size);
    stats->assignments += 3;

    long pi = i + 1;
    qs_rec(arr, low, pi - 1, size, cmp, stats);
    qs_rec(arr, pi + 1, high, size, cmp, stats);
  }
}
void sort_quick_gen(void *base, size_t n, size_t size, CompareFunc cmp,
                    SortStats *stats) {
  if (n < 2)
    return;
  qs_rec((char *)base, 0, n - 1, size, cmp, stats);
}

// --- Helpers ---

static void style_button(GtkWidget *btn, const char *color_str) {
  GdkRGBA color;
  gdk_rgba_parse(&color, color_str);
  gtk_widget_override_background_color(btn, GTK_STATE_FLAG_NORMAL, &color);
}

static GtkWidget *create_card(const char *title) {
  GtkWidget *frame = gtk_frame_new(title);
  GtkWidget *label = gtk_frame_get_label_widget(GTK_FRAME(frame));
  if (label) {
    char *markup = g_strdup_printf("<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
  }
  return frame;
}

static size_t get_element_size(DataType t) {
  switch (t) {
  case TYPE_INT:
    return sizeof(int);
  case TYPE_DOUBLE:
    return sizeof(double);
  case TYPE_CHAR:
    return sizeof(char);
  case TYPE_STRING:
    return sizeof(char *);
  }
  return 0;
}

static CompareFunc get_comparator(DataType t) {
  switch (t) {
  case TYPE_INT:
    return cmp_int;
  case TYPE_DOUBLE:
    return cmp_double;
  case TYPE_CHAR:
    return cmp_char;
  case TYPE_STRING:
    return cmp_str;
  }
  return NULL;
}

static void free_data() {
  if (!widgets_sort->array)
    return;
  if (widgets_sort->current_type == TYPE_STRING) {
    char **strs = (char **)widgets_sort->array;
    for (size_t i = 0; i < widgets_sort->size; i++)
      if (strs[i])
        free(strs[i]);
  }
  free(widgets_sort->array);
  widgets_sort->array = NULL;
  widgets_sort->size = 0;
}

static void update_text_view(GtkWidget *view, void *arr, size_t size,
                             DataType t) {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
  if (!arr || size == 0) {
    gtk_text_buffer_set_text(buffer, "", -1);
    return;
  }

  size_t limit = size > 500 ? 500 : size;
  GString *gs = g_string_new("");
  char temp[64];
  for (size_t i = 0; i < limit; i++) {
    switch (t) {
    case TYPE_INT:
      snprintf(temp, 64, "%d", ((int *)arr)[i]);
      break;
    case TYPE_DOUBLE:
      snprintf(temp, 64, "%.2f", ((double *)arr)[i]);
      break;
    case TYPE_CHAR:
      snprintf(temp, 64, "'%c'", ((char *)arr)[i]);
      break;
    case TYPE_STRING:
      snprintf(temp, 64, "\"%s\"", ((char **)arr)[i]);
      break;
    }
    g_string_append(gs, temp);
    if (i < limit - 1)
      g_string_append(gs, ", ");
  }
  if (size > limit)
    g_string_append(gs, "...");
  gtk_text_buffer_set_text(buffer, gs->str, -1);
  g_string_free(gs, TRUE);
}

// --- NEW Manual Input Logic (Single Add) ---

static void on_add_manual_value(GtkWidget *btn, gpointer data) {
  const char *text =
      gtk_entry_get_text(GTK_ENTRY(widgets_sort->entry_manual_val));
  if (!text || strlen(text) == 0)
    return;

  // Resize array
  size_t new_size = widgets_sort->size + 1;
  size_t el_size = get_element_size(widgets_sort->current_type);
  void *new_arr = realloc(widgets_sort->array, new_size * el_size);
  if (!new_arr)
    return; // Alloc fail

  widgets_sort->array = new_arr;

  // Parse and append
  size_t i = widgets_sort->size;
  switch (widgets_sort->current_type) {
  case TYPE_INT:
    ((int *)widgets_sort->array)[i] = atoi(text);
    break;
  case TYPE_DOUBLE:
    ((double *)widgets_sort->array)[i] = atof(text);
    break;
  case TYPE_CHAR:
    ((char *)widgets_sort->array)[i] = text[0];
    break;
  case TYPE_STRING:
    ((char **)widgets_sort->array)[i] = strdup(text);
    break;
  }

  widgets_sort->size = new_size;

  // Clear input
  gtk_entry_set_text(GTK_ENTRY(widgets_sort->entry_manual_val), "");

  // Update View
  update_text_view(widgets_sort->text_raw, widgets_sort->array,
                   widgets_sort->size, widgets_sort->current_type);
  // Clear sorted view since data changed
  update_text_view(widgets_sort->text_sorted, NULL, 0, 0);

  // Reset benchmark data as content changed
  widgets_sort->has_bench_data = 0;
  gtk_widget_queue_draw(widgets_sort->drawing_area);
}

// --- Logic Actions ---

static void on_mode_toggled(GtkToggleButton *btn, gpointer data) {
  gboolean manual = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(widgets_sort->radio_manual));

  if (manual)
    gtk_widget_show(widgets_sort->box_manual_input);
  else
    gtk_widget_hide(widgets_sort->box_manual_input);

  gtk_widget_set_sensitive(widgets_sort->entry_size, !manual);
}

static void on_generate(GtkWidget *btn, gpointer data) {
  // If manual, generate button might just refresh or validate, but we rely on
  // Add button now. Actually, standard behavior:
  if (gtk_toggle_button_get_active(
          GTK_TOGGLE_BUTTON(widgets_sort->radio_manual))) {
    // In manual mode, maybe clear or just acknowledge?
    // User said "User inserts one value at a time".
    // So "Generate" button is primarily for Auto mode.
    // But let's verify display just in case.
    update_text_view(widgets_sort->text_raw, widgets_sort->array,
                     widgets_sort->size, widgets_sort->current_type);
    return;
  }

  // AUTO MODE Logic
  const char *size_str =
      gtk_entry_get_text(GTK_ENTRY(widgets_sort->entry_size));
  int size = atoi(size_str);
  if (size <= 0)
    size = 100;
  else if (size > 10000)
    size = 10000;

  free_data();
  widgets_sort->size = size;
  widgets_sort->array =
      malloc(size * get_element_size(widgets_sort->current_type));

  for (int i = 0; i < size; i++) {
    switch (widgets_sort->current_type) {
    case TYPE_INT:
      ((int *)widgets_sort->array)[i] = rand() % 1000;
      break;
    case TYPE_DOUBLE:
      ((double *)widgets_sort->array)[i] = (rand() % 1000) / 10.0;
      break;
    case TYPE_CHAR:
      ((char *)widgets_sort->array)[i] = 'A' + (rand() % 26);
      break;
    case TYPE_STRING: {
      char buf[6];
      for (int k = 0; k < 5; k++)
        buf[k] = 'a' + rand() % 26;
      buf[5] = 0;
      ((char **)widgets_sort->array)[i] = strdup(buf);
    } break;
    }
  }

  update_text_view(widgets_sort->text_raw, widgets_sort->array,
                   widgets_sort->size, widgets_sort->current_type);
  update_text_view(widgets_sort->text_sorted, NULL, 0, 0);
  widgets_sort->has_bench_data = 0;
  gtk_widget_queue_draw(widgets_sort->drawing_area);
}

static void on_type_changed(GtkComboBox *combo, gpointer data) {
  DataType new_type = gtk_combo_box_get_active(combo);
  if (new_type != widgets_sort->current_type) {
    widgets_sort->current_type = new_type;
    free_data(); // Clear data on type change to avoid corruption
    update_text_view(widgets_sort->text_raw, NULL, 0, 0);
    update_text_view(widgets_sort->text_sorted, NULL, 0, 0);
  }
}

static void copy_data(void *dest, void *src, size_t size, DataType t) {
  size_t es = get_element_size(t);
  if (t == TYPE_STRING) {
    char **s_src = (char **)src;
    char **s_dest = (char **)dest;
    for (size_t i = 0; i < size; i++)
      s_dest[i] = strdup(s_src[i]);
  } else {
    memcpy(dest, src, size * es);
  }
}

static void run_algo(int id) {
  if (!widgets_sort->array || widgets_sort->size == 0)
    return;

  size_t el_size = get_element_size(widgets_sort->current_type);
  void *copy = malloc(widgets_sort->size * el_size);
  copy_data(copy, widgets_sort->array, widgets_sort->size,
            widgets_sort->current_type);

  SortStats stats = {0, 0};
  CompareFunc cmp = get_comparator(widgets_sort->current_type);

  switch (id) {
  case 0:
    sort_bubble_gen(copy, widgets_sort->size, el_size, cmp, &stats);
    break;
  case 1:
    sort_insertion_gen(copy, widgets_sort->size, el_size, cmp, &stats);
    break;
  case 2:
    sort_shell_gen(copy, widgets_sort->size, el_size, cmp, &stats);
    break;
  case 3:
    sort_quick_gen(copy, widgets_sort->size, el_size, cmp, &stats);
    break;
  }

  update_text_view(widgets_sort->text_sorted, copy, widgets_sort->size,
                   widgets_sort->current_type);

  if (widgets_sort->current_type == TYPE_STRING) {
    for (size_t i = 0; i < widgets_sort->size; i++)
      free(((char **)copy)[i]);
  }
  free(copy);
}

static void on_sort_bubble(GtkWidget *btn, gpointer data) { run_algo(0); }
static void on_sort_insert(GtkWidget *btn, gpointer data) { run_algo(1); }
static void on_sort_shell(GtkWidget *btn, gpointer data) { run_algo(2); }
static void on_sort_quick(GtkWidget *btn, gpointer data) { run_algo(3); }

static void on_reset(GtkWidget *btn, gpointer data) {
  free_data();
  update_text_view(widgets_sort->text_raw, NULL, 0, 0);
  update_text_view(widgets_sort->text_sorted, NULL, 0, 0);
  gtk_entry_set_text(GTK_ENTRY(widgets_sort->entry_manual_val), "");
  widgets_sort->has_bench_data = 0;
  gtk_widget_queue_draw(widgets_sort->drawing_area);
}

// --- Persistence ---

static void on_save(GtkWidget *btn, gpointer data) {
  if (!widgets_sort->array || widgets_sort->size == 0)
    return;

  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Sauvegarder", NULL, GTK_FILE_CHOOSER_ACTION_SAVE, "Annuler",
      GTK_RESPONSE_CANCEL, "Sauvegarder", GTK_RESPONSE_ACCEPT, NULL);
  GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
  gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
  gtk_file_chooser_set_current_name(chooser, "data.csv");

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename = gtk_file_chooser_get_filename(chooser);
    FILE *f = fopen(filename, "w");
    if (f) {
      // Write Type Header
      fprintf(f, "%d\n", widgets_sort->current_type);
      // Write Elements
      for (size_t i = 0; i < widgets_sort->size; i++) {
        switch (widgets_sort->current_type) {
        case TYPE_INT:
          fprintf(f, "%d", ((int *)widgets_sort->array)[i]);
          break;
        case TYPE_DOUBLE:
          fprintf(f, "%.4f", ((double *)widgets_sort->array)[i]);
          break;
        case TYPE_CHAR:
          fprintf(f, "%c", ((char *)widgets_sort->array)[i]);
          break;
        case TYPE_STRING:
          fprintf(f, "%s", ((char **)widgets_sort->array)[i]);
          break;
        }
        if (i < widgets_sort->size - 1)
          fprintf(f, ",");
      }
      fclose(f);
    }
    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

static void on_load(GtkWidget *btn, gpointer data) {
  GtkWidget *dialog = gtk_file_chooser_dialog_new(
      "Charger", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, "Annuler",
      GTK_RESPONSE_CANCEL, "Ouvrir", GTK_RESPONSE_ACCEPT, NULL);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    FILE *f = fopen(filename, "r");
    if (f) {
      int type_int;
      if (fscanf(f, "%d\n", &type_int) == 1) {
        // Read rest of file
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char line[256];
        fgets(line, sizeof(line), f); // Skip 1st line

        long data_pos = ftell(f);
        long data_len = fsize - data_pos;

        if (data_len > 0) {
          char *buf = malloc(data_len + 1);
          fread(buf, 1, data_len, f);
          buf[data_len] = 0;

          // Free existing
          free_data();

          // Re-Implement parse from csv buffer logic or similar
          // For simplicity, reusing basic token parsing for load
          size_t count = 0;
          char *dup = strdup(buf);
          char *p = strtok(dup, " ,\n\r");
          while (p) {
            count++;
            p = strtok(NULL, " ,\n\r");
          }
          free(dup);

          widgets_sort->current_type = (DataType)type_int;
          widgets_sort->size = count;
          widgets_sort->array =
              malloc(count * get_element_size(widgets_sort->current_type));

          dup = strdup(buf);
          p = strtok(dup, " ,\n\r");
          size_t i = 0;
          while (p && i < count) {
            switch (widgets_sort->current_type) {
            case TYPE_INT:
              ((int *)widgets_sort->array)[i] = atoi(p);
              break;
            case TYPE_DOUBLE:
              ((double *)widgets_sort->array)[i] = atof(p);
              break;
            case TYPE_CHAR:
              ((char *)widgets_sort->array)[i] = p[0];
              break;
            case TYPE_STRING:
              ((char **)widgets_sort->array)[i] = strdup(p);
              break;
            }
            i++;
            p = strtok(NULL, " ,\n\r");
          }
          free(dup);
          free(buf);

          // Update UI
          gtk_combo_box_set_active(GTK_COMBO_BOX(widgets_sort->combo_type),
                                   type_int);
          // Don't necessarily switch to manual, but update data
          update_text_view(widgets_sort->text_raw, widgets_sort->array,
                           widgets_sort->size, widgets_sort->current_type);
          update_text_view(widgets_sort->text_sorted, NULL, 0, 0);
        }
      }
      fclose(f);
    }
    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

static void on_compare_all(GtkWidget *btn, gpointer data) {
  // Dynamic Size Calculation
  const char *s_n = gtk_entry_get_text(GTK_ENTRY(widgets_sort->entry_size));
  int max_n = atoi(s_n);
  if (max_n <= 0)
    max_n = 1000;

  // Create 5 steps
  for (int i = 0; i < MAX_POINTS; i++) {
    BENCH_SIZES[i] = (max_n * (i + 1)) / MAX_POINTS;
    if (BENCH_SIZES[i] == 0)
      BENCH_SIZES[i] = 1;
  }

  CompareFunc cmp = get_comparator(widgets_sort->current_type);
  size_t es = get_element_size(widgets_sort->current_type);

  AlgoBenchmark *benches[] = {
      &widgets_sort->bench_bulle, &widgets_sort->bench_insertion,
      &widgets_sort->bench_shell, &widgets_sort->bench_quick};

  for (int b = 0; b < 4; b++) {
    for (int i = 0; i < MAX_POINTS; i++) {
      // Benchmark requires Random data, not manual
      int n = BENCH_SIZES[i];
      void *arr = malloc(n * es);

      for (int k = 0; k < n; k++) {
        switch (widgets_sort->current_type) {
        case TYPE_INT:
          ((int *)arr)[k] = rand() % 10000;
          break;
        case TYPE_DOUBLE:
          ((double *)arr)[k] = (double)rand() / RAND_MAX * 1000.0;
          break;
        case TYPE_CHAR:
          ((char *)arr)[k] = 'A' + rand() % 26;
          break;
        case TYPE_STRING: {
          char buf[5];
          for (int j = 0; j < 4; j++)
            buf[j] = 'a' + rand() % 26;
          buf[4] = 0;
          ((char **)arr)[k] = strdup(buf);
        } break;
        }
      }

      SortStats stats = {0, 0};
      clock_t start = clock();
      switch (b) {
      case 0:
        sort_bubble_gen(arr, n, es, cmp, &stats);
        break;
      case 1:
        sort_insertion_gen(arr, n, es, cmp, &stats);
        break;
      case 2:
        sort_shell_gen(arr, n, es, cmp, &stats);
        break;
      case 3:
        sort_quick_gen(arr, n, es, cmp, &stats);
        break;
      }
      clock_t end = clock();
      benches[b]->times[i] = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

      if (widgets_sort->current_type == TYPE_STRING) {
        for (int k = 0; k < n; k++)
          free(((char **)arr)[k]);
      }
      free(arr);
    }
  }
  widgets_sort->has_bench_data = 1;
  gtk_widget_queue_draw(widgets_sort->drawing_area);
}

// --- Chart Drawing (Same as before) ---

static void draw_chart(cairo_t *cr, double w, double h) {
  double margin = 50.0;
  double graph_w = w - 2 * margin;
  double graph_h = h - 2 * margin;

  cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
  double dashes[] = {4.0};
  cairo_set_dash(cr, dashes, 1, 0);
  cairo_set_line_width(cr, 1.0);

  for (int i = 0; i <= 5; i++) {
    double y = margin + i * (graph_h / 5);
    cairo_move_to(cr, margin, y);
    cairo_line_to(cr, margin + graph_w, y);
  }
  for (int i = 0; i < MAX_POINTS; i++) {
    double x = margin + i * (graph_w / (MAX_POINTS - 1));
    cairo_move_to(cr, x, margin);
    cairo_line_to(cr, x, margin + graph_h);
  }
  cairo_stroke(cr);
  cairo_set_dash(cr, NULL, 0, 0);

  cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
  cairo_set_line_width(cr, 2.0);
  cairo_move_to(cr, margin, margin);
  cairo_line_to(cr, margin, margin + graph_h);
  cairo_move_to(cr, margin, margin + graph_h);
  cairo_line_to(cr, margin + graph_w, margin + graph_h);
  cairo_stroke(cr);

  cairo_save(cr);
  cairo_translate(cr, 15, h / 2);
  cairo_rotate(cr, -G_PI / 2);
  cairo_set_font_size(cr, 12);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_show_text(cr, "Temps (ms)");
  cairo_restore(cr);

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 14);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, "Performance Comparée", &ext);
  cairo_move_to(cr, w / 2 - ext.width / 2, margin / 2 + 5);
  cairo_show_text(cr, "Performance Comparée");

  if (!widgets_sort->has_bench_data)
    return;

  double max_time = 0;
  AlgoBenchmark *benches[] = {
      &widgets_sort->bench_bulle, &widgets_sort->bench_insertion,
      &widgets_sort->bench_shell, &widgets_sort->bench_quick};
  for (int b = 0; b < 4; b++)
    for (int i = 0; i < MAX_POINTS; i++)
      if (benches[b]->times[i] > max_time)
        max_time = benches[b]->times[i];
  if (max_time == 0)
    max_time = 1;

  cairo_set_font_size(cr, 10);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  for (int i = 0; i <= 5; i++) {
    double val = max_time * (1.0 - i / 5.0);
    char buf[32];
    snprintf(buf, 32, "%.0f", val);
    cairo_text_extents(cr, buf, &ext);
    cairo_move_to(cr, margin - ext.width - 5,
                  margin + i * (graph_h / 5) + ext.height / 2);
    cairo_show_text(cr, buf);
  }

  double legend_x = margin + 20;
  double legend_y = margin + 10;
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_rectangle(cr, legend_x - 5, legend_y - 5, 100, 4 * 20 + 10);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);

  for (int b = 0; b < 4; b++) {
    gdk_cairo_set_source_rgba(cr, &benches[b]->color);
    cairo_set_line_width(cr, 2.5);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
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

    for (int i = 0; i < MAX_POINTS; i++) {
      double x = margin + (i * (graph_w / (MAX_POINTS - 1)));
      double y =
          (margin + graph_h) - (benches[b]->times[i] / max_time * graph_h);
      cairo_new_path(cr);
      if (benches[b]->marker_type == 0)
        cairo_arc(cr, x, y, 3, 0, 2 * G_PI);
      else if (benches[b]->marker_type == 1)
        cairo_rectangle(cr, x - 3, y - 3, 6, 6);
      else if (benches[b]->marker_type == 2) {
        cairo_move_to(cr, x, y - 4);
        cairo_line_to(cr, x + 3, y + 3);
        cairo_line_to(cr, x - 3, y + 3);
        cairo_close_path(cr);
      } else {
        cairo_move_to(cr, x, y - 4);
        cairo_line_to(cr, x + 3, y);
        cairo_line_to(cr, x, y + 4);
        cairo_line_to(cr, x - 3, y);
        cairo_close_path(cr);
      }
      cairo_fill(cr);
    }
    double ly = legend_y + b * 20 + 10;
    cairo_move_to(cr, legend_x, ly);
    cairo_line_to(cr, legend_x + 20, ly);
    cairo_stroke(cr);

    cairo_new_path(cr);
    double mx = legend_x + 10;
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
    cairo_move_to(cr, legend_x + 25, ly + 4);
    cairo_show_text(cr, benches[b]->name);
  }

  cairo_set_source_rgb(cr, 0, 0, 0);
  for (int i = 0; i < MAX_POINTS; i++) {
    double x = margin + (i * (graph_w / (MAX_POINTS - 1)));
    char buf[16];
    snprintf(buf, 16, "%d", BENCH_SIZES[i]);
    cairo_text_extents(cr, buf, &ext);
    cairo_move_to(cr, x - ext.width / 2, margin + graph_h + 15);
    cairo_show_text(cr, buf);
  }
}

static gboolean on_draw_area(GtkWidget *widget, cairo_t *cr, gpointer data) {
  guint w = gtk_widget_get_allocated_width(widget);
  guint h = gtk_widget_get_allocated_height(widget);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_paint(cr);
  draw_chart(cr, w, h);
  return FALSE;
}

// --- UI ---

GtkWidget *create_tab_sort(void) {
  widgets_sort = malloc(sizeof(TabSortWidgets));
  widgets_sort->array = NULL;
  widgets_sort->size = 0;
  widgets_sort->current_type = TYPE_INT;

  gdk_rgba_parse(&widgets_sort->bench_bulle.color, "#D9534F");
  widgets_sort->bench_bulle.name = "Bulle";
  widgets_sort->bench_bulle.marker_type = 0;
  gdk_rgba_parse(&widgets_sort->bench_insertion.color, "#F0AD4E");
  widgets_sort->bench_insertion.name = "Insertion";
  widgets_sort->bench_insertion.marker_type = 1;
  gdk_rgba_parse(&widgets_sort->bench_shell.color, "#A569BD");
  widgets_sort->bench_shell.name = "Shell";
  widgets_sort->bench_shell.marker_type = 2;
  gdk_rgba_parse(&widgets_sort->bench_quick.color, "#5CB85C");
  widgets_sort->bench_quick.name = "Rapide";
  widgets_sort->bench_quick.marker_type = 3;

  GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_container_set_border_width(GTK_CONTAINER(main_paned), 10);
  gtk_paned_set_position(GTK_PANED(main_paned), 380);

  // Sidebar
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_size_request(sidebar, 350, -1);

  // Config Card
  GtkWidget *frame_conf = create_card("Configuration");
  GtkWidget *box_conf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(box_conf), 10);
  gtk_container_add(GTK_CONTAINER(frame_conf), box_conf);

  GtkWidget *box_mode = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(box_mode), gtk_label_new("Mode :"), FALSE, FALSE,
                     0);
  widgets_sort->radio_auto =
      gtk_radio_button_new_with_label(NULL, "Automatique");
  widgets_sort->radio_manual = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(widgets_sort->radio_auto), "Manuel");
  g_signal_connect(widgets_sort->radio_auto, "toggled",
                   G_CALLBACK(on_mode_toggled), NULL);
  gtk_box_pack_start(GTK_BOX(box_mode), widgets_sort->radio_auto, FALSE, FALSE,
                     5);
  gtk_box_pack_start(GTK_BOX(box_mode), widgets_sort->radio_manual, FALSE,
                     FALSE, 5);
  gtk_box_pack_start(GTK_BOX(box_conf), box_mode, FALSE, FALSE, 0);

  widgets_sort->combo_type = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_sort->combo_type),
                                 "Entiers");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_sort->combo_type),
                                 "Réels");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_sort->combo_type),
                                 "Caractères");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_sort->combo_type),
                                 "Chaînes");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widgets_sort->combo_type), 0);
  g_signal_connect(widgets_sort->combo_type, "changed",
                   G_CALLBACK(on_type_changed), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), widgets_sort->combo_type, FALSE, FALSE,
                     0);

  // Size Entry
  widgets_sort->entry_size = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(widgets_sort->entry_size), "1000");
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_sort->entry_size),
                                 "Taille (N)");
  gtk_box_pack_start(GTK_BOX(box_conf), widgets_sort->entry_size, FALSE, FALSE,
                     0);

  // Manual Input (Single Value)
  widgets_sort->box_manual_input = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

  widgets_sort->entry_manual_val = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_sort->entry_manual_val),
                                 "Valeur");
  gtk_box_pack_start(GTK_BOX(widgets_sort->box_manual_input),
                     widgets_sort->entry_manual_val, TRUE, TRUE, 0);

  GtkWidget *btn_add = gtk_button_new_with_label("+");
  style_button(btn_add, "#17a2b8"); // Cyan
  g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_manual_value), NULL);
  gtk_box_pack_start(GTK_BOX(widgets_sort->box_manual_input), btn_add, FALSE,
                     FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box_conf), widgets_sort->box_manual_input, FALSE,
                     FALSE, 0);

  GtkWidget *btn_gen = gtk_button_new_with_label("Générer (Auto)");
  style_button(btn_gen, "#007BFF");
  g_signal_connect(btn_gen, "clicked", G_CALLBACK(on_generate), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), btn_gen, FALSE, FALSE, 0);

  GtkWidget *btn_reset = gtk_button_new_with_label("Reset");
  style_button(btn_reset, "#DC3545");
  g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset), NULL);
  gtk_box_pack_start(GTK_BOX(box_conf), btn_reset, FALSE, FALSE, 0);

  // Persistence
  gtk_box_pack_start(GTK_BOX(box_conf),
                     gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
                     FALSE, 5);
  GtkWidget *box_p = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *btn_s = gtk_button_new_with_label("Sauvegarder");
  style_button(btn_s, "#6610f2");
  GtkWidget *btn_l = gtk_button_new_with_label("Charger");
  style_button(btn_l, "#ffc107");
  g_signal_connect(btn_s, "clicked", G_CALLBACK(on_save), NULL);
  g_signal_connect(btn_l, "clicked", G_CALLBACK(on_load), NULL);
  gtk_box_pack_start(GTK_BOX(box_p), btn_s, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_p), btn_l, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_conf), box_p, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(sidebar), frame_conf, FALSE, FALSE, 0);

  // Algos
  GtkWidget *frame_al = create_card("Algorithmes");
  GtkWidget *box_al = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(frame_al), box_al);
  gtk_container_set_border_width(GTK_CONTAINER(box_al), 10);

  GtkWidget *btn1 = gtk_button_new_with_label("Tri Bulle");
  g_signal_connect(btn1, "clicked", G_CALLBACK(on_sort_bubble), NULL);
  gtk_box_pack_start(GTK_BOX(box_al), btn1, FALSE, FALSE, 0);

  GtkWidget *btn2 = gtk_button_new_with_label("Tri Insertion");
  g_signal_connect(btn2, "clicked", G_CALLBACK(on_sort_insert), NULL);
  gtk_box_pack_start(GTK_BOX(box_al), btn2, FALSE, FALSE, 0);

  GtkWidget *btn3 = gtk_button_new_with_label("Tri Shell");
  g_signal_connect(btn3, "clicked", G_CALLBACK(on_sort_shell), NULL);
  gtk_box_pack_start(GTK_BOX(box_al), btn3, FALSE, FALSE, 0);

  GtkWidget *btn4 = gtk_button_new_with_label("Tri Rapide");
  g_signal_connect(btn4, "clicked", G_CALLBACK(on_sort_quick), NULL);
  gtk_box_pack_start(GTK_BOX(box_al), btn4, FALSE, FALSE, 0);

  GtkWidget *btn_cmp = gtk_button_new_with_label("Comparer Tout");
  style_button(btn_cmp, "#28A745");
  g_signal_connect(btn_cmp, "clicked", G_CALLBACK(on_compare_all), NULL);
  gtk_box_pack_start(GTK_BOX(box_al), btn_cmp, FALSE, FALSE, 10);

  gtk_box_pack_start(GTK_BOX(sidebar), frame_al, FALSE, FALSE, 0);
  gtk_paned_add1(GTK_PANED(main_paned), sidebar);

  // Content
  GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  GtkWidget *box_dat = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_size_request(box_dat, -1, 150);

  GtkWidget *fr_raw = gtk_frame_new("DONNÉES BRUTES");
  GtkWidget *sc_raw = gtk_scrolled_window_new(NULL, NULL);
  widgets_sort->text_raw = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets_sort->text_raw), FALSE);
  gtk_container_add(GTK_CONTAINER(sc_raw), widgets_sort->text_raw);
  gtk_container_add(GTK_CONTAINER(fr_raw), sc_raw);

  GtkWidget *fr_sort = gtk_frame_new("RÉSULTAT TRIÉ");
  GtkWidget *sc_sort = gtk_scrolled_window_new(NULL, NULL);
  widgets_sort->text_sorted = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets_sort->text_sorted), FALSE);
  gtk_container_add(GTK_CONTAINER(sc_sort), widgets_sort->text_sorted);
  gtk_container_add(GTK_CONTAINER(fr_sort), sc_sort);

  gtk_box_pack_start(GTK_BOX(box_dat), fr_raw, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_dat), fr_sort, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), box_dat, FALSE, FALSE, 0);

  GtkWidget *fr_vis = create_card("Visualisation");
  widgets_sort->drawing_area = gtk_drawing_area_new();
  g_signal_connect(widgets_sort->drawing_area, "draw", G_CALLBACK(on_draw_area),
                   NULL);
  gtk_container_add(GTK_CONTAINER(fr_vis), widgets_sort->drawing_area);
  gtk_box_pack_start(GTK_BOX(content), fr_vis, TRUE, TRUE, 0);

  gtk_paned_add2(GTK_PANED(main_paned), content);
  on_mode_toggled(NULL, NULL);

  return main_paned;
}
