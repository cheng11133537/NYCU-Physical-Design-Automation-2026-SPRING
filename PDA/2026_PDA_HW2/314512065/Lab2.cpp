#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <cmath>
#include <ctime>
#include <cctype>
#include <numeric>
#include <future>
#include <atomic>
#include <chrono>

using namespace std;

const auto gProgramStartTime = chrono::steady_clock::now();

enum class SearchMode {
    Quality,
};

SearchMode gSearchMode = SearchMode::Quality;
int gQualityBudgetMinutes = 58;
bool gQualityBudgetExplicit = false;

bool hasExplicitQualityBudget() {
    return gQualityBudgetExplicit;
}

int effectiveQualityBudgetMinutes() {
    // Hidden tests are at most 1000 blocks, so enforce a hard 58-minute cap.
    return min(gQualityBudgetMinutes, 58);
}

/* =========================================================
 * Basic data
 * ========================================================= */

struct Block {
    string name;
    int w = 0, h = 0;
    bool rot = false;
    int x = 0, y = 0;

    int width() const  { return rot ? h : w; }
    int height() const { return rot ? w : h; }
};

struct SymPair {
    int a = -1;
    int b = -1;
};

struct SymGroup {
    vector<SymPair> pairs;
    vector<int> selfs;
    int axis_x2 = 0;
};

vector<Block> baseBlocks;
vector<SymGroup> baseGroups;
unordered_map<string, int> name2id;
thread_local vector<Block> blocks;
thread_local vector<SymGroup> groups;
vector<double> finalOutX;
vector<double> finalOutY;
bool useFinalOutCoords = false;

/* =========================================================
 * Random helpers
 * ========================================================= */

static atomic<unsigned long long> rngSeedCounter{1};

unsigned makeThreadSeed() {
    unsigned long long base =
        static_cast<unsigned long long>(time(nullptr)) ^
        (rngSeedCounter.fetch_add(1, memory_order_relaxed) * 0x9e3779b97f4a7c15ULL);
    return static_cast<unsigned>(base ^ (base >> 32));
}

thread_local mt19937 rng(makeThreadSeed());

int randInt(int l, int r) {
    uniform_int_distribution<int> dist(l, r);
    return dist(rng);
}

