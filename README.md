# Physical Design Automation (PDA)

本課程實作涵蓋 VLSI 實體設計自動化中的四個核心階段，包含 **電路分割、平面規劃、標準元件合法化與繞線樹建構**。

整體專案從 Hypergraph Partitioning 開始，逐步實作具對稱限制的 Floorplanning、Standard Cell Placement Legalization，最後以 Manhattan Distance 建立 Rectilinear Minimum Spanning Tree，練習圖論、幾何演算法、資料結構與啟發式最佳化方法在 EDA 問題中的應用。

---

## 開發與測試環境

* **作業系統：** Windows / Linux
* **開發語言：** C++
* **編譯器：** g++
* **建置工具：** GNU Make
* **輔助工具：** OpenROAD、Tcl、官方 Checker
* **核心技術：** Hypergraph Partitioning、FM Algorithm、B*-tree、Contour Packing、Placement Legalization、Sweep Line、Kruskal MST、Disjoint Set Union

---

## 專案導覽（Project Overview）

### HW1：Balanced 3-Way Hypergraph Partitioning

本作業將電路 Netlist 建模為 Hypergraph，並將 Cells 分配至三個 Partition。

系統需在滿足各 Partition 面積平衡限制的條件下，盡可能降低跨越不同 Partition 的 Cut Nets 數量。

**實作內容：**

* 建立 Cell、Net 與 Partition 的 Hypergraph 資料結構
* 根據 Balance Factor 控制各區域面積上下限
* 使用多種初始分割策略產生不同初始解
* 使用 Pairwise FM Refinement 改善 Cutsize
* 透過 Gain Calculation 與 Best-Prefix Rollback 保留較佳移動結果
* 搭配局部搜尋提升分割品質

---

### HW2：Symmetry-Constrained Floorplanning

本作業處理具有對稱限制的 Floorplanning 問題。

除了需要避免 Blocks 重疊並降低 Floorplan 面積，也必須滿足 Symmetric Block Pair 與 Self-Symmetric Block 的配置限制。

**實作內容：**

* 使用 B*-tree 表示 Blocks 之間的相對位置
* 使用 Contour Structure 快速計算合法放置座標
* 支援 Block Rotation
* 建立 Symmetry Group 與共同對稱軸
* 搜尋不同的群組排列與旋轉組合
* 以 Floorplan Area 與合法性作為主要評估標準

---

### HW3：Standard Cell Placement Legalization

本作業實作 Standard Cell Legalizer，將存在重疊、未對齊或超出合法範圍的 Cells 移動至合法位置。

目標是在滿足 Placement Rules 的前提下，盡可能降低 Cells 相對於原始位置的位移量。

**實作內容：**

* 根據 Placement Rows 建立合法放置區域
* 使用 Free Segments 管理各 Row 的可用空間
* 修正 Cell Overlap、Site Alignment 與 Row Alignment
* 支援 Multi-Height Cells
* 以 Manhattan Displacement 評估移動成本
* 執行最終合法性檢查與位置修復
* 使用 OpenROAD 與 Tcl Script 產生及驗證測試資料

---

### HW4：Rectilinear Minimum Spanning Tree Routing

本作業針對大量二維 Pins 建立 Rectilinear Minimum Spanning Tree。

任兩點之間的 Edge Weight 使用 Manhattan Distance，目標是在連接所有 Pins 的情況下，最小化整棵 Tree 的總線長。

**實作內容：**

* 使用 Manhattan Distance 計算 Edge Weight
* 避免建立完整的 `O(n²)` 點對圖
* 透過座標轉換與 Sweep Line 產生 Candidate Edges
* 處理具有相同座標的 Duplicate Pins
* 使用 Kruskal Algorithm 建立 Minimum Spanning Tree
* 使用 Disjoint Set Union 進行 Cycle Detection
* 優化大型 Testcase 的輸入與記憶體使用效率

---

## 專案目錄結構（Repository Layout）

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
    │   ├── OpenROAD scripts
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

## 技術重點整理

* 實作 Balanced 3-Way Hypergraph Partitioning
* 使用 FM Refinement 降低 Cutsize
* 使用 B*-tree 與 Contour 完成 Floorplanning
* 處理 Symmetric Pair 與 Self-Symmetric Block
* 建立 Row-Based Standard Cell Legalizer
* 修正 Cell Overlap、Site Alignment 與 Placement Boundary
* 使用 Sweep Line 降低 RMST Candidate Edge 數量
* 使用 Kruskal 與 Disjoint Set Union 建立 Minimum Spanning Tree
* 針對大型測試資料進行時間與記憶體最佳化

---

## Large File Notice

HW4 部分大型 Testcase 超過 GitHub 的一般單檔大小限制，因此未上傳至 Repository，並已加入 `.gitignore`。

```text
PDA/2026_PDA_HW4/testcase/case4/input.dat
PDA/2026_PDA_HW4/testcase/case5/input.dat
PDA/2026_PDA_HW4/testcase/case6/input.dat
PDA/2026_PDA_HW4/testcase/case7/input.dat
```

---

## Author

**榮誠 邱**
Student ID：314512065

---

## Note

This repository is for academic coursework and experiment record keeping.
