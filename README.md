# Physical Design Automation (PDA)

This repository contains a series of programming assignments covering four representative problems in **VLSI Physical Design Automation**, including **circuit partitioning, symmetry-constrained floorplanning, standard-cell placement legalization, and rectilinear minimum spanning tree construction**.

The projects progress from hypergraph-based circuit partitioning to floorplanning under symmetry constraints, row-based placement legalization, and efficient Manhattan-distance spanning-tree construction. Through these assignments, the repository explores the application of **graph algorithms, computational geometry, data structures, heuristic optimization, and EDA tools** to physical design problems.

---

## Development and Testing Environment

* **Operating Systems:** Windows / Linux
* **Programming Language:** C++
* **Compiler:** g++
* **Build System:** GNU Make
* **EDA / Supporting Tools:** OpenROAD, Tcl, official checkers
* **Core Techniques:** Hypergraph Partitioning, FM Refinement, Multilevel Coarsening, B*-tree, Contour Packing, Simulated Annealing, Placement Legalization, Sweep Line, Kruskal's Algorithm, Disjoint Set Union

---

## Project Overview

### HW1: Balanced 3-Way Hypergraph Partitioning

This assignment models a circuit netlist as a **hypergraph**, where cells are represented as vertices and nets are represented as hyperedges.

The objective is to divide all cells into three balanced partitions while minimizing the number of nets crossing multiple partitions.

**Implemented features:**

* Built hypergraph data structures for cells, nets, and partitions
* Enforced partition-size constraints according to the specified balance factor
* Implemented multiple randomized and degree-/weight-aware initial partitioning strategies
* Applied **pairwise FM refinement** between partition pairs
* Implemented gain computation and bucket-based candidate selection
* Used **best-prefix rollback** to retain the best sequence of FM moves
* Implemented **multilevel coarsening and uncoarsening**
* Applied recursive refinement across multiple graph levels
* Added guided V-cycle and multi-start optimization to further improve cutsize
* Evaluated solution quality using the number of cut nets while preserving balance constraints

---

### HW2: Symmetry-Constrained Floorplanning

This assignment solves a floorplanning problem in which blocks must be placed without overlap while satisfying symmetry constraints.

In addition to minimizing the overall floorplan area, the placement must preserve the geometric relationships of **symmetric block pairs** and **self-symmetric blocks** with respect to their shared symmetry axes.

**Implemented features:**

* Represented block placement using a **B*-tree**
* Used a **contour structure** to efficiently determine legal block coordinates
* Supported block rotation
* Modeled symmetry groups containing symmetric pairs and self-symmetric blocks
* Constructed symmetry-aware local group layouts
* Maintained a common symmetry axis for each symmetry group
* Explored different block orders, group arrangements, rotations, and tree structures
* Applied **Simulated Annealing** for floorplan optimization
* Used multi-start and two-phase search strategies to improve solution quality
* Applied deterministic local refinement after global search
* Evaluated solutions primarily according to floorplan area and placement legality

---

### HW3: Standard Cell Placement Legalization

This assignment implements a **standard-cell legalizer** that transforms an initially illegal placement into a legal one.

The legalizer relocates cells that overlap, violate placement-site alignment, occupy blocked regions, or lie outside valid placement rows while attempting to minimize displacement from their original positions.

**Implemented features:**

* Parsed placement data extracted from OpenROAD
* Constructed row-based legal placement regions
* Managed available row space using **free-segment interval structures**
* Accounted for macros, blockages, and other placement obstacles
* Enforced site alignment and row alignment
* Prevented cell overlap and die-boundary violations
* Supported **multi-height standard cells**
* Evaluated placement candidates using Manhattan displacement
* Added a density-aware overflow penalty to discourage locally congested placement
* Searched candidate positions across multiple rows and free segments
* Rebuilt the placement when necessary to guarantee legality
* Applied conservative row-local refinement for single-height cells
* Performed final self-checks for overlap, alignment, and boundary legality
* Generated placement commands as Tcl output for OpenROAD
* Used OpenROAD and Tcl scripts to extract test data and verify placement legality