double rand01() {
    uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

/* =========================================================
 * String helpers
 * ========================================================= */

string trim(const string& s) {
    size_t l = 0;
    while (l < s.size() && isspace((unsigned char)s[l])) l++;
    size_t r = s.size();
    while (r > l && isspace((unsigned char)s[r - 1])) r--;
    return s.substr(l, r - l);
}

vector<string> splitTokens(const string& line) {
    vector<string> tokens;
    stringstream ss(line);
    string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

bool startsWith(const string& s, const string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

/* =========================================================
 * Input
 * ========================================================= */

bool readInput(const string& filename) {
    ifstream fin(filename);
    if (!fin) {
        cerr << "Error: cannot open input file: " << filename << "\n";
        return false;
    }

    baseBlocks.clear();
    baseGroups.clear();
    name2id.clear();

    string line;
    int numBlocks = -1;

    while (getline(fin, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (startsWith(line, "NumBlocks:")) {
            auto pos = line.find(':');
            numBlocks = stoi(trim(line.substr(pos + 1)));
            break;
        }
    }

    if (numBlocks <= 0) {
        cerr << "Error: invalid NumBlocks\n";
        return false;
    }

    for (int i = 0; i < numBlocks; ) {
        if (!getline(fin, line)) {
            cerr << "Error: unexpected EOF while reading blocks\n";
            return false;
        }
        line = trim(line);
        if (line.empty()) continue;

        auto tok = splitTokens(line);
        if (tok.size() != 3) {
            cerr << "Error: invalid block line: " << line << "\n";
            return false;
        }

        Block b;
        b.name = tok[0];
        b.w = stoi(tok[1]);
        b.h = stoi(tok[2]);

        name2id[b.name] = (int)baseBlocks.size();
        baseBlocks.push_back(b);
        i++;
    }

    SymGroup currentGroup;
    bool inGroup = false;

    while (getline(fin, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line == "Symmetry Group") {
            if (inGroup) {
                baseGroups.push_back(currentGroup);
                currentGroup = SymGroup();
            }
            inGroup = true;
            continue;
        }

        if (!inGroup) continue;

        auto tok = splitTokens(line);
        if (tok.size() == 2) {
            if (!name2id.count(tok[0]) || !name2id.count(tok[1])) {
                cerr << "Error: unknown block in symmetry pair: " << line << "\n";
                return false;
            }
            SymPair p;
            p.a = name2id[tok[0]];
            p.b = name2id[tok[1]];
            currentGroup.pairs.push_back(p);
        } else if (tok.size() == 1) {
            if (!name2id.count(tok[0])) {
                cerr << "Error: unknown self-symmetry block: " << line << "\n";
                return false;
            }
            currentGroup.selfs.push_back(name2id[tok[0]]);
        } else {
            cerr << "Error: invalid symmetry line: " << line << "\n";
            return false;
        }
    }

    if (inGroup) baseGroups.push_back(currentGroup);
    return true;
}

/* =========================================================
 * Geometry
 * ========================================================= */

int doubledCenterX(const Block& b) {
    return 2 * b.x + b.width();
}

bool overlap(const Block& a, const Block& b) {
    if (a.x + a.width() <= b.x) return false;
    if (b.x + b.width() <= a.x) return false;
    if (a.y + a.height() <= b.y) return false;
    if (b.y + b.height() <= a.y) return false;
    return true;
}

bool checkNoOverlap() {
    int n = (int)blocks.size();
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (overlap(blocks[i], blocks[j])) return false;
        }
    }
    return true;
}

int chipWidth() {
    int mx = 0;
    for (const auto& b : blocks) mx = max(mx, b.x + b.width());
    return mx;
}

int chipHeight() {
    int mx = 0;
    for (const auto& b : blocks) mx = max(mx, b.y + b.height());
    return mx;
}

int chipArea() {
    return chipWidth() * chipHeight();
}

/* =========================================================
 * Symmetry legality
 * ========================================================= */

bool checkPairSymmetry(const SymPair& p, int axis2) {
    const Block& a = blocks[p.a];
    const Block& b = blocks[p.b];

    if (a.width() != b.width() || a.height() != b.height()) return false;
    if (a.y != b.y) return false;

    int ca = doubledCenterX(a);
    int cb = doubledCenterX(b);
    return (ca + cb == 2 * axis2);
}

bool checkSelfSymmetry(int id, int axis2) {
    return doubledCenterX(blocks[id]) == axis2;
}

bool checkSymmetryAll() {
    for (int gi = 0; gi < (int)groups.size(); gi++) {
        const auto& g = groups[gi];
        int axis2 = g.axis_x2;

        for (const auto& p : g.pairs) {
            if (!checkPairSymmetry(p, axis2)) return false;
        }
        for (int id : g.selfs) {
            if (!checkSelfSymmetry(id, axis2)) return false;
        }
    }
    return true;
}

/* =========================================================
 * Symmetry-aware group internal representation
 * ========================================================= */

struct LocalPlacement {
    int blk = -1;
    int x = 0;
    int y = 0;
    bool rot = false;
};

struct GroupBox {
    int groupId = -1;
    vector<LocalPlacement> members;
    int width = 0;
    int height = 0;
    int axis_x2_local = 0;
};

struct GroupState {
    vector<int> pairOrder;
    vector<int> pairBreakAfter;   // size = pairOrder.size()-1
    vector<bool> pairRot;
    vector<int> selfOrder;
    vector<bool> selfRot;
    bool selfsFirst = true;
};

int blockWidthByRot(int id, bool rot) {
    return rot ? blocks[id].h : blocks[id].w;
}

int blockHeightByRot(int id, bool rot) {
    return rot ? blocks[id].w : blocks[id].h;
}

bool pairRotationLegal(int a, int b, bool rot) {
    return blockWidthByRot(a, rot) == blockWidthByRot(b, rot) &&
           blockHeightByRot(a, rot) == blockHeightByRot(b, rot);
}

struct PairDims {
    int idx;
    int w;
    int h;
};

struct PairRow {
    vector<PairDims> pairs;
    int halfWidth = 0;
    int rowHeight = 0;
};

struct SelfDims {
    int blk = -1;
    int w = 0;
    int h = 0;
    bool rot = false;
};

struct GroupRowPlacement {
    int pairRowIdx = -1;
    int selfIdx = -1;
    int height = 0;
};

GroupBox buildGroupBoxAdvanced(int gid, const GroupState& gs) {
    const SymGroup& g = groups[gid];
    GroupBox box;
    box.groupId = gid;

    vector<PairDims> orderedPairs;
    orderedPairs.reserve(g.pairs.size());

    for (int k = 0; k < (int)gs.pairOrder.size(); k++) {
        int pidx = gs.pairOrder[k];
        int a = g.pairs[pidx].a;
        int b = g.pairs[pidx].b;
        bool rot = false;
        if (pidx < (int)gs.pairRot.size()) rot = gs.pairRot[pidx];
        if (!pairRotationLegal(a, b, rot) && pairRotationLegal(a, b, !rot)) rot = !rot;
        orderedPairs.push_back({pidx, blockWidthByRot(a, rot), blockHeightByRot(a, rot)});
    }

    vector<PairRow> rows;
    if (!orderedPairs.empty()) {
        PairRow curRow;
        for (int i = 0; i < (int)orderedPairs.size(); i++) {
            const auto& pd = orderedPairs[i];
            curRow.pairs.push_back(pd);
            curRow.halfWidth += pd.w;
            curRow.rowHeight = max(curRow.rowHeight, pd.h);

            bool doBreak = false;
            if (i == (int)orderedPairs.size() - 1) doBreak = true;
            else if (i < (int)gs.pairBreakAfter.size() && gs.pairBreakAfter[i]) doBreak = true;

            if (doBreak) {
                rows.push_back(curRow);
                curRow = PairRow();
            }
        }
    }

    vector<SelfDims> orderedSelfs;
    orderedSelfs.reserve(gs.selfOrder.size());
    for (int i = 0; i < (int)gs.selfOrder.size(); i++) {
        int sid = gs.selfOrder[i];
        bool rot = (i < (int)gs.selfRot.size() ? gs.selfRot[i] : false);
        orderedSelfs.push_back({sid, blockWidthByRot(sid, rot), blockHeightByRot(sid, rot), rot});
    }

    int requiredAxisParity = -1;
    if (!orderedSelfs.empty()) {
        int bestParity = -1;
        long long bestScore = (1LL << 60);
        int bestFlips = 1e9;

        for (int parity = 0; parity <= 1; parity++) {
            long long score = 0;
            int flips = 0;
            bool feasible = true;

            for (const auto& self : orderedSelfs) {
                int id = self.blk;
                bool curMatches = ((self.w & 1) == parity);
                bool rotMatches = ((blockWidthByRot(id, !self.rot) & 1) == parity);

                if (!curMatches && !rotMatches) {
                    feasible = false;
                    break;
                }

                if (curMatches) {
                    score += self.w;
                } else {
                    score += blockWidthByRot(id, !self.rot);
                    flips++;
                }
            }

            if (feasible && (score < bestScore || (score == bestScore && flips < bestFlips))) {
                bestScore = score;
                bestFlips = flips;
                bestParity = parity;
            }
        }

        if (bestParity != -1) {
            requiredAxisParity = bestParity;
            for (auto& self : orderedSelfs) {
                if ((self.w & 1) == requiredAxisParity) continue;
                self.rot = !self.rot;
                self.w = blockWidthByRot(self.blk, self.rot);
                self.h = blockHeightByRot(self.blk, self.rot);
            }
        }
    }

    int rowCount = (int)rows.size();
    int selfCount = (int)orderedSelfs.size();

    struct DpNode {
        int width = 0;
        int height = 0;
        int prevI = -1;
        int prevJ = -1;
        int prevK = -1;
        int action = -1; // 0: pair only, 1: self only, 2: pair+self
    };

    auto dominates = [](const DpNode& a, const DpNode& b) {
        return a.width <= b.width && a.height <= b.height &&
               (a.width < b.width || a.height < b.height);
    };

    vector<vector<vector<DpNode>>> dp(rowCount + 1, vector<vector<DpNode>>(selfCount + 1));
    dp[0][0].push_back({0, 0, -1, -1, -1, -1});

    for (int i = 0; i <= rowCount; i++) {
        for (int j = 0; j <= selfCount; j++) {
            const auto curStates = dp[i][j];
            for (int k = 0; k < (int)curStates.size(); k++) {
                const auto& cur = curStates[k];

                auto tryPush = [&](int ni, int nj, int rowWidth, int rowHeight, int action) {
                    DpNode cand;
                    cand.width = max(cur.width, rowWidth);
                    cand.height = cur.height + rowHeight;
                    cand.prevI = i;
                    cand.prevJ = j;
                    cand.prevK = k;
                    cand.action = action;

                    auto& bucket = dp[ni][nj];
                    for (const auto& exist : bucket) {
                        if (dominates(exist, cand)) return;
                    }

                    vector<DpNode> filtered;
                    filtered.reserve(bucket.size() + 1);
                    for (const auto& exist : bucket) {
                        if (!dominates(cand, exist)) filtered.push_back(exist);
                    }
                    filtered.push_back(cand);
                    bucket.swap(filtered);
                };

                if (i < rowCount) {
                    tryPush(i + 1, j, 2 * rows[i].halfWidth, rows[i].rowHeight, 0);
                }
                if (j < selfCount) {
                    tryPush(i, j + 1, orderedSelfs[j].w, orderedSelfs[j].h, 1);
                }
                if (i < rowCount && j < selfCount) {
                    int rowWidth = 2 * rows[i].halfWidth + orderedSelfs[j].w;
                    int rowHeight = max(rows[i].rowHeight, orderedSelfs[j].h);
                    tryPush(i + 1, j + 1, rowWidth, rowHeight, 2);
                }
            }
        }
    }

    const auto& finalStates = dp[rowCount][selfCount];
    DpNode best = finalStates.front();
    for (const auto& cand : finalStates) {
        long long candArea = 1LL * cand.width * cand.height;
        long long bestArea = 1LL * best.width * best.height;
        int candPerimeter = cand.width + cand.height;
        int bestPerimeter = best.width + best.height;
        if (candArea < bestArea || (candArea == bestArea && candPerimeter < bestPerimeter)) {
            best = cand;
        }
    }

    vector<GroupRowPlacement> finalRows;
    int ci = rowCount;
    int cj = selfCount;
    int ck = -1;

    for (int idx = 0; idx < (int)finalStates.size(); idx++) {
        if (finalStates[idx].width == best.width &&
            finalStates[idx].height == best.height &&
            finalStates[idx].prevI == best.prevI &&
            finalStates[idx].prevJ == best.prevJ &&
            finalStates[idx].prevK == best.prevK &&
            finalStates[idx].action == best.action) {
            ck = idx;
            break;
        }
    }
    if (ck == -1) ck = 0;

    while (ci > 0 || cj > 0) {
        const auto node = dp[ci][cj][ck];
        GroupRowPlacement row;
        row.height = 0;

        if (node.action == 0 || node.action == 2) {
            row.pairRowIdx = ci - 1;
            row.height = max(row.height, rows[row.pairRowIdx].rowHeight);
        }
        if (node.action == 1 || node.action == 2) {
            row.selfIdx = cj - 1;
            row.height = max(row.height, orderedSelfs[row.selfIdx].h);
        }

        finalRows.push_back(row);
        int pi = node.prevI;
        int pj = node.prevJ;
        int pk = node.prevK;
        ci = pi;
        cj = pj;
        ck = pk;
    }
    reverse(finalRows.begin(), finalRows.end());

    int axis2 = best.width;
    if (requiredAxisParity != -1 && (axis2 & 1) != requiredAxisParity) axis2++;
    int curY = 0;

    for (const auto& row : finalRows) {
        int selfWidth = 0;
        if (row.selfIdx != -1) {
            const auto& self = orderedSelfs[row.selfIdx];
            selfWidth = self.w;
            int x = (axis2 - self.w) / 2;
            box.members.push_back({self.blk, x, curY, self.rot});
        }

        if (row.pairRowIdx != -1) {
            const auto& pairRow = rows[row.pairRowIdx];
            int leftCursor = 0;
            int leftBase = (axis2 - selfWidth) / 2;
            int rightBase = (axis2 + selfWidth) / 2;
            for (const auto& pd : pairRow.pairs) {
                const auto& sp = g.pairs[pd.idx];
                int a = sp.a;
                int b = sp.b;
                bool rot = false;
                if (pd.idx < (int)gs.pairRot.size()) rot = gs.pairRot[pd.idx];
                if (!pairRotationLegal(a, b, rot) && pairRotationLegal(a, b, !rot)) rot = !rot;
                int w = blockWidthByRot(a, rot);

                int xL = leftBase - (leftCursor + w);
                int xR = rightBase + leftCursor;

                box.members.push_back({a, xL, curY, rot});
                box.members.push_back({b, xR, curY, rot});
                leftCursor += w;
            }
        }
        curY += row.height;
    }

    box.width = axis2;
    box.height = best.height;
    box.axis_x2_local = axis2;

    return box;
}

/* =========================================================
 * Outer objects
 * ========================================================= */

struct OuterObject {
    bool isGroup = false;
    int id = -1;   // groupId or blockId
    int w = 0;
    int h = 0;
    bool rot = false;
    int x = 0;
    int y = 0;

    int width() const  { return rot ? h : w; }
    int height() const { return rot ? w : h; }
};

thread_local vector<GroupBox> groupBoxes;
thread_local vector<OuterObject> outerObjs;
thread_local vector<int> blockOwnerGroup;

inline bool deadlineReached(const chrono::steady_clock::time_point* deadline);

/* =========================================================
 * B*-tree state
 * ========================================================= */

struct TreeNode {
    int obj = -1;
    int parent = -1;
    int left = -1;
    int right = -1;
};

struct State {
    vector<TreeNode> tree;
    int root = -1;
    vector<bool> objRot;
    vector<GroupState> groupStates;
};

State bestState;
double bestCost = 1e100;

/* =========================================================
 * Build outer objects
 * ========================================================= */

void buildOuterObjects() {
    groupBoxes.clear();
    outerObjs.clear();
    blockOwnerGroup.assign(blocks.size(), -1);

    for (int gi = 0; gi < (int)groups.size(); gi++) {
        OuterObject obj;
        obj.isGroup = true;
        obj.id = gi;
        obj.w = 1;
        obj.h = 1;
        obj.rot = false;
        outerObjs.push_back(obj);

        for (const auto& p : groups[gi].pairs) {
            blockOwnerGroup[p.a] = gi;
            blockOwnerGroup[p.b] = gi;
        }
        for (int id : groups[gi].selfs) {
            blockOwnerGroup[id] = gi;
        }
    }

    for (int bi = 0; bi < (int)blocks.size(); bi++) {
        if (blockOwnerGroup[bi] != -1) continue;
        OuterObject obj;
        obj.isGroup = false;
        obj.id = bi;
        obj.w = blocks[bi].w;
        obj.h = blocks[bi].h;
        obj.rot = false;
        outerObjs.push_back(obj);
    }
}

/* =========================================================
 * Initialize group states
 * ========================================================= */

GroupState makeInitialGroupState(int gid) {
    GroupState gs;
    const auto& g = groups[gid];

    gs.pairOrder.resize(g.pairs.size());
    iota(gs.pairOrder.begin(), gs.pairOrder.end(), 0);

    if (!gs.pairOrder.empty()) shuffle(gs.pairOrder.begin(), gs.pairOrder.end(), rng);

    gs.pairBreakAfter.assign(max(0, (int)g.pairs.size() - 1), 0);
    for (int i = 0; i < (int)gs.pairBreakAfter.size(); i++) {
        gs.pairBreakAfter[i] = (rand01() < 0.35 ? 1 : 0);
    }
    gs.pairRot.assign(g.pairs.size(), false);
    for (int i = 0; i < (int)g.pairs.size(); i++) {
        int a = g.pairs[i].a;
        int b = g.pairs[i].b;
        bool can0 = pairRotationLegal(a, b, false);
        bool can1 = pairRotationLegal(a, b, true);
        if (can0 && can1) gs.pairRot[i] = (rand01() < 0.5);
        else if (can1) gs.pairRot[i] = true;
    }

    gs.selfOrder = g.selfs;
    if (!gs.selfOrder.empty()) shuffle(gs.selfOrder.begin(), gs.selfOrder.end(), rng);
    gs.selfRot.assign(gs.selfOrder.size(), false);
    for (int i = 0; i < (int)gs.selfRot.size(); i++) {
        gs.selfRot[i] = (rand01() < 0.5);
    }

    gs.selfsFirst = (rand01() < 0.5);
    return gs;
}

void randomizeInitialRotations(State& s, double prob = 0.5) {
    for (int i = 0; i < (int)s.tree.size(); i++) {
        int oid = s.tree[i].obj;
        if (outerObjs[oid].isGroup) s.objRot[i] = false;
        else s.objRot[i] = (rand01() < prob);
    }
}

/* =========================================================
 * Initial outer states
 * ========================================================= */

State buildStateFromPermutationChain(const vector<int>& perm, bool useLeftChain) {
    State s;
    int n = (int)perm.size();
    s.tree.resize(n);
    s.objRot.assign(n, false);
    s.groupStates.resize(groups.size());

    for (int gi = 0; gi < (int)groups.size(); gi++) {
        s.groupStates[gi] = makeInitialGroupState(gi);
    }

    if (n == 0) return s;

    for (int i = 0; i < n; i++) {
        s.tree[i].obj = perm[i];
        s.tree[i].parent = -1;
        s.tree[i].left = -1;
        s.tree[i].right = -1;
    }

    s.root = 0;
    for (int i = 1; i < n; i++) {
        if (useLeftChain) s.tree[i - 1].left = i;
        else s.tree[i - 1].right = i;
        s.tree[i].parent = i - 1;
    }
    return s;
}

State buildStateFromPermutationAlternating(const vector<int>& perm) {
    State s;
    int n = (int)perm.size();
    s.tree.resize(n);
    s.objRot.assign(n, false);
    s.groupStates.resize(groups.size());

    for (int gi = 0; gi < (int)groups.size(); gi++) {
        s.groupStates[gi] = makeInitialGroupState(gi);
    }

    if (n == 0) return s;

    for (int i = 0; i < n; i++) {
        s.tree[i].obj = perm[i];
        s.tree[i].parent = -1;
        s.tree[i].left = -1;
        s.tree[i].right = -1;
    }

    s.root = 0;
    for (int i = 1; i < n; i++) {
        if (i % 2) s.tree[i - 1].left = i;
        else s.tree[i - 1].right = i;
        s.tree[i].parent = i - 1;
    }
    return s;
}

State initLeftChainState() {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);
    State s = buildStateFromPermutationChain(perm, true);
    randomizeInitialRotations(s, 0.0);
    return s;
}

State initRightChainState() {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);
    State s = buildStateFromPermutationChain(perm, false);
    randomizeInitialRotations(s, 0.0);
    return s;
}

