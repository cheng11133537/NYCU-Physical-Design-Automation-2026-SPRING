#include <bits/stdc++.h>
using namespace std;

struct Vertex {
    string name;
    vector<int> incident_nets;
    int weight = 1;
    int part = -1;
};

struct Net {
    string name;
    vector<int> pins;
};

struct Hypergraph {
    double balance_factor = 0.0;
    vector<Vertex> vertices;
    vector<Net> nets;
    unordered_map<string, int> vertex_id;
};

struct CoarsenResult {
    Hypergraph coarse_hg;
    vector<int> fine_to_coarse;
    vector<vector<int>> coarse_to_fine;
};

struct MoveRecord {
    int vid;
    int from_part;
    int to_part;
    int gain;
};

struct CandidateMove {
    int vid = -1;
    int from_part = -1;
    int to_part = -1;
    int gain = INT_MIN;
    bool valid = false;
};

struct BucketEntry {
    int vid;
    int gain;
    int version;
};

struct CoarsenEdgeScore {
    double score;
    int u, v;
};

class BucketList {
public:
    BucketList() = default;

    void init(int max_gain_bound) {
        max_gain = max(0, max_gain_bound);
        offset = max_gain;
        buckets.assign(2 * max_gain + 1, {});
        current_max_idx = -1;
    }

    void insert(const BucketEntry& e) {
        int clipped_gain = max(-max_gain, min(max_gain, e.gain));
        int idx = clipped_gain + offset;
        buckets[idx].push_back({e.vid, clipped_gain, e.version});
        current_max_idx = max(current_max_idx, idx);
    }

    bool empty() {
        while (current_max_idx >= 0 && buckets[current_max_idx].empty()) {
            current_max_idx--;
        }
        return current_max_idx < 0;
    }

    BucketEntry popBest() {
        while (current_max_idx >= 0 && buckets[current_max_idx].empty()) {
            current_max_idx--;
        }
        if (current_max_idx < 0) return {-1, INT_MIN, -1};

        BucketEntry e = buckets[current_max_idx].back();
        buckets[current_max_idx].pop_back();
        return e;
    }

private:
    int max_gain = 0;
    int offset = 0;
    int current_max_idx = -1;
    vector<vector<BucketEntry>> buckets;
};

class PartitionSolver {
public:
    struct SolutionSnapshot {
        int cutsize = INT_MAX;
        vector<int> parts;
    };

    Hypergraph hg;
    mt19937 rng{random_device{}()};

    // --------------------------------------------------
    // Parsing
    // --------------------------------------------------
    bool parseInput(const string& input_file) {
        ifstream fin(input_file);
        if (!fin.is_open()) {
            cerr << "Error: cannot open input file: " << input_file << "\n";
            return false;
        }

        if (!(fin >> hg.balance_factor)) {
            cerr << "Error: failed to read balance factor.\n";
            return false;
        }

        string token;
        while (fin >> token) {
            if (token != "NET") {
                cerr << "Error: expected token 'NET', got '" << token << "'\n";
                return false;
            }

            Net net;
            if (!(fin >> net.name)) {
                cerr << "Error: failed to read net name.\n";
                return false;
            }

            while (fin >> token) {
                size_t semicolon_pos = token.find(';');
                if (semicolon_pos != string::npos) {
                    string cell_name = token.substr(0, semicolon_pos);
                    if (!cell_name.empty()) {
                        int vid = getOrCreateVertex(cell_name);
                        net.pins.push_back(vid);
                    }
                    break;
                }

                int vid = getOrCreateVertex(token);
                net.pins.push_back(vid);
            }

            if (net.pins.empty()) continue;

            int net_id = (int)hg.nets.size();
            hg.nets.push_back(net);

            for (int v : hg.nets.back().pins) {
                hg.vertices[v].incident_nets.push_back(net_id);
            }
        }

        return true;
    }

    // --------------------------------------------------
    // Basic info
    // --------------------------------------------------
    int numVertices() const {
        return (int)hg.vertices.size();
    }

    int totalWeight() const {
        int sum = 0;
        for (const auto& v : hg.vertices) sum += v.weight;
        return sum;
    }

    int lowerBound() const {
        int W = totalWeight();
        double lb = (1.0 - hg.balance_factor) * W / 3.0;
        return (int)ceil(lb - 1e-9);
    }

    int upperBound() const {
        int W = totalWeight();
        double ub = (1.0 + hg.balance_factor) * W / 3.0;
        return (int)floor(ub + 1e-9);
    }

    array<int, 3> getPartSizes() const {
        array<int, 3> sz = {0, 0, 0};
        for (const auto& v : hg.vertices) {
            if (0 <= v.part && v.part < 3) sz[v.part] += v.weight;
        }
        return sz;
    }

    bool checkAllAssigned() const {
        for (const auto& v : hg.vertices) {
            if (v.part < 0 || v.part >= 3) return false;
        }
        return true;
    }

    bool checkBalance() const {
        if (!checkAllAssigned()) return false;

        auto sz = getPartSizes();
        int lb = lowerBound();
        int ub = upperBound();

        for (int i = 0; i < 3; i++) {
            if (sz[i] < lb || sz[i] > ub) return false;
        }
        return true;
    }

    int computeCutsize() const {
        int cutsize = 0;

        for (const auto& net : hg.nets) {
            bool seen[3] = {false, false, false};

            for (int v : net.pins) {
                int p = hg.vertices[v].part;
                if (p < 0 || p >= 3) return -1;

                if (!seen[p]) {
                    seen[p] = true;
                    if (seen[0] + seen[1] + seen[2] >= 2) {
                        cutsize++;
                        break;
                    }
                }
            }
        }

        return cutsize;
    }

    // --------------------------------------------------
    // Snapshot
    // --------------------------------------------------
    SolutionSnapshot captureSolution(int known_cutsize = INT_MAX) const {
        SolutionSnapshot sol;
        sol.cutsize = (known_cutsize == INT_MAX ? computeCutsize() : known_cutsize);
        sol.parts.resize(hg.vertices.size());
        for (int i = 0; i < (int)hg.vertices.size(); i++) {
            sol.parts[i] = hg.vertices[i].part;
        }
        return sol;
    }

    void restoreSolution(const SolutionSnapshot& sol) {
        if ((int)sol.parts.size() != (int)hg.vertices.size()) {
            return;
        }
        for (int i = 0; i < (int)hg.vertices.size(); i++) {
            hg.vertices[i].part = sol.parts[i];
        }
    }

