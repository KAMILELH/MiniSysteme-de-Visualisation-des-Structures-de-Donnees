#include "backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to copy data based on type
static void *copy_data(void *data, DataType type) {
  if (!data)
    return NULL;
  switch (type) {
  case TYPE_INT: {
    int *new_val = malloc(sizeof(int));
    *new_val = *(int *)data;
    return new_val;
  }
  case TYPE_DOUBLE: {
    double *new_val = malloc(sizeof(double));
    *new_val = *(double *)data;
    return new_val;
  }
  case TYPE_CHAR: {
    char *new_val = malloc(sizeof(char));
    *new_val = *(char *)data;
    return new_val;
  }
  case TYPE_STRING: {
    return strdup((char *)data);
  }
  default:
    return NULL;
  }
}

// Helper to free data
static void free_data(void *data, DataType type) {
  if (data)
    free(data);
}

// Helper to get node at index
static Node *get_node_at(LinkedList *list, int index) {
  if (index < 0 || index >= (int)list->size)
    return NULL;
  Node *curr = list->head;
  for (int i = 0; i < index; i++)
    curr = curr->next;
  return curr;
}

void list_init(LinkedList *list, DataType type, int is_doubly) {
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
  list->type = type;
  list->is_doubly = is_doubly;
}

static Node *create_node(void *value, DataType type) {
  Node *new_node = (Node *)malloc(sizeof(Node));
  if (!new_node)
    return NULL;
  new_node->data = copy_data(value, type);
  new_node->next = NULL;
  new_node->prev = NULL;
  return new_node;
}

void list_append(LinkedList *list, void *value) {
  Node *new_node = create_node(value, list->type);
  if (!new_node)
    return;

  if (list->tail == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->tail->next = new_node;
    if (list->is_doubly)
      new_node->prev = list->tail;
    list->tail = new_node;
  }
  list->size++;
}

void list_prepend(LinkedList *list, void *value) {
  Node *new_node = create_node(value, list->type);
  if (!new_node)
    return;

  if (list->head == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->head->prev = list->is_doubly ? new_node : NULL; // Only if doubly
    if (list->is_doubly)
      list->head->prev = new_node;
    new_node->next = list->head;
    list->head = new_node;
  }
  list->size++;
}

void list_insert_at(LinkedList *list, int index, void *value) {
  if (index <= 0) {
    list_prepend(list, value);
    return;
  }
  if (index >= (int)list->size) {
    list_append(list, value);
    return;
  }

  Node *new_node = create_node(value, list->type);
  Node *curr = get_node_at(list, index); // Node currently at index

  if (!curr) { // Should not happen given checks, but safe
    list_append(list, value);
    return;
  }

  // Insert before curr
  Node *prev = NULL;

  if (list->is_doubly) {
    prev = curr->prev;
  } else {
    // Traverse to find prev
    prev = list->head;
    while (prev->next != curr) {
      prev = prev->next;
    }
  }

  prev->next = new_node;
  new_node->next = curr;

  if (list->is_doubly) {
    new_node->prev = prev;
    curr->prev = new_node;
  }
  list->size++;
}

void list_remove_val(LinkedList *list, void *value, CompareFunc cmp) {
  // Requires cmp function to find value.
  // Implementation intentionally skipped for now or simple pointer check?
  // Using cmp from backend.h logic
  (void)list;
  (void)value;
  (void)cmp;
}

void list_remove_at(LinkedList *list, int index) {
  if (index < 0 || index >= (int)list->size)
    return;

  Node *to_del = get_node_at(list, index);
  if (!to_del)
    return;

  Node *prev = to_del->prev;
  Node *next = to_del->next;

  if (!list->is_doubly && index > 0) {
    // Find prev manually
    prev = list->head;
    while (prev->next != to_del)
      prev = prev->next;
  }

  if (prev) {
    prev->next = next;
  } else {
    list->head = next;
  }

  if (next) {
    if (list->is_doubly)
      next->prev = prev;
  } else {
    list->tail = prev;
  }

  // Correct tail for singly if removed tail
  if (!list->is_doubly && !next) {
    list->tail = prev;
  }

  free_data(to_del->data, list->type);
  free(to_del);
  list->size--;
}

void list_clear(LinkedList *list) {
  Node *current = list->head;
  while (current != NULL) {
    Node *next = current->next;
    free_data(current->data, list->type);
    free(current);
    current = next;
  }
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
}

void *list_get(LinkedList *list, int index) {
  Node *n = get_node_at(list, index);
  return n ? n->data : NULL;
}

static void bubble_sort_list(LinkedList *list, CompareFunc cmp) {
  if (!list->head)
    return;
  int swapped;
  Node *ptr1;
  Node *lptr = NULL;
  do {
    swapped = 0;
    ptr1 = list->head;
    while (ptr1->next != lptr) {
      if (cmp(ptr1->data, ptr1->next->data) > 0) {
        void *temp = ptr1->data;
        ptr1->data = ptr1->next->data;
        ptr1->next->data = temp;
        swapped = 1;
      }
      ptr1 = ptr1->next;
    }
    lptr = ptr1;
  } while (swapped);
}

