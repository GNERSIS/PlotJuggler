// SPDX-License-Identifier: MPL-2.0
//
// mosaico_fetch_bench — instrumented Mosaico Flight benchmark CLI.
// See docs/superpowers/specs/2026-04-27-mosaico-fetch-bench-design.md

#include "bench/bench_core.hpp"
#include "bench/csv_sinks.hpp"
#include "flight/mosaico_client.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string uri;
    std::string sequence;
    std::vector<std::string> topics;
    std::string topics_file;
    int64_t start_ns = -1;
    int64_t end_ns   = -1;
    std::string start_iso;
    int64_t duration_s = -1;
    int repeat = 1;
    bool use_cache = false;
    int retain_batches = -1;  // -1 = default (false); 0/1 explicit
    uint32_t sample_interval_ms = 100;
    std::string output_dir = ".";
    std::string output_prefix;
    bool no_csv = false;
    double retention_warn = 0.15;
    double retention_fail = 0.70;
    int verbosity = 1;
    bool show_help = false;
    bool show_version = false;
};

void usage(const char* prog)
{
    std::fprintf(stderr,
        "usage: %s --uri grpc+tls://HOST:PORT --sequence NAME\n"
        "          (--topics A,B,C | --topics-file PATH)\n"
        "          (--start-ns NS --end-ns NS | --start-iso ISO --duration-s N)\n"
        "          [--repeat N=1] [--use-cache] [--retain-batches=0|1]\n"
        "          [--sample-interval-ms N=100]\n"
        "          [--output-dir DIR=.] [--output-prefix NAME] [--no-csv]\n"
        "          [--retention-warn 0.15] [--retention-fail 0.70]\n"
        "          [-q | -v] [-h | --help] [--version]\n", prog);
}

int parseArgs(int argc, char** argv, Args& a)
{
    auto eqVal = [](const char* arg, const char* prefix) -> const char* {
        size_t n = std::strlen(prefix);
        return (std::strncmp(arg, prefix, n) == 0) ? arg + n : nullptr;
    };
    auto split = [](const std::string& s, char sep) {
        std::vector<std::string> out; std::string cur;
        for (char c : s) { if (c == sep) { if (!cur.empty()) out.push_back(cur); cur.clear(); } else cur.push_back(c); }
        if (!cur.empty()) out.push_back(cur);
        return out;
    };
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if      (auto v = eqVal(arg, "--uri="))                a.uri = v;
        else if (auto v = eqVal(arg, "--sequence="))           a.sequence = v;
        else if (auto v = eqVal(arg, "--topics="))             a.topics = split(v, ',');
        else if (auto v = eqVal(arg, "--topics-file="))        a.topics_file = v;
        else if (auto v = eqVal(arg, "--start-ns="))           a.start_ns = std::atoll(v);
        else if (auto v = eqVal(arg, "--end-ns="))             a.end_ns = std::atoll(v);
        else if (auto v = eqVal(arg, "--start-iso="))          a.start_iso = v;
        else if (auto v = eqVal(arg, "--duration-s="))         a.duration_s = std::atoll(v);
        else if (auto v = eqVal(arg, "--repeat="))             a.repeat = std::atoi(v);
        else if (std::strcmp(arg, "--use-cache") == 0)         a.use_cache = true;
        else if (auto v = eqVal(arg, "--retain-batches="))     a.retain_batches = std::atoi(v);
        else if (auto v = eqVal(arg, "--sample-interval-ms=")) a.sample_interval_ms = static_cast<uint32_t>(std::atoi(v));
        else if (auto v = eqVal(arg, "--output-dir="))         a.output_dir = v;
        else if (auto v = eqVal(arg, "--output-prefix="))      a.output_prefix = v;
        else if (std::strcmp(arg, "--no-csv") == 0)            a.no_csv = true;
        else if (auto v = eqVal(arg, "--retention-warn="))     a.retention_warn = std::atof(v);
        else if (auto v = eqVal(arg, "--retention-fail="))     a.retention_fail = std::atof(v);
        else if (std::strcmp(arg, "-q") == 0)                  a.verbosity = 0;
        else if (std::strcmp(arg, "-v") == 0)                  a.verbosity = 2;
        else if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) { a.show_help = true; return 0; }
        else if (std::strcmp(arg, "--version") == 0)           { a.show_version = true; return 0; }
        else { std::fprintf(stderr, "unknown arg: %s\n", arg); return 4; }
    }

    if (a.show_help || a.show_version) return 0;

    // Validation
    if (a.uri.empty())       { std::fprintf(stderr, "--uri required\n"); return 4; }
    if (a.sequence.empty())  { std::fprintf(stderr, "--sequence required\n"); return 4; }
    if (a.topics.empty() && a.topics_file.empty()) {
        std::fprintf(stderr, "either --topics or --topics-file required\n"); return 4;
    }
    if (!a.topics_file.empty()) {
        std::ifstream f(a.topics_file);
        if (!f) { std::fprintf(stderr, "cannot read --topics-file: %s\n", a.topics_file.c_str()); return 4; }
        std::string line;
        while (std::getline(f, line)) if (!line.empty() && line.front() != '#') a.topics.push_back(line);
    }
    if (a.start_ns < 0 || a.end_ns < 0) {
        std::fprintf(stderr, "--start-ns and --end-ns required (ISO support deferred)\n");
        return 4;
    }
    if (a.repeat < 1) a.repeat = 1;
    return 0;
}

std::atomic<bool> g_interrupted{false};
std::chrono::steady_clock::time_point g_last_sigint{};