    // --------------------------------------------------
    // Initial partition
    // --------------------------------------------------
    bool buildRandomBalancedPartition() {
        int n = numVertices();
        int lb = lowerBound();
        int ub = upperBound();

        if (3 * lb > totalWeight() || 3 * ub < totalWeight()) {
            return false;
        }

        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        vector<int> rand_key(n, 0);
        for (int i = 0; i < n; i++) rand_key[i] = (int)(rng() & 0x7fffffff);

        // Diversified initialization modes to reduce multi-start similarity.
        // 0: high-degree first (classic), 1: heavy-first, 2: random-heavy mix, 3: low-degree first.
        int init_mode = (int)(rng() % 4);
        shuffle(order.begin(), order.end(), rng);
        if (init_mode == 0) {
            stable_sort(order.begin(), order.end(), [&](int a, int b) {
                int deg_a = (int)hg.vertices[a].incident_nets.size();
                int deg_b = (int)hg.vertices[b].incident_nets.size();
                if (deg_a != deg_b) return deg_a > deg_b;
                if (hg.vertices[a].weight != hg.vertices[b].weight) {
                    return hg.vertices[a].weight > hg.vertices[b].weight;
                }
                return rand_key[a] < rand_key[b];
            });
        } else if (init_mode == 1) {
            stable_sort(order.begin(), order.end(), [&](int a, int b) {
                if (hg.vertices[a].weight != hg.vertices[b].weight) {
                    return hg.vertices[a].weight > hg.vertices[b].weight;
                }
                int deg_a = (int)hg.vertices[a].incident_nets.size();
                int deg_b = (int)hg.vertices[b].incident_nets.size();
                if (deg_a != deg_b) return deg_a > deg_b;
                return rand_key[a] < rand_key[b];
            });
        } else if (init_mode == 2) {
            stable_sort(order.begin(), order.end(), [&](int a, int b) {
                long long score_a = 1LL * hg.vertices[a].weight * 3 +
                                    (long long)hg.vertices[a].incident_nets.size() +
                                    (rand_key[a] & 31);
                long long score_b = 1LL * hg.vertices[b].weight * 3 +
                                    (long long)hg.vertices[b].incident_nets.size() +
                                    (rand_key[b] & 31);
                if (score_a != score_b) return score_a > score_b;
                return rand_key[a] < rand_key[b];
            });
        } else {
            stable_sort(order.begin(), order.end(), [&](int a, int b) {
                int deg_a = (int)hg.vertices[a].incident_nets.size();
                int deg_b = (int)hg.vertices[b].incident_nets.size();
                if (deg_a != deg_b) return deg_a < deg_b;
                if (hg.vertices[a].weight != hg.vertices[b].weight) {
                    return hg.vertices[a].weight > hg.vertices[b].weight;
                }
                return rand_key[a] < rand_key[b];
            });
        }

        for (auto& v : hg.vertices) v.part = -1;

        array<int, 3> sz = {0, 0, 0};
        const int target_int = totalWeight() / 3;

        auto incrementalCutCost = [&](int vid, int to_part) -> int {
            int delta = 0;
            for (int net_id : hg.vertices[vid].incident_nets) {
                bool seen[3] = {false, false, false};
                int before = 0;
                for (int u : hg.nets[net_id].pins) {
                    if (u == vid) continue;
                    int p = hg.vertices[u].part;
                    if (p >= 0 && !seen[p]) {
                        seen[p] = true;
                        before++;
                    }
                }

                int after = before + (!seen[to_part]);
                delta += (before < 2 && after >= 2);
            }
            return delta;
        };

        auto pickBestPart = [&](int vid, bool require_lb) -> int {
            int w = hg.vertices[vid].weight;
            vector<int> candidates;
            candidates.reserve(3);

            for (int p = 0; p < 3; p++) {
                if (sz[p] + w > ub) continue;
                if (require_lb && sz[p] >= lb) continue;
                candidates.push_back(p);
            }

            if (candidates.empty()) return -1;

            auto makeKey = [&](int p) {
                int cut_delta = incrementalCutCost(vid, p);
                int target_dist = abs((sz[p] + w) - target_int);
                int lb_deficit = require_lb ? max(0, lb - (sz[p] + w)) : 0;
                int size_after = sz[p] + w;
                int random_jitter = (int)(rng() % 5);

                if (init_mode == 0) {
                    return tuple<int,int,int,int,int>{cut_delta, target_dist, lb_deficit, size_after, random_jitter};
                } else if (init_mode == 1) {
                    return tuple<int,int,int,int,int>{target_dist, cut_delta, lb_deficit, size_after, random_jitter};
                } else if (init_mode == 2) {
                    return tuple<int,int,int,int,int>{lb_deficit, cut_delta, target_dist, size_after, random_jitter};
                }
                return tuple<int,int,int,int,int>{target_dist, lb_deficit, cut_delta, size_after, random_jitter};
            };

            vector<pair<tuple<int,int,int,int,int>, int>> scored;
            scored.reserve(candidates.size());
            for (int p : candidates) scored.push_back({makeKey(p), p});
            sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });

