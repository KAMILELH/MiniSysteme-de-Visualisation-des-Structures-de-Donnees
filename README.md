# MiniSysteme-de-Visualisation-des-Structures-de-Donnees
# Data Structures & Algorithms Visualizer

A comprehensive C application using GTK+ 3 to visualize and interact with various core computer science data structures and algorithms. This project provides an interactive graphical interface for sorting arrays, managing linked lists, manipulating trees, and traversing graphs.

## Features

The application is divided into four main modules accessible via tabs:

### 1. Arrays & Sorting (Tableaux & Tri)
- **Visual Sorting**: Watch sorting algorithms in action.
- **Algorithms Implemented**:
  - Bubble Sort
  - Insertion Sort
  - Shell Sort
  - Quick Sort
- **Performance Analysis**: Benchmark different algorithms.

### 2. Linked Lists (Listes Chaînées)
- **Interactive Management**: Create and modify Singly and Doubly linked lists.
- **Visual Operations**:
  - Insert (Head, Tail, Specific Position)
  - Delete nodes
  - Sort the list
- **Visualization**: Graphic representation of nodes and pointers.

### 3. Trees (Arbres)
- **Structures**:
  - Binary Search Trees (BST)
  - N-ary Trees
- **Operations**:
  - Insertion and Deletion
  - Tree Balance metrics
- **Traversals**:
  - Pre-order (Préordre)
  - In-order (Inordre)
  - Post-order (Postordre)
  - BFS (Breadth-First Search)
- **Conversion**: Tools to transform N-ary trees to Binary trees.

### 4. Graphs (Graphes)
- **Graph Construction**:
  - Create Nodes by clicking
  - specific Edges (Directed/Undirected)
  - Dynamic Adjacency Matrix
- **Algorithms**:
  - Dijkstra's Algorithm
  - Bellman-Ford
  - Floyd-Warshall (All-pairs shortest path)

## Prerequisites

To build and run this project, you need:

- **GCC** (GNU Compiler Collection)
- **Make**
- **GTK+ 3.0** development libraries (`libgtk-3-dev` on Debian/Ubuntu, `gtk3` on MSYS2/Windows)
- **pkg-config**

## Build Instructions

### Linux / Windows (MSYS2)

1.  Clone the repository and navigate to the project folder:
    ```bash
    git clone https://github.com/your-username/sorter-project.git
    cd sorter-project
    ```

2.  Build the project using Make:
    ```bash
    make
    ```

3.  Run the application:
    ```bash
    ./sorter.exe
    ```

### Using Code::Blocks
Open `SorterProject.cbp` in Code::Blocks and click "Build and Run". Ensure your global compiler settings have the correct GTK+ 3 include and linker paths set up.

## Project Structure

```
SorterProject/
├── src/
│   ├── backend/        # Implementation of algorithms (C logic)
│   ├── gui/            # GTK+ interface code
│   └── main.c          # Entry point
├── include/            # Header files
├── style.css           # UI Styling
├── Makefile            # Build script
└── SorterProject.cbp   # Code::Blocks project file
```

## Styling
The application interface is styled using `style.css`. Ensure this file is present in the working directory when running the executable to load the custom visual themes.
