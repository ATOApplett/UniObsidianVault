// bacteria_omp.cpp
// Protein 6-mer "stochastic deviation" profiles for a list of bacteria, followed by
// all-pairs cosine correlation. OpenMP version that prints the same text as the
// sequential programs (apart from the timing line).
//
// Build:  g++ -O2 -std=c++17 -fopenmp -ffp-contract=off bacteria_omp.cpp -o bacteria_omp
// Run:    ./bacteria_omp [list_file=list.txt] [data_dir=data] [threads=auto]

#include <omp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int    K   = 6;       // k-mer length
constexpr int    AA  = 20;      // amino-acid classes
constexpr double EPS = 1e-10;

constexpr std::size_t ipow(std::size_t b, int e) { return e == 0 ? 1 : b * ipow(b, e - 1); }
constexpr std::size_t N4 = ipow(AA, K - 2);  // 20^4
constexpr std::size_t N5 = N4 * AA;          // 20^5
constexpr std::size_t N6 = N5 * AA;          // 20^6

// Letter -> class table (same mapping as the original code[] array). -1 = ignore.
std::array<int8_t, 256> make_table() {
    static const int8_t code[26] = {0, 2, 1, 2, 3, 4, 5, 6, 7, -1, 8, 9, 10,
                                    11, -1, 12, 13, 14, 15, 16, 1, 17, 18, 5, 19, 3};
    std::array<int8_t, 256> t;
    t.fill(-1);
    for (int i = 0; i < 26; ++i) t[static_cast<unsigned char>('A' + i)] = code[i];
    return t;
}
const std::array<int8_t, 256> kTable = make_table();

// Sparse profile: ascending k-mer indices with their deviation values.
struct Profile {
    std::vector<uint32_t> idx;
    std::vector<double>   val;
    double                norm = 0.0;  // sqrt(sum of squares)
};

Profile build_profile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to open file " + path);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // ---- 1. count 6-mers, 5-mers and single residues (serial: stream is stateful) ----
    std::vector<uint32_t> c6(N6, 0), c5(N5, 0);
    uint64_t c1[AA] = {0};
    uint64_t total6 = 0, total1 = 0, records = 0;

    std::size_t ctx = 0;   // rolling index of the last (K-1) residues
    int filled = 0;        // residues seen since the last header (capped at K-1)
    bool in_header = false;
    for (const char ch : text) {
        if (in_header) {
            if (ch == '\n') in_header = false;
            continue;
        }
        if (ch == '>') { in_header = true; filled = 0; ctx = 0; continue; }
        const int enc = kTable[static_cast<unsigned char>(ch)];
        if (enc < 0) continue;  // newline, '\r', or unsupported letter

        ++c1[enc];
        ++total1;
        if (filled < K - 1) {
            ctx = ctx * AA + enc;
            if (++filled == K - 1) { ++c5[ctx]; ++records; }
        } else {
            ++c6[ctx * AA + enc];
            ++total6;
            ctx = (ctx % N4) * AA + enc;
            ++c5[ctx];
        }
    }

    // ---- 2. marginal probabilities ----
    const double denom5 = static_cast<double>(total6 + records);
    const double half   = static_cast<double>(total6) * 0.5;
    double p1d[AA];
    for (int i = 0; i < AA; ++i) p1d[i] = static_cast<double>(c1[i]) / static_cast<double>(total1);

    std::vector<double> p5(N5);
    #pragma omp parallel for schedule(static) if (!omp_in_parallel())
    for (long i = 0; i < static_cast<long>(N5); ++i) p5[i] = static_cast<double>(c5[i]) / denom5;
    std::vector<uint32_t>().swap(c5);

    // ---- 3. sweep all 20^6 cells; each thread owns one contiguous block of 5-mer
    //         prefixes, so the concatenated output is automatically sorted ----
    const int maxT = omp_get_max_threads();
    std::vector<std::vector<uint32_t>> bi(maxT);
    std::vector<std::vector<double>>   bv(maxT);

    #pragma omp parallel if (!omp_in_parallel())
    {
        const int         nt  = omp_get_num_threads();
        const int         tid = omp_get_thread_num();
        const std::size_t lo  = N5 * tid / nt;
        const std::size_t hi  = N5 * (tid + 1) / nt;
        auto& oi = bi[tid];
        auto& ov = bv[tid];
        oi.reserve(1 << 16);
        ov.reserve(1 << 16);

        for (std::size_t a = lo; a < hi; ++a) {           // a = i / 20
            const double      p1    = p5[a];
            const double      p4    = p1d[a / N4];        // first residue of the 6-mer
            const std::size_t tail  = (a % N4) * AA;      // (i % 20^5) without last residue
            const std::size_t base6 = a * AA;
            for (int last = 0; last < AA; ++last) {
                const double s = (p1 * p1d[last] + p5[tail + last] * p4) * half;
                if (s > EPS) {
                    const double v = (static_cast<double>(c6[base6 + last]) - s) / s;
                    if (v != 0.0) {
                        oi.push_back(static_cast<uint32_t>(base6 + last));
                        ov.push_back(v);
                    }
                }
            }
        }
    }

    // ---- 4. concatenate thread blocks using prefix offsets ----
    std::vector<std::size_t> off(maxT + 1, 0);
    for (int t = 0; t < maxT; ++t) off[t + 1] = off[t] + bi[t].size();

    Profile p;
    p.idx.resize(off[maxT]);
    p.val.resize(off[maxT]);
    #pragma omp parallel for schedule(static) if (!omp_in_parallel())
    for (int t = 0; t < maxT; ++t) {
        std::copy(bi[t].begin(), bi[t].end(), p.idx.begin() + off[t]);
        std::copy(bv[t].begin(), bv[t].end(), p.val.begin() + off[t]);
    }

    // Vector length: same accumulation order as the sequential merge (ascending index).
    double sq = 0.0;
    for (const double v : p.val) sq += v * v;
    p.norm = std::sqrt(sq);
    return p;
}

