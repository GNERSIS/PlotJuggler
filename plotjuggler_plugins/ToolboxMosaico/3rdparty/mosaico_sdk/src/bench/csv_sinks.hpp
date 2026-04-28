// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "bench/bench_core.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace mosaico::bench {

// Slugify topic name → CSV-safe column fragment.
std::string slugifyTopic(std::string_view topic);

// Writes one row per event using the spec §5 wide schema. Topic columns
// are pre-declared at construction so headers are stable per run.
class CsvDetailSink : public MetricsSink {
public:
    CsvDetailSink(const std::string& path, const std::vector<std::string>& topics);
    ~CsvDetailSink() override;

    CsvDetailSink(const CsvDetailSink&) = delete;
    CsvDetailSink& operator=(const CsvDetailSink&) = delete;

    void onPhase  (const PhaseEvent&)   override;
    void onBatch  (const BatchEvent&)   override;
    void onSample (const SampleEvent&)  override;
    void onSummary(const BenchSummary&) override;

private:
    void writeHeader();
    // Number of columns is constant per run; we write a row by populating a
    // small vector<std::string> of cells and joining with commas.
    void writeRow(const std::vector<std::pair<int /*column index*/, std::string /*value*/>>& cells, int run_id);

    FILE* f_ = nullptr;
    std::vector<std::string> topic_slugs_;
    // Column index map. Total columns = fixed_count + 8 per topic.
    int idx_total_cum_bytes_mb_ = 0;
    int idx_total_cum_batches_  = 0;
    int idx_total_thru_         = 0;
    int idx_net_cwnd_           = 0;
    int idx_net_srtt_           = 0;
    int idx_net_retrans_        = 0;
    int idx_net_acked_          = 0;
    int idx_net_rcv_            = 0;
    int idx_proc_rss_           = 0;
    int idx_proc_vsz_           = 0;
    int idx_proc_heap_          = 0;
    int idx_proc_cpu_           = 0;
    int idx_proc_ratio_         = 0;
    int idx_phase_dispatch_     = 0;
    int idx_phase_first_sch_    = 0;
    int idx_phase_first_batch_  = 0;
    int idx_phase_last_batch_   = 0;
    int idx_phase_return_       = 0;
    int n_columns_              = 0;
    // Per-topic base index; topic k's batch_bytes is at base+0, rows at base+1, etc.
    std::vector<int> per_topic_base_;
    enum TopicCol { kBatchBytes=0, kBatchRows, kCumBytesMb, kGapMs, kThruMbps,
                    kPhaseSchema, kPhaseFirstBatch, kPhaseLastBatch, kTopicColCount };

    // Running aggregates needed for total/* columns.
    int64_t cum_bytes_ = 0, cum_batches_ = 0;
    double  last_sample_t_s_ = 0;
    int64_t last_sample_cum_bytes_ = 0;
    double  rss_start_kb_ = 0;  // captured on first sample for ratio
    bool    has_rss_start_ = false;
    GapTracker gap_;
};

// Single-row summary writer. Header is emitted on first run.
class CsvSummarySink : public MetricsSink {
public:
    explicit CsvSummarySink(const std::string& path);
    ~CsvSummarySink() override;

    CsvSummarySink(const CsvSummarySink&) = delete;
    CsvSummarySink& operator=(const CsvSummarySink&) = delete;

    void onPhase  (const PhaseEvent&)   override {}
    void onBatch  (const BatchEvent&)   override {}
    void onSample (const SampleEvent&)  override {}
    void onSummary(const BenchSummary& s) override;

private:
    FILE* f_ = nullptr;
    bool  header_written_ = false;
};

// Helper: build the canonical timestamped output paths.
std::string defaultDetailPath  (const std::string& dir, const std::string& prefix);
std::string defaultSummaryPath (const std::string& dir, const std::string& prefix);

}  // namespace mosaico::bench
