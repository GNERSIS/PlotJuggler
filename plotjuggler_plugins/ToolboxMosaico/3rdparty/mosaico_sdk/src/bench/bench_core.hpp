// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <arrow/status.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mosaico { class MosaicoClient; }

namespace mosaico::bench {

struct BenchParams {
    // Fetch
    std::string sequence_name;
    std::vector<std::string> topics;
    int64_t start_ns = 0;
    int64_t end_ns   = 0;
    bool retain_batches = false;   // default = streaming path
    bool use_cache      = false;   // bench bypasses cache by default
    // Bench knobs
    int      run_id              = 0;
    uint32_t sample_interval_ms  = 100;
    double   retention_warn      = 0.15;
    double   retention_fail      = 0.70;
    std::atomic<bool>* interrupted = nullptr;
};

struct PhaseEvent  { double t_s = 0; std::string phase; std::string topic; };
struct BatchEvent  { double t_s = 0; std::string topic; int64_t bytes = 0; int64_t rows = 0;
                     double gap_ms_same_topic = 0.0; };
struct SampleEvent { double t_s = 0;
                     uint32_t cwnd_segs = 0; double srtt_ms = 0.0;
                     uint64_t retrans_total = 0; uint64_t bytes_acked = 0;
                     uint32_t rcv_space_kb = 0;
                     uint64_t rss_kb = 0; uint64_t vsz_kb = 0; uint64_t heap_kb = 0;
                     double cpu_pct = 0.0; };

struct BenchSummary {
    int run_id = 0;
    double wall_time_s = 0;
    int64_t cum_bytes = 0;
    int64_t n_batches = 0;
    int     n_topics = 0;
    double avg_mbps = 0, peak_mbps = 0;
    double max_inter_batch_gap_ms = 0;
    uint64_t retrans_total = 0;
    uint64_t rss_start_kb = 0;
    uint64_t rss_peak_kb = 0;
    uint64_t rss_end_kb = 0;
    double   retention_ratio = 0;
    enum Verdict { kStreaming, kMixed, kRetentionBug } verdict = kStreaming;
    uint64_t heap_peak_kb = 0;
    double   cpu_avg_pct = 0, cpu_peak_pct = 0;
    arrow::Status status;
    std::string error;
};

class MetricsSink {
public:
    virtual ~MetricsSink() = default;
    virtual void onPhase  (const PhaseEvent&)   = 0;
    virtual void onBatch  (const BatchEvent&)   = 0;
    virtual void onSample (const SampleEvent&)  = 0;
    virtual void onSummary(const BenchSummary&) = 0;
};

class SinkDispatcher {
public:
    explicit SinkDispatcher(std::vector<MetricsSink*> sinks) : sinks_(std::move(sinks)) {}

    void dispatchPhase  (const PhaseEvent& e)    { std::lock_guard<std::mutex> g(mu_); for (auto* s : sinks_) s->onPhase(e); }
    void dispatchBatch  (const BatchEvent& e)    { std::lock_guard<std::mutex> g(mu_); for (auto* s : sinks_) s->onBatch(e); }
    void dispatchSample (const SampleEvent& e)   { std::lock_guard<std::mutex> g(mu_); for (auto* s : sinks_) s->onSample(e); }
    void dispatchSummary(const BenchSummary& s)  { std::lock_guard<std::mutex> g(mu_); for (auto* sk: sinks_) sk->onSummary(s); }

private:
    std::vector<MetricsSink*> sinks_;
    std::mutex mu_;  // serializes worker-thread phase/batch with sampler-thread sample
};

class GapTracker {
public:
    // Returns gap-since-previous-on-same-topic in milliseconds.
    // First observation per topic returns 0.0.
    double observe(const std::string& topic, double t_s);
private:
    std::unordered_map<std::string, double> last_t_s_;
};

class SummaryAccumulator {
public:
    SummaryAccumulator(int run_id, double warn, double fail);

    void onPhaseDispatch(double t_s);
    void onPhaseReturn  (double t_s);
    void onBatch(double t_s, const std::string& topic, int64_t bytes, int64_t rows);
    void onSample(double /*t_s*/, const SampleEvent& s);  // t_s already in s

    BenchSummary build() const;

private:
    int run_id_;
    double warn_, fail_;
    double t_dispatch_s_ = 0, t_return_s_ = 0;
    bool   has_first_sample_ = false;
    int64_t cum_bytes_ = 0, n_batches_ = 0;
    std::unordered_set<std::string> topics_;
    GapTracker gap_;
    double max_gap_ms_ = 0;

    uint64_t rss_start_kb_ = 0, rss_peak_kb_ = 0, rss_end_kb_ = 0;
    uint64_t heap_peak_kb_ = 0;
    uint64_t retrans_total_ = 0;
    double cpu_sum_ = 0, cpu_peak_ = 0;
    int    cpu_n_ = 0;

    // throughput windows: per-second buckets driven by samples
    double last_sample_t_s_ = 0;
    int64_t last_sample_cum_bytes_ = 0;
    double  peak_window_mbps_ = 0;
};

// Synchronous: blocks until the fetch finishes or `interrupted` is set.
// Implementation in subsequent tasks.
arrow::Status runInstrumentedFetch(MosaicoClient& client,
                                   const BenchParams& params,
                                   const std::vector<MetricsSink*>& sinks);

// Helper: classify a retention ratio against thresholds.
BenchSummary::Verdict classifyVerdict(double ratio, double warn, double fail);

}  // namespace mosaico::bench
