#include "backend.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  TreeNode *bst_root;
  NaryNode *nary_root;
  int is_nary; // 0=Binary, 1=N-Ary

  GtkWidget *drawing_area;

  // Configuration
  GtkWidget *combo_type;
  GtkWidget *radio_binary;
  GtkWidget *radio_nary;

  DataType current_type;
  CompareFunc current_cmp;

  // Saisie
  GtkWidget *entry_val;
  GtkWidget *entry_rand_n;

  // Ops
  GtkWidget *btn_pre;
  GtkWidget *btn_in;
  GtkWidget *btn_post;
  GtkWidget *btn_bfs;

  GtkWidget *label_stats;

  // Interaction
  double zoom;
  double offset_x, offset_y;
  double start_pan_x, start_pan_y;
  gboolean is_panning;

  void *selected_node; // Ptr to TreeNode or NaryNode

  GtkWidget *popup_menu;

} TabTreeWidgets;

static TabTreeWidgets *widgets_tree;

// --- Comparators ---
static int cmp_int(const void *a, const void *b) {
  return (*(int *)a) - (*(int *)b);
}
static int cmp_double(const void *a, const void *b) {
  double d = (*(double *)a) - (*(double *)b);
  if (d > 0)
    return 1;
  if (d < 0)
    return -1;
  return 0;
}
static int cmp_char(const void *a, const void *b) {
  return (*(char *)a) - (*(char *)b);
}
static int cmp_str(const void *a, const void *b) {
  return strcmp((char *)a, (char *)b);
}

// --- Helpers ---

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

static void style_button_color(GtkWidget *btn, const char *color_str) {
  GdkRGBA color;
  gdk_rgba_parse(&color, color_str);
  gtk_widget_override_background_color(btn, GTK_STATE_FLAG_NORMAL, &color);
}