            // Randomized tie pick among top options to diversify starts.
            int top_k = min<int>(3, scored.size());
            int chosen_idx = (int)(rng() % top_k);
            return scored[chosen_idx].second;
        };

        for (int idx : order) {
            int w = hg.vertices[idx].weight;
            int best_p = pickBestPart(idx, true);

            if (best_p == -1) {
                best_p = pickBestPart(idx, false);
            }

            if (best_p == -1) {
                return false;
            }

            hg.vertices[idx].part = best_p;
            sz[best_p] += w;
        }

        for (int p = 0; p < 3; p++) {
            if (sz[p] < lb) {
                return false;
            }
        }

        return checkBalance();
    }

    bool buildFirstBalancedPartition() {
        int lb = lowerBound();
        int ub = upperBound();
        int total = totalWeight();

        if (3 * lb > total || 3 * ub < total) {
            return false;
        }

        array<int, 3> target = {lb, lb, lb};
        int remaining = total - 3 * lb;
        for (int p = 0; p < 3 && remaining > 0; p++) {
            int add = min(remaining, ub - target[p]);
            target[p] += add;
            remaining -= add;
        }

        if (remaining != 0) {
            return false;
        }

        for (auto& v : hg.vertices) v.part = -1;

        int part = 0;
        int used = 0;
        for (auto& v : hg.vertices) {
            while (part < 3 && used >= target[part]) {
                part++;
                used = 0;
            }
            if (part >= 3 || used + v.weight > target[part]) {
                return false;
            }
            v.part = part;
            used += v.weight;
        }

        return checkBalance();
    }

    void applyMove(int vid, int to_part) {
        hg.vertices[vid].part = to_part;
    }

    // --------------------------------------------------
    // Net-part counts
    // --------------------------------------------------
    vector<array<int,3>> buildNetPartCount() const {
        vector<array<int,3>> cnt(hg.nets.size(), {0,0,0});
        for (int net_id = 0; net_id < (int)hg.nets.size(); net_id++) {
            for (int v : hg.nets[net_id].pins) {
                int p = hg.vertices[v].part;
                cnt[net_id][p]++;
            }
        }
        return cnt;
    }

    vector<uint8_t> buildNetDistinctPartCount(const vector<array<int,3>>& cnt) const {
        vector<uint8_t> distinct(hg.nets.size(), 0);
        for (int net_id = 0; net_id < (int)hg.nets.size(); net_id++) {
            distinct[net_id] =
                static_cast<uint8_t>((cnt[net_id][0] > 0) + (cnt[net_id][1] > 0) + (cnt[net_id][2] > 0));
        }
        return distinct;
    }

    void updateNetPartStatsForMove(int vid, int from_part, int to_part,
                                   vector<array<int,3>>& cnt,
                                   vector<uint8_t>& distinct_cnt) const {
        for (int net_id : hg.vertices[vid].incident_nets) {
            distinct_cnt[net_id] -= (cnt[net_id][from_part] == 1);
            distinct_cnt[net_id] += (cnt[net_id][to_part] == 0);
            cnt[net_id][from_part]--;
            cnt[net_id][to_part]++;
        }
    }

    // --------------------------------------------------
    // Pairwise FM support
    // --------------------------------------------------
    bool isMovableInPair(int vid, int A, int B) const {
        int p = hg.vertices[vid].part;
        return (p == A || p == B);
    }

    int otherPartInPair(int p, int A, int B) const {
        return (p == A ? B : A);
    }

    bool isMoveLegalPair(int vid, int from_part, int to_part) const {
        auto sz = getPartSizes();
        int lb = lowerBound();
        int ub = upperBound();

        sz[from_part] -= hg.vertices[vid].weight;
        sz[to_part]   += hg.vertices[vid].weight;

        for (int i = 0; i < 3; i++) {
            if (sz[i] < lb || sz[i] > ub) return false;
        }
        return true;
    }

    bool isMoveLegalPairWithSizes(int vid,
                                  int from_part,
                                  int to_part,
                                  const array<int,3>& part_sizes,
                                  int lb,
                                  int ub) const {
        array<int,3> sz = part_sizes;
        sz[from_part] -= hg.vertices[vid].weight;
        sz[to_part]   += hg.vertices[vid].weight;

        for (int i = 0; i < 3; i++) {
            if (sz[i] < lb || sz[i] > ub) return false;
        }
        return true;
    }

    int computeMoveGainPairFast(int vid,
                                int from_part,
                                int to_part,
                                const vector<array<int,3>>& cnt,
                                const vector<uint8_t>& distinct_cnt,
                                const array<int,3>& part_sizes,
                                int lb,
                                int ub) const {
        if (!isMoveLegalPairWithSizes(vid, from_part, to_part, part_sizes, lb, ub)) {
            return INT_MIN;
        }

        int gain = 0;
        for (int net_id : hg.vertices[vid].incident_nets) {
            int before_cnt = distinct_cnt[net_id];
            int after_cnt = before_cnt;
            after_cnt -= (cnt[net_id][from_part] == 1);
            after_cnt += (cnt[net_id][to_part] == 0);
            int before_cost = (before_cnt >= 2 ? 1 : 0);
            int after_cost  = (after_cnt >= 2 ? 1 : 0);
            gain += (before_cost - after_cost);
        }

        return gain;
    }

    CandidateMove computeBestLegalMovePairFast(int vid,
                                               int A,
                                               int B,
                                               const vector<array<int,3>>& cnt,
                                               const vector<uint8_t>& distinct_cnt,
                                               const array<int,3>& part_sizes,
                                               int lb,
                                               int ub) const {
        CandidateMove best;
        if (!isMovableInPair(vid, A, B)) return best;

        int from_part = hg.vertices[vid].part;
        int to_part = otherPartInPair(from_part, A, B);

        int gain = computeMoveGainPairFast(vid, from_part, to_part, cnt, distinct_cnt, part_sizes, lb, ub);
        if (gain == INT_MIN) return best;

        best.valid = true;
        best.vid = vid;
        best.from_part = from_part;
        best.to_part = to_part;
        best.gain = gain;
        return best;
    }

    int maxGainBound() const {
        int mx = 0;
        for (const auto& v : hg.vertices) {
            mx = max(mx, (int)v.incident_nets.size());
        }
        return mx;
    }

    // --------------------------------------------------
    // Pairwise 2-way FM refinement
    // --------------------------------------------------
    bool fmRefinePair(int A, int B, bool verbose = false) {
        int n = (int)hg.vertices.size();
        int gain_bound = maxGainBound();
        int initial_cut = computeCutsize();
        int lb = lowerBound();
        int ub = upperBound();
        array<int,3> part_sizes = getPartSizes();

        vector<int> backup_parts(n);
        for (int i = 0; i < n; i++) backup_parts[i] = hg.vertices[i].part;

        vector<array<int,3>> net_part_cnt = buildNetPartCount();
        vector<uint8_t> net_distinct_cnt = buildNetDistinctPartCount(net_part_cnt);
        vector<char> locked(n, 0);
        vector<char> inactive(n, 0);
        vector<int> best_gain(n, INT_MIN);
        vector<int> version(n, 0);
        vector<int> touched_stamp(n, 0);
        int stamp = 1;
        int inactive_cnt = 0;
        vector<int> vertices_in_pair;
        vertices_in_pair.reserve(n);
        for (int vid = 0; vid < n; vid++) {
            if (isMovableInPair(vid, A, B)) vertices_in_pair.push_back(vid);
        }

        BucketList bucket;
        bucket.init(gain_bound);

        auto refreshVertex = [&](int vid) {
            if (locked[vid]) return;
            if (!isMovableInPair(vid, A, B)) return;

            CandidateMove cand = computeBestLegalMovePairFast(
                vid,
                A,
                B,
                net_part_cnt,
                net_distinct_cnt,
                part_sizes,
                lb,
                ub
            );

            version[vid]++;

            if (!cand.valid) {
                best_gain[vid] = INT_MIN;
                if (!inactive[vid]) {
                    inactive[vid] = 1;
                    inactive_cnt++;
                }
                return;
            }

            best_gain[vid] = cand.gain;
            if (inactive[vid]) {
                inactive[vid] = 0;
                inactive_cnt--;
            }
            bucket.insert({vid, cand.gain, version[vid]});
        };

        for (int vid : vertices_in_pair) {
            refreshVertex(vid);
        }

        vector<MoveRecord> moves;
        moves.reserve(vertices_in_pair.size());

        auto extractBestLegal = [&]() -> CandidateMove {
            while (!bucket.empty()) {
                BucketEntry e = bucket.popBest();
                if (e.vid < 0) return CandidateMove();

                int vid = e.vid;
                if (locked[vid]) continue;
                if (!isMovableInPair(vid, A, B)) continue;
                if (e.version != version[vid]) continue;

                CandidateMove cur = computeBestLegalMovePairFast(
                    vid,
                    A,
                    B,
                    net_part_cnt,
                    net_distinct_cnt,
                    part_sizes,
                    lb,
                    ub
                );

                if (!cur.valid) {
                    version[vid]++;
                    best_gain[vid] = INT_MIN;
                    if (!inactive[vid]) {
                        inactive[vid] = 1;
                        inactive_cnt++;
                    }
                    continue;
                }

                if (cur.gain != best_gain[vid] || cur.gain != e.gain) {
                    version[vid]++;
                    best_gain[vid] = cur.gain;
                    if (inactive[vid]) {
                        inactive[vid] = 0;
                        inactive_cnt--;
                    }
                    bucket.insert({vid, cur.gain, version[vid]});
                    continue;
                }

                return cur;
            }

            return CandidateMove();
        };

        int movable_cnt = (int)vertices_in_pair.size();

        for (int step = 0; step < movable_cnt; step++) {
            CandidateMove best = extractBestLegal();
            if (!best.valid) break;

            int vid = best.vid;
            int from_part = best.from_part;
            int to_part = best.to_part;
            int gain = best.gain;

            applyMove(vid, to_part);
            updateNetPartStatsForMove(vid, from_part, to_part, net_part_cnt, net_distinct_cnt);
            part_sizes[from_part] -= hg.vertices[vid].weight;
            part_sizes[to_part] += hg.vertices[vid].weight;

            locked[vid] = 1;
            if (inactive[vid]) {
                inactive[vid] = 0;
                inactive_cnt--;
            }
            moves.push_back({vid, from_part, to_part, gain});

            vector<int> affected;
            affected.reserve(64);
            if (stamp == INT_MAX) {
                fill(touched_stamp.begin(), touched_stamp.end(), 0);
                stamp = 1;
            }
            stamp++;
            for (int net_id : hg.vertices[vid].incident_nets) {
                for (int u : hg.nets[net_id].pins) {
                    if (!locked[u] && isMovableInPair(u, A, B) && touched_stamp[u] != stamp) {
                        touched_stamp[u] = stamp;
                        affected.push_back(u);
                    }
                }
            }

            for (int u : affected) {
                refreshVertex(u);
            }

            if (inactive_cnt > 0) {
                for (int u : vertices_in_pair) {
                    if (!locked[u] && inactive[u]) {
                        refreshVertex(u);
                    }
                }
            }
        }

        if (moves.empty()) {
            return false;
        }

        int best_prefix_sum = INT_MIN;
        int best_k = -1;
        int prefix_sum = 0;

        for (int i = 0; i < (int)moves.size(); i++) {
            prefix_sum += moves[i].gain;
            if (prefix_sum > best_prefix_sum) {
                best_prefix_sum = prefix_sum;
                best_k = i;
            }
        }

        if (best_prefix_sum <= 0) {
            for (int i = 0; i < n; i++) {
                hg.vertices[i].part = backup_parts[i];
            }
            return false;
        }

        for (int i = 0; i < n; i++) {
            hg.vertices[i].part = backup_parts[i];
        }
        for (int i = 0; i <= best_k; i++) {
            applyMove(moves[i].vid, moves[i].to_part);
        }

        int final_cut = computeCutsize();
        return final_cut < initial_cut;
    }

    bool greedyRefine3WayPass() {
        int n = (int)hg.vertices.size();
        int lb = lowerBound();
        int ub = upperBound();
        array<int,3> part_sizes = getPartSizes();
        vector<array<int,3>> net_part_cnt = buildNetPartCount();
        vector<uint8_t> net_distinct_cnt = buildNetDistinctPartCount(net_part_cnt);

        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return hg.vertices[a].incident_nets.size() > hg.vertices[b].incident_nets.size();
        });

        bool improved = false;

        for (int vid : order) {
            int from_part = hg.vertices[vid].part;
            CandidateMove best;

            for (int to_part = 0; to_part < 3; to_part++) {
                if (to_part == from_part) continue;

                int gain = computeMoveGainPairFast(
                    vid,
                    from_part,
                    to_part,
                    net_part_cnt,
                    net_distinct_cnt,
                    part_sizes,
                    lb,
                    ub
                );

                if (gain == INT_MIN || gain <= 0) continue;

                if (!best.valid || gain > best.gain) {
                    best.valid = true;
                    best.vid = vid;
                    best.from_part = from_part;
                    best.to_part = to_part;
                    best.gain = gain;
                }
            }

            if (!best.valid) continue;

            applyMove(best.vid, best.to_part);
            updateNetPartStatsForMove(best.vid, best.from_part, best.to_part, net_part_cnt, net_distinct_cnt);
            part_sizes[best.from_part] -= hg.vertices[best.vid].weight;
            part_sizes[best.to_part] += hg.vertices[best.vid].weight;
            improved = true;
        }

        return improved;
    }

    bool tabuRefine3Way(int max_steps, int tabu_tenure) {
        int n = (int)hg.vertices.size();
        if (n == 0 || max_steps <= 0 || tabu_tenure <= 0) return false;

        int lb = lowerBound();
        int ub = upperBound();
        array<int,3> part_sizes = getPartSizes();
        vector<array<int,3>> net_part_cnt = buildNetPartCount();
        vector<uint8_t> net_distinct_cnt = buildNetDistinctPartCount(net_part_cnt);

        vector<int> tabu_until(n, 0);
        vector<int> tabu_back_part(n, -1); // forbid moving back to this part until tabu expires

        int current_cut = computeCutsize();
        int best_cut = current_cut;
        SolutionSnapshot best_sol = captureSolution(current_cut);
        int no_improve_steps = 0;

        struct TabuMove {
            int vid = -1;
            int from_part = -1;
            int to_part = -1;
            int gain = INT_MIN;
            bool valid = false;
        };

        for (int step = 1; step <= max_steps; step++) {
            vector<char> is_boundary(n, 0);
            for (int net_id = 0; net_id < (int)hg.nets.size(); net_id++) {
                if (net_distinct_cnt[net_id] >= 2) {
                    for (int vid : hg.nets[net_id].pins) {
                        is_boundary[vid] = 1;
                    }
                }
            }

            TabuMove best_mv;
            for (int vid = 0; vid < n; vid++) {
                if (!is_boundary[vid]) continue;

                int from_part = hg.vertices[vid].part;
                for (int to_part = 0; to_part < 3; to_part++) {
                    if (to_part == from_part) continue;

                    int gain = computeMoveGainPairFast(
                        vid,
                        from_part,
                        to_part,
                        net_part_cnt,
                        net_distinct_cnt,
                        part_sizes,
                        lb,
                        ub
                    );
                    if (gain == INT_MIN) continue;

                    bool is_tabu = (tabu_until[vid] > step && tabu_back_part[vid] == to_part);
                    int candidate_cut = current_cut - gain;
                    bool aspiration = candidate_cut < best_cut;
                    if (is_tabu && !aspiration) continue;

                    if (!best_mv.valid || gain > best_mv.gain) {
                        best_mv = {vid, from_part, to_part, gain, true};
                    }
                }
            }

            if (!best_mv.valid) break;

            applyMove(best_mv.vid, best_mv.to_part);
            updateNetPartStatsForMove(
                best_mv.vid,
                best_mv.from_part,
                best_mv.to_part,
                net_part_cnt,
                net_distinct_cnt
            );
            part_sizes[best_mv.from_part] -= hg.vertices[best_mv.vid].weight;
            part_sizes[best_mv.to_part] += hg.vertices[best_mv.vid].weight;

            current_cut -= best_mv.gain;
            tabu_until[best_mv.vid] = step + tabu_tenure;
            tabu_back_part[best_mv.vid] = best_mv.from_part;

            if (current_cut < best_cut) {
                best_cut = current_cut;
                best_sol = captureSolution(current_cut);
                no_improve_steps = 0;
            } else {
                no_improve_steps++;
            }

            if (no_improve_steps >= max(20, tabu_tenure * 3)) {
                break;
            }
        }

        if (best_cut < computeCutsize()) {
            restoreSolution(best_sol);
            return true;
        }
        return false;
    }

    bool greedySwapPairPass(int A, int B, int max_cand = 80) {
        int n = (int)hg.vertices.size();
        int lb = lowerBound();
        int ub = upperBound();
        array<int,3> part_sizes = getPartSizes();
        vector<array<int,3>> net_part_cnt = buildNetPartCount();
        vector<uint8_t> net_distinct_cnt = buildNetDistinctPartCount(net_part_cnt);

        vector<char> is_boundary(n, 0);
        for (int net_id = 0; net_id < (int)hg.nets.size(); net_id++) {
            if (net_distinct_cnt[net_id] >= 2) {
                for (int vid : hg.nets[net_id].pins) {
                    is_boundary[vid] = 1;
                }
            }
        }

        vector<int> candA, candB;
        candA.reserve(n / 3 + 8);
        candB.reserve(n / 3 + 8);

        for (int vid = 0; vid < n; vid++) {
            int p = hg.vertices[vid].part;
            if (p == A && is_boundary[vid]) candA.push_back(vid);
            if (p == B && is_boundary[vid]) candB.push_back(vid);
        }

        if (candA.empty() || candB.empty()) {
            return false;
        }

        auto degreeCmp = [&](int x, int y) {
            return hg.vertices[x].incident_nets.size() > hg.vertices[y].incident_nets.size();
        };
        stable_sort(candA.begin(), candA.end(), degreeCmp);
        stable_sort(candB.begin(), candB.end(), degreeCmp);

        max_cand = max(8, max_cand);
        if ((int)candA.size() > max_cand) candA.resize(max_cand);
        if ((int)candB.size() > max_cand) candB.resize(max_cand);

        vector<int> net_stamp(hg.nets.size(), 0);
        vector<uint8_t> net_role(hg.nets.size(), 0);
        int stamp = 0;

        auto computeSwapGainFast = [&](int u, int v) -> int {
            int pu = hg.vertices[u].part;
            int pv = hg.vertices[v].part;
            if (pu != A || pv != B) return INT_MIN;

            int wu = hg.vertices[u].weight;
            int wv = hg.vertices[v].weight;
            array<int,3> sz = part_sizes;
            sz[A] = sz[A] - wu + wv;
            sz[B] = sz[B] - wv + wu;
            for (int p = 0; p < 3; p++) {
                if (sz[p] < lb || sz[p] > ub) return INT_MIN;
            }

            if (stamp == INT_MAX) {
                fill(net_stamp.begin(), net_stamp.end(), 0);
                stamp = 0;
            }
            ++stamp;

            vector<int> touched;
            touched.reserve(
                hg.vertices[u].incident_nets.size() +
                hg.vertices[v].incident_nets.size()
            );

            for (int net_id : hg.vertices[u].incident_nets) {
                if (net_stamp[net_id] != stamp) {
                    net_stamp[net_id] = stamp;
                    net_role[net_id] = 0;
                    touched.push_back(net_id);
                }
                net_role[net_id] |= 1;
            }
            for (int net_id : hg.vertices[v].incident_nets) {
                if (net_stamp[net_id] != stamp) {
                    net_stamp[net_id] = stamp;
                    net_role[net_id] = 0;
                    touched.push_back(net_id);
                }
                net_role[net_id] |= 2;
            }

            int gain = 0;
            for (int net_id : touched) {
                int before_cnt = net_distinct_cnt[net_id];
                int before_cost = (before_cnt >= 2 ? 1 : 0);

                array<int,3> c = net_part_cnt[net_id];
                uint8_t role = net_role[net_id];
                if (role & 1) {
                    c[A]--;
                    c[B]++;
                }
                if (role & 2) {
                    c[B]--;
                    c[A]++;
                }

                int after_cnt = (c[0] > 0) + (c[1] > 0) + (c[2] > 0);
                int after_cost = (after_cnt >= 2 ? 1 : 0);
                gain += (before_cost - after_cost);
            }
            return gain;
        };

        int best_u = -1;
        int best_v = -1;
        int best_gain = 0;

        for (int u : candA) {
            for (int v : candB) {
                int gain = computeSwapGainFast(u, v);
                if (gain > best_gain) {
                    best_gain = gain;
                    best_u = u;
                    best_v = v;
                }
            }
        }

        if (best_u == -1 || best_v == -1 || best_gain <= 0) {
            return false;
        }

        int wu = hg.vertices[best_u].weight;
        int wv = hg.vertices[best_v].weight;
        applyMove(best_u, B);
        updateNetPartStatsForMove(best_u, A, B, net_part_cnt, net_distinct_cnt);
        part_sizes[A] -= wu;
        part_sizes[B] += wu;

        applyMove(best_v, A);
        updateNetPartStatsForMove(best_v, B, A, net_part_cnt, net_distinct_cnt);
        part_sizes[B] -= wv;
        part_sizes[A] += wv;
        return true;
    }

    void fmRefineAllPairs(bool verbose = false) {
        const int MAX_STALL_ROUNDS = 3;
        int stall_rounds = 0;

        while (stall_rounds < MAX_STALL_ROUNDS) {
            int before = computeCutsize();
            SolutionSnapshot round_backup = captureSolution(before);

            bool improved = false;
            vector<pair<int, int>> pair_order = {{0, 1}, {0, 2}, {1, 2}};
            shuffle(pair_order.begin(), pair_order.end(), rng);
            for (const auto& pr : pair_order) {
                improved |= fmRefinePair(pr.first, pr.second, verbose);
            }
            int adaptive_swap_cand = (stall_rounds > 0 ? 120 : 80);
            for (const auto& pr : pair_order) {
                const int MAX_SWAP_REPEATS = 3;
                for (int rep = 0; rep < MAX_SWAP_REPEATS; rep++) {
                    bool swapped = greedySwapPairPass(pr.first, pr.second, adaptive_swap_cand);
                    if (!swapped) break;
                    improved = true;
                }
            }
            improved |= greedyRefine3WayPass();
            improved |= tabuRefine3Way(80, 7);

            int after = computeCutsize();
            if (!improved || after >= before) {
                restoreSolution(round_backup);
                stall_rounds++;
            } else {
                stall_rounds = 0;
            }
        }
    }

    SolutionSnapshot improveByPerturbation(int trials, int move_count, bool verbose = false) {
        SolutionSnapshot best_sol = captureSolution();
        int n = (int)hg.vertices.size();
        if (n == 0 || trials <= 0 || move_count <= 0) {
            return best_sol;
        }

        int lb = lowerBound();
        int ub = upperBound();
        vector<int> all_vertices(n);
        iota(all_vertices.begin(), all_vertices.end(), 0);

        for (int t = 0; t < trials; t++) {
            restoreSolution(best_sol);
            array<int,3> sz = getPartSizes();
            vector<array<int,3>> net_part_cnt = buildNetPartCount();
            vector<uint8_t> net_distinct_cnt = buildNetDistinctPartCount(net_part_cnt);
            vector<char> is_boundary(n, 0);
            vector<int> boundary;
            vector<int> interior;
            boundary.reserve(n);
            interior.reserve(n);

            for (int net_id = 0; net_id < (int)hg.nets.size(); net_id++) {
                if (net_distinct_cnt[net_id] >= 2) {
                    for (int vid : hg.nets[net_id].pins) {
                        is_boundary[vid] = 1;
                    }
                }
            }

            for (int vid : all_vertices) {
                if (is_boundary[vid]) boundary.push_back(vid);
                else interior.push_back(vid);
            }

            shuffle(boundary.begin(), boundary.end(), rng);
            shuffle(interior.begin(), interior.end(), rng);
            vector<int> order;
            order.reserve(n);
            order.insert(order.end(), boundary.begin(), boundary.end());
            order.insert(order.end(), interior.begin(), interior.end());
            int moved = 0;

            for (int vid : order) {
                int from_part = hg.vertices[vid].part;
                int best_to_part = -1;
                int best_gain = INT_MIN;
                for (int to_part = 0; to_part < 3; to_part++) {
                    if (to_part == from_part) continue;

                    int gain = computeMoveGainPairFast(
                        vid,
                        from_part,
                        to_part,
                        net_part_cnt,
                        net_distinct_cnt,
                        sz,
                        lb,
                        ub
                    );
                    if (gain == INT_MIN) continue;

                    int cur_imbalance = abs((sz[to_part] + hg.vertices[vid].weight) - (sz[from_part] - hg.vertices[vid].weight));
                    int best_imbalance = (best_to_part == -1)
                        ? INT_MAX
                        : abs((sz[best_to_part] + hg.vertices[vid].weight) - (sz[from_part] - hg.vertices[vid].weight));

                    if (best_to_part == -1 || gain > best_gain ||
                        (gain == best_gain && cur_imbalance < best_imbalance)) {
                        best_gain = gain;
                        best_to_part = to_part;
                    }
                }

                if (best_to_part == -1) continue;

                hg.vertices[vid].part = best_to_part;
                sz[from_part] -= hg.vertices[vid].weight;
                sz[best_to_part] += hg.vertices[vid].weight;
                updateNetPartStatsForMove(vid, from_part, best_to_part, net_part_cnt, net_distinct_cnt);
                moved++;

                if (moved >= move_count) break;
            }

            if (moved == 0) continue;

            fmRefineAllPairs(false);

            int cur_cut = computeCutsize();
            if (cur_cut < best_sol.cutsize) {
                best_sol = captureSolution(cur_cut);
                if (verbose) {
                    cout << "[Perturb] trial " << (t + 1)
                         << ", improved cutsize = " << cur_cut << "\n";
                }
            }
        }

        restoreSolution(best_sol);
        return best_sol;
    }

    SolutionSnapshot polishWithPerturbSchedules(const vector<int>& move_counts,
                                                int trials_per_schedule,
                                                bool verbose = false) {
        SolutionSnapshot global_best = captureSolution();
        SolutionSnapshot seed_best = global_best;

        for (int move_count : move_counts) {
            SolutionSnapshot cur = improveByPerturbation(trials_per_schedule, move_count, verbose);
            if (cur.cutsize < global_best.cutsize) {
                global_best = cur;
                seed_best = cur;
            }
        }

        return global_best;
    }

    // --------------------------------------------------
    // Multi-start
    // --------------------------------------------------
    SolutionSnapshot solveWithMultiStart(int num_starts, bool verbose = false) {
        SolutionSnapshot best_sol;

        for (int t = 1; t <= num_starts; t++) {
            if (!buildRandomBalancedPartition()) continue;

            fmRefineAllPairs(false);

            int cur_cut = computeCutsize();
            if (cur_cut < best_sol.cutsize) {
                best_sol = captureSolution(cur_cut);
                if (verbose) {
                    cout << "[MultiStart] new best at start " << t
                         << ", cutsize = " << cur_cut << "\n";
                }
            }
        }

        if (best_sol.cutsize == INT_MAX) {
            cerr << "Error: multi-start failed to find any feasible solution.\n";
            return best_sol;
        }

        restoreSolution(best_sol);
        return best_sol;
    }

    // --------------------------------------------------
    // Heavy-edge-like coarsening adjacency
    // --------------------------------------------------
    vector<CoarsenEdgeScore> buildHeavyEdgeScoreList(bool same_part_only = false) const {
        unordered_map<unsigned long long, double> edge_score;
        size_t reserve_hint = 0;
        for (const auto& net : hg.nets) {
            int sz = (int)net.pins.size();
            if (sz > 1) reserve_hint += (size_t)sz * (sz - 1) / 2;
        }
        edge_score.reserve(max<size_t>(16, reserve_hint / 2 + 1));

        for (const auto& net : hg.nets) {
            const auto& pins = net.pins;
            int sz = (int)pins.size();
            if (sz <= 1) continue;

            double contrib = 1.0 / (sz - 1);
            for (int i = 0; i < sz; i++) {
                for (int j = i + 1; j < sz; j++) {
                    int u = pins[i];
                    int v = pins[j];
                    if (same_part_only &&
                        hg.vertices[u].part >= 0 &&
                        hg.vertices[v].part >= 0 &&
                        hg.vertices[u].part != hg.vertices[v].part) {
                        continue;
                    }
                    if (u > v) swap(u, v);
                    unsigned long long key =
                        (static_cast<unsigned long long>(static_cast<unsigned int>(u)) << 32) |
                        static_cast<unsigned int>(v);
                    edge_score[key] += contrib;
                }
            }
        }

        vector<CoarsenEdgeScore> edges;
        edges.reserve(edge_score.size());
        for (const auto& [key, score] : edge_score) {
            int u = (int)(key >> 32);
            int v = (int)(key & 0xffffffffu);
            edges.push_back({score, u, v});
        }
        return edges;
    }

    // --------------------------------------------------
    // Coarsening
    // --------------------------------------------------
    CoarsenResult coarsenOneLevel(bool same_part_only = false) const {
        CoarsenResult result;

        int n = (int)hg.vertices.size();
        if (n == 0) return result;

        vector<char> matched(n, 0);
        vector<int> fine_to_coarse(n, -1);
        vector<vector<int>> coarse_to_fine;
        coarse_to_fine.reserve(n);

        vector<CoarsenEdgeScore> edges = buildHeavyEdgeScoreList(same_part_only);
        sort(edges.begin(), edges.end(), [](const CoarsenEdgeScore& a, const CoarsenEdgeScore& b) {
            if (fabs(a.score - b.score) > 1e-12) return a.score > b.score;
            if (a.u != b.u) return a.u < b.u;
            return a.v < b.v;
        });

        int max_cluster_weight = upperBound();

        for (const auto& e : edges) {
            int u = e.u;
            int v = e.v;
            if (matched[u] || matched[v]) continue;
            if (same_part_only &&
                hg.vertices[u].part >= 0 &&
                hg.vertices[v].part >= 0 &&
                hg.vertices[u].part != hg.vertices[v].part) {
                continue;
            }

            int merged_weight = hg.vertices[u].weight + hg.vertices[v].weight;
            if (merged_weight > max_cluster_weight) continue;

            int cid = (int)coarse_to_fine.size();
            coarse_to_fine.push_back({u, v});
            fine_to_coarse[u] = cid;
            fine_to_coarse[v] = cid;
            matched[u] = matched[v] = true;
        }

        for (int u = 0; u < n; u++) {
            if (!matched[u]) {
                int cid = (int)coarse_to_fine.size();
                coarse_to_fine.push_back({u});
                fine_to_coarse[u] = cid;
                matched[u] = true;
            }
        }

        Hypergraph coarse;
        coarse.balance_factor = hg.balance_factor;

        int coarse_n = (int)coarse_to_fine.size();
        coarse.vertices.resize(coarse_n);
        coarse.vertex_id.reserve(max(16, coarse_n * 2));
        coarse.nets.reserve(hg.nets.size());

        for (int cid = 0; cid < coarse_n; cid++) {
            const auto& members = coarse_to_fine[cid];

            string cname = "C" + to_string(cid);
            coarse.vertices[cid].name = cname;
            coarse.vertices[cid].part = -1;
            coarse.vertices[cid].weight = 0;
            coarse.vertex_id[cname] = cid;

            int common_part = hg.vertices[members[0]].part;
            for (int fv : members) {
                coarse.vertices[cid].weight += hg.vertices[fv].weight;
                if (hg.vertices[fv].part != common_part) {
                    common_part = -1;
                }
            }
            coarse.vertices[cid].part = common_part;
        }

        vector<int> coarse_pins;
        for (int net_id = 0; net_id < (int)hg.nets.size(); net_id++) {
            const auto& net = hg.nets[net_id];
            coarse_pins.clear();
            coarse_pins.reserve(net.pins.size());

            for (int fv : net.pins) {
                coarse_pins.push_back(fine_to_coarse[fv]);
            }

            sort(coarse_pins.begin(), coarse_pins.end());
            coarse_pins.erase(unique(coarse_pins.begin(), coarse_pins.end()), coarse_pins.end());

            if ((int)coarse_pins.size() <= 1) continue;

            Net cnet;
            cnet.name = "CN" + to_string((int)coarse.nets.size());
            cnet.pins = coarse_pins;

            int cnet_id = (int)coarse.nets.size();
            coarse.nets.push_back(cnet);

            for (int cv : coarse_pins) {
                coarse.vertices[cv].incident_nets.push_back(cnet_id);
            }
        }

        result.coarse_hg = coarse;
        result.fine_to_coarse = fine_to_coarse;
        result.coarse_to_fine = coarse_to_fine;
        return result;
    }

    // --------------------------------------------------
    // Projection
    // --------------------------------------------------
    void projectPartitionFromCoarse(const CoarsenResult& cr, const Hypergraph& coarse_hg) {
        if ((int)cr.fine_to_coarse.size() != (int)hg.vertices.size()) {
            return;
        }

        for (int fv = 0; fv < (int)hg.vertices.size(); fv++) {
            int cv = cr.fine_to_coarse[fv];
            if (cv < 0 || cv >= (int)coarse_hg.vertices.size()) {
                return;
            }

            int part = coarse_hg.vertices[cv].part;
            if (part < 0 || part >= 3) {
                return;
            }

            hg.vertices[fv].part = part;
        }
    }

    // --------------------------------------------------
    // Debug
    // --------------------------------------------------
    void printSummary() const {
        cout << "Balance factor r = " << hg.balance_factor << "\n";
        cout << "#Vertices = " << hg.vertices.size() << "\n";
        cout << "#Nets = " << hg.nets.size() << "\n";
        cout << "Lower bound = " << lowerBound()
             << ", Upper bound = " << upperBound() << "\n";

        if (checkAllAssigned()) {
            auto sz = getPartSizes();
            cout << "G1 size = " << sz[0]
                 << ", G2 size = " << sz[1]
                 << ", G3 size = " << sz[2] << "\n";
            cout << "Balanced? " << (checkBalance() ? "YES" : "NO") << "\n";
            cout << "Cutsize = " << computeCutsize() << "\n";
        } else {
            cout << "Partition not assigned yet.\n";
        }
    }

    void printPartitionDetail(const string& title) const {
        cout << "=== " << title << " ===\n";

        if (!checkAllAssigned()) {
            cout << "Partition not assigned yet.\n\n";
            return;
        }

        vector<string> groups[3];
        for (const auto& v : hg.vertices) {
            groups[v.part].push_back(v.name);
        }

        for (int i = 0; i < 3; i++) {
            sort(groups[i].begin(), groups[i].end());
        }

        int cutsize = computeCutsize();
        auto sz = getPartSizes();

        cout << "Cutsize = " << cutsize << "\n";
        for (int i = 0; i < 3; i++) {
            cout << "G" << (i + 1) << " (" << sz[i] << "): ";
            for (const auto& name : groups[i]) {
                cout << name << " ";
            }
            cout << "\n";
        }
        cout << "\n";
    }

    void printHypergraph(const Hypergraph& g, const string& title) const {
        cout << "=== " << title << " ===\n";
        cout << "Vertices: " << g.vertices.size() << "\n";
        for (int i = 0; i < (int)g.vertices.size(); i++) {
            cout << "  [" << i << "] " << g.vertices[i].name
                 << " weight=" << g.vertices[i].weight << "\n";
        }

        cout << "Nets: " << g.nets.size() << "\n";
        for (int i = 0; i < (int)g.nets.size(); i++) {
            cout << "  " << g.nets[i].name << " : ";
            for (int v : g.nets[i].pins) {
                cout << g.vertices[v].name << " ";
            }
            cout << "\n";
        }
        cout << "\n";
    }

    // --------------------------------------------------
    // Output
    // --------------------------------------------------
    bool writeOutput(const string& output_file) const {
        ofstream fout(output_file);
        if (!fout.is_open()) {
            cerr << "Error: cannot open output file: " << output_file << "\n";
            return false;
        }

        if (!checkAllAssigned()) {
            cerr << "Error: some vertices are unassigned.\n";
            return false;
        }

        int cutsize = computeCutsize();
        if (cutsize < 0) {
            cerr << "Error: invalid cutsize.\n";
            return false;
        }

        vector<string> groups[3];
        for (const auto& v : hg.vertices) {
            groups[v.part].push_back(v.name);
        }

        fout << "Cutsize = " << cutsize << "\n";
        for (int i = 0; i < 3; i++) {
            fout << "G" << (i + 1) << " " << groups[i].size() << "\n";
            for (const auto& name : groups[i]) {
                fout << name << " ";
            }
            fout << ";\n";
        }

        return true;
    }