void onSigint(int)
{
    auto now = std::chrono::steady_clock::now();
    if (g_interrupted.load() &&
        std::chrono::duration_cast<std::chrono::seconds>(now - g_last_sigint).count() < 2) {
        std::_Exit(130);
    }
    g_interrupted = true;
    g_last_sigint = now;
}

class LambdaSink : public mosaico::bench::MetricsSink {
public:
    using SummaryFn = std::function<void(const mosaico::bench::BenchSummary&)>;
    using BatchFn   = std::function<void(const mosaico::bench::BatchEvent&)>;
    using SampleFn  = std::function<void(const mosaico::bench::SampleEvent&)>;

    LambdaSink(SummaryFn fs, BatchFn fb = {}, SampleFn fsa = {})
        : on_summary_(std::move(fs)), on_batch_(std::move(fb)), on_sample_(std::move(fsa)) {}

    void onPhase(const mosaico::bench::PhaseEvent&)    override {}
    void onBatch(const mosaico::bench::BatchEvent& e)  override { if (on_batch_)  on_batch_(e); }
    void onSample(const mosaico::bench::SampleEvent& e) override { if (on_sample_) on_sample_(e); }
    void onSummary(const mosaico::bench::BenchSummary& s) override { on_summary_(s); }
private:
    SummaryFn on_summary_;
    BatchFn   on_batch_;
    SampleFn  on_sample_;
};

}  // namespace

int main(int argc, char** argv)
{
    Args a;
    if (int rc = parseArgs(argc, argv, a); rc != 0) return rc;
    if (a.show_help)    { usage(argv[0]); return 0; }
    if (a.show_version) { std::printf("mosaico_fetch_bench 0.1.0\n"); return 0; }

    std::signal(SIGINT,  onSigint);
    std::signal(SIGTERM, onSigint);
    std::signal(SIGPIPE, SIG_IGN);

    // MosaicoClient constructor establishes the connection pool.
    // Verify connectivity by calling version() — fails fast on bad URI/network.
    static constexpr int kTimeoutSec  = 30;
    static constexpr int kPoolSize    = 4;
    mosaico::MosaicoClient client(a.uri, kTimeoutSec, kPoolSize);
    if (auto ver = client.version(); !ver.ok()) {
        std::fprintf(stderr, "connect failed: %s\n", ver.status().ToString().c_str());
        return 3;
    }

    mosaico::bench::BenchParams params;
    params.sequence_name      = a.sequence;
    params.topics             = a.topics;
    params.start_ns           = a.start_ns;
    params.end_ns             = a.end_ns;
    params.use_cache          = a.use_cache;
    params.retain_batches     = (a.retain_batches == 1);
    params.sample_interval_ms = a.sample_interval_ms;
    params.retention_warn     = a.retention_warn;
    params.retention_fail     = a.retention_fail;
    params.interrupted        = &g_interrupted;

    std::unique_ptr<mosaico::bench::CsvDetailSink>  detail;
    std::unique_ptr<mosaico::bench::CsvSummarySink> summary;
    if (!a.no_csv) {
        auto detail_path  = mosaico::bench::defaultDetailPath (a.output_dir, a.output_prefix);
        auto summary_path = mosaico::bench::defaultSummaryPath(a.output_dir, a.output_prefix);
        detail  = std::make_unique<mosaico::bench::CsvDetailSink>(detail_path, a.topics);
        summary = std::make_unique<mosaico::bench::CsvSummarySink>(summary_path);
        std::fprintf(stderr, "writing detail=%s summary=%s\n", detail_path.c_str(), summary_path.c_str());
    }

    auto exit_for_verdict = [](mosaico::bench::BenchSummary::Verdict v) -> int {
        switch (v) {
            case mosaico::bench::BenchSummary::kStreaming:    return 0;
            case mosaico::bench::BenchSummary::kMixed:        return 1;
            case mosaico::bench::BenchSummary::kRetentionBug: return 2;
        }
        return 0;
    };

    int worst_exit = 0;

    for (int k = 0; k < a.repeat; ++k) {
        if (g_interrupted.load()) break;
        params.run_id = k;

        mosaico::bench::BenchSummary final_summary{};
        LambdaSink capture([&](const mosaico::bench::BenchSummary& s){ final_summary = s; });

        std::vector<mosaico::bench::MetricsSink*> sinks;
        if (detail)  sinks.push_back(detail.get());
        if (summary) sinks.push_back(summary.get());
        sinks.push_back(&capture);

        std::fprintf(stderr, "[run %d/%d] dispatching\n", k + 1, a.repeat);
        auto status = mosaico::bench::runInstrumentedFetch(client, params, sinks);
        const char* verdict_s = (final_summary.verdict == mosaico::bench::BenchSummary::kStreaming)     ? "streaming"
                              : (final_summary.verdict == mosaico::bench::BenchSummary::kMixed)         ? "mixed"
                                                                                                        : "retention_bug";
        std::fprintf(stderr,
            "[run %d] %s in %.1fs — %.1f Mbps avg, peak %.1f, RSS %.0f→%.0f MB, ratio %.3f → %s\n",
            k, status.ok() ? "ok" : "FAIL",
            final_summary.wall_time_s,
            final_summary.avg_mbps, final_summary.peak_mbps,
            static_cast<double>(final_summary.rss_start_kb) / 1024.0,
            static_cast<double>(final_summary.rss_peak_kb)  / 1024.0,
            final_summary.retention_ratio, verdict_s);

        if (!status.ok()) worst_exit = std::max(worst_exit, 3);
        else              worst_exit = std::max(worst_exit, exit_for_verdict(final_summary.verdict));
    }
    return worst_exit;
}