static void *parse_input(const char *txt) {
  if (!txt)
    return NULL;
  void *data = NULL;
  switch (widgets_tree->current_type) {
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

static char *format_data(void *data) {
  char *buf = malloc(64);
  if (!data) {
    strcpy(buf, "?");
    return buf;
  }
  switch (widgets_tree->current_type) {
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

// --- Layout Engine ---

typedef struct DrawNode {
  char label[64];
  double x, y;
  struct DrawNode **children;
  int child_count;
  void *original_ptr; // Pointer to backend node
} DrawNode;

// Constants for Layout
#define LAYOUT_Y_GAP 80.0
#define LAYOUT_X_UNIT 80.0
#define NODE_RADIUS 20.0
#define SUBTREE_GAP 40.0

static DrawNode *create_draw_node(void *data, double y, void *original) {
  DrawNode *n = malloc(sizeof(DrawNode));
  if (data) {
    char *s = format_data(data);
    snprintf(n->label, 64, "%s", s);
    free(s);
  } else {
    strcpy(n->label, "?");
  }
  n->x = 0;
  n->y = y;
  n->children = NULL;
  n->child_count = 0;
  n->original_ptr = original;
  return n;
}

static void add_draw_child(DrawNode *parent, DrawNode *child) {
  parent->child_count++;
  parent->children =
      realloc(parent->children, parent->child_count * sizeof(DrawNode *));
  parent->children[parent->child_count - 1] = child;
}

static void free_draw_tree(DrawNode *root) {
  if (!root)
    return;
  for (int i = 0; i < root->child_count; i++)
    free_draw_tree(root->children[i]);
  if (root->children)
    free(root->children);
  free(root);
}

// Helper to shift a subtree
static void move_draw_tree(DrawNode *node, double dx) {
  if (!node)
    return;
  node->x += dx;
  for (int i = 0; i < node->child_count; i++)
    move_draw_tree(node->children[i], dx);
}

// Recursive Layout returning Width of the subtree
static double bst_layout_separation(DrawNode *node, TreeNode *backend) {
  if (!node || !backend)
    return 0;

  double width = 0;

  // Leaf
  if (!backend->left && !backend->right) {
    node->x = 0;                          // Relative to subtree start
    return NODE_RADIUS * 2 + SUBTREE_GAP; // Min width
  }

  DrawNode *left_d = NULL;
  DrawNode *right_d = NULL;
  int c_idx = 0;
  if (backend->left)
    left_d = node->children[c_idx++];
  if (backend->right)
    right_d = node->children[c_idx++];

  double w_left = 0;
  double w_right = 0;

  if (backend->left) {
    w_left = bst_layout_separation(left_d, backend->left);
  }

  if (backend->right) {
    w_right = bst_layout_separation(right_d, backend->right);
    // Shift Right Subtree
    move_draw_tree(right_d, w_left);
  }

  width = w_left + w_right;
  if (width < NODE_RADIUS * 2 + SUBTREE_GAP)
    width = NODE_RADIUS * 2 + SUBTREE_GAP;

  // Position Root
  if (backend->left && backend->right) {
    // Center between children's centers
    node->x = (left_d->x + right_d->x) / 2.0;
  } else if (backend->left) {
    // Slant Right (Parent is to the right of Left Child)
    node->x = left_d->x + NODE_RADIUS;
  } else if (backend->right) {
    // Slant Left (Parent is to the left of Right Child)
    node->x = right_d->x - NODE_RADIUS;
  }

  return width;
}

static DrawNode *bst_convert_structure(TreeNode *root, int depth) {
  if (!root)
    return NULL;
  DrawNode *node =
      create_draw_node(root->data, depth * LAYOUT_Y_GAP + 50.0, root);
  if (root->left)
    add_draw_child(node, bst_convert_structure(root->left, depth + 1));
  if (root->right)
    add_draw_child(node, bst_convert_structure(root->right, depth + 1));
  return node;
}

static DrawNode *nary_convert_structure(NaryNode *root, int depth) {
  if (!root)
    return NULL;
  DrawNode *node =
      create_draw_node(root->data, depth * LAYOUT_Y_GAP + 50.0, root);
  for (size_t i = 0; i < root->child_count; i++) {
    add_draw_child(node, nary_convert_structure(root->children[i], depth + 1));
  }
  return node;
}

// Modified N-ary layout to use separation
static double nary_layout_separation(DrawNode *node) {
  if (node->child_count == 0) {
    node->x = 0;
    return NODE_RADIUS * 2 + SUBTREE_GAP;
  }

  double current_x = 0;
  double first_child_x = 0;
  double last_child_x = 0;

  for (int i = 0; i < node->child_count; i++) {
    double w = nary_layout_separation(node->children[i]);
    move_draw_tree(node->children[i], current_x);

    if (i == 0)
      first_child_x = node->children[i]->x;
    if (i == node->child_count - 1)
      last_child_x = node->children[i]->x;

    current_x += w;
  }

  node->x = (first_child_x + last_child_x) / 2.0;
  return current_x;
}

// Rendering
static void render_edges(cairo_t *cr, DrawNode *node) {
  if (!node)
    return;

  cairo_set_source_rgb(cr, 0.6, 0.6, 0.6); // Grey edges
  cairo_set_line_width(cr, 1.5);

  for (int i = 0; i < node->child_count; i++) {
    // Top of Child
    double cx = node->children[i]->x;
    double cy = node->children[i]->y; // Center
    // Bottom of Parent
    double px = node->x;
    double py = node->y;

    // Draw from Center to Center (draw_node covers overlap)
    cairo_move_to(cr, px, py);
    cairo_line_to(cr, cx, cy);
    cairo_stroke(cr);

    render_edges(cr, node->children[i]);
  }
}

static void render_nodes(cairo_t *cr, DrawNode *node) {
  if (!node)
    return;

  // Self
  double radius = NODE_RADIUS;

  if (node->original_ptr == widgets_tree->selected_node) {
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2); // Red Hi-light
    cairo_arc(cr, node->x, node->y, radius + 2, 0, 2 * G_PI);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1.0, 0.95, 0.9); // Pale bg
  } else {
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White bg
  }
  cairo_arc(cr, node->x, node->y, radius, 0, 2 * G_PI);
  cairo_fill(cr);

  // Border
  if (node->original_ptr == widgets_tree->selected_node)
    cairo_set_source_rgb(cr, 0.8, 0.0, 0.0);
  else
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

  cairo_set_line_width(cr, 2.0);
  cairo_arc(cr, node->x, node->y, radius, 0, 2 * G_PI);
  cairo_stroke(cr);

  // Text
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  cairo_text_extents_t extents;
  cairo_text_extents(cr, node->label, &extents);
  cairo_move_to(cr, node->x - extents.width / 2 - extents.x_bearing,
                node->y - extents.height / 2 - extents.y_bearing);
  cairo_show_text(cr, node->label);

  // Children
  for (int i = 0; i < node->child_count; i++)
    render_nodes(cr, node->children[i]);
}

// Layout Generator
static DrawNode *generate_current_layout() {
  DrawNode *d_root = NULL;
  if (widgets_tree->is_nary) {
    if (widgets_tree->nary_root) {
      d_root = nary_convert_structure(widgets_tree->nary_root, 0);
      nary_layout_separation(d_root);
      move_draw_tree(d_root, 400.0 - d_root->x); // Center primitive
    }
  } else {
    if (widgets_tree->bst_root) {
      d_root = bst_convert_structure(widgets_tree->bst_root, 0);
      bst_layout_separation(d_root, widgets_tree->bst_root);
      move_draw_tree(d_root, 400.0 - d_root->x);
    }
  }
  return d_root;
}

// Hit Test
static void *hit_test_recursive(DrawNode *node, double mx, double my) {
  if (!node)
    return NULL;
  double dx = mx - node->x;
  double dy = my - node->y;
  // Radius ~20
  if (dx * dx + dy * dy <= 400.0) {
    return node->original_ptr;
  }
  for (int i = 0; i < node->child_count; i++) {
    void *res = hit_test_recursive(node->children[i], mx, my);
    if (res)
      return res;
  }
  return NULL;
}

// --- Events ---

static gboolean on_draw_tree(GtkWidget *widget, cairo_t *cr, gpointer data) {
  cairo_set_source_rgb(cr, 0.95, 0.95, 0.95); // Light Gray BG
  cairo_paint(cr);

  // Apply Transform
  cairo_translate(cr, widgets_tree->offset_x, widgets_tree->offset_y);
  cairo_scale(cr, widgets_tree->zoom, widgets_tree->zoom);

  DrawNode *d_root = generate_current_layout();
  if (d_root) {
    render_edges(cr, d_root);
    render_nodes(cr, d_root);
    free_draw_tree(d_root);
  }
  return FALSE;
}

static gboolean on_scroll_tree(GtkWidget *w, GdkEventScroll *e, gpointer d) {
  if (e->direction == GDK_SCROLL_UP)
    widgets_tree->zoom *= 1.1;
  else if (e->direction == GDK_SCROLL_DOWN)
    widgets_tree->zoom /= 1.1;
  gtk_widget_queue_draw(widgets_tree->drawing_area);
  return TRUE;
}

static void on_menu_edit_val(GtkWidget *item, gpointer d) {
  // Show dialog to edit value
  if (!widgets_tree->selected_node)
    return;

  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Modifier Valeur", NULL, GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL,
      "Valider", GTK_RESPONSE_ACCEPT, NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *entry = gtk_entry_new();
  gtk_container_add(GTK_CONTAINER(content), entry);
  gtk_widget_show_all(content);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    const char *txt = gtk_entry_get_text(GTK_ENTRY(entry));
    void *new_data = parse_input(txt);
    if (new_data) {
      // Free old data? Depends on ownership model.
      // For simplicity, we just overwrite pointer.
      // Real implementation should free old if dynamically allocated.
      // Assuming we do.
      if (widgets_tree->is_nary) {
        free(((NaryNode *)widgets_tree->selected_node)->data);
        ((NaryNode *)widgets_tree->selected_node)->data = new_data;
      } else {
        free(((TreeNode *)widgets_tree->selected_node)->data);
        ((TreeNode *)widgets_tree->selected_node)->data = new_data;
      }
      gtk_widget_queue_draw(widgets_tree->drawing_area);
    }
  }
  gtk_widget_destroy(dialog);
}

static void on_menu_del_node(GtkWidget *item, gpointer d) {
  if (!widgets_tree->selected_node)
    return;

  // Call generic delete (using pointer based for both mostly?
  // No, BST uses value based usually, but we have generic delete_bst.
  // Actually, delete_bst requires data to search.
  // Pointer-based delete for BST is harder if we don't have parent pointers.
  // N-ary has delete_nary_ptr.
  // BST: we must use delete_bst with value.

  if (widgets_tree->is_nary)
    delete_nary_ptr(&widgets_tree->nary_root,
                    (NaryNode *)widgets_tree->selected_node);
  else {
    // Need data to find it.
    void *data = ((TreeNode *)widgets_tree->selected_node)->data;
    widgets_tree->bst_root =
        delete_bst(widgets_tree->bst_root, data, widgets_tree->current_cmp);
  }

  widgets_tree->selected_node = NULL;
  gtk_widget_queue_draw(widgets_tree->drawing_area);
}

static void on_menu_add_child(GtkWidget *item, gpointer d) {
  if (!widgets_tree->selected_node || !widgets_tree->is_nary)
    return;
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Ajouter Enfant", NULL, GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL,
      "Ajouter", GTK_RESPONSE_ACCEPT, NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *entry = gtk_entry_new();
  // Default text based on type?
  gtk_container_add(GTK_CONTAINER(content), entry);
  gtk_widget_show_all(content);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    const char *txt = gtk_entry_get_text(GTK_ENTRY(entry));
    void *val = parse_input(txt);
    if (val) {
      add_nary_child((NaryNode *)widgets_tree->selected_node, val);
      gtk_widget_queue_draw(widgets_tree->drawing_area);
    }
  }
  gtk_widget_destroy(dialog);
}

