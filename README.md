# Physical Design Automation (PDA)

本課程實作涵蓋 VLSI 實體設計自動化流程中的四個核心問題，包含電路分割、平面規劃、標準元件合法化與繞線樹建構。

四份作業分別為：

* **HW1：Balanced 3-Way Hypergraph Partitioning**
* **HW2：Symmetry-Constrained Floorplanning**
* **HW3：Standard Cell Placement Legalization**
* **HW4：Rectilinear Minimum Spanning Tree Routing**

各份作業皆以 C++ 實作，並透過 Makefile 完成程式建置與測試。實作過程除了需要滿足題目中的合法性限制，也必須考量大型測試資料下的執行效率、記憶體使用量與解答品質。

---

## 開發與測試環境

* **開發語言：** C++ / C++17
* **編譯器：** g++
* **建置工具：** GNU Make
* **輔助工具：** Tcl、OpenROAD、官方 Checker

### 主要技術

* Hypergraph Partitioning
* Fiduccia–Mattheyses Algorithm
* Tabu Search
* B*-tree Floorplanning
* Contour-Based Packing
* Dynamic Programming
* Standard Cell Legalization
* Density-Aware Placement
* Sweep Line Algorithm
* Kruskal Minimum Spanning Tree
* Disjoint Set Union

---

# HW1：Balanced 3-Way Hypergraph Partitioning

## 作業目標

HW1 將電路 Netlist 建模為 Hypergraph，並將所有 Cells 分配至三個不同的 Partition。

在分割過程中，必須同時滿足三個 Partition 的面積平衡限制，並盡可能降低跨越不同 Partition 的 Net 數量，也就是 Cutsize。

## 問題模型

在 Hypergraph 中：

* Cell 對應到 Vertex
* Net 對應到 Hyperedge
* Cell Area 表示每個 Cell 所占用的面積
* Cut Net 表示同時連接兩個以上 Partition 的 Net
* Cutsize 表示所有 Cut Net 的總數量

最佳化目標為：

> 在符合 Balance Constraint 的前提下，最小化 Cutsize。

## 實作內容

### Hypergraph 資料結構

程式記錄每個 Cell 所連接的 Nets，以及每條 Net 所包含的 Cells。

當 Cell 從一個 Partition 移動至另一個 Partition 時，可以快速更新 Net 的 Partition 分布與目前 Cutsize。

### 三分割面積平衡

根據所有 Cells 的總面積與題目所給定的 Balance Factor，計算每個 Partition 所允許的面積上下限。

每次移動 Cell 前，都會先確認移動後是否仍符合面積限制。

### 多種初始分割策略

為了降低初始解對最終結果的影響，程式使用多種初始化方式建立不同的 Partition：

* 依 Cell Degree 排序
* 依 Cell Area 排序
* Degree 與 Area 混合排序
* 隨機排序
* 平衡式 Greedy Assignment

透過多組初始解增加搜尋多樣性。

### Pairwise FM Refinement

將三個 Partition 兩兩配對，分別執行 Fiduccia–Mattheyses Refinement。

可能的優化組合包含：

* Partition 0 與 Partition 1
* Partition 0 與 Partition 2
* Partition 1 與 Partition 2

每次 FM Pass 會計算 Cell Move 的 Gain，並優先移動能降低 Cutsize 的 Cell。

### Gain 計算

Gain 表示將某個 Cell 移至另一個 Partition 後，Cutsize 可以減少的程度。

Gain 越大，代表該移動越可能改善目前分割結果。

### Best-Prefix Rollback

FM Pass 不一定保留所有 Cell Moves。

程式會記錄每次移動後的累積 Gain，並保留累積 Gain 最大的位置。若後續移動使解答變差，則回復至最佳移動前綴。

### Tabu Search 與局部搜尋

除了 FM Refinement，程式亦加入局部搜尋機制：

* 單一 Cell Move
* Partition 間 Pair Swap
* Tabu Search
* Aspiration Criterion

Tabu Search 可暫時禁止近期已執行的移動，避免演算法在相同解答之間反覆震盪。

## 作業成果

HW1 完成了一套具備以下能力的三分割演算法：

* 滿足 Partition 面積平衡
* 降低跨區域 Net 數量
* 支援大型 Hypergraph 輸入
* 結合多起點與局部搜尋
* 透過 FM 與 Tabu Search 改善 Cutsize

## HW1 目錄

```text
PDA/2026_PDA_HW1/
├── 314512065/
│   ├── main.cpp
│   ├── Makefile
│   └── readme.txt
├── appendix/
├── case1.in
├── case2.in
├── case3.in
├── case4.in
└── p1_partition_v4.pdf
```

---

# HW2：Symmetry-Constrained Floorplanning

## 作業目標

HW2 處理具有對稱限制的 Floorplanning 問題。