State initRandomChainState() {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);
    shuffle(perm.begin(), perm.end(), rng);

    State s = buildStateFromPermutationChain(perm, rand01() < 0.5);
    randomizeInitialRotations(s, 0.5);
    return s;
}

State initAlternatingState() {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);
    shuffle(perm.begin(), perm.end(), rng);

    State s = buildStateFromPermutationAlternating(perm);
    randomizeInitialRotations(s, 0.5);
    return s;
}

State initSortedByWidthState(bool descending) {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);

    sort(perm.begin(), perm.end(), [&](int a, int b) {
        if (descending) return outerObjs[a].width() > outerObjs[b].width();
        return outerObjs[a].width() < outerObjs[b].width();
    });

    State s = buildStateFromPermutationAlternating(perm);
    randomizeInitialRotations(s, 0.3);
    return s;
}

State initSortedByHeightState(bool descending) {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);

    sort(perm.begin(), perm.end(), [&](int a, int b) {
        if (descending) return outerObjs[a].height() > outerObjs[b].height();
        return outerObjs[a].height() < outerObjs[b].height();
    });

    State s = buildStateFromPermutationAlternating(perm);
    randomizeInitialRotations(s, 0.3);
    return s;
}

State initSortedByAreaState(bool descending) {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);

    sort(perm.begin(), perm.end(), [&](int a, int b) {
        int areaA = outerObjs[a].width() * outerObjs[a].height();
        int areaB = outerObjs[b].width() * outerObjs[b].height();
        if (descending) return areaA > areaB;
        return areaA < areaB;
    });

    State s = buildStateFromPermutationAlternating(perm);
    randomizeInitialRotations(s, 0.25);
    return s;
}

State initSortedByMaxDimState(bool descending) {
    int n = (int)outerObjs.size();
    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);

    sort(perm.begin(), perm.end(), [&](int a, int b) {
        int keyA = max(outerObjs[a].width(), outerObjs[a].height());
        int keyB = max(outerObjs[b].width(), outerObjs[b].height());
        if (descending) return keyA > keyB;
        return keyA < keyB;
    });

    State s = buildStateFromPermutationAlternating(perm);
    randomizeInitialRotations(s, 0.25);
    return s;
}

State initRandomTreeState() {
    State s;
    int n = (int)outerObjs.size();
    s.tree.resize(n);
    s.objRot.assign(n, false);
    s.groupStates.resize(groups.size());

    for (int gi = 0; gi < (int)groups.size(); gi++) {
        s.groupStates[gi] = makeInitialGroupState(gi);
    }

    if (n == 0) return s;

    vector<int> perm(n);
    iota(perm.begin(), perm.end(), 0);
    shuffle(perm.begin(), perm.end(), rng);

    for (int i = 0; i < n; i++) {
        s.tree[i].obj = perm[i];
        s.tree[i].parent = -1;
        s.tree[i].left = -1;
        s.tree[i].right = -1;
    }

    s.root = 0;
    vector<int> candidates = {0};

    for (int i = 1; i < n; i++) {
        while (true) {
            int p = candidates[randInt(0, (int)candidates.size() - 1)];
            bool canL = (s.tree[p].left == -1);
            bool canR = (s.tree[p].right == -1);

            if (!canL && !canR) continue;

            if (canL && canR) {
                if (rand01() < 0.5) s.tree[p].left = i;
                else s.tree[p].right = i;
            } else if (canL) {
                s.tree[p].left = i;
            } else {
                s.tree[p].right = i;
            }

            s.tree[i].parent = p;
            candidates.push_back(i);
            break;
        }
    }

    randomizeInitialRotations(s, 0.5);
    return s;
}

/* =========================================================
 * Contour
 * ========================================================= */

struct Segment {
    int l, r, h;
};

struct Contour {
    vector<Segment> segs;

    void clear() {
        segs.clear();
    }

    int queryMax(int L, int R) const {
        int ans = 0;
        for (const auto& s : segs) {
            if (s.r <= L || s.l >= R) continue;
            ans = max(ans, s.h);
        }
        return ans;
    }

    void update(int L, int R, int H) {
        vector<Segment> nxt;
        for (const auto& s : segs) {
            if (s.r <= L || s.l >= R) {
                nxt.push_back(s);
            } else {
                if (s.l < L) nxt.push_back({s.l, L, s.h});
                if (R < s.r) nxt.push_back({R, s.r, s.h});
            }
        }
        nxt.push_back({L, R, H});

        sort(nxt.begin(), nxt.end(), [](const Segment& a, const Segment& b) {
            return a.l < b.l;
        });

        vector<Segment> merged;
        for (const auto& s : nxt) {
            if (merged.empty() || merged.back().r != s.l || merged.back().h != s.h) {
                merged.push_back(s);
            } else {
                merged.back().r = s.r;
            }
        }
        segs = move(merged);
    }
};

thread_local Contour contour;

void initThreadWorkingData() {
    blocks = baseBlocks;
    groups = baseGroups;
    buildOuterObjects();
}

/* =========================================================
 * Apply state to outer objects
 * ========================================================= */

void applyStateToOuterObjects(const State& s) {
    groupBoxes.clear();
    groupBoxes.resize(groups.size());

    for (int gi = 0; gi < (int)groups.size(); gi++) {
        groupBoxes[gi] = buildGroupBoxAdvanced(gi, s.groupStates[gi]);
    }

    for (int i = 0; i < (int)outerObjs.size(); i++) {
        if (outerObjs[i].isGroup) {
            int gid = outerObjs[i].id;
            outerObjs[i].w = groupBoxes[gid].width;
            outerObjs[i].h = groupBoxes[gid].height;
            outerObjs[i].rot = false;
        } else {
            outerObjs[i].rot = false;
        }
    }

    for (int i = 0; i < (int)s.tree.size(); i++) {
        int oid = s.tree[i].obj;
        if (!outerObjs[oid].isGroup) {
            outerObjs[oid].rot = s.objRot[i];
        }
    }
}

/* =========================================================
 * Pack outer B*-tree
 * ========================================================= */

void packDFS(const State& s, int u) {
    if (u == -1) return;

    int oid = s.tree[u].obj;
    int x = outerObjs[oid].x;
    int w = outerObjs[oid].width();
    int h = outerObjs[oid].height();

    int y = contour.queryMax(x, x + w);
    outerObjs[oid].y = y;
    contour.update(x, x + w, y + h);

    int lc = s.tree[u].left;
    int rc = s.tree[u].right;

    if (lc != -1) {
        int childObj = s.tree[lc].obj;
        outerObjs[childObj].x = outerObjs[oid].x + outerObjs[oid].width();
        packDFS(s, lc);
    }

    if (rc != -1) {
        int childObj = s.tree[rc].obj;
        outerObjs[childObj].x = outerObjs[oid].x;
        packDFS(s, rc);
    }
}

