#include <bits/stdc++.h>
using namespace std;

struct Inst {
    string name;
    long long x = 0, y = 0, w = 0, h = 0;
    string orient = "R0";
    string type;
    long long newX = 0, newY = 0;
    int id = 0;
};

struct Row {
    long long y = 0;
    map<long long, long long> freeSegs; // [l, r)
};

long long dieLLX, dieLLY, dieURX, dieURY;
long long siteW, siteH, dbu;
chrono::steady_clock::time_point programStart;
double softTimeLimitSec = 28.0 * 60.0;

bool timeAlmostUp() {
    if (programStart.time_since_epoch().count() == 0) return false;
    double elapsed = chrono::duration<double>(chrono::steady_clock::now() - programStart).count();
    return elapsed >= softTimeLimitSec;
}

long long alignUp(long long v, long long base, long long step) {
    if (step <= 0) return v;
    if (v <= base) return base;
    return base + ((v - base + step - 1) / step) * step;
}

long long alignDown(long long v, long long base, long long step) {
    if (step <= 0) return v;
    if (v <= base) return base;
    return base + ((v - base) / step) * step;
}

int rowIdFromY(long long y, int numRows) {
    if (numRows <= 0) return 0;
    int id = (int)((y - dieLLY) / siteH);
    return min(max(id, 0), numRows - 1);
}

struct DensityGrid {
    long long g = 1;
    int nx = 0, ny = 0;
    double threshold = 0.5;
    vector<double> area;

    void init(double thresholdPercent) {
        g = max(1LL, 10LL * dbu);
        nx = max(1LL, (dieURX - dieLLX + g - 1) / g);
        ny = max(1LL, (dieURY - dieLLY + g - 1) / g);
        threshold = max(0.01, thresholdPercent / 100.0);
        area.assign((size_t)nx * (size_t)ny, 0.0);
    }

    double overlapArea(int bx, int by, long long x, long long y, long long w, long long h) const {
        long long gx0 = dieLLX + (long long)bx * g;
        long long gx1 = min(dieURY > dieLLY ? dieURX : dieURX, gx0 + g);
        long long gy0 = dieLLY + (long long)by * g;
        long long gy1 = min(dieURY, gy0 + g);
        long long ox = max(0LL, min(x + w, gx1) - max(x, gx0));
        long long oy = max(0LL, min(y + h, gy1) - max(y, gy0));
        return (double)ox * (double)oy;
    }

    void appendTouchedBins(long long x, long long y, long long w, long long h, vector<int> &ids) const {
        if (nx <= 0 || ny <= 0) return;
        int bx0 = max(0LL, (x - dieLLX) / g);
        int by0 = max(0LL, (y - dieLLY) / g);
        int bx1 = min((long long)nx - 1, (x + w - 1 - dieLLX) / g);
        int by1 = min((long long)ny - 1, (y + h - 1 - dieLLY) / g);
        if (bx0 > bx1 || by0 > by1) return;
        ids.reserve(ids.size() + (size_t)(bx1 - bx0 + 1) * (size_t)(by1 - by0 + 1));
        for (int by = by0; by <= by1; ++by) {
            int base = by * nx;
            for (int bx = bx0; bx <= bx1; ++bx) ids.push_back(base + bx);
        }
    }

    double overflowPenalty(long long x, long long y, long long w, long long h) const {
        vector<int> ids;
        appendTouchedBins(x, y, w, h, ids);
        if (ids.empty()) return 0.0;

        double cap = (double)g * (double)g * threshold;
        double penalty = 0.0;
        for (int id : ids) {
            int by = id / nx;
            int bx = id - by * nx;
            double add = overlapArea(bx, by, x, y, w, h);
            double after = area[id] + add;
            if (after > cap) penalty += (after - cap) / cap;
        }
        return penalty / (double)ids.size() * (double)g;
    }