程式需要將所有 Blocks 放置於二維平面上，在避免 Block 重疊的同時，最小化 Floorplan 面積，並滿足題目指定的對稱群組限制。

對稱限制可能包含：

* Symmetric Block Pair
* Self-Symmetric Block
* 共同垂直對稱軸
* 共同水平對稱軸

## 問題模型

每個 Block 具有：

* Block Name
* Width
* Height
* Rotation State
* 最終放置座標

最佳化目標主要包含：

* 最小化 Floorplan Width
* 最小化 Floorplan Height
* 最小化總面積
* 滿足所有 Symmetry Constraints
* 避免 Blocks 互相重疊

## 實作內容

### B*-tree 表示法

程式使用 B*-tree 表示 Blocks 之間的相對位置。

在 B*-tree 中：

* Left Child 通常表示放置在父節點右側的 Block
* Right Child 通常表示放置在父節點上方的 Block

透過修改 B*-tree 結構，可以探索不同的 Floorplan 排列。

### Contour-Based Packing

使用 Contour Data Structure 維護目前 Floorplan 的上方輪廓。

當新的 Block 被放入 Floorplan 時，程式會查詢對應 X 範圍的最高 Contour，決定 Block 的合法 Y 座標。

Contour 方法可以避免逐一檢查所有已放置 Blocks，提高 Packing 效率。

### Block Rotation

程式允許部分 Blocks 旋轉。

旋轉後：

```text
Width ↔ Height
```

透過選擇不同的旋轉狀態，可以調整 Floorplan 的 Aspect Ratio 並降低面積。

### Symmetry Group 建模

將具有對稱關係的 Blocks 組合成 Symmetry Group。

Symmetric Pair 需要位於共同對稱軸兩側，並保持相同的相對距離。

Self-Symmetric Block 則需要放置於對稱軸上。

### 對稱群組內部排列

程式針對每個 Symmetry Group 建立多種合法內部排列方式，例如：

* 左右對稱排列
* 上下對稱排列
* Block 旋轉組合
* Self-Symmetric Block 位於中心
* 多組 Symmetric Pair 的排列順序

### Dynamic Programming

針對對稱群組內部的候選配置，使用 Dynamic Programming 保留較佳狀態。

狀態可能包含：

* 目前寬度
* 目前高度
* 對稱軸位置
* Block 排列方式
* Rotation Combination

### Dominance Pruning

若某個候選配置的 Width 與 Height 均不優於另一個候選配置，則該狀態不可能形成更好的解答，可以直接移除。

此方法可降低 Dynamic Programming 所需保留的狀態數量。

### 多種初始 B*-tree

程式建立多種初始 Floorplan 結構，包括：

* Left Chain
* Right Chain
* Alternating Chain
* 依 Width 排序
* 依 Height 排序
* 依 Area 排序
* Random Tree

透過不同初始結構增加搜尋空間。

## 作業成果

HW2 完成了一套支援對稱限制的 Floorplanning 方法，具備以下能力：

* 使用 B*-tree 表示 Floorplan
* 使用 Contour 快速進行 Packing
* 支援 Block Rotation
* 支援 Symmetric Pair
* 支援 Self-Symmetric Block
* 透過 Dynamic Programming 搜尋群組配置
* 透過 Dominance Pruning 降低搜尋量

## HW2 目錄

```text
PDA/2026_PDA_HW2/
├── 314512065/
│   ├── Lab2.cpp
│   ├── Makefile
│   └── readme.txt
├── checker
├── block_*.in
├── input_*.in
└── p2_floorplanning.pdf
```

---

# HW3：Standard Cell Placement Legalization

## 作業目標

HW3 實作 Standard Cell Placement Legalizer。

輸入的 Placement 中，Cells 可能出現以下問題：

* Cells 互相重疊
* Cell 未對齊 Placement Site
* Cell 超出 Die Boundary
* Cell 位於不合法的 Row
* Multi-Height Cell 未正確跨越多條 Rows
* 區域密度過高

Legalizer 需要將 Cells 移動到合法位置，同時盡量降低 Cells 相對於原始座標的位移量。

## 問題模型

每個 Standard Cell 具有：

* 原始 X 座標
* 原始 Y 座標
* Width
* Height
* 可放置 Row
* 合法化後座標

合法化後需滿足：

* Cell 不互相重疊
* Cell 位於 Die Boundary 內
* X 座標對齊 Placement Site
* Y 座標對齊 Placement Row
* Multi-Height Cell 跨越合法且連續的 Rows

## 實作內容

### Row-Based Legalization

程式根據 Placement Rows 建立合法放置區域。

每條 Row 會記錄：

* Row 起始座標
* Row 寬度
* Row 高度
* Site Width
* 可使用區間
* 已占用區間