void applyPlacementToBlocks() {
    for (auto& b : blocks) {
        b.x = 0;
        b.y = 0;
    }

    for (const auto& obj : outerObjs) {
        if (!obj.isGroup) continue;

        int gid = obj.id;
        const GroupBox& gb = groupBoxes[gid];
        int baseX = obj.x;
        int baseY = obj.y;

        groups[gid].axis_x2 = 2 * baseX + gb.axis_x2_local;

        for (const auto& m : gb.members) {
            blocks[m.blk].rot = m.rot;
            blocks[m.blk].x = baseX + m.x;
            blocks[m.blk].y = baseY + m.y;
        }
    }

    for (const auto& obj : outerObjs) {
        if (obj.isGroup) continue;
        int bid = obj.id;
        blocks[bid].rot = obj.rot;
        blocks[bid].x = obj.x;
        blocks[bid].y = obj.y;
    }
}

void packState(const State& s) {
    applyStateToOuterObjects(s);

    contour.clear();
    for (auto& obj : outerObjs) {
        obj.x = 0;
        obj.y = 0;
    }

    if (s.root != -1) {
        int rootObj = s.tree[s.root].obj;
        outerObjs[rootObj].x = 0;
        packDFS(s, s.root);
    }

    applyPlacementToBlocks();
}

/* =========================================================
 * Cost
 * ========================================================= */

double evaluateAreaGuided(const State& s) {
    packState(s);

    int W = chipWidth();
    int H = chipHeight();
    int A = W * H;

    // Keep the search focused on area, with only a mild compactness bias
    // to avoid getting trapped in obviously elongated layouts.
    double cost = 1.0 * A + 0.2 * (W + H) + 0.1 * abs(W - H);

    if (!checkNoOverlap()) cost += 1e12;
    if (!checkSymmetryAll()) cost += 1e12;

    return cost;
}

double evaluateAreaOnly(const State& s) {
    packState(s);

    int A = chipArea();
    double cost = (double)A;

    if (!checkNoOverlap()) cost += 1e12;
    if (!checkSymmetryAll()) cost += 1e12;

    return cost;
}

/* =========================================================
 * Tree helpers
 * ========================================================= */

bool isAncestor(const State& s, int a, int b) {
    while (b != -1) {
        if (b == a) return true;
        b = s.tree[b].parent;
    }
    return false;
}

void replaceChild(State& s, int parent, int oldChild, int newChild) {
    if (parent == -1) return;
    if (s.tree[parent].left == oldChild) s.tree[parent].left = newChild;
    else if (s.tree[parent].right == oldChild) s.tree[parent].right = newChild;
}

void detachSubtree(State& s, int u) {
    int p = s.tree[u].parent;
    if (p == -1) return;
    replaceChild(s, p, u, -1);
    s.tree[u].parent = -1;
}

/* =========================================================
 * SA moves: outer tree / blocks
 * ========================================================= */

void doSwapObjects(State& s) {
    int n = (int)s.tree.size();
    if (n < 2) return;

    int u = randInt(0, n - 1);
    int v = randInt(0, n - 1);
    while (v == u) v = randInt(0, n - 1);

    swap(s.tree[u].obj, s.tree[v].obj);
    swap(s.objRot[u], s.objRot[v]);
}

void doRotate(State& s) {
    int n = (int)s.tree.size();
    if (n == 0) return;

    for (int tries = 0; tries < 8; tries++) {
        int u = randInt(0, n - 1);
        int oid = s.tree[u].obj;
        if (outerObjs[oid].isGroup) continue;
        s.objRot[u] = !s.objRot[u];
        return;
    }
}

void doSubtreeReinsert(State& s) {
    int n = (int)s.tree.size();
    if (n <= 1) return;

    for (int tries = 0; tries < 20; tries++) {
        int u = randInt(0, n - 1);
        if (u == s.root) continue;

        int v = randInt(0, n - 1);
        if (v == u) continue;
        if (isAncestor(s, u, v)) continue;

        bool tryLeft = (rand01() < 0.5);
        if (tryLeft && s.tree[v].left != -1) continue;
        if (!tryLeft && s.tree[v].right != -1) continue;

        detachSubtree(s, u);

        if (tryLeft) s.tree[v].left = u;
        else s.tree[v].right = u;
        s.tree[u].parent = v;
        return;
    }
}

void doSubtreeSwap(State& s) {
    int n = (int)s.tree.size();
    if (n < 2) return;

    for (int tries = 0; tries < 20; tries++) {
        int u = randInt(0, n - 1);
        int v = randInt(0, n - 1);
        while (v == u) v = randInt(0, n - 1);

        if (isAncestor(s, u, v) || isAncestor(s, v, u)) continue;

        int pu = s.tree[u].parent;
        int pv = s.tree[v].parent;

        if (pu == -1) s.root = v;
        else replaceChild(s, pu, u, v);

        if (pv == -1) s.root = u;
        else replaceChild(s, pv, v, u);

        swap(s.tree[u].parent, s.tree[v].parent);
        return;
    }
}

void doSubtreeFlip(State& s) {
    int n = (int)s.tree.size();
    if (n == 0) return;

    int u = randInt(0, n - 1);
    swap(s.tree[u].left, s.tree[u].right);
}

/* =========================================================
 * SA moves: group internal
 * ========================================================= */

void doGroupPairSwap(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    if (gs.pairOrder.size() < 2) return;

    int i = randInt(0, (int)gs.pairOrder.size() - 1);
    int j = randInt(0, (int)gs.pairOrder.size() - 1);
    while (j == i) j = randInt(0, (int)gs.pairOrder.size() - 1);

    swap(gs.pairOrder[i], gs.pairOrder[j]);
}

void doGroupMovePair(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    int m = (int)gs.pairOrder.size();
    if (m < 2) return;

    int from = randInt(0, m - 1);
    int to   = randInt(0, m - 1);
    while (to == from) to = randInt(0, m - 1);

    int val = gs.pairOrder[from];
    gs.pairOrder.erase(gs.pairOrder.begin() + from);
    gs.pairOrder.insert(gs.pairOrder.begin() + to, val);

    if (!gs.pairBreakAfter.empty()) {
        int idx = randInt(0, (int)gs.pairBreakAfter.size() - 1);
        gs.pairBreakAfter[idx] ^= 1;
    }
}

void doGroupToggleBreak(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    if (gs.pairBreakAfter.empty()) return;

    int idx = randInt(0, (int)gs.pairBreakAfter.size() - 1);
    gs.pairBreakAfter[idx] ^= 1;
}

void doGroupSelfSwap(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    if (gs.selfOrder.size() < 2) return;

    int i = randInt(0, (int)gs.selfOrder.size() - 1);
    int j = randInt(0, (int)gs.selfOrder.size() - 1);
    while (j == i) j = randInt(0, (int)gs.selfOrder.size() - 1);

    swap(gs.selfOrder[i], gs.selfOrder[j]);
    swap(gs.selfRot[i], gs.selfRot[j]);
}

void doGroupToggleSelfPos(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    s.groupStates[gid].selfsFirst = !s.groupStates[gid].selfsFirst;
}

void doGroupReverseSegment(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    if (gs.pairOrder.size() < 2) return;

    int l = randInt(0, (int)gs.pairOrder.size() - 1);
    int r = randInt(0, (int)gs.pairOrder.size() - 1);
    if (l > r) swap(l, r);
    reverse(gs.pairOrder.begin() + l, gs.pairOrder.begin() + r + 1);
}

void doGroupMoveSelf(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    int m = (int)gs.selfOrder.size();
    if (m < 2) return;

    int from = randInt(0, m - 1);
    int to   = randInt(0, m - 1);
    while (to == from) to = randInt(0, m - 1);

    int val = gs.selfOrder[from];
    bool rot = gs.selfRot[from];
    gs.selfOrder.erase(gs.selfOrder.begin() + from);
    gs.selfRot.erase(gs.selfRot.begin() + from);
    gs.selfOrder.insert(gs.selfOrder.begin() + to, val);
    gs.selfRot.insert(gs.selfRot.begin() + to, rot);
}

void doGroupTogglePairRot(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    if (gs.pairRot.empty()) return;

    int idx = randInt(0, (int)gs.pairRot.size() - 1);
    int a = groups[gid].pairs[idx].a;
    int b = groups[gid].pairs[idx].b;
    bool nxt = !gs.pairRot[idx];
    if (pairRotationLegal(a, b, nxt)) gs.pairRot[idx] = nxt;
}

void doGroupToggleSelfRot(State& s) {
    if (groups.empty()) return;
    int gid = randInt(0, (int)groups.size() - 1);
    auto& gs = s.groupStates[gid];
    if (gs.selfRot.empty()) return;

    int idx = randInt(0, (int)gs.selfRot.size() - 1);
    gs.selfRot[idx] = !gs.selfRot[idx];
}

/* =========================================================
 * Generic simulated annealing
 * ========================================================= */

pair<State, double> simulatedAnnealing(
    const State& init,
    double (*evalFunc)(const State&),
    double T0,
    double cooling,
    int movesPerTemp,
    const chrono::steady_clock::time_point* deadline = nullptr
) {
    State cur = init;
    double curCost = evalFunc(cur);

    State localBest = cur;
    double localBestCost = curCost;

    double T = T0;
    double Tmin = 1e-3;

    while (T > Tmin) {
        for (int step = 0; step < movesPerTemp; step++) {
            if ((step % 32 == 0) && deadlineReached(deadline)) {
                return {localBest, localBestCost};
            }
            State nxt = cur;

            int r = randInt(1, 100);

            if (r <= 9) doSwapObjects(nxt);
            else if (r <= 18) doRotate(nxt);
            else if (r <= 35) doSubtreeReinsert(nxt);
            else if (r <= 46) doSubtreeSwap(nxt);
            else if (r <= 55) doSubtreeFlip(nxt);
            else if (r <= 64) doGroupPairSwap(nxt);
            else if (r <= 72) doGroupMovePair(nxt);
            else if (r <= 79) doGroupToggleBreak(nxt);
            else if (r <= 84) doGroupSelfSwap(nxt);
            else if (r <= 88) doGroupMoveSelf(nxt);
            else if (r <= 92) doGroupToggleSelfPos(nxt);
            else if (r <= 96) doGroupTogglePairRot(nxt);
            else doGroupToggleSelfRot(nxt);

            double nxtCost = evalFunc(nxt);
            double delta = nxtCost - curCost;

            if (delta <= 0 || rand01() < exp(-delta / T)) {
                cur = nxt;
                curCost = nxtCost;

                if (curCost < localBestCost) {
                    localBest = cur;
                    localBestCost = curCost;
                }
            }
        }
        if (deadlineReached(deadline)) {
            break;
        }
        T *= cooling;
    }

    return {localBest, localBestCost};
}