    void add(long long x, long long y, long long w, long long h) {
        vector<int> ids;
        appendTouchedBins(x, y, w, h, ids);
        for (int id : ids) {
            int by = id / nx;
            int bx = id - by * nx;
            area[id] += overlapArea(bx, by, x, y, w, h);
        }
    }
};

void subtractInterval(map<long long, long long> &freeSegs, long long l, long long r) {
    if (l >= r) return;

    auto it = freeSegs.upper_bound(l);
    if (it != freeSegs.begin()) --it;

    vector<pair<long long, long long>> addBack;
    vector<long long> eraseKeys;

    while (it != freeSegs.end()) {
        long long a = it->first;
        long long b = it->second;

        if (b <= l) {
            ++it;
            continue;
        }
        if (a >= r) break;

        eraseKeys.push_back(a);
        if (a < l) addBack.push_back({a, l});
        if (r < b) addBack.push_back({r, b});
        ++it;
    }

    for (long long k : eraseKeys) freeSegs.erase(k);
    for (auto [a, b] : addBack) {
        if (a < b) freeSegs[a] = b;
    }
}

void addFreeInterval(map<long long, long long> &freeSegs, long long l, long long r) {
    if (l >= r) return;

    auto it = freeSegs.lower_bound(l);
    if (it != freeSegs.begin()) {
        auto p = it;
        --p;
        if (p->second >= l) {
            l = min(l, p->first);
            r = max(r, p->second);
            it = freeSegs.erase(p);
        }
    }

    while (it != freeSegs.end() && it->first <= r) {
        r = max(r, it->second);
        it = freeSegs.erase(it);
    }

    freeSegs[l] = r;
}

bool xIsFree(const Row &row, long long x, long long w) {
    auto it = row.freeSegs.upper_bound(x);
    if (it == row.freeSegs.begin()) return false;
    --it;
    return it->first <= x && x + w <= it->second;
}

bool intervalCandidate(long long l, long long r, const Inst &cell, long long &x) {
    long long lo = alignUp(l, dieLLX, siteW);
    long long hi = alignDown(r - cell.w, dieLLX, siteW);
    if (lo > hi) return false;

    long long cand = min(max(cell.x, lo), hi);
    cand = alignDown(cand, dieLLX, siteW);
    if (cand < lo) cand += siteW;
    if (cand > hi) cand = hi;

    if (cand < lo || cand > hi) return false;
    x = cand;
    return true;
}

vector<long long> candidateXsSingleRow(const Row &row, const Inst &cell, int limit) {
    vector<long long> xs;
    if (row.freeSegs.empty()) return xs;

    vector<pair<long long, long long>> segs;
    segs.reserve(row.freeSegs.size());
    for (auto [l, r] : row.freeSegs) {
        long long x;
        if (intervalCandidate(l, r, cell, x)) {
            segs.push_back({l, r});
        }
    }

    sort(segs.begin(), segs.end(), [&](const auto &a, const auto &b) {
        long long ax, bx;
        intervalCandidate(a.first, a.second, cell, ax);
        intervalCandidate(b.first, b.second, cell, bx);
        return llabs(ax - cell.x) < llabs(bx - cell.x);
    });

    int used = 0;
    for (auto [l, r] : segs) {
        if (used++ >= limit) break;
        long long x;
        if (!intervalCandidate(l, r, cell, x)) continue;

        xs.push_back(x);
        xs.push_back(alignUp(l, dieLLX, siteW));
        xs.push_back(alignDown(r - cell.w, dieLLX, siteW));

        // A few local candidates around original x.
        long long base = alignDown(cell.x, dieLLX, siteW);
        for (int d = -2; d <= 2; ++d) {
            long long cand = base + (long long)d * siteW;
            if (cand >= alignUp(l, dieLLX, siteW) && cand <= alignDown(r - cell.w, dieLLX, siteW)) {
                xs.push_back(cand);
            }
        }
    }

    sort(xs.begin(), xs.end());
    xs.erase(unique(xs.begin(), xs.end()), xs.end());
    return xs;
}