static gboolean on_button_press_tree(GtkWidget *w, GdkEventButton *e,
                                     gpointer d) {
  // Calc World Coords
  double wx = (e->x - widgets_tree->offset_x) / widgets_tree->zoom;
  double wy = (e->y - widgets_tree->offset_y) / widgets_tree->zoom;

  DrawNode *layout = generate_current_layout();
  void *hit = hit_test_recursive(layout, wx, wy);
  free_draw_tree(layout);

  if (e->button == 1) { // Left: Select or Pan
    if (hit) {
      widgets_tree->selected_node = hit;
      widgets_tree->is_panning = FALSE;

      // Double Click?
      if (e->type == GDK_2BUTTON_PRESS) {
        on_menu_edit_val(NULL, NULL);
      }
    } else {
      widgets_tree->selected_node = NULL;
      widgets_tree->is_panning = TRUE;
      widgets_tree->start_pan_x = e->x - widgets_tree->offset_x;
      widgets_tree->start_pan_y = e->y - widgets_tree->offset_y;
    }
    gtk_widget_queue_draw(widgets_tree->drawing_area);
  } else if (e->button == 3) { // Right: Menu
    if (hit) {
      widgets_tree->selected_node = hit;
      gtk_widget_queue_draw(widgets_tree->drawing_area);

      GtkWidget *menu = gtk_menu_new();
      GtkWidget *item_edit = gtk_menu_item_new_with_label("Modifier la valeur");
      g_signal_connect(item_edit, "activate", G_CALLBACK(on_menu_edit_val),
                       NULL);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_edit);

      if (widgets_tree->is_nary) {
        GtkWidget *item_add = gtk_menu_item_new_with_label("Ajouter un enfant");
        g_signal_connect(item_add, "activate", G_CALLBACK(on_menu_add_child),
                         NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_add);
      }

      GtkWidget *item_del = gtk_menu_item_new_with_label("Supprimer le nœud");
      g_signal_connect(item_del, "activate", G_CALLBACK(on_menu_del_node),
                       NULL);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_del);

      gtk_widget_show_all(menu);
      gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)e);
    }
  }
  return TRUE;
}

