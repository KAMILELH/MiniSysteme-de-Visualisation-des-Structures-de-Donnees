#include "backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Helper for swapping
static void swap(int *a, int *b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

// --- 1. Bubble Sort ---
void sort_bubble(int *arr, size_t n, SortStats *stats) {
  stats->comparisons = 0;
  stats->assignments = 0;

  for (size_t i = 0; i < n - 1; i++) {
    for (size_t j = 0; j < n - i - 1; j++) {
      stats->comparisons++;
      if (arr[j] > arr[j + 1]) {
        stats->assignments++; // Counting the logical swap action
        swap(&arr[j], &arr[j + 1]);
      }
    }
  }
}

// --- 2. Insertion Sort ---
void sort_insertion(int *arr, size_t n, SortStats *stats) {
  stats->comparisons = 0;
  stats->assignments = 0;

  for (size_t i = 1; i < n; i++) {
    int key = arr[i];
    int j = i - 1;

    // logic: while (j >= 0 && arr[j] > key)
    while (j >= 0) {
      stats->comparisons++;
      if (arr[j] > key) {
        stats->assignments++;
        arr[j + 1] = arr[j];
        j = j - 1;
      } else {
        break;
      }
    }
    arr[j + 1] = key;
  }
}

// --- 3. Shell Sort ---
void sort_shell(int *arr, size_t n, SortStats *stats) {
  stats->comparisons = 0;
  stats->assignments = 0;

  for (size_t gap = n / 2; gap > 0; gap /= 2) {
    for (size_t i = gap; i < n; i++) {
      int temp = arr[i];
      size_t j;
      for (j = i; j >= gap; j -= gap) {
        stats->comparisons++;
        if (arr[j - gap] > temp) {
          stats->assignments++;
          arr[j] = arr[j - gap];
        } else {
          break;
        }
      }
      arr[j] = temp;
    }
  }
}

// --- 4. Quick Sort ---
static size_t partition(int *arr, int low, int high, SortStats *stats) {
  int pivot = arr[high];
  int i = (low - 1);

  for (int j = low; j <= high - 1; j++) {
    stats->comparisons++;
    if (arr[j] <
        pivot) { // Changed condition to match standard quicksort (ascending)
      i++;
      stats->assignments++;
      swap(&arr[i], &arr[j]);
    }
  }
  stats->assignments++;
  swap(&arr[i + 1], &arr[high]);
  return (i + 1);
}

static void quick_recursive(int *arr, int low, int high, SortStats *stats) {
  if (low < high) {
    size_t pi = partition(arr, low, high, stats);
    // Be careful with unsigned size_t valid range vs int indices
    // Since we pass int low/high, we cast pi back to int
    quick_recursive(arr, low, (int)pi - 1, stats);
    quick_recursive(arr, (int)pi + 1, high, stats);
  }
}

void sort_quick(int *arr, size_t n, SortStats *stats) {
  stats->comparisons = 0;
  stats->assignments = 0;
  if (n > 0) {
    quick_recursive(arr, 0, (int)n - 1, stats);
  }
}

void generate_array(int *arr, size_t n, int type) {
  if (type == 0) { // Random
    for (size_t i = 0; i < n; i++) {
      arr[i] = rand() % 100; // 0-99
    }
  } else if (type == 1) { // Sorted
    for (size_t i = 0; i < n; i++) {
      arr[i] = (int)i;
    }
  } else if (type == 2) { // Reversed
    for (size_t i = 0; i < n; i++) {
      arr[i] = (int)(n - 1 - i);
    }
  }
}