#include "backend.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

Graph *graph_init(int capacity) {
  Graph *g = malloc(sizeof(Graph));
  g->capacity = capacity;
  g->count = 0;
  g->nodes = malloc(capacity * sizeof(GraphNode *));
  g->matrix = malloc(capacity * sizeof(float *));
  for (int i = 0; i < capacity; i++) {
    g->matrix[i] = malloc(capacity * sizeof(float));
    for (int j = 0; j < capacity; j++) {
      g->matrix[i][j] = (i == j) ? 0 : INF;
    }
  }
  return g;
}

void graph_free(Graph *g) {
  if (!g)
    return;
  for (int i = 0; i < g->capacity; i++)
    free(g->matrix[i]);
  free(g->matrix);
  for (int i = 0; i < g->count; i++)
    free(g->nodes[i]);
  free(g->nodes);
  free(g);
}

int graph_add_node(Graph *g, void *data) {
  if (g->count >= g->capacity)
    return -1;
  GraphNode *n = malloc(sizeof(GraphNode));
  n->id = g->count;
  n->data = data;
  n->x = 0;
  n->y = 0;
  g->nodes[g->count] = n;
  g->count++;
  return n->id;
}

void graph_add_edge(Graph *g, int src, int dest, float weight) {
  if (src < 0 || src >= g->count || dest < 0 || dest >= g->count)
    return;
  g->matrix[src][dest] = weight;
  // For directed graphs, we only set one way.
  // If undirected is needed, user calls both ways?
  // User asked for "L'arête doit être orientée (flèche)", so directed.
}

void graph_remove_node(Graph *g, int id) {
  if (id < 0 || id >= g->count)
    return;

  // 1. Free node
  free(g->nodes[id]);

  // 2. Shift nodes array
  for (int i = id; i < g->count - 1; i++) {
    g->nodes[i] = g->nodes[i + 1];
    g->nodes[i]->id = i; // Update ID
  }

  // 3. Shift Matrix Rows
  free(g->matrix[id]); // Free row
  for (int i = id; i < g->count - 1; i++) {
    g->matrix[i] = g->matrix[i + 1];
  }
  // Allocate new last row (empty) to maintain capacity or just leave pointer?
  // Better to just shift logical count and leave garbage at end?
  // But wait, we allocated capacity rows.
  // Actually, we must Re-allocate or shift.
  // The matrix is [capacity][capacity].
  // Removing node 'id' means removing Row 'id' AND Column 'id'.
  // This is expensive with current [capacity][capacity] strict alloc.
  // Simpler approach: Just shift data in the matrix without reallocs.

  // Shift Rows Up
  for (int i = id; i < g->count - 1; i++) {
    for (int j = 0; j < g->capacity; j++) { // Copy entire row content
      g->matrix[i][j] = g->matrix[i + 1][j];
    }
  }
  // Reset last logical row (now garbage)
  for (int j = 0; j < g->capacity; j++)
    g->matrix[g->count - 1][j] = INF;

  // Shift Cols Left (in every row)
  for (int i = 0; i < g->capacity; i++) {
    for (int j = id; j < g->count - 1; j++) {
      g->matrix[i][j] = g->matrix[i][j + 1];
    }
    // Reset last logical col
    g->matrix[i][g->count - 1] = INF;
  }

  g->count--;
}

// --- Algorithms ---

// Helper: Find min dist node not visited
static int min_dist(float *dist, int *visited, int n) {
  float min = INF;
  int min_index = -1;
  for (int i = 0; i < n; i++) {
    if (!visited[i] && dist[i] <= min) {
      min = dist[i];
      min_index = i;
    }
  }
  return min_index;
}

void algo_dijkstra(Graph *g, int start_node, float **dist_out, int **prev_out) {
  int n = g->count;
  float *dist = malloc(n * sizeof(float));
  int *prev = malloc(n * sizeof(int));
  int *visited = calloc(n, sizeof(int));

  for (int i = 0; i < n; i++) {
    dist[i] = INF;
    prev[i] = -1;
  }
  dist[start_node] = 0;

  for (int count = 0; count < n - 1; count++) {
    int u = min_dist(dist, visited, n);
    if (u == -1)
      break;
    visited[u] = 1;

    for (int v = 0; v < n; v++) {
      if (!visited[v] && g->matrix[u][v] != INF && dist[u] != INF &&
          dist[u] + g->matrix[u][v] < dist[v]) {
        dist[v] = dist[u] + g->matrix[u][v];
        prev[v] = u;
      }
    }
  }
  free(visited);
  *dist_out = dist;
  *prev_out = prev;
}

void algo_bellman(Graph *g, int start_node, float **dist_out, int **prev_out) {
  int n = g->count;
  float *dist = malloc(n * sizeof(float));
  int *prev = malloc(n * sizeof(int));

  for (int i = 0; i < n; i++) {
    dist[i] = INF;
    prev[i] = -1;
  }
  dist[start_node] = 0;

  // Relax n-1 times
  for (int i = 0; i < n - 1; i++) {
    for (int u = 0; u < n; u++) {
      for (int v = 0; v < n; v++) {
        if (g->matrix[u][v] != INF && dist[u] != INF &&
            dist[u] + g->matrix[u][v] < dist[v]) {
          dist[v] = dist[u] + g->matrix[u][v];
          prev[v] = u;
        }
      }
    }
  }
  // Check neg cycle (optional, not requested explicitly but good practice)

  *dist_out = dist;
  *prev_out = prev;
}

void algo_floyd(Graph *g, float **dist_matrix_out, int **next_matrix_out) {
  int n = g->count;
  float *dist = malloc(n * n * sizeof(float)); // Flattened
  int *next = malloc(n * n * sizeof(int));

  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      dist[i * n + j] = g->matrix[i][j];
      if (g->matrix[i][j] != INF && i != j)
        next[i * n + j] = j;
      else
        next[i * n + j] = -1;
    }
  }

  for (int k = 0; k < n; k++) {
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        if (dist[i * n + k] != INF && dist[k * n + j] != INF &&
            dist[i * n + k] + dist[k * n + j] < dist[i * n + j]) {
          dist[i * n + j] = dist[i * n + k] + dist[k * n + j];
          next[i * n + j] = next[i * n + k];
        }
      }
    }
  }

  *dist_matrix_out = dist;
  *next_matrix_out = next;
}