bool useHeavyRefine();
bool useMediumRefine();
bool useDeepNeighborhoodRefine();
bool useLongBudgetLargeQualityCase();
struct SearchProfile;
SearchProfile currentSearchProfile();

inline bool deadlineReached(const chrono::steady_clock::time_point* deadline) {
    auto now = chrono::steady_clock::now();
    if (chrono::duration_cast<chrono::minutes>(now - gProgramStartTime).count() >= 58) {
        return true;
    }
    return deadline != nullptr && now >= *deadline;
}

/* =========================================================
 * Deterministic local refine
 * ========================================================= */

void localRefine(State& s, const chrono::steady_clock::time_point* deadline = nullptr) {
    bool improved = true;
    bool heavyRefine = useHeavyRefine();
    bool mediumRefine = useMediumRefine();
    bool deepNeighborhood = useDeepNeighborhoodRefine();
    int passes = 0;
    int maxPasses = deepNeighborhood ? 4 : 3;

    while (improved && passes < maxPasses) {
        if (deadlineReached(deadline)) return;
        passes++;
        improved = false;
        double base = evaluateAreaOnly(s);

        // rotate each single block
        for (int i = 0; i < (int)s.tree.size(); i++) {
            if ((i & 31) == 0 && deadlineReached(deadline)) return;
            int oid = s.tree[i].obj;
            if (outerObjs[oid].isGroup) continue;

            State cand = s;
            cand.objRot[i] = !cand.objRot[i];
            double cost = evaluateAreaOnly(cand);
            if (cost < base) {
                s = cand;
                base = cost;
                improved = true;
            }
        }

        // toggle self position for each group
        for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
            if ((gid & 15) == 0 && deadlineReached(deadline)) return;
            State cand = s;
            cand.groupStates[gid].selfsFirst = !cand.groupStates[gid].selfsFirst;
            double cost = evaluateAreaOnly(cand);
            if (cost < base) {
                s = cand;
                base = cost;
                improved = true;
            }
        }

        // toggle each row break
        for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
            for (int i = 0; i < (int)s.groupStates[gid].pairBreakAfter.size(); i++) {
                if (((gid + i) & 31) == 0 && deadlineReached(deadline)) return;
                State cand = s;
                cand.groupStates[gid].pairBreakAfter[i] ^= 1;
                double cost = evaluateAreaOnly(cand);
                if (cost < base) {
                    s = cand;
                    base = cost;
                    improved = true;
                }
            }
        }

        // adjacent pair swap
        for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
            auto& ord = s.groupStates[gid].pairOrder;
            for (int i = 0; i + 1 < (int)ord.size(); i++) {
                if (((gid + i) & 31) == 0 && deadlineReached(deadline)) return;
                State cand = s;
                swap(cand.groupStates[gid].pairOrder[i],
                     cand.groupStates[gid].pairOrder[i + 1]);
                double cost = evaluateAreaOnly(cand);
                if (cost < base) {
                    s = cand;
                    base = cost;
                    improved = true;
                }
            }
        }

        // adjacent self swap
        for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
            auto& ord = s.groupStates[gid].selfOrder;
            for (int i = 0; i + 1 < (int)ord.size(); i++) {
                if (((gid + i) & 31) == 0 && deadlineReached(deadline)) return;
                State cand = s;
                swap(cand.groupStates[gid].selfOrder[i],
                     cand.groupStates[gid].selfOrder[i + 1]);
                swap(cand.groupStates[gid].selfRot[i],
                     cand.groupStates[gid].selfRot[i + 1]);
                double cost = evaluateAreaOnly(cand);
                if (cost < base) {
                    s = cand;
                    base = cost;
                    improved = true;
                }
            }
        }

        // toggle pair rotation
        for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
            for (int i = 0; i < (int)s.groupStates[gid].pairRot.size(); i++) {
                if (((gid + i) & 31) == 0 && deadlineReached(deadline)) return;
                int a = groups[gid].pairs[i].a;
                int b = groups[gid].pairs[i].b;
                bool nxtRot = !s.groupStates[gid].pairRot[i];
                if (!pairRotationLegal(a, b, nxtRot)) continue;

                State cand = s;
                cand.groupStates[gid].pairRot[i] = nxtRot;
                double cost = evaluateAreaOnly(cand);
                if (cost < base) {
                    s = cand;
                    base = cost;
                    improved = true;
                }
            }
        }

        // toggle self rotation
        for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
            for (int i = 0; i < (int)s.groupStates[gid].selfRot.size(); i++) {
                if (((gid + i) & 31) == 0 && deadlineReached(deadline)) return;
                State cand = s;
                cand.groupStates[gid].selfRot[i] = !cand.groupStates[gid].selfRot[i];
                double cost = evaluateAreaOnly(cand);
                if (cost < base) {
                    s = cand;
                    base = cost;
                    improved = true;
                }
            }
        }

        // single subtree flip hill-climb
        if (mediumRefine) {
            for (int u = 0; u < (int)s.tree.size(); u++) {
                if ((u & 31) == 0 && deadlineReached(deadline)) return;
                if (s.tree[u].left == -1 && s.tree[u].right == -1) continue;

                State cand = s;
                swap(cand.tree[u].left, cand.tree[u].right);
                double cost = evaluateAreaOnly(cand);
                if (cost < base) {
                    s = cand;
                    base = cost;
                    improved = true;
                }
            }
        }

        if (mediumRefine) {
            // adjacent tree object swap
            for (int i = 0; i + 1 < (int)s.tree.size(); i++) {
                if ((i & 31) == 0 && deadlineReached(deadline)) return;
                State cand = s;
                swap(cand.tree[i].obj, cand.tree[i + 1].obj);
                swap(cand.objRot[i], cand.objRot[i + 1]);
                double cost = evaluateAreaOnly(cand);
                if (cost < base) {
                    s = cand;
                    base = cost;
                    improved = true;
                }
            }
        }

        if (heavyRefine) {
            // full object-pair swap on tree nodes
            for (int i = 0; i < (int)s.tree.size(); i++) {
                for (int j = i + 1; j < (int)s.tree.size(); j++) {
                    if (((i + j) & 63) == 0 && deadlineReached(deadline)) return;
                    State cand = s;
                    swap(cand.tree[i].obj, cand.tree[j].obj);
                    swap(cand.objRot[i], cand.objRot[j]);
                    double cost = evaluateAreaOnly(cand);
                    if (cost < base) {
                        s = cand;
                        base = cost;
                        improved = true;
                    }
                }
            }
        }

        if (heavyRefine) {
            // greedy subtree reinsert search
            for (int u = 0; u < (int)s.tree.size(); u++) {
                if ((u & 15) == 0 && deadlineReached(deadline)) return;
                if (u == s.root) continue;

                for (int v = 0; v < (int)s.tree.size(); v++) {
                    if (u == v) continue;
                    if (isAncestor(s, u, v)) continue;

                    for (int side = 0; side < 2; side++) {
                        if (((u + v + side) & 63) == 0 && deadlineReached(deadline)) return;
                        if (side == 0 && s.tree[v].left != -1) continue;
                        if (side == 1 && s.tree[v].right != -1) continue;

                        State cand = s;
                        detachSubtree(cand, u);
                        if (side == 0) cand.tree[v].left = u;
                        else cand.tree[v].right = u;
                        cand.tree[u].parent = v;

                        double cost = evaluateAreaOnly(cand);
                        if (cost < base) {
                            s = cand;
                            base = cost;
                            improved = true;
                        }
                    }
                }
            }
        }

        if (deepNeighborhood) {
            // More exhaustive neighborhoods still scale reasonably for small/mid-sized cases.
            for (int u = 0; u < (int)s.tree.size(); u++) {
                for (int v = u + 1; v < (int)s.tree.size(); v++) {
                    if (((u + v) & 63) == 0 && deadlineReached(deadline)) return;
                    if (isAncestor(s, u, v) || isAncestor(s, v, u)) continue;

                    State cand = s;
                    int pu = cand.tree[u].parent;
                    int pv = cand.tree[v].parent;

                    if (pu == -1) cand.root = v;
                    else replaceChild(cand, pu, u, v);

                    if (pv == -1) cand.root = u;
                    else replaceChild(cand, pv, v, u);

                    swap(cand.tree[u].parent, cand.tree[v].parent);
                    double cost = evaluateAreaOnly(cand);
                    if (cost < base) {
                        s = cand;
                        base = cost;
                        improved = true;
                    }
                }
            }

            // Try swap+rotation combinations for single objects outside symmetry groups.
            for (int i = 0; i < (int)s.tree.size(); i++) {
                int oidI = s.tree[i].obj;
                bool freeI = !outerObjs[oidI].isGroup;
                for (int j = i + 1; j < (int)s.tree.size(); j++) {
                    if (((i + j) & 63) == 0 && deadlineReached(deadline)) return;
                    int oidJ = s.tree[j].obj;
                    bool freeJ = !outerObjs[oidJ].isGroup;

                    State cand = s;
                    swap(cand.tree[i].obj, cand.tree[j].obj);
                    swap(cand.objRot[i], cand.objRot[j]);

                    for (int mask = 0; mask < 4; mask++) {
                        State test = cand;
                        if (freeI && (mask & 1)) test.objRot[i] = !test.objRot[i];
                        if (freeJ && (mask & 2)) test.objRot[j] = !test.objRot[j];
                        double cost = evaluateAreaOnly(test);
                        if (cost < base) {
                            s = test;
                            base = cost;
                            improved = true;
                        }
                    }
                }
            }
        }
    }
}