vector<pair<long long, long long>> intersectIntervals(
    const vector<pair<long long, long long>> &a,
    const map<long long, long long> &b
) {
    vector<pair<long long, long long>> res;
    auto it = b.begin();

    for (auto [l, r] : a) {
        while (it != b.end() && it->second <= l) ++it;
        auto jt = it;
        while (jt != b.end() && jt->first < r) {
            long long nl = max(l, jt->first);
            long long nr = min(r, jt->second);
            if (nl < nr) res.push_back({nl, nr});
            ++jt;
        }
    }

    return res;
}

vector<long long> candidateXsMultiRow(const vector<Row> &rows, int rowStart, int rowSpan, const Inst &cell, int limit) {
    vector<pair<long long, long long>> common;
    for (auto [l, r] : rows[rowStart].freeSegs) common.push_back({l, r});

    for (int i = 1; i < rowSpan && !common.empty(); ++i) {
        common = intersectIntervals(common, rows[rowStart + i].freeSegs);
    }

    vector<pair<long long, long long>> feasible;
    for (auto seg : common) {
        long long x;
        if (intervalCandidate(seg.first, seg.second, cell, x)) feasible.push_back(seg);
    }

    sort(feasible.begin(), feasible.end(), [&](const auto &a, const auto &b) {
        long long ax, bx;
        intervalCandidate(a.first, a.second, cell, ax);
        intervalCandidate(b.first, b.second, cell, bx);
        return llabs(ax - cell.x) < llabs(bx - cell.x);
    });

    vector<long long> xs;
    for (int i = 0; i < (int)feasible.size() && i < limit; ++i) {
        long long x;
        auto [l, r] = feasible[i];
        if (!intervalCandidate(l, r, cell, x)) continue;

        xs.push_back(x);
        xs.push_back(alignUp(l, dieLLX, siteW));
        xs.push_back(alignDown(r - cell.w, dieLLX, siteW));
    }

    sort(xs.begin(), xs.end());
    xs.erase(unique(xs.begin(), xs.end()), xs.end());
    return xs;
}

double placementCost(const Inst &cell, long long x, long long y, double alpha, const DensityGrid &density) {
    double disp = (double)llabs(x - cell.x) + (double)llabs(y - cell.y);
    double congestion = density.overflowPenalty(x, y, cell.w, cell.h);
    return disp + (1.0 - alpha) * 0.001 * congestion;
}

bool findBestPlacement(
    const vector<Row> &rows,
    const Inst &cell,
    double alpha,
    const DensityGrid &density,
    long long &bestX,
    long long &bestY
) {
    int numRows = (int)rows.size();
    int rowSpan = (int)((cell.h + siteH - 1) / siteH);
    if (rowSpan <= 0 || rowSpan > numRows) return false;

    int originalRow = rowIdFromY(cell.y, numRows);
    int intervalLimit = alpha < 0.35 ? 128 : (alpha < 0.65 ? 96 : 64);

    bool found = false;
    double bestCost = numeric_limits<double>::infinity();

    auto tryRow = [&](int r) {
        if (r < 0 || r + rowSpan > numRows) return;
        long long y = rows[r].y;

        // Important: multi-height cells must end inside die.
        if (y < dieLLY || y + cell.h > dieURY) return;

        vector<long long> xs;
        if (rowSpan == 1) xs = candidateXsSingleRow(rows[r], cell, intervalLimit);
        else xs = candidateXsMultiRow(rows, r, rowSpan, cell, intervalLimit);

        for (long long x : xs) {
            if (x < dieLLX || x + cell.w > dieURX) continue;

            bool ok = true;
            for (int rr = r; rr < r + rowSpan; ++rr) {
                if (!xIsFree(rows[rr], x, cell.w)) {
                    ok = false;
                    break;
                }
            }
            if (!ok) continue;

            double cost = placementCost(cell, x, y, alpha, density);
            if (!found || cost < bestCost) {
                found = true;
                bestCost = cost;
                bestX = x;
                bestY = y;
            }
        }
    };

    // Search all rows. This is safer than early break for dense hidden cases.
    for (int d = 0; d < numRows; ++d) {
        int r1 = originalRow + d;
        int r2 = originalRow - d;
        tryRow(r1);
        if (d != 0) tryRow(r2);
        if (timeAlmostUp()) break;
    }

    return found;
}

