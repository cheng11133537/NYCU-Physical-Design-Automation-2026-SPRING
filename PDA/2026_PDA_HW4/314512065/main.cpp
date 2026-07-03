#include <bits/stdc++.h>
using namespace std;

struct Point {
    int x, y;
    int id;
};

struct Edge {
    long long w;
    int u, v;

    bool operator<(const Edge& other) const {
        return w < other.w;
    }
};

struct DSU {
    vector<int> parent;
    vector<unsigned char> rankv;

    DSU(int n = 0) {
        init(n);
    }

    void init(int n) {
        parent.resize(n);
        rankv.assign(n, 0);
        for (int i = 0; i < n; i++) {
            parent[i] = i;
        }
    }

    int find(int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }

    bool unite(int a, int b) {
        int ra = find(a);
        int rb = find(b);

        if (ra == rb) return false;

        if (rankv[ra] < rankv[rb]) {
            swap(ra, rb);
        }

        parent[rb] = ra;

        if (rankv[ra] == rankv[rb]) {
            rankv[ra]++;
        }

        return true;
    }
};

static inline long long manhattan_dist(const Point& a, const Point& b) {
    return llabs((long long)a.x - b.x) + llabs((long long)a.y - b.y);
}

struct FastScanner {
    static constexpr size_t BUFFER_SIZE = 1 << 20;

    FILE* file;
    char buffer[BUFFER_SIZE];
    size_t pos = 0;
    size_t len = 0;

    explicit FastScanner(const char* path) {
        file = fopen(path, "rb");
    }

    ~FastScanner() {
        if (file) {
            fclose(file);
        }
    }

    bool good() const {
        return file != nullptr;
    }

    int get_char() {
        if (pos == len) {
            len = fread(buffer, 1, BUFFER_SIZE, file);
            pos = 0;

            if (len == 0) {
                return EOF;
            }
        }

        return buffer[pos++];
    }

    template <typename T>
    bool read_int(T& out) {
        int c = get_char();

        while (c != EOF && c <= ' ') {
            c = get_char();
        }

        if (c == EOF) {
            return false;
        }

        int sign = 1;
        if (c == '-') {
            sign = -1;
            c = get_char();
        }

        T value = 0;
        while (c > ' ') {
            value = value * 10 + (c - '0');
            c = get_char();
        }

        out = value * sign;
        return true;
    }
};

/*
    Generate candidate edges for Manhattan MST.

    This is the standard 4-transform sweep-line method.
    For each transformed coordinate system:
      1. Sort points by x + y.
      2. Maintain active points using key = -y.
      3. Add only useful nearest-neighbor candidates.
      4. Rotate / reflect coordinates for the next direction.

    Total candidate edges are O(n).
*/
void add_duplicate_zero_edges(const vector<Point>& pts, vector<int>& order, vector<Edge>& edges) {
    int n = (int)pts.size();

    sort(order.begin(), order.end(), [&](int a, int b) {
        if (pts[a].x != pts[b].x) return pts[a].x < pts[b].x;
        if (pts[a].y != pts[b].y) return pts[a].y < pts[b].y;
        return pts[a].id < pts[b].id;
    });

    for (int i = 1; i < n; i++) {
        const Point& a = pts[order[i - 1]];
        const Point& b = pts[order[i]];

        if (a.x == b.x && a.y == b.y) {
            edges.push_back({0, a.id, b.id});
        }
    }
}

void generate_candidate_edges(vector<Point>& pts, vector<int>& order, vector<Edge>& edges) {
    int n = (int)pts.size();

    for (int rot = 0; rot < 4; rot++) {
        sort(order.begin(), order.end(), [&](int a, int b) {
            long long sa = pts[a].x + pts[a].y;
            long long sb = pts[b].x + pts[b].y;

            if (sa != sb) return sa < sb;
            if (pts[a].x != pts[b].x) return pts[a].x < pts[b].x;
            if (pts[a].y != pts[b].y) return pts[a].y < pts[b].y;
            return pts[a].id < pts[b].id;
        });

        map<long long, int> active;

        for (int idx : order) {
            Point& p = pts[idx];

            auto it = active.lower_bound(-p.y);

            while (it != active.end()) {
                int j = it->second;
                Point& q = pts[j];

                /*
                    If this condition fails, later active points will not be useful
                    for this sweep direction.
                */
                if (p.x - q.x < p.y - q.y) {
                    break;
                }

                long long w = manhattan_dist(p, q);
                edges.push_back({w, p.id, q.id});

                auto erase_it = it;
                ++it;
                active.erase(erase_it);
            }

            active[-p.y] = idx;
        }

        /*
            Coordinate transforms:
            Repeatedly swap / reflect to cover all four Manhattan directions.
        */
        for (int i = 0; i < n; i++) {
            if (rot % 2 == 0) {
                swap(pts[i].x, pts[i].y);
            } else {
                pts[i].x = -pts[i].x;
            }
        }
    }
}

long long kruskal_mst(int n, vector<Edge>& edges) {
    sort(edges.begin(), edges.end());

    DSU dsu(n);
    long long total = 0;
    int used = 0;

    for (const Edge& e : edges) {
        if (dsu.unite(e.u, e.v)) {
            total += e.w;
            used++;

            if (used == n - 1) {
                break;
            }
        }
    }

    return total;
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " input.dat output.dat\n";
        return 1;
    }

    FastScanner scanner(argv[1]);
    if (!scanner.good()) {
        cerr << "Error: Cannot open input file.\n";
        return 1;
    }

    int n = 0;
    if (!scanner.read_int(n)) {
        cerr << "Error: Invalid input file.\n";
        return 1;
    }

    vector<Point> pts;
    pts.reserve(n);

    for (int i = 0; i < n; i++) {
        int x = 0;
        int y = 0;
        if (!scanner.read_int(x) || !scanner.read_int(y)) {
            cerr << "Error: Invalid input file.\n";
            return 1;
        }

        pts.push_back({x, y, i});
    }

    vector<Edge> edges;
    edges.reserve((size_t)4 * n + 10);

    if (n <= 1) {
        ofstream fout(argv[2]);
        fout << 0 << '\n';
        return 0;
    }

    vector<int> order(n);
    iota(order.begin(), order.end(), 0);

    add_duplicate_zero_edges(pts, order, edges);
    generate_candidate_edges(pts, order, edges);

    long long answer = kruskal_mst(n, edges);

    ofstream fout(argv[2]);
    if (!fout) {
        cerr << "Error: Cannot open output file.\n";
        return 1;
    }

    fout << answer << '\n';

    return 0;
}