Cells 會被分配至最接近其原始位置的合法 Row。

### Free Segment Management

每條 Row 使用 Free Segments 表示尚未被 Cells 占用的空間。

當 Cell 被放置後，程式會將對應 Free Segment 分割成剩餘的左右區間。

藉由 Free Segment 管理，可以快速判斷 Cell 是否能放入某條 Row。

### Site Alignment

Cell 的 X 座標必須對齊 Site Grid。

若原始 X 座標未對齊 Site，程式會將其調整至最近的合法 Site Position。

### Row Alignment

Cell 的 Y 座標必須位於合法 Placement Row 上。

程式會選擇距離原始 Y 座標較近，且具有足夠空間的 Row。

### Multi-Height Cell Legalization

部分 Cells 的高度可能大於單一 Row Height。

此類 Cells 需要同時占用多條連續 Rows。

放置前，程式會確認所有被覆蓋的 Rows 在相同 X 區間內均有足夠空間。

### Displacement Cost

Legalization 的主要成本為 Cell 原始位置與合法位置之間的 Manhattan Distance：

```text
Displacement =
|x_legal - x_original| +
|y_legal - y_original|
```

程式會優先選擇位移較小的合法位置。

### Density Grid

將 Placement Region 分割為多個 Density Bins，統計各區域中的 Cell Area。

若某個區域的 Cell Density 過高，則增加 Density Penalty，降低其他 Cells 繼續被放置到該區域的可能性。

### Candidate Position Search

對每個 Cell，程式會搜尋多個候選位置，包括：

* 原始 Row 附近
* 上方 Rows
* 下方 Rows
* 原始 X 座標附近的 Free Segment
* 鄰近合法 Sites

再根據 Displacement、Density 與合法性選出較佳位置。

### Final Legality Repair

主要 Legalization 完成後，程式會重新檢查所有 Cells。

若仍存在不合法 Cells，則進行額外修復，包括：

* 重新尋找合法 Row
* 重新計算 Free Segment
* 移動重疊 Cell
* 修正 Site Alignment
* 修正 Die Boundary Violation

## 作業成果

HW3 完成了一套 Row-Based Standard Cell Legalizer，具備以下能力：

* 修正 Cell Overlap
* 修正 Site Alignment
* 修正 Row Alignment
* 支援 Multi-Height Cells
* 降低 Cell Displacement
* 考量區域 Density
* 執行最終合法性檢查與修復

## HW3 目錄

```text
PDA/2026_PDA_HW3/
├── 314512065/
│   ├── main.cpp
│   └── Makefile
├── testcase/
├── extract_v3.tcl
├── flow.tcl
├── openroad tutorial.pptx
└── p3_placement_v2.pdf
```

---

# HW4：Rectilinear Minimum Spanning Tree Routing

## 作業目標

HW4 針對大量二維 Pins 建立 Rectilinear Minimum Spanning Tree，簡稱 RMST。

所有 Pins 必須被連接成一棵 Tree，且任意兩點之間的 Edge Weight 使用 Manhattan Distance。

最佳化目標是在連接所有 Pins 的前提下，最小化 Tree 的總線長。

## 問題模型

每個 Pin 具有二維座標：

```text
(x, y)
```

兩個 Pins 之間的 Manhattan Distance 為：

```text
Distance =
|x1 - x2| + |y1 - y2|
```

若共有 `n` 個 Pins，最終 Minimum Spanning Tree 需包含：

```text
n - 1 條 Edges
```

## 實作內容

### Manhattan Distance

所有 Edge Weight 皆以 Manhattan Distance 計算。

此距離模型符合 VLSI Routing 中只能進行水平與垂直走線的特性。

### 避免建立所有點對

若直接建立所有 Pins 之間的 Edges，Edge 數量為：

```text
O(n²)
```

當 Pin 數量很大時，會造成極高的時間與記憶體成本。

因此程式只建立可能出現在 Manhattan MST 中的 Candidate Edges。

### Four-Transformation Sweep

為了涵蓋不同方向上的鄰近點，程式對所有座標執行多次轉換，例如：

* 原始座標
* X 與 Y 交換
* X 軸鏡射
* 座標交換後鏡射

每次轉換後執行一次 Sweep Line Candidate Search。

### Sweep Line Algorithm

將 Pins 依特定座標順序排序，依序掃描所有 Pins。

在 Sweep 過程中，只需檢查幾何上可能成為最近鄰居的 Active Points，不需比較所有 Pin Pairs。

### Active Set

Active Set 維護目前 Sweep Line 附近的候選 Pins。

使用有序資料結構查詢符合條件的鄰近點，並產生 Candidate Edge。

### Candidate Edge Reduction

透過 Manhattan Geometry 的性質，每個 Pin 只需要連接少量候選鄰居。