void occupy(vector<Row> &rows, const Inst &cell, DensityGrid &density) {
    int r0 = rowIdFromY(cell.newY, (int)rows.size());
    int rowSpan = (int)((cell.h + siteH - 1) / siteH);

    for (int r = r0; r < r0 + rowSpan && r < (int)rows.size(); ++r) {
        subtractInterval(rows[r].freeSegs, cell.newX, cell.newX + cell.w);
    }

    density.add(cell.newX, cell.newY, cell.w, cell.h);
}

// -----------------------------------------------------------------------------
// Final safety repair.
// This function does NOT optimize score. Its purpose is to guarantee that the
// output model is self-consistent: site-aligned, inside die, and non-overlapping
// according to our row/free-segment model. It rebuilds free rows from scratch and
// places every cell again. This avoids the previous bug where refinement passes
// made an initially legal result illegal.
// -----------------------------------------------------------------------------
int rebuildLegalPlacement(vector<Inst> &cells, const vector<Row> &baseRows, double alpha, double threshold) {
    vector<Row> rows = baseRows;
    DensityGrid density;
    density.init(threshold);

    vector<int> order(cells.size());
    iota(order.begin(), order.end(), 0);

    sort(order.begin(), order.end(), [&](int ia, int ib) {
        const Inst &a = cells[ia];
        const Inst &b = cells[ib];

        bool am = a.h > siteH;
        bool bm = b.h > siteH;
        if (am != bm) return am > bm;

        int ra = rowIdFromY(a.y, (int)baseRows.size());
        int rb = rowIdFromY(b.y, (int)baseRows.size());
        if (ra != rb) return ra < rb;

        if (a.x != b.x) return a.x < b.x;
        if (a.h != b.h) return a.h > b.h;
        if (a.w != b.w) return a.w > b.w;
        return a.id < b.id;
    });

    int failed = 0;

    for (int idx : order) {
        Inst &cell = cells[idx];

        long long x = 0, y = 0;
        if (findBestPlacement(rows, cell, alpha, density, x, y)) {
            cell.newX = x;
            cell.newY = y;
            occupy(rows, cell, density);
            continue;
        }

        // Last-resort sequential scan. This avoids dumping failed cells at dieLLX/dieLLY.
        bool placed = false;
        int rowSpan = (int)((cell.h + siteH - 1) / siteH);
        for (int r = 0; r + rowSpan <= (int)rows.size() && !placed; ++r) {
            if (rows[r].y + cell.h > dieURY) continue;

            vector<pair<long long, long long>> common;
            for (auto [l, rr] : rows[r].freeSegs) common.push_back({l, rr});
            for (int k = 1; k < rowSpan && !common.empty(); ++k) {
                common = intersectIntervals(common, rows[r + k].freeSegs);
            }

            for (auto [l, rr] : common) {
                long long lo = alignUp(l, dieLLX, siteW);
                long long hi = alignDown(rr - cell.w, dieLLX, siteW);
                if (lo <= hi) {
                    cell.newX = lo;
                    cell.newY = rows[r].y;
                    occupy(rows, cell, density);
                    placed = true;
                    break;
                }
            }
        }

        if (!placed) {
            ++failed;

            // Keep it on a legal site as much as possible, but report failure.
            int r = rowIdFromY(cell.y, (int)rows.size());
            cell.newX = min(max(alignDown(cell.x, dieLLX, siteW), dieLLX), dieURX - cell.w);
            cell.newY = rows[r].y;

            cerr << "[WARN] Cannot legally place cell: " << cell.name << "\n";
        }

        if ((idx & 1023) == 0 && timeAlmostUp()) {
            cerr << "[INFO] Soft time limit reached during legal rebuild.\n";
            break;
        }
    }

    return failed;
}