static gboolean on_motion_tree(GtkWidget *w, GdkEventMotion *e, gpointer d) {
  if (widgets_tree->is_panning) {
    widgets_tree->offset_x = e->x - widgets_tree->start_pan_x;
    widgets_tree->offset_y = e->y - widgets_tree->start_pan_y;
    gtk_widget_queue_draw(widgets_tree->drawing_area);
  }
  return TRUE;
}

static gboolean on_button_release_tree(GtkWidget *w, GdkEventButton *e,
                                       gpointer d) {
  if (e->button == 1)
    widgets_tree->is_panning = FALSE;
  return TRUE;
}

// --- Logic ---

static void update_ui_state() {
  if (widgets_tree->is_nary) {
    gtk_widget_set_sensitive(widgets_tree->btn_in, FALSE);
  } else {
    gtk_widget_set_sensitive(widgets_tree->btn_in, TRUE);
  }
}

static void free_current_tree() {
  widgets_tree->bst_root = NULL;
  widgets_tree->nary_root = NULL;
}

static void on_clear_tree(GtkWidget *b, gpointer d) {
  free_current_tree();
  gtk_widget_queue_draw(widgets_tree->drawing_area);
  gtk_label_set_text(GTK_LABEL(widgets_tree->label_stats),
                     "Taille: 0 | Hauteur: 0");
}