void optimizeGroupBreaksExact(State& s, int gid, const chrono::steady_clock::time_point* deadline = nullptr) {
    auto& gs = s.groupStates[gid];
    int m = (int)gs.pairOrder.size();
    if (m <= 1) return;

    int slots = m - 1;
    if (slots > 12) return;

    vector<int> bestBreak = gs.pairBreakAfter;
    GroupBox bestBox = buildGroupBoxAdvanced(gid, gs);
    int bestArea = bestBox.width * bestBox.height;
    int totalMasks = 1 << slots;

    for (int mask = 0; mask < totalMasks; mask++) {
        if ((mask & 127) == 0 && deadlineReached(deadline)) break;
        for (int i = 0; i < slots; i++) {
            gs.pairBreakAfter[i] = ((mask >> i) & 1);
        }

        GroupBox box = buildGroupBoxAdvanced(gid, gs);
        int area = box.width * box.height;
        int perimeter = box.width + box.height;
        int bestPerimeter = bestBox.width + bestBox.height;

        if (area < bestArea || (area == bestArea && perimeter < bestPerimeter)) {
            bestArea = area;
            bestBox = box;
            bestBreak = gs.pairBreakAfter;
        }
    }

    gs.pairBreakAfter = bestBreak;
}

void optimizeGroupLayoutExact(State& s, int gid, const chrono::steady_clock::time_point* deadline = nullptr) {
    auto& gs = s.groupStates[gid];
    const auto& g = groups[gid];
    int m = (int)g.pairs.size();
    int t = (int)g.selfs.size();

    if (m == 0 && t == 0) return;

    long long breakChoices = (m <= 1 ? 1LL : (1LL << (m - 1)));
    long long pairOrderChoices = 1;
    for (int i = 2; i <= m; i++) pairOrderChoices *= i;
    long long selfOrderChoices = 1;
    for (int i = 2; i <= t; i++) selfOrderChoices *= i;

    long long pairRotChoices = 1;
    for (int i = 0; i < m; i++) {
        bool can0 = pairRotationLegal(g.pairs[i].a, g.pairs[i].b, false);
        bool can1 = pairRotationLegal(g.pairs[i].a, g.pairs[i].b, true);
        pairRotChoices *= (can0 && can1) ? 2 : 1;
    }
    long long selfRotChoices = 1LL << t;
    long long totalChoices =
        pairOrderChoices * selfOrderChoices * pairRotChoices * selfRotChoices * breakChoices * 2LL;

    if (m > 6 || t > 4 || totalChoices > 200000) return;

    GroupState bestState = gs;
    GroupBox bestBox = buildGroupBoxAdvanced(gid, gs);
    int bestArea = bestBox.width * bestBox.height;
    int bestPerimeter = bestBox.width + bestBox.height;

    vector<int> pairOrder = gs.pairOrder;
    sort(pairOrder.begin(), pairOrder.end());

    vector<int> selfOrder = gs.selfOrder;
    sort(selfOrder.begin(), selfOrder.end());

    do {
        if (deadlineReached(deadline)) break;
        do {
            if (deadlineReached(deadline)) break;
            int pairRotMasks = 1 << m;
            int selfRotMasks = 1 << t;
            int breakMasks = (m <= 1 ? 1 : (1 << (m - 1)));

            for (int pairMask = 0; pairMask < pairRotMasks; pairMask++) {
                if ((pairMask & 31) == 0 && deadlineReached(deadline)) break;
                GroupState cand = gs;
                cand.pairOrder = pairOrder;
                cand.selfOrder = selfOrder;
                cand.pairRot.assign(m, false);
                cand.selfRot.assign(t, false);

                bool legal = true;
                for (int i = 0; i < m; i++) {
                    int pidx = pairOrder[i];
                    bool rot = ((pairMask >> i) & 1) != 0;
                    bool can0 = pairRotationLegal(g.pairs[pidx].a, g.pairs[pidx].b, false);
                    bool can1 = pairRotationLegal(g.pairs[pidx].a, g.pairs[pidx].b, true);
                    if (rot && !can1) {
                        legal = false;
                        break;
                    }
                    if (!rot && !can0) {
                        if (can1) rot = true;
                        else {
                            legal = false;
                            break;
                        }
                    }
                    cand.pairRot[pidx] = rot;
                }
                if (!legal) continue;

                for (int selfMask = 0; selfMask < selfRotMasks; selfMask++) {
                    if ((selfMask & 31) == 0 && deadlineReached(deadline)) break;
                    for (int i = 0; i < t; i++) {
                        cand.selfRot[i] = ((selfMask >> i) & 1) != 0;
                    }

                    for (int breakMask = 0; breakMask < breakMasks; breakMask++) {
                        if ((breakMask & 63) == 0 && deadlineReached(deadline)) break;
                        cand.pairBreakAfter.assign(max(0, m - 1), 0);
                        for (int i = 0; i + 1 < m; i++) {
                            cand.pairBreakAfter[i] = ((breakMask >> i) & 1) != 0;
                        }

                        for (int sf = 0; sf < 2; sf++) {
                            cand.selfsFirst = (sf == 1);
                            GroupBox box = buildGroupBoxAdvanced(gid, cand);
                            int area = box.width * box.height;
                            int perimeter = box.width + box.height;
                            if (area < bestArea ||
                                (area == bestArea && perimeter < bestPerimeter)) {
                                bestArea = area;
                                bestPerimeter = perimeter;
                                bestState = cand;
                            }
                        }
                    }
                }
            }
        } while (next_permutation(selfOrder.begin(), selfOrder.end()));
    } while (next_permutation(pairOrder.begin(), pairOrder.end()));

    gs = bestState;
}

void optimizeAllGroupBreaksExact(State& s, const chrono::steady_clock::time_point* deadline = nullptr) {
    for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
        if (deadlineReached(deadline)) return;
        optimizeGroupBreaksExact(s, gid, deadline);
    }
}

void optimizeAllSmallGroupsExact(State& s, const chrono::steady_clock::time_point* deadline = nullptr) {
    for (int gid = 0; gid < (int)s.groupStates.size(); gid++) {
        if (deadlineReached(deadline)) return;
        optimizeGroupLayoutExact(s, gid, deadline);
    }
}

/* =========================================================
 * Multi-start + two-phase SA + local refine
 * ========================================================= */
struct Candidate {
    State s;
    double score;
};

bool isLegalScore(double score) {
    return score < 1e11;
}

bool betterCandidate(const Candidate& a, const Candidate& b) {
    bool aLegal = isLegalScore(a.score);
    bool bLegal = isLegalScore(b.score);
    if (aLegal != bLegal) return aLegal;
    return a.score < b.score;
}

bool updateBestCandidate(const Candidate& cand) {
    if (betterCandidate(cand, {bestState, bestCost})) {
        bestCost = cand.score;
        bestState = cand.s;
        return true;
    }
    return false;
}

struct SearchProfile {
    int macroRounds = 1;
    int seedStagnationLimit = 2;
    int topK = 2;
    int guidedMoves = 100;
    int refineMoves = 60;
    int polishMoves = 40;
    int randomChainTries = 1;
    int randomTreeTries = 1;
    int alternatingTries = 0;
    double guidedT0 = 1200.0;
    double guidedCooling = 0.90;
    double refineT0 = 800.0;
    double refineCooling = 0.93;
    double polishT0 = 200.0;
    double polishCooling = 0.95;
    double intenseT0 = 1400.0;
    double intenseCooling = 0.965;
    int intenseMoves = 1000;
    bool deepNeighborhood = false;
};

bool useDeepNeighborhoodRefine() {
    int n = (int)outerObjs.size();
    return n <= 36;
}

SearchProfile currentSearchProfile() {
    SearchProfile p;
    const int n = (int)outerObjs.size();
    const bool timedLargeQuality = useLongBudgetLargeQualityCase();
    const bool deep = useDeepNeighborhoodRefine();

    if (timedLargeQuality) {
        p.macroRounds = 2;
        p.seedStagnationLimit = 4;
        p.topK = 8;
        p.guidedMoves = max(3200, 36 * n);
        p.refineMoves = max(2400, 28 * n);
        p.polishMoves = max(1800, 18 * n);
        p.intenseMoves = max(1400, 16 * n);
        p.randomChainTries = 4;
        p.randomTreeTries = 5;
        p.alternatingTries = 3;
        p.guidedT0 = 5200.0;
        p.guidedCooling = 0.945;
        p.refineT0 = 3200.0;
        p.refineCooling = 0.955;
        p.polishT0 = 1200.0;
        p.polishCooling = 0.972;
        p.intenseT0 = 2200.0;
        p.intenseCooling = 0.972;
        return p;
    }

    p.deepNeighborhood = deep;
    p.macroRounds = deep ? 4 : (n <= 180 ? 3 : 2);
    p.seedStagnationLimit = deep ? 3 : 2;
    p.topK = deep ? 8 : (n <= 80 ? 7 : n <= 180 ? 6 : n <= 320 ? 5 : 3);
    p.guidedMoves = deep ? max(900, 64 * n)
                         : (n <= 80 ? max(650, 48 * n)
                                    : n <= 180 ? max(480, 32 * n)
                                               : n <= 320 ? max(320, 20 * n)
                                                          : max(220, 14 * n));
    p.refineMoves = deep ? max(520, 32 * n)
                         : (n <= 80 ? max(360, 28 * n)
                                    : n <= 180 ? max(260, 18 * n)
                                               : n <= 320 ? max(200, 12 * n)
                                                          : max(150, 9 * n));
    p.polishMoves = deep ? max(320, 20 * n)
                         : (n <= 80 ? max(260, 18 * n)
                                    : n <= 180 ? max(190, 12 * n)
                                               : n <= 320 ? max(150, 9 * n)
                                                          : max(120, 7 * n));
    p.intenseMoves = deep ? max(1100, 14 * n)
                          : (n <= 320 ? max(1000, 12 * n)
                                      : max(800, 10 * n));
    p.randomChainTries = deep ? 10 : (n <= 80 ? 9 : n <= 180 ? 7 : n <= 320 ? 5 : 3);
    p.randomTreeTries = deep ? 12 : (n <= 80 ? 11 : n <= 180 ? 8 : n <= 320 ? 6 : 3);
    p.alternatingTries = deep ? 6 : (n <= 80 ? 7 : n <= 180 ? 5 : n <= 320 ? 3 : 2);
    p.guidedT0 = deep ? 2600.0 : 3000.0;
    p.guidedCooling = deep ? 0.936 : 0.92;
    p.refineT0 = deep ? 1400.0 : 1800.0;
    p.refineCooling = deep ? 0.946 : 0.94;
    p.polishT0 = deep ? 480.0 : 420.0;
    p.polishCooling = deep ? 0.962 : 0.96;
    p.intenseT0 = deep ? 1500.0 : 1400.0;
    p.intenseCooling = deep ? 0.966 : 0.965;
    return p;
}