// Dot product of two sparse, index-sorted profiles.
double dot(const Profile& a, const Profile& b) {
    std::size_t i = 0, j = 0;
    const std::size_t na = a.idx.size(), nb = b.idx.size();
    double acc = 0.0;
    while (i < na && j < nb) {
        const uint32_t x = a.idx[i], y = b.idx[j];
        if (x < y)      ++i;
        else if (y < x) ++j;
        else            acc += a.val[i++] * b.val[j++];
    }
    return acc;
}

std::vector<std::string> read_list(const std::string& list_path, const std::string& dir) {
    std::ifstream in(list_path);
    if (!in) {
        std::fprintf(stderr, "Error: failed to open file %s (Hint: check your working directory)\n",
                     list_path.c_str());
        std::exit(1);
    }
    long n = 0;
    if (!(in >> n) || n < 0) {
        std::fprintf(stderr, "Error: malformed list file (missing count)\n");
        std::exit(1);
    }
    std::vector<std::string> paths;
    paths.reserve(static_cast<std::size_t>(n));
    for (long i = 0; i < n; ++i) {
        std::string name;
        if (!(in >> name)) {
            std::fprintf(stderr, "Error: malformed list file (bad name at line %ld)\n", i + 2);
            std::exit(1);
        }
        paths.push_back(dir + "/" + name + ".faa");
    }
    return paths;
}

}  // namespace

int main(int argc, char* argv[]) {
    const auto t0 = std::chrono::steady_clock::now();

    const std::string list_path = argc > 1 ? argv[1] : "list.txt";
    const std::string data_dir  = argc > 2 ? argv[2] : "data";
    if (argc > 3) {
        const int t = std::atoi(argv[3]);
        if (t > 0) omp_set_num_threads(t);
    }

    const std::vector<std::string> files = read_list(list_path, data_dir);
    const int n = static_cast<int>(files.size());
    std::vector<Profile> prof(n);

    // ---- load phase: one file per thread when there are enough files; otherwise load
    //      files one at a time and let each file's sweep use all threads ----
    std::string error;
    const bool across_files = n >= omp_get_max_threads();
    #pragma omp parallel for schedule(dynamic, 1) if (across_files)
    for (int i = 0; i < n; ++i) {
        try {
            prof[i] = build_profile(files[i]);
        } catch (const std::exception& e) {
            #pragma omp critical(report_error)
            if (error.empty()) error = e.what();
        }
    }
    if (!error.empty()) {
        std::fprintf(stderr, "Error: %s\n", error.c_str());
        return 1;
    }
    for (int i = 0; i < n; ++i) std::printf("load %d of %d\n", i + 1, n);

    // ---- compare phase: flatten the triangle so dynamic scheduling balances well ----
    std::vector<std::pair<int, int>> pairs;
    pairs.reserve(static_cast<std::size_t>(n) * (n > 0 ? n - 1 : 0) / 2);
    for (int i = 0; i < n - 1; ++i)
        for (int j = i + 1; j < n; ++j) pairs.emplace_back(i, j);

    std::vector<double> corr(pairs.size());
    #pragma omp parallel for schedule(dynamic, 2)
    for (long k = 0; k < static_cast<long>(pairs.size()); ++k) {
        const Profile& a = prof[pairs[k].first];
        const Profile& b = prof[pairs[k].second];
        corr[k] = dot(a, b) / (a.norm * b.norm);
    }

    // ---- print in the original order (serial, so output is deterministic) ----
    for (std::size_t k = 0; k < pairs.size(); ++k)
        std::printf("%2d %2d -> %.20lf\n", pairs[k].first, pairs[k].second, corr[k]);

    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("time elapsed: %.3f seconds\n", secs);
    return 0;
}