static void insertion_sort_list(LinkedList *list, CompareFunc cmp) {
  if (!list->head || !list->head->next)
    return;
  Node *sorted = NULL;
  Node *curr = list->head;

  // We will rebuild the list in 'sorted' order
  // But since we are generic handling void*, swapping data might be easier than
  // rewiring. Let's do data swapping implementation of Insertion Sort for
  // simplicity with doubly linked lists.

  Node *i, *j;
  for (i = list->head->next; i != NULL; i = i->next) {
    void *key = i->data;
    j = i->prev;

    // Move elements of list[0..i-1], that are greater than key
    // But wait, accessing j->prev requires Doubly.
    // If Singly, we can't easily do `j = i->prev`.

    // Generic approach for Singly/Doubly:
    // Use a standard Insertion Sort by extracting node and inserting into
    // sorted list? Or just swapping data O(N^2).

    // Let's stick to data swapping for now, assuming small lists for this UI.

    // Backward scan? Only possible if Doubly.
    if (list->is_doubly) {
      while (j != NULL && cmp(j->data, key) > 0) {
        j->next->data = j->data;
        j = j->prev;
      }
      if (j == NULL)
        list->head->data = key;
      else
        j->next->data = key;
    } else {
      // Forward scan for Singly Linked List (less efficient but works)
      // Actually, Insertion Sort on Singly Linked List typically inserts nodes
      // into a new sorted head. Let's implement that.
    }
  }
}

// Better Insertion Sort (Node extraction) for Singly/Doubly
static void insertion_sort_nodes(LinkedList *list, CompareFunc cmp) {
  if (!list->head || !list->head->next)
    return;

  Node *sorted = NULL;
  Node *curr = list->head;

  while (curr != NULL) {
    Node *next = curr->next;

    // Insert curr into sorted
    if (sorted == NULL || cmp(curr->data, sorted->data) < 0) {
      curr->next = sorted;
      if (sorted && list->is_doubly)
        sorted->prev = curr;
      sorted = curr;
      sorted->prev = NULL; // Head prev is null
    } else {
      Node *s = sorted;
      while (s->next != NULL && cmp(s->next->data, curr->data) < 0) {
        s = s->next;
      }
      curr->next = s->next;
      if (s->next && list->is_doubly)
        s->next->prev = curr;
      s->next = curr;
      if (list->is_doubly)
        curr->prev = s;
    }
    curr = next;
  }

  list->head = sorted;
  // Fix tail
  Node *t = list->head;
  while (t && t->next)
    t = t->next;
  list->tail = t;
}

// --- Quick Sort (Native) ---

static Node *get_tail(Node *cur) {
  while (cur != NULL && cur->next != NULL)
    cur = cur->next;
  return cur;
}

static Node *partition(Node *head, Node *end, Node **newHead, Node **newEnd,
                       CompareFunc cmp) {
  Node *pivot = end;
  Node *prev = NULL, *cur = head, *tail = pivot;

  // During partition, both the head and end of the list might change
  // which is updated in the newHead and newEnd variables
  while (cur != pivot) {
    if (cmp(cur->data, pivot->data) < 0) {
      // Keep node
      if ((*newHead) == NULL)
        (*newHead) = cur;
      prev = cur;
      cur = cur->next;
    } else {
      // Move node to tail
      if (prev)
        prev->next = cur->next;
      Node *tmp = cur->next;
      cur->next = NULL;
      tail->next = cur;
      tail = cur;
      cur = tmp;
    }
  }

  if ((*newHead) == NULL)
    (*newHead) = pivot;
  (*newEnd) = tail;

  return pivot;
}

static Node *quick_sort_rec(Node *head, Node *end, CompareFunc cmp) {
  if (!head || head == end)
    return head;

  Node *newHead = NULL, *newEnd = NULL;
  Node *pivot = partition(head, end, &newHead, &newEnd, cmp);

  if (newHead != pivot) {
    Node *tmp = newHead;
    while (tmp->next != pivot)
      tmp = tmp->next;
    tmp->next = NULL;

    newHead = quick_sort_rec(newHead, tmp, cmp);

    tmp = get_tail(newHead);
    tmp->next = pivot;
  }

  pivot->next = quick_sort_rec(pivot->next, newEnd, cmp);

  return newHead;
}

static void quick_sort_list(LinkedList *list, CompareFunc cmp) {
  if (!list->head)
    return;
  list->head = quick_sort_rec(list->head, get_tail(list->head), cmp);

  // Fix tail and prev pointers
  Node *curr = list->head;
  Node *prev = NULL;
  while (curr) {
    if (list->is_doubly)
      curr->prev = prev;
    prev = curr;
    curr = curr->next;
  }
  list->tail = prev;
}

// --- Shell Sort (via Array) ---
// Note: Shell sort relies heavily on random access.
// Efficient implementations on LLs are rare/complex.
// Converting to array, sorting, and rebuilding is valid O(N) overhead for
// O(N^1.5) sort.

static void shell_sort_list(LinkedList *list, CompareFunc cmp) {
  if (list->size < 2)
    return;

  // 1. Convert to Array
  void **arr = malloc(list->size * sizeof(void *));
  Node *curr = list->head;
  for (size_t i = 0; i < list->size; i++) {
    arr[i] = curr->data;
    curr = curr->next;
  }

  // 2. Shell Sort Array
  for (size_t gap = list->size / 2; gap > 0; gap /= 2) {
    for (size_t i = gap; i < list->size; i++) {
      void *temp = arr[i];
      size_t j;
      for (j = i; j >= gap && cmp(arr[j - gap], temp) > 0; j -= gap) {
        arr[j] = arr[j - gap];
      }
      arr[j] = temp;
    }
  }

  // 3. Rebuild List Data
  curr = list->head;
  for (size_t i = 0; i < list->size; i++) {
    curr->data = arr[i];
    curr = curr->next;
  }

  free(arr);
}

void list_sort(LinkedList *list, int algo_id, CompareFunc cmp) {
  if (!list || !list->head || !cmp)
    return;

  switch (algo_id) {
  case 0:
    bubble_sort_list(list, cmp);
    break;
  case 1:
    insertion_sort_nodes(list, cmp);
    break;
  case 2:
    shell_sort_list(list, cmp);
    break;
  case 3:
    quick_sort_list(list, cmp);
    break;
  default:
    bubble_sort_list(list, cmp);
    break;
  }
}