bool useLongBudgetLargeQualityCase() {
    return (int)outerObjs.size() > 250;
}

bool useTimedQualitySearch() {
    return hasExplicitQualityBudget() || useLongBudgetLargeQualityCase();
}

chrono::steady_clock::time_point largeQualityDeadline() {
    return chrono::steady_clock::now() + chrono::minutes(effectiveQualityBudgetMinutes());
}

enum class InitKind {
    LeftChain,
    RightChain,
    SortedWidth,
    SortedHeight,
    SortedArea,
    SortedMaxDim,
    RandomChain,
    RandomTree,
    Alternating,
    ReuseBest,
};

struct InitTask {
    InitKind kind;
    bool descending = false;
};

State buildInitialStateFromTask(const InitTask& task) {
    switch (task.kind) {
        case InitKind::LeftChain:   return initLeftChainState();
        case InitKind::RightChain:  return initRightChainState();
        case InitKind::SortedWidth: return initSortedByWidthState(task.descending);
        case InitKind::SortedHeight:return initSortedByHeightState(task.descending);
        case InitKind::SortedArea:  return initSortedByAreaState(task.descending);
        case InitKind::SortedMaxDim:return initSortedByMaxDimState(task.descending);
        case InitKind::RandomChain: return initRandomChainState();
        case InitKind::RandomTree:  return initRandomTreeState();
        case InitKind::Alternating: return initAlternatingState();
        case InitKind::ReuseBest:   return bestState;
    }
    return initRandomTreeState();
}

bool useHeavyRefine() {
    int n = (int)outerObjs.size();
    return n <= 72;
}

bool useMediumRefine() {
    int n = (int)outerObjs.size();
    return n <= 180;
}

void runMultiStartSA() {
    bestCost = 1e100;
    const SearchProfile profile = currentSearchProfile();
    const int MACRO_ROUNDS = profile.macroRounds;
    long long perfectArea = 0;
    for (const auto& b : baseBlocks) {
        perfectArea += 1LL * b.w * b.h;
    }
    const bool explicitTimedQuality = hasExplicitQualityBudget();
    const bool timedQualitySearch = useTimedQualitySearch();
    const bool longBudgetLargeQuality = useLongBudgetLargeQualityCase();
    const int timedStagnationLimit = explicitTimedQuality ? 6 : 4;
    const chrono::steady_clock::time_point overallDeadline =
        timedQualitySearch ? largeQualityDeadline() : chrono::steady_clock::time_point{};
    int timedNoImproveRounds = 0;

    for (int round = 0; round < MACRO_ROUNDS; round++) {
        initThreadWorkingData();
        if (deadlineReached(timedQualitySearch ? &overallDeadline : nullptr)) break;
        if (bestCost <= perfectArea) break;

        vector<InitTask> tasks;
        int n = (int)outerObjs.size();
        if (longBudgetLargeQuality) {
            tasks.push_back({InitKind::SortedArea, true});
            tasks.push_back({InitKind::SortedArea, false});
            tasks.push_back({InitKind::SortedMaxDim, true});
            tasks.push_back({InitKind::SortedMaxDim, false});
            tasks.push_back({InitKind::SortedWidth, true});
            tasks.push_back({InitKind::SortedHeight, true});
            tasks.push_back({InitKind::SortedHeight, false});
            tasks.push_back({InitKind::RandomTree});
            tasks.push_back({InitKind::RandomChain});
            tasks.push_back({InitKind::Alternating});
        } else {
            tasks.push_back({InitKind::LeftChain});
            tasks.push_back({InitKind::RightChain});
            tasks.push_back({InitKind::SortedWidth, true});
            tasks.push_back({InitKind::SortedWidth, false});
            tasks.push_back({InitKind::SortedHeight, true});
            tasks.push_back({InitKind::SortedHeight, false});
            tasks.push_back({InitKind::SortedArea, true});
            tasks.push_back({InitKind::SortedArea, false});
            tasks.push_back({InitKind::SortedMaxDim, true});
            tasks.push_back({InitKind::SortedMaxDim, false});
        }

        const int RANDOM_CHAIN_TRIES = profile.randomChainTries;
        const int RANDOM_TREE_TRIES = profile.randomTreeTries;
        const int ALT_TRIES = profile.alternatingTries;

        for (int i = 0; i < RANDOM_CHAIN_TRIES; i++) tasks.push_back({InitKind::RandomChain});
        for (int i = 0; i < RANDOM_TREE_TRIES; i++)  tasks.push_back({InitKind::RandomTree});
        for (int i = 0; i < ALT_TRIES; i++)          tasks.push_back({InitKind::Alternating});
        if (round > 0 && bestCost < 1e99) tasks.push_back({InitKind::ReuseBest});

        const int TOP_K = profile.topK;
        vector<future<Candidate>> initialFutures;
        initialFutures.reserve(tasks.size());
        chrono::steady_clock::time_point guidedDeadline = overallDeadline;
        if (timedQualitySearch) {
            guidedDeadline = chrono::steady_clock::now() + chrono::minutes(max(1, gQualityBudgetMinutes / 3));
            if (guidedDeadline > overallDeadline) guidedDeadline = overallDeadline;
        }

        for (const auto& task : tasks) {
            initialFutures.push_back(async(launch::async, [task, guidedDeadline, longBudgetLargeQuality, timedQualitySearch, profile]() -> Candidate {
                initThreadWorkingData();
                if (deadlineReached(timedQualitySearch ? &guidedDeadline : nullptr)) {
                    return {buildInitialStateFromTask(task), 1e100};
                }
                State init = buildInitialStateFromTask(task);
                Candidate best = {init, evaluateAreaGuided(init)};
                auto [candState, candCost] = simulatedAnnealing(
                    init,
                    evaluateAreaGuided,
                    profile.guidedT0,
                    profile.guidedCooling,
                    longBudgetLargeQuality ? max(1200, 24 * (int)outerObjs.size()) : profile.guidedMoves,
                    timedQualitySearch ? &guidedDeadline : nullptr
                );
                Candidate sa = {candState, candCost};
                return betterCandidate(sa, best) ? sa : best;
            }));
        }

        vector<Candidate> topBalanced;
        topBalanced.reserve(tasks.size());
        for (auto& fut : initialFutures) topBalanced.push_back(fut.get());

        for (const auto& cand : topBalanced) {
            updateBestCandidate(cand);
        }

        if (timedQualitySearch && deadlineReached(&overallDeadline)) break;

        sort(topBalanced.begin(), topBalanced.end(),
             [](const Candidate& a, const Candidate& b) {
                 return betterCandidate(a, b);
             });
        if ((int)topBalanced.size() > TOP_K) topBalanced.resize(TOP_K);
        auto refineSeed = [&](const Candidate& seed) -> Candidate {
            initThreadWorkingData();
            State cur = seed.s;
            Candidate best = seed;
            optimizeAllSmallGroupsExact(cur, timedQualitySearch ? &overallDeadline : nullptr);
            optimizeAllGroupBreaksExact(cur, timedQualitySearch ? &overallDeadline : nullptr);
            if (deadlineReached(timedQualitySearch ? &overallDeadline : nullptr)) {
                best.score = evaluateAreaOnly(cur);
                best.s = cur;
                return best;
            }

            auto [refinedState, refinedCost] = simulatedAnnealing(
                cur,
                evaluateAreaOnly,
                profile.refineT0,
                profile.refineCooling,
                longBudgetLargeQuality ? max(1000, 20 * (int)outerObjs.size()) : profile.refineMoves,
                timedQualitySearch ? &overallDeadline : nullptr
            );

            optimizeAllSmallGroupsExact(refinedState, timedQualitySearch ? &overallDeadline : nullptr);
            optimizeAllGroupBreaksExact(refinedState, timedQualitySearch ? &overallDeadline : nullptr);
            localRefine(refinedState, timedQualitySearch ? &overallDeadline : nullptr);
            if (deadlineReached(timedQualitySearch ? &overallDeadline : nullptr)) {
                Candidate refined = {refinedState, evaluateAreaOnly(refinedState)};
                return betterCandidate(refined, best) ? refined : best;
            }

            auto [polishedState, polishedCost] = simulatedAnnealing(
                refinedState,
                evaluateAreaOnly,
                profile.polishT0,
                profile.polishCooling,
                longBudgetLargeQuality ? max(700, 12 * (int)outerObjs.size()) : profile.polishMoves,
                timedQualitySearch ? &overallDeadline : nullptr
            );

            optimizeAllSmallGroupsExact(polishedState, timedQualitySearch ? &overallDeadline : nullptr);
            optimizeAllGroupBreaksExact(polishedState, timedQualitySearch ? &overallDeadline : nullptr);
            localRefine(polishedState, timedQualitySearch ? &overallDeadline : nullptr);
            optimizeAllSmallGroupsExact(polishedState, timedQualitySearch ? &overallDeadline : nullptr);
            optimizeAllGroupBreaksExact(polishedState, timedQualitySearch ? &overallDeadline : nullptr);

            double finalArea = evaluateAreaOnly(polishedState);
            Candidate polished = {polishedState, finalArea};
            return betterCandidate(polished, best) ? polished : best;
        };

        if (timedQualitySearch) {
            vector<future<Candidate>> refineFutures;
            refineFutures.reserve(topBalanced.size());

            for (const auto& seed : topBalanced) {
                refineFutures.push_back(async(launch::async, [seed, refineSeed]() -> Candidate {
                    return refineSeed(seed);
                }));
            }

            for (auto& fut : refineFutures) {
                Candidate result = fut.get();
                updateBestCandidate(result);
            }
        } else {
            int nonImproveSeeds = 0;
            for (const auto& seed : topBalanced) {
                Candidate result = refineSeed(seed);
                if (updateBestCandidate(result)) {
                    nonImproveSeeds = 0;
                } else {
                    nonImproveSeeds++;
                    if (nonImproveSeeds >= profile.seedStagnationLimit) {
                        break;
                    }
                }
            }
        }

        if (timedQualitySearch) {
            int stagnationRounds = 0;
            while (chrono::steady_clock::now() < overallDeadline) {
                if (bestCost <= perfectArea) break;
                initThreadWorkingData();
                State cur = bestState;

                optimizeAllSmallGroupsExact(cur, &overallDeadline);
                optimizeAllGroupBreaksExact(cur, &overallDeadline);
                localRefine(cur, &overallDeadline);
                if (deadlineReached(&overallDeadline)) break;

                if (stagnationRounds > 0) {
                    int shakeCount = min((int)cur.tree.size(), 2 + stagnationRounds + (useDeepNeighborhoodRefine() ? 1 : 0));
                    for (int t = 0; t < shakeCount; t++) {
                        if ((t & 15) == 0 && deadlineReached(&overallDeadline)) break;
                        doSwapObjects(cur);
                        doRotate(cur);
                        if (useDeepNeighborhoodRefine()) {
                            doSubtreeReinsert(cur);
                            doSubtreeFlip(cur);
                        }
                        if (!groups.empty()) {
                            doGroupToggleBreak(cur);
                            doGroupToggleSelfPos(cur);
                            if (useDeepNeighborhoodRefine()) {
                                doGroupTogglePairRot(cur);
                                doGroupToggleSelfRot(cur);
                            }
                        }
                    }
                }

                auto [intenseState, intenseCost] = simulatedAnnealing(
                    cur,
                    evaluateAreaOnly,
                    profile.intenseT0,
                    profile.intenseCooling,
                    longBudgetLargeQuality ? max(1400, 16 * n) : profile.intenseMoves,
                    &overallDeadline
                );

                optimizeAllSmallGroupsExact(intenseState, &overallDeadline);
                optimizeAllGroupBreaksExact(intenseState, &overallDeadline);
                localRefine(intenseState, &overallDeadline);

                Candidate intense = {intenseState, evaluateAreaOnly(intenseState)};
                if (updateBestCandidate(intense)) {
                    stagnationRounds = 0;
                    timedNoImproveRounds = 0;
                } else {
                    stagnationRounds++;
                    timedNoImproveRounds++;
                    if (stagnationRounds >= timedStagnationLimit ||
                        timedNoImproveRounds >= timedStagnationLimit * 2) {
                        break;
                    }
                }
            }
        }

        if (timedQualitySearch && timedNoImproveRounds >= timedStagnationLimit * 2) break;
    }

    if (explicitTimedQuality) {
        int n = (int)outerObjs.size();
        int finalTimedStagnation = 0;
        while (chrono::steady_clock::now() < overallDeadline) {
            if (bestCost <= perfectArea) break;
            initThreadWorkingData();
            State cur = bestState;

            int shakeCount = min(max(4, n / 20), max(4, n));
            for (int t = 0; t < shakeCount; t++) {
                if ((t & 15) == 0 && deadlineReached(&overallDeadline)) break;
                doSwapObjects(cur);
                doRotate(cur);
                if ((int)cur.tree.size() > 2) {
                    doSubtreeSwap(cur);
                    doSubtreeFlip(cur);
                }
                if (!groups.empty()) {
                    doGroupToggleBreak(cur);
                    doGroupToggleSelfPos(cur);
                    doGroupTogglePairRot(cur);
                    doGroupToggleSelfRot(cur);
                }
            }

            optimizeAllSmallGroupsExact(cur, &overallDeadline);
            optimizeAllGroupBreaksExact(cur, &overallDeadline);
            localRefine(cur, &overallDeadline);
            if (deadlineReached(&overallDeadline)) break;

            auto [timedState, timedCost] = simulatedAnnealing(
                cur,
                evaluateAreaOnly,
                2200.0,
                0.972,
                max(1500, 18 * n),
                &overallDeadline
            );

            optimizeAllSmallGroupsExact(timedState, &overallDeadline);
            optimizeAllGroupBreaksExact(timedState, &overallDeadline);
            localRefine(timedState, &overallDeadline);

            Candidate timedCand = {timedState, evaluateAreaOnly(timedState)};
            if (updateBestCandidate(timedCand)) {
                finalTimedStagnation = 0;
            } else {
                finalTimedStagnation++;
                if (finalTimedStagnation >= timedStagnationLimit) break;
            }
        }
    }

    initThreadWorkingData();
    packState(bestState);
}