// Optional local refinement that is guaranteed not to change row membership.
// It only repacks cells inside each row segment. Disabled by default for safety.
long long placeSiteMultipleOrder(vector<Inst> &cells, const vector<int> &order, long long l, long long r, bool commit) {
    if (order.empty()) return 0;

    long long totalSites = 0;
    vector<long long> widthSites(order.size());

    for (int i = 0; i < (int)order.size(); ++i) {
        const Inst &cell = cells[order[i]];
        if (cell.w % siteW != 0) return LLONG_MAX / 4;
        widthSites[i] = cell.w / siteW;
        totalSites += widthSites[i];
    }

    long long segL = alignUp(l, dieLLX, siteW);
    long long segLast = alignDown(r - totalSites * siteW, dieLLX, siteW);
    if (segL > segLast) return LLONG_MAX / 4;

    long long loP = (segL - dieLLX) / siteW;
    long long hiP = (segLast - dieLLX) / siteW;

    vector<long long> prefix(order.size(), 0);
    for (int i = 1; i < (int)order.size(); ++i) {
        prefix[i] = prefix[i - 1] + widthSites[i - 1];
    }

    struct Block {
        int first = 0, last = 0;
        vector<long long> vals;
        long long median = 0;
    };

    auto updateMedian = [](Block &b) {
        sort(b.vals.begin(), b.vals.end());
        b.median = b.vals[(b.vals.size() - 1) / 2];
    };

    vector<Block> blocks;

    for (int i = 0; i < (int)order.size(); ++i) {
        long long targetSite = (cells[order[i]].x - dieLLX + siteW / 2) / siteW;
        Block b;
        b.first = b.last = i;
        b.vals.push_back(targetSite - prefix[i]);
        updateMedian(b);
        blocks.push_back(b);

        while (blocks.size() >= 2) {
            Block &a = blocks[blocks.size() - 2];
            Block &c = blocks[blocks.size() - 1];
            if (a.median <= c.median) break;

            a.last = c.last;
            a.vals.insert(a.vals.end(), c.vals.begin(), c.vals.end());
            updateMedian(a);
            blocks.pop_back();
        }
    }

    vector<long long> p(order.size());
    for (auto &b : blocks) {
        long long value = min(max(b.median, loP), hiP);
        for (int i = b.first; i <= b.last; ++i) p[i] = value;
    }

    long long cost = 0;
    for (int i = 0; i < (int)order.size(); ++i) {
        long long x = dieLLX + (p[i] + prefix[i]) * siteW;
        cost += llabs(x - cells[order[i]].x);
        if (commit) cells[order[i]].newX = x;
    }

    return cost;
}

void safeRefineSingleHeightRows(vector<Inst> &cells, const vector<Row> &baseRows) {
    // This is conservative. It repacks only cells that are already in the same
    // free segment and have exactly one-row height. It does not move cells across rows.
    int numRows = (int)baseRows.size();
    vector<vector<int>> singleByRow(numRows);
    vector<vector<int>> fixedByRow(numRows);

    for (int i = 0; i < (int)cells.size(); ++i) {
        int r = rowIdFromY(cells[i].newY, numRows);
        if (cells[i].h == siteH && cells[i].newY == baseRows[r].y) {
            singleByRow[r].push_back(i);
        } else {
            int rowSpan = (int)((cells[i].h + siteH - 1) / siteH);
            for (int rr = r; rr < r + rowSpan && rr < numRows; ++rr) {
                fixedByRow[rr].push_back(i);
            }
        }
    }

    for (int r = 0; r < numRows; ++r) {
        if (timeAlmostUp()) return;

        map<long long, long long> freeSegs = baseRows[r].freeSegs;

        for (int idx : fixedByRow[r]) {
            const Inst &cell = cells[idx];
            subtractInterval(freeSegs, cell.newX, cell.newX + cell.w);
        }

        for (auto [l, rr] : freeSegs) {
            vector<int> ids;
            for (int idx : singleByRow[r]) {
                const Inst &cell = cells[idx];
                if (l <= cell.newX && cell.newX + cell.w <= rr) ids.push_back(idx);
            }

            if (ids.empty()) continue;
            sort(ids.begin(), ids.end(), [&](int ia, int ib) {
                if (cells[ia].newX != cells[ib].newX) return cells[ia].newX < cells[ib].newX;
                return cells[ia].id < cells[ib].id;
            });

            // Only commit if the segment has enough capacity.
            long long totalW = 0;
            bool ok = true;
            for (int idx : ids) {
                if (cells[idx].w % siteW != 0) ok = false;
                totalW += cells[idx].w;
            }
            if (!ok || alignUp(l, dieLLX, siteW) + totalW > rr) continue;
            placeSiteMultipleOrder(cells, ids, l, rr, true);
        }
    }
}