private:
    int getOrCreateVertex(const string& name) {
        auto it = hg.vertex_id.find(name);
        if (it != hg.vertex_id.end()) return it->second;

        int id = (int)hg.vertices.size();
        hg.vertex_id[name] = id;

        Vertex v;
        v.name = name;
        hg.vertices.push_back(v);
        return id;
    }
};

// --------------------------------------------------
// Recursive multilevel solve
// --------------------------------------------------
PartitionSolver solveMultilevelRecursive(const Hypergraph& input_hg,
                                         int level,
                                         int max_levels,
                                         int coarse_threshold,
                                         int num_starts,
                                         bool verbose) {
    PartitionSolver solver;
    solver.hg = input_hg;

    if (verbose) {
        cout << "=== Multilevel Level " << level << " ===\n";
        cout << "Vertices = " << solver.hg.vertices.size()
             << ", Nets = " << solver.hg.nets.size() << "\n";
    }

    if ((int)input_hg.vertices.size() <= coarse_threshold || level >= max_levels) {
        if (verbose) {
            cout << "[Level " << level << "] solve directly with multi-start\n";
        }
        solver.solveWithMultiStart(num_starts, false);
        return solver;
    }

    CoarsenResult cr = solver.coarsenOneLevel();

    if ((int)cr.coarse_hg.vertices.size() >= (int)input_hg.vertices.size()) {
        if (verbose) {
            cout << "[Level " << level << "] no more reduction, solve directly\n";
        }
        solver.solveWithMultiStart(num_starts, false);
        return solver;
    }

    if (verbose) {
        cout << "[Level " << level << "] coarsen: "
             << input_hg.vertices.size() << " -> "
             << cr.coarse_hg.vertices.size() << " vertices\n";
    }

    PartitionSolver coarse_solver = solveMultilevelRecursive(
        cr.coarse_hg,
        level + 1,
        max_levels,
        coarse_threshold,
        num_starts,
        verbose
    );

    solver.projectPartitionFromCoarse(cr, coarse_solver.hg);

    if (verbose) {
        cout << "[Level " << level << "] after projection, cutsize = "
             << solver.computeCutsize() << "\n";
    }

    solver.fmRefineAllPairs(false);

    if (verbose) {
        cout << "[Level " << level << "] after refine, cutsize = "
             << solver.computeCutsize() << "\n";
    }

    return solver;
}