static void on_config_change(GtkWidget *w, gpointer d) {
  // Update Type
  int type_idx =
      gtk_combo_box_get_active(GTK_COMBO_BOX(widgets_tree->combo_type));
  DataType next_type = TYPE_INT;
  CompareFunc next_cmp = cmp_int;

  if (type_idx == 0) {
    next_type = TYPE_INT;
    next_cmp = cmp_int;
  } else if (type_idx == 1) {
    next_type = TYPE_DOUBLE;
    next_cmp = cmp_double;
  } else if (type_idx == 2) {
    next_type = TYPE_CHAR;
    next_cmp = cmp_char;
  } else if (type_idx == 3) {
    next_type = TYPE_STRING;
    next_cmp = cmp_str;
  }

  gboolean nary =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets_tree->radio_nary));

  if (widgets_tree->current_type != next_type ||
      widgets_tree->is_nary != nary) {
    widgets_tree->current_type = next_type;
    widgets_tree->current_cmp = next_cmp;
    widgets_tree->is_nary = nary;
    free_current_tree();
    update_ui_state();
    gtk_widget_queue_draw(widgets_tree->drawing_area);
    gtk_label_set_text(GTK_LABEL(widgets_tree->label_stats),
                       "Taille: 0 | Hauteur: 0");
  }
}

static void on_insert(GtkWidget *b, gpointer d) {
  const char *txt = gtk_entry_get_text(GTK_ENTRY(widgets_tree->entry_val));
  void *val = parse_input(txt);
  if (!val)
    return;

  if (widgets_tree->is_nary) {
    if (!widgets_tree->nary_root) {
      widgets_tree->nary_root = create_nary_node(val);
    } else {
      if (widgets_tree->selected_node) {
        add_nary_child((NaryNode *)widgets_tree->selected_node, val);
      } else {
        add_nary_child(widgets_tree->nary_root, val);
      }
    }
  } else {
    widgets_tree->bst_root =
        insert_bst(widgets_tree->bst_root, val, widgets_tree->current_cmp);
  }
  gtk_widget_queue_draw(widgets_tree->drawing_area);
}

static void on_delete(GtkWidget *b, gpointer d) {
  const char *txt = gtk_entry_get_text(GTK_ENTRY(widgets_tree->entry_val));
  void *val = parse_input(txt);
  if (!val)
    return;

  if (widgets_tree->is_nary) {
    delete_nary(widgets_tree->nary_root, val, widgets_tree->current_cmp);
  } else {
    widgets_tree->bst_root =
        delete_bst(widgets_tree->bst_root, val, widgets_tree->current_cmp);
  }
  gtk_widget_queue_draw(widgets_tree->drawing_area);
}

static void on_gen_rand(GtkWidget *b, gpointer d) {
  free_current_tree();
  int n = atoi(gtk_entry_get_text(GTK_ENTRY(widgets_tree->entry_rand_n)));
  if (n <= 0)
    n = 10;

  // Array to keep track of nodes for random attachment
  void **nodes = malloc(n * sizeof(void *));
  int count = 0;

  for (int i = 0; i < n; i++) {
    void *val = NULL;
    int r = rand();

    // Generate Random Data based on type
    switch (widgets_tree->current_type) {
    case TYPE_INT:
      val = malloc(sizeof(int));
      *(int *)val = r % 100;
      break;
    case TYPE_DOUBLE:
      val = malloc(sizeof(double));
      *(double *)val = (r % 1000) / 10.0;
      break;
    case TYPE_CHAR:
      val = malloc(sizeof(char));
      *(char *)val = 'A' + (r % 26);
      break;
    case TYPE_STRING: {
      char buf[16];
      snprintf(buf, 16, "S-%d", r % 100);
      val = strdup(buf);
    } break;
    default:
      break;
    }

    if (!val)
      continue;

    if (widgets_tree->is_nary) {
      if (!widgets_tree->nary_root) {
        widgets_tree->nary_root = create_nary_node(val);
        nodes[count++] = widgets_tree->nary_root;
      } else {
        // Pick random parent
        int p_idx = rand() % count;
        NaryNode *parent = (NaryNode *)nodes[p_idx];
        add_nary_child(parent, val);
        nodes[count++] = parent->children[parent->child_count - 1];
      }
    } else {
      widgets_tree->bst_root =
          insert_bst(widgets_tree->bst_root, val, widgets_tree->current_cmp);
    }
  }

  if (nodes)
    free(nodes);
  gtk_widget_queue_draw(widgets_tree->drawing_area);
}