bool selfCheckNoOverlap(const vector<Inst> &cells, const vector<Row> &baseRows) {
    int numRows = (int)baseRows.size();

    vector<vector<int>> byRow(numRows);
    for (int i = 0; i < (int)cells.size(); ++i) {
        const Inst &c = cells[i];

        if (c.newX < dieLLX || c.newX + c.w > dieURX) {
            cerr << "[SELF_CHECK] Out of die X: " << c.name << "\n";
            return false;
        }
        if (c.newY < dieLLY || c.newY + c.h > dieURY) {
            cerr << "[SELF_CHECK] Out of die Y: " << c.name << "\n";
            return false;
        }
        if ((c.newX - dieLLX) % siteW != 0) {
            cerr << "[SELF_CHECK] X not site aligned: " << c.name << "\n";
            return false;
        }
        if ((c.newY - dieLLY) % siteH != 0) {
            cerr << "[SELF_CHECK] Y not row aligned: " << c.name << "\n";
            return false;
        }

        int r0 = rowIdFromY(c.newY, numRows);
        int span = (int)((c.h + siteH - 1) / siteH);
        for (int r = r0; r < r0 + span && r < numRows; ++r) {
            byRow[r].push_back(i);
        }
    }

    for (int r = 0; r < numRows; ++r) {
        auto &v = byRow[r];
        sort(v.begin(), v.end(), [&](int ia, int ib) {
            if (cells[ia].newX != cells[ib].newX) return cells[ia].newX < cells[ib].newX;
            return cells[ia].id < cells[ib].id;
        });

        long long lastR = LLONG_MIN;
        string lastName;
        for (int idx : v) {
            const Inst &c = cells[idx];
            if (c.newX < lastR) {
                cerr << "[SELF_CHECK] Overlap in row " << r << ": " << lastName << " and " << c.name << "\n";
                return false;
            }
            lastR = c.newX + c.w;
            lastName = c.name;
        }
    }

    return true;
}

