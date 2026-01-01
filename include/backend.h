#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>

// Structure to track sorting statistics
typedef struct {
  unsigned long comparisons;
  unsigned long assignments;
} SortStats;

// Array structure (optional, but good for encapsulation)
typedef struct {
  int *data;
  size_t size;
} IntArray;

// Array Utils
void generate_array(int *arr, size_t n,
                    int type); // 0=Random, 1=Sorted, 2=Reversed

// Sorting Algorithms
void sort_bubble(int *arr, size_t n, SortStats *stats);
void sort_insertion(int *arr, size_t n, SortStats *stats);
void sort_shell(int *arr, size_t n, SortStats *stats);
void sort_quick(int *arr, size_t n, SortStats *stats);

void sort_quick(int *arr, size_t n, SortStats *stats);

// --- Linked Lists (Generic) ---

// Placeholder for data type enum (e.g., INT, FLOAT, STRING, CUSTOM)
typedef enum {
  TYPE_INT,
  TYPE_DOUBLE,
  TYPE_CHAR,
  TYPE_STRING,
  TYPE_CUSTOM
} DataType;

// Placeholder for comparison function pointer
typedef int (*CompareFunc)(const void *, const void *);

typedef struct Node {
  void *data;
  struct Node *next;
  struct Node *prev;
} Node;

typedef struct {
  Node *head;
  Node *tail;
  size_t size;
  DataType type; // Store the type for casting
  int is_doubly; // 0 = Single, 1 = Double
} LinkedList;

void list_init(LinkedList *list, DataType type, int is_doubly);
void list_append(LinkedList *list, void *value);
void list_prepend(LinkedList *list, void *value);
void list_insert_at(LinkedList *list, int index, void *value);
void list_remove_val(LinkedList *list, void *value, CompareFunc cmp);
void list_remove_at(LinkedList *list, int index);
void list_clear(LinkedList *list);
void *list_get(LinkedList *list, int index); // Returns pointer to data
void list_sort(LinkedList *list, int algo_id,
               CompareFunc cmp); // Bubble/Selection logic on list

// Tree Structures
typedef struct TreeNode {
  void *data; // Generic data
  struct TreeNode *left;
  struct TreeNode *right;
} TreeNode;

typedef struct NaryNode {
  void *data; // Generic data
  struct NaryNode **children;
  size_t child_count;
} NaryNode;

// Binary Tree Ops
TreeNode *create_bst_node(void *data);
TreeNode *insert_bst(TreeNode *root, void *data, CompareFunc cmp);
TreeNode *delete_bst(TreeNode *root, void *data, CompareFunc cmp);
int height_bst(TreeNode *root);
int size_bst(TreeNode *root);

// N-ary Tree Ops
NaryNode *create_nary_node(void *data);
NaryNode *add_nary_child(NaryNode *parent, void *data);
void delete_nary(NaryNode *root, void *data, CompareFunc cmp); // Logical delete
void delete_nary_ptr(NaryNode **root, NaryNode *node);
void sort_nary_children(NaryNode *root, CompareFunc cmp);
int height_nary(NaryNode *root);
int size_nary(NaryNode *root);

// Transformation
TreeNode *nary_to_binary(NaryNode *root);

// Traversals (Return allocated string)
// Need DataType to know how to print
char *bst_preorder(TreeNode *root, DataType type);
char *bst_inorder(TreeNode *root, DataType type);
char *bst_postorder(TreeNode *root, DataType type);
char *bst_bfs(TreeNode *root, DataType type);

char *nary_preorder(NaryNode *root, DataType type);
char *nary_postorder(NaryNode *root, DataType type);
char *nary_bfs(NaryNode *root, DataType type);

// Graph Structures
#define INF 1e9

typedef struct {
  int id;
  int x, y;   // For visualization
  void *data; // Optional payload
} GraphNode;

typedef struct {
  int capacity;
  int count;
  GraphNode **nodes;
  float **matrix; // Adjacency Matrix (weights)
} Graph;

// Graph Ops
Graph *graph_init(int capacity);
void graph_free(Graph *g);
int graph_add_node(Graph *g, void *data);
void graph_add_edge(Graph *g, int src, int dest, float weight);
void graph_remove_node(
    Graph *g, int id); // Logical remove? Or shift? Python doesn't show remove
                       // implementation clearly but UI might need it.

// Algorithms
// All return dynamic arrays that must be freed by caller
void algo_dijkstra(Graph *g, int start_node, float **dist_out, int **prev_out);
void algo_bellman(Graph *g, int start_node, float **dist_out, int **prev_out);
void algo_floyd(Graph *g, float **dist_matrix_out,
                int **next_matrix_out); // Returns flattened 2D arrays (n*n)

#endif