// Traversals
static void show_traversal(const char *title, char *result) {
  if (!result)
    return;
  GtkWidget *dlg = gtk_message_dialog_new(
      NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", result);
  gtk_window_set_title(GTK_WINDOW(dlg), title);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
  free(result);
}

static void on_pre(GtkWidget *b, gpointer d) {
  if (widgets_tree->is_nary)
    show_traversal(
        "Parcours Préfixe",
        nary_preorder(widgets_tree->nary_root, widgets_tree->current_type));
  else
    show_traversal(
        "Parcours Préfixe",
        bst_preorder(widgets_tree->bst_root, widgets_tree->current_type));
}
static void on_in(GtkWidget *b, gpointer d) {
  if (!widgets_tree->is_nary)
    show_traversal("Parcours Infixe", bst_inorder(widgets_tree->bst_root,
                                                  widgets_tree->current_type));
}
static void on_post(GtkWidget *b, gpointer d) {
  if (widgets_tree->is_nary)
    show_traversal(
        "Parcours Postfixe",
        nary_postorder(widgets_tree->nary_root, widgets_tree->current_type));
  else
    show_traversal(
        "Parcours Postfixe",
        bst_postorder(widgets_tree->bst_root, widgets_tree->current_type));
}
static void on_bfs(GtkWidget *b, gpointer d) {
  if (widgets_tree->is_nary)
    show_traversal("Parcours Largeur", nary_bfs(widgets_tree->nary_root,
                                                widgets_tree->current_type));
  else
    show_traversal("Parcours Largeur",
                   bst_bfs(widgets_tree->bst_root, widgets_tree->current_type));
}

static void on_calc(GtkWidget *b, gpointer d) {
  char buf[64];
  if (widgets_tree->is_nary) {
    snprintf(buf, 64, "Taille: %d | Hauteur: %d",
             size_nary(widgets_tree->nary_root),
             height_nary(widgets_tree->nary_root));
  } else {
    snprintf(buf, 64, "Taille: %d | Hauteur: %d",
             size_bst(widgets_tree->bst_root),
             height_bst(widgets_tree->bst_root));
  }
  gtk_label_set_text(GTK_LABEL(widgets_tree->label_stats), buf);
}

static void on_sort_nary(GtkWidget *b, gpointer d) {
  if (widgets_tree->is_nary && widgets_tree->nary_root) {
    sort_nary_children(widgets_tree->nary_root, widgets_tree->current_cmp);
    gtk_widget_queue_draw(widgets_tree->drawing_area);
  }
}

static void on_transform(GtkWidget *b, gpointer d) {
  if (widgets_tree->is_nary && widgets_tree->nary_root) {

    // Convert only works well if we assume standard logic?
    // nary_to_binary uses generic data now.

    TreeNode *bin = nary_to_binary(widgets_tree->nary_root);
    // Switch mode
    widgets_tree->is_nary = 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets_tree->radio_binary),
                                 TRUE);

    widgets_tree->bst_root = bin;
    widgets_tree->nary_root = NULL; // Moved

    gtk_widget_queue_draw(widgets_tree->drawing_area);
    update_ui_state();
  }
}

// --- Init ---