void buildConservativeLegalPlacement() {
    initThreadWorkingData();
    useFinalOutCoords = true;
    finalOutX.assign(blocks.size(), 0.0);
    finalOutY.assign(blocks.size(), 0.0);

    for (auto& b : blocks) {
        b.rot = false;
        b.x = 0;
        b.y = 0;
    }

    int curY = 0;
    for (int gid = 0; gid < (int)groups.size(); gid++) {
        const auto& g = groups[gid];
        int axis2 = 0;
        for (const auto& sp : g.pairs) {
            int a = sp.a;
            int b = sp.b;
            bool rot = false;
            if (!pairRotationLegal(a, b, rot) && pairRotationLegal(a, b, true)) rot = true;
            axis2 = max(axis2, 2 * blockWidthByRot(a, rot));
        }
        for (int id : g.selfs) {
            axis2 = max(axis2, blockWidthByRot(id, false));
        }
        if (axis2 == 0) axis2 = 1;

        groups[gid].axis_x2 = axis2;

        for (const auto& sp : g.pairs) {
            int a = sp.a;
            int b = sp.b;
            bool rot = false;
            if (!pairRotationLegal(a, b, rot) && pairRotationLegal(a, b, true)) rot = true;

            int w = blockWidthByRot(a, rot);
            int h = blockHeightByRot(a, rot);
            blocks[a].rot = rot;
            blocks[b].rot = rot;
            blocks[a].x = 0;
            blocks[b].x = axis2 - w;
            blocks[a].y = curY;
            blocks[b].y = curY;
            finalOutX[a] = 0.0;
            finalOutX[b] = axis2 - w;
            finalOutY[a] = curY;
            finalOutY[b] = curY;
            curY += h;
        }

        for (int id : g.selfs) {
            bool rot = false;
            int w = blockWidthByRot(id, rot);
            int h = blockHeightByRot(id, rot);
            double x = (axis2 - w) / 2.0;
            blocks[id].rot = rot;
            blocks[id].x = (int)floor(x);
            blocks[id].y = curY;
            finalOutX[id] = x;
            finalOutY[id] = curY;
            curY += h;
        }
    }

    for (int id = 0; id < (int)blocks.size(); id++) {
        if (blockOwnerGroup[id] != -1) continue;
        blocks[id].rot = false;
        blocks[id].x = 0;
        blocks[id].y = curY;
        finalOutX[id] = 0.0;
        finalOutY[id] = curY;
        curY += blocks[id].height();
    }
}

void ensureLegalPlacement() {
    useFinalOutCoords = false;
    if (checkNoOverlap() && checkSymmetryAll()) return;
    buildConservativeLegalPlacement();
}

/* =========================================================
 * Output / debug
 * ========================================================= */

bool writeOutput(const string& filename) {
    ofstream fout(filename, ios::binary);
    if (!fout) return false;

    auto writeCoord = [&](double v) {
        double rounded = round(v);
        if (fabs(v - rounded) < 1e-9) {
            fout << (long long)rounded;
        } else {
            fout.setf(ios::fixed, ios::floatfield);
            fout.precision(1);
            fout << v;
            fout.unsetf(ios::floatfield);
            fout.precision(6);
        }
    };

    for (int i = 0; i < (int)blocks.size(); i++) {
        const auto& b = blocks[i];
        fout << b.name << " ";
        if (useFinalOutCoords) writeCoord(finalOutX[i]);
        else fout << b.x;
        fout << " ";
        if (useFinalOutCoords) writeCoord(finalOutY[i]);
        else fout << b.y;
        fout << " " << (b.rot ? 1 : 0) << "\n";
    }
    return true;
}

/* =========================================================
 * Main
 * ========================================================= */

int main(int argc, char* argv[]) {
    auto startTime = chrono::steady_clock::now();

    string inputFile = "block_100.in";
    string outputFile = "output.dat";
    auto parseModeArg = [](const string& mode) -> bool {
        if (mode == "quality") {
            gSearchMode = SearchMode::Quality;
            gQualityBudgetMinutes = 58;
            gQualityBudgetExplicit = false;
            return true;
        }
        const string prefix = "quality:";
        if (mode.rfind(prefix, 0) == 0) {
            string mins = mode.substr(prefix.size());
            if (mins.empty()) return false;
            for (char c : mins) {
                if (!isdigit((unsigned char)c)) return false;
            }
            int budget = stoi(mins);
            if (budget <= 0) return false;
            gSearchMode = SearchMode::Quality;
            gQualityBudgetMinutes = budget;
            gQualityBudgetExplicit = true;
            return true;
        }
        return false;
    };

    if (argc == 4) {
        inputFile = argv[1];
        outputFile = argv[2];
        string mode = argv[3];
        if (!parseModeArg(mode)) {
            cerr << "Unknown mode: " << mode << "\n";
            cerr << "Usage: " << argv[0] << " [input.dat output.dat [quality|quality:MINUTES]]\n";
            return 1;
        }
    } else if (argc == 3) {
        inputFile = argv[1];
        outputFile = argv[2];
    } else if (argc != 1) {
        cerr << "Usage: " << argv[0] << " [input.dat output.dat [quality|quality:MINUTES]]\n";
        return 1;
    }

    if (!readInput(inputFile)) {
        cerr << "Failed to read input.\n";
        return 1;
    }

    initThreadWorkingData();
    buildConservativeLegalPlacement();

    cout << "Chip area   = " << chipArea() << "\n";
    auto endTime = chrono::steady_clock::now();
    double elapsedSec = chrono::duration<double>(endTime - startTime).count();
    cout << "Execution time = " << elapsedSec << " s\n";

    if (!writeOutput(outputFile)) {
        cerr << "Failed to write output.\n";
        return 1;
    }

    return 0;
}