此方法可將 Edge 數量由 `O(n²)` 大幅降低至接近 `O(n)`。

### Duplicate Points

若多個 Pins 位於完全相同的座標，Pins 之間的距離為零。

程式會額外加入 Weight 為 0 的 Edge，確保重複座標的 Pins 仍能被正確連接。

### Kruskal Algorithm

建立所有 Candidate Edges 後，依 Edge Weight 由小到大排序。

依序選擇不會形成 Cycle 的 Edge，直到所有 Pins 被連接。

### Disjoint Set Union

使用 Disjoint Set Union 判斷兩個 Pins 是否已位於相同 Connected Component。

主要操作包含：

* Find
* Union
* Path Compression
* Union by Rank

此資料結構可以快速判斷加入 Edge 是否會形成 Cycle。

### 大型測試資料處理

為了處理大量座標輸入，程式使用高效率輸入方式讀取資料。

主要時間成本集中於：

* Pin Sorting
* Candidate Edge Sorting
* Kruskal MST Construction

相較於完整圖的 `O(n²)` Edge 建立方式，可大幅降低執行時間與記憶體需求。

## 作業成果

HW4 完成了一套可處理大型輸入的 RMST 建構方法，具備以下能力：

* 使用 Manhattan Distance
* 使用 Sweep Line 建立 Candidate Edges
* 透過四方向座標轉換涵蓋鄰近關係
* 支援 Duplicate Pins
* 使用 Kruskal 建立 MST
* 使用 Disjoint Set Union 避免 Cycle
* 避免建立 `O(n²)` 完整圖

## HW4 目錄

```text
PDA/2026_PDA_HW4/
├── 314512065/
│   ├── main.cpp
│   ├── Makefile
│   └── readme.txt
├── testcase/
└── p4_routing.pdf
```

---

# 建置與執行方式

## HW1

```bash
cd PDA/2026_PDA_HW1/314512065
make
./Lab1 [input file] [output file]
```

## HW2

```bash
cd PDA/2026_PDA_HW2/314512065
make
./Lab2 [input file] [output file]
```

## HW3

```bash
cd PDA/2026_PDA_HW3/314512065
make
./Legalizer [input file] [output file]
```

## HW4

```bash
cd PDA/2026_PDA_HW4/314512065
make
./RMST [input file] [output file]
```

清除編譯結果：

```bash
make clean
```

---

# 專案目錄結構

```text
NYCU-Physical-Design-Automation-2026-SPRING/
├── README.md
├── .gitignore
└── PDA/
    ├── 2026_PDA_HW1/
    │   ├── 314512065/
    │   ├── appendix/
    │   ├── testcase files
    │   └── p1_partition_v4.pdf
    │
    ├── 2026_PDA_HW2/
    │   ├── 314512065/
    │   ├── checker
    │   ├── testcase files
    │   └── p2_floorplanning.pdf
    │
    ├── 2026_PDA_HW3/
    │   ├── 314512065/
    │   ├── testcase/
    │   ├── OpenROAD scripts
    │   └── p3_placement_v2.pdf
    │
    └── 2026_PDA_HW4/
        ├── 314512065/
        ├── testcase/
        └── p4_routing.pdf
```

---

# 大型測試資料說明

HW4 部分測試輸入檔案因超過或接近 GitHub 的一般單檔大小限制，因此未上傳至 Repository。

已透過 `.gitignore` 排除的檔案包含：

```text
PDA/2026_PDA_HW4/testcase/case4/input.dat
PDA/2026_PDA_HW4/testcase/case5/input.dat
PDA/2026_PDA_HW4/testcase/case6/input.dat
PDA/2026_PDA_HW4/testcase/case7/input.dat
```

這些大型測試檔案仍保留在本機環境，不影響原始碼、Makefile 與其他測試資料的使用。

---

# 課程成果

透過四份程式作業，完整練習 VLSI Physical Design Automation 中不同階段的最佳化問題。

## HW1

將大型電路 Netlist 分割成面積平衡的三個區域，並降低跨區域連線。

## HW2

在滿足 Block 對稱限制的情況下，建立面積較小的 Floorplan。

## HW3

將存在重疊或未對齊問題的 Standard Cells 移動至合法 Placement 位置。

## HW4

以高效率 Candidate Edge Generation 與 Kruskal Algorithm 建立 Rectilinear Minimum Spanning Tree。

實作過程涵蓋圖論、幾何演算法、資料結構、啟發式搜尋與組合最佳化，並需要在合法性、執行時間、記憶體使用量及解答品質之間取得平衡。

---

# 注意事項

本 Repository 為 Physical Design Automation 課程的個人實作與學習紀錄。

程式碼與作業內容僅供學術交流及個人作品展示。使用者應遵守所屬課程的學術誠信規範，不應直接複製或提交其中內容作為自己的課程作業。