GtkWidget *create_tab_tree(void) {
  widgets_tree = malloc(sizeof(TabTreeWidgets));
  widgets_tree->bst_root = NULL;
  widgets_tree->nary_root = NULL;
  widgets_tree->is_nary = 0;
  widgets_tree->current_type = TYPE_INT;
  widgets_tree->current_cmp = cmp_int;
  widgets_tree->zoom = 1.0;
  widgets_tree->offset_x = 0;
  widgets_tree->offset_y = 0;
  widgets_tree->selected_node = NULL;
  widgets_tree->is_panning = FALSE;

  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned), 350);

  // Sidebar
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_size_request(sidebar, 330, -1);
  gtk_widget_set_margin_start(sidebar, 10);
  gtk_widget_set_margin_top(sidebar, 10);
  gtk_widget_set_margin_end(sidebar, 10); // Spacing from paned handle

  // 1. CONFIGURATION
  GtkWidget *fr_conf = create_card_frame("1. Configuration");
  GtkWidget *box_conf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(fr_conf), box_conf);

  gtk_box_pack_start(GTK_BOX(box_conf), gtk_label_new("Type :"), FALSE, FALSE,
                     0);
  widgets_tree->combo_type = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_tree->combo_type),
                                 "Entiers");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_tree->combo_type),
                                 "Réels");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_tree->combo_type),
                                 "Caractères");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets_tree->combo_type),
                                 "Chaînes");

  gtk_combo_box_set_active(GTK_COMBO_BOX(widgets_tree->combo_type), 0);
  g_signal_connect(widgets_tree->combo_type, "changed",
                   G_CALLBACK(on_config_change), NULL);

  gtk_box_pack_start(GTK_BOX(box_conf), widgets_tree->combo_type, FALSE, FALSE,
                     0);

  gtk_box_pack_start(GTK_BOX(box_conf), gtk_label_new("Structure :"), FALSE,
                     FALSE, 0);
  widgets_tree->radio_binary = gtk_radio_button_new_with_label(NULL, "Binaire");
  widgets_tree->radio_nary = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(widgets_tree->radio_binary), "N-Aire");
  g_signal_connect(widgets_tree->radio_binary, "toggled",
                   G_CALLBACK(on_config_change), NULL);

  GtkWidget *box_radios = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_pack_start(GTK_BOX(box_radios), widgets_tree->radio_binary, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_radios), widgets_tree->radio_nary, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_conf), box_radios, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sidebar), fr_conf, FALSE, FALSE, 0);

  // 2. SAISIE
  GtkWidget *fr_saisie = create_card_frame("2. Saisie");
  GtkWidget *box_saisie = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(fr_saisie), box_saisie);

  gtk_box_pack_start(GTK_BOX(box_saisie), gtk_label_new("Manuel :"), FALSE,
                     FALSE, 0);
  widgets_tree->entry_val = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(widgets_tree->entry_val),
                                 "Valeur...");
  gtk_box_pack_start(GTK_BOX(box_saisie), widgets_tree->entry_val, FALSE, FALSE,
                     0);

  GtkWidget *box_man_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *btn_ins = gtk_button_new_with_label("Insérer");
  style_button_color(btn_ins, "#007BFF");
  g_signal_connect(btn_ins, "clicked", G_CALLBACK(on_insert), NULL);
  GtkWidget *btn_del = gtk_button_new_with_label("Supprimer");
  style_button_color(btn_del, "#DC3545");
  g_signal_connect(btn_del, "clicked", G_CALLBACK(on_delete), NULL);
  gtk_box_pack_start(GTK_BOX(box_man_btns), btn_ins, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_man_btns), btn_del, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_saisie), box_man_btns, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box_saisie), gtk_label_new("Aléatoire :"), FALSE,
                     FALSE, 0);
  GtkWidget *box_rand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  widgets_tree->entry_rand_n = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(widgets_tree->entry_rand_n), "10");
  GtkWidget *btn_gen = gtk_button_new_with_label("Générer");
  style_button_color(btn_gen, "#28A745");
  g_signal_connect(btn_gen, "clicked", G_CALLBACK(on_gen_rand), NULL);
  gtk_box_pack_start(GTK_BOX(box_rand), widgets_tree->entry_rand_n, TRUE, TRUE,
                     0);
  gtk_box_pack_start(GTK_BOX(box_rand), btn_gen, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box_saisie), box_rand, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sidebar), fr_saisie, FALSE, FALSE, 0);

  // 3. OPERATIONS
  GtkWidget *fr_ops = create_card_frame("3. Opérations");
  GtkWidget *box_ops = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(fr_ops), box_ops);

  gtk_box_pack_start(GTK_BOX(box_ops), gtk_label_new("Parcours :"), FALSE,
                     FALSE, 0);
  GtkWidget *grid_trav = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid_trav), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid_trav), 5);
  widgets_tree->btn_pre = gtk_button_new_with_label("Pré");
  widgets_tree->btn_in = gtk_button_new_with_label("In");
  widgets_tree->btn_post = gtk_button_new_with_label("Post");
  widgets_tree->btn_bfs = gtk_button_new_with_label("Largeur");
  g_signal_connect(widgets_tree->btn_pre, "clicked", G_CALLBACK(on_pre), NULL);
  g_signal_connect(widgets_tree->btn_in, "clicked", G_CALLBACK(on_in), NULL);
  g_signal_connect(widgets_tree->btn_post, "clicked", G_CALLBACK(on_post),
                   NULL);
  g_signal_connect(widgets_tree->btn_bfs, "clicked", G_CALLBACK(on_bfs), NULL);

  gtk_grid_attach(GTK_GRID(grid_trav), widgets_tree->btn_pre, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid_trav), widgets_tree->btn_in, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid_trav), widgets_tree->btn_post, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid_trav), widgets_tree->btn_bfs, 1, 1, 1, 1);
  gtk_box_pack_start(GTK_BOX(box_ops), grid_trav, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box_ops),
                     gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
                     FALSE, 5);
  gtk_box_pack_start(GTK_BOX(box_ops), gtk_label_new("Métriques & Tri :"),
                     FALSE, FALSE, 0);
  widgets_tree->label_stats = gtk_label_new("Taille: 0 | Hauteur: 0");
  gtk_box_pack_start(GTK_BOX(box_ops), widgets_tree->label_stats, FALSE, FALSE,
                     0);

  GtkWidget *box_metrics = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *btn_calc = gtk_button_new_with_label("Calculer");
  GtkWidget *btn_sort_n = gtk_button_new_with_label("Trier (N-Aire)");
  g_signal_connect(btn_calc, "clicked", G_CALLBACK(on_calc), NULL);
  g_signal_connect(btn_sort_n, "clicked", G_CALLBACK(on_sort_nary), NULL);
  gtk_box_pack_start(GTK_BOX(box_metrics), btn_calc, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_metrics), btn_sort_n, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box_ops), box_metrics, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box_ops),
                     gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
                     FALSE, 5);
  GtkWidget *btn_trans = gtk_button_new_with_label("Transformer (N -> B)");
  g_signal_connect(btn_trans, "clicked", G_CALLBACK(on_transform), NULL);
  gtk_box_pack_start(GTK_BOX(box_ops), btn_trans, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(sidebar), fr_ops, FALSE, FALSE, 0);

  // 4. USAGE / AIDE
  GtkWidget *exp_help = gtk_expander_new("Aide / Utilisation");
  GtkWidget *lbl_help =
      gtk_label_new("<b>Boutons Insérer/Supprimer :</b>\n"
                    "Agissent sur la racine ou le nœud sélectionné\n"
                    "en utilisant la valeur saisie.\n"
                    "<i>Pour BST:</i> Insert/Delete par recherche.\n"
                    "<i>Pour N-Aire:</i> Insert comme dernier enfant.\n\n"
                    "<b>Clic-Droit (Contextuel) :</b>\n"
                    "Utilisez le clic droit sur un nœud pour :\n"
                    "- Modifier sa valeur.\n"
                    "- Ajouter un enfant directement sous ce nœud.\n"
                    "- Supprimer ce nœud spécifique.");
  gtk_label_set_use_markup(GTK_LABEL(lbl_help), TRUE);
  gtk_container_add(GTK_CONTAINER(exp_help), lbl_help);
  gtk_box_pack_start(GTK_BOX(sidebar), exp_help, FALSE, FALSE, 0);

  // Footer
  GtkWidget *btn_clear = gtk_button_new_with_label("Tout Effacer");
  style_button_color(btn_clear, "#DC3545");
  g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_clear_tree), NULL);
  gtk_box_pack_start(GTK_BOX(sidebar), btn_clear, FALSE, FALSE, 10);

  // Drawing Area
  widgets_tree->drawing_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(widgets_tree->drawing_area, 800, 600);

  // Events
  gtk_widget_add_events(widgets_tree->drawing_area,
                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                            GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);

  g_signal_connect(widgets_tree->drawing_area, "draw", G_CALLBACK(on_draw_tree),
                   widgets_tree);
  g_signal_connect(widgets_tree->drawing_area, "scroll-event",
                   G_CALLBACK(on_scroll_tree), NULL);
  g_signal_connect(widgets_tree->drawing_area, "button-press-event",
                   G_CALLBACK(on_button_press_tree), NULL);
  g_signal_connect(widgets_tree->drawing_area, "button-release-event",
                   G_CALLBACK(on_button_release_tree), NULL);
  g_signal_connect(widgets_tree->drawing_area, "motion-notify-event",
                   G_CALLBACK(on_motion_tree), NULL);

  // Wrap sidebar in scroll
  GtkWidget *scroll_sidebar = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_sidebar),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scroll_sidebar), sidebar);

  // Wrap drawing area in scroll
  GtkWidget *scroll_drawing = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(scroll_drawing), widgets_tree->drawing_area);

  gtk_paned_pack1(GTK_PANED(paned), scroll_sidebar, FALSE, FALSE);
  gtk_paned_pack2(GTK_PANED(paned), scroll_drawing, TRUE, FALSE);

  gtk_widget_show_all(paned);
  return paned;
}