---

### HW4: Rectilinear Minimum Spanning Tree Construction

This assignment constructs a **Rectilinear Minimum Spanning Tree (RMST)** for a large set of two-dimensional pins.

The edge weight between two pins is defined by their **Manhattan distance**, and the objective is to connect all pins with minimum total spanning-tree cost.

Instead of generating all possible \(O(n^2)\) edges, the implementation uses geometric properties of Manhattan MSTs to generate only a small set of candidate edges.

**Implemented features:**

* Used Manhattan distance as the edge-weight metric
* Avoided construction of the complete \(O(n^2)\) graph
* Applied coordinate transformations to cover multiple Manhattan directions
* Used a **sweep-line algorithm** to generate useful nearest-neighbor candidate edges
* Explicitly handled duplicate pins located at identical coordinates
* Applied **Kruskal's algorithm** to construct the minimum spanning tree
* Used **Disjoint Set Union (DSU)** for efficient connectivity and cycle detection
* Implemented buffered fast input for large test cases
* Reduced memory usage by storing only candidate edges rather than all point pairs
* Optimized the implementation for large-scale input instances

---

## Repository Layout

```text
NYCU-Physical-Design-Automation-2026-SPRING/
├── README.md
├── .gitignore
└── PDA/
    ├── 2026_PDA_HW1/
    │   ├── 314512065/
    │   │   ├── main.cpp
    │   │   ├── Makefile
    │   │   └── readme.txt
    │   ├── appendix/
    │   ├── testcase files
    │   └── p1_partition_v4.pdf
    │
    ├── 2026_PDA_HW2/
    │   ├── 314512065/
    │   │   ├── Lab2.cpp
    │   │   ├── Makefile
    │   │   └── readme.txt
    │   ├── checker
    │   ├── testcase files
    │   └── p2_floorplanning.pdf
    │
    ├── 2026_PDA_HW3/
    │   ├── 314512065/
    │   │   ├── main.cpp
    │   │   └── Makefile
    │   ├── testcase/
    │   ├── flow.tcl
    │   ├── extract_v3.tcl
    │   ├── OpenROAD tutorial materials
    │   └── p3_placement_v2.pdf
    │
    └── 2026_PDA_HW4/
        ├── 314512065/
        │   ├── main.cpp
        │   ├── Makefile
        │   └── readme.txt
        ├── testcase/
        └── p4_routing.pdf
```

---

## Technical Highlights

* Implemented balanced **3-way hypergraph partitioning**
* Applied **multilevel coarsening / uncoarsening** and pairwise FM refinement
* Used gain-based bucket selection and best-prefix rollback to reduce cutsize
* Implemented **B*-tree-based floorplanning**
* Used contour packing for efficient coordinate computation
* Supported symmetric pairs and self-symmetric blocks
* Applied **Simulated Annealing** and local refinement for floorplan optimization
* Built a row-based standard-cell legalizer
* Managed placement availability through free-segment interval structures
* Supported obstacles and multi-height cells during legalization
* Minimized Manhattan displacement while considering placement density
* Integrated OpenROAD and Tcl for placement extraction and validation
* Generated Manhattan MST candidate edges using coordinate transformation and sweep-line techniques
* Constructed RMSTs using **Kruskal's algorithm and Disjoint Set Union**
* Optimized runtime, input processing, and memory usage for large test cases

---

## Large File Notice

Several large HW4 test-case files exceed GitHub's normal single-file size limit and are therefore excluded from the repository through `.gitignore`.

```text
PDA/2026_PDA_HW4/testcase/case4/input.dat
PDA/2026_PDA_HW4/testcase/case5/input.dat
PDA/2026_PDA_HW4/testcase/case6/input.dat
PDA/2026_PDA_HW4/testcase/case7/input.dat
```

---

## Author

**榮誠 邱**

Student ID: 314512065

---

## Note

This repository is maintained for academic coursework, implementation practice, and experiment record keeping.