int main(int argc, char **argv) {
    programStart = chrono::steady_clock::now();

    if (const char *envLimit = getenv("LEGALIZER_SOFT_LIMIT_SEC")) {
        double v = atof(envLimit);
        if (v > 0.0) softTimeLimitSec = v;
    }

    if (argc != 5) {
        cerr << "Usage: ./Legalizer <alpha> <threshold> <input.gp> <output.tcl>\n";
        return 1;
    }

    double alpha = atof(argv[1]);
    double threshold = atof(argv[2]);
    alpha = min(1.0, max(0.0, alpha));

    string inputFile = argv[3];
    string outputFile = argv[4];

    ifstream fin(inputFile);
    if (!fin) {
        cerr << "Cannot open input file: " << inputFile << "\n";
        return 1;
    }

    string key;
    fin >> key >> dbu;
    fin >> key >> dieLLX >> dieLLY;
    fin >> key >> dieURX >> dieURY;
    fin >> key >> siteW;
    fin >> key >> siteH;

    string line;
    getline(fin, line);
    while (getline(fin, line)) {
        if (!line.empty()) break;
    }

    vector<Inst> cells;
    vector<Inst> obstacles;

    while (getline(fin, line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        vector<string> tok;
        string s;
        while (iss >> s) tok.push_back(s);
        if (tok.size() < 6) continue;

        Inst inst;
        inst.name = tok[0];
        inst.x = stoll(tok[1]);
        inst.y = stoll(tok[2]);
        inst.w = stoll(tok[3]);
        inst.h = stoll(tok[4]);

        if (tok.size() >= 7) {
            inst.orient = tok[5];
            inst.type = tok[6];
        } else {
            inst.orient = "R0";
            inst.type = tok[5];
        }

        if (inst.type == "CELL") {
            inst.id = (int)cells.size();
            cells.push_back(inst);
        } else {
            obstacles.push_back(inst);
        }
    }

    int numRows = (int)((dieURY - dieLLY) / siteH);
    if (numRows <= 0) {
        cerr << "Invalid row count.\n";
        return 1;
    }

    vector<Row> baseRows(numRows);
    for (int i = 0; i < numRows; ++i) {
        baseRows[i].y = dieLLY + (long long)i * siteH;
        baseRows[i].freeSegs[dieLLX] = dieURX;
    }

    for (const auto &obs : obstacles) {
        long long ox0 = max(obs.x, dieLLX);
        long long ox1 = min(obs.x + obs.w, dieURX);
        long long oy0 = max(obs.y, dieLLY);
        long long oy1 = min(obs.y + obs.h, dieURY);
        if (ox0 >= ox1 || oy0 >= oy1) continue;

        int r0 = max(0LL, (oy0 - dieLLY) / siteH);
        int r1 = min((long long)numRows - 1, (oy1 - 1 - dieLLY) / siteH);

        for (int r = r0; r <= r1; ++r) {
            subtractInterval(baseRows[r].freeSegs, ox0, ox1);
        }
    }

    int failed = rebuildLegalPlacement(cells, baseRows, alpha, threshold);

    // Conservative refinement only. If it ever causes self-check failure, rebuild again.
    if (failed == 0 && !timeAlmostUp()) {
        vector<Inst> backup = cells;
        safeRefineSingleHeightRows(cells, baseRows);
        if (!selfCheckNoOverlap(cells, baseRows)) {
            cerr << "[INFO] Conservative refine rejected; restoring legal placement.\n";
            cells = backup;
        }
    }

    if (!selfCheckNoOverlap(cells, baseRows)) {
        cerr << "[INFO] Self-check failed after output optimization; rebuilding once more.\n";
        failed = rebuildLegalPlacement(cells, baseRows, alpha, threshold);
    }

    ofstream fout(outputFile);
    if (!fout) {
        cerr << "Cannot open output file: " << outputFile << "\n";
        return 1;
    }

    fout << fixed << setprecision(6);

    sort(cells.begin(), cells.end(), [](const Inst &a, const Inst &b) {
        return a.id < b.id;
    });

    for (const auto &cell : cells) {
        fout << "place_cell -inst_name " << cell.name
             << " -orient R0"
             << " -origin {"
             << (double)cell.newX / (double)dbu << " "
             << (double)cell.newY / (double)dbu << "}\n";
    }

    cerr << "DBU = " << dbu << "\n";
    cerr << "Die = (" << dieLLX << "," << dieLLY << ") to (" << dieURX << "," << dieURY << ")\n";
    cerr << "Site = " << siteW << " x " << siteH << "\n";
    cerr << "Rows = " << numRows << "\n";
    cerr << "Cells = " << cells.size() << "\n";
    cerr << "Obstacles = " << obstacles.size() << "\n";
    cerr << "Failed = " << failed << "\n";
    cerr << "Output written to " << outputFile << "\n";

    return failed == 0 ? 0 : 2;
}