PartitionSolver solveGuidedVCycleRecursive(const Hypergraph& input_hg,
                                          int level,
                                          int max_levels,
                                          int coarse_threshold,
                                          bool verbose) {
    PartitionSolver solver;
    solver.hg = input_hg;

    if ((int)input_hg.vertices.size() <= coarse_threshold || level >= max_levels) {
        solver.fmRefineAllPairs(false);
        return solver;
    }

    CoarsenResult cr = solver.coarsenOneLevel(true);
    if ((int)cr.coarse_hg.vertices.size() >= (int)input_hg.vertices.size()) {
        solver.fmRefineAllPairs(false);
        return solver;
    }

    if (verbose) {
        cout << "[Guided Level " << level << "] coarsen: "
             << input_hg.vertices.size() << " -> "
             << cr.coarse_hg.vertices.size() << " vertices\n";
    }

    PartitionSolver coarse_solver = solveGuidedVCycleRecursive(
        cr.coarse_hg,
        level + 1,
        max_levels,
        coarse_threshold,
        verbose
    );

    solver.projectPartitionFromCoarse(cr, coarse_solver.hg);
    solver.fmRefineAllPairs(false);
    return solver;
}

// --------------------------------------------------
// Main
// --------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: ./Lab1 [input file name] [output file name]\n";
        return 1;
    }

    string input_file = argv[1];
    string output_file = argv[2];

    PartitionSolver solver;
    if (!solver.parseInput(input_file)) {
        return 1;
    }

    if (!solver.buildFirstBalancedPartition()) {
        cerr << "Error: failed to build a feasible balanced partition.\n";
        return 1;
    }

    if (!solver.writeOutput(output_file)) {
        return 1;
    }

    return 0;
}
