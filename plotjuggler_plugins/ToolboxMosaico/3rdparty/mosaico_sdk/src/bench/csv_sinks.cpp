// SPDX-License-Identifier: MPL-2.0
#include "bench/csv_sinks.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace mosaico::bench {

std::string slugifyTopic(std::string_view topic)
{
    std::string out;
    out.reserve(topic.size());
    for (char c : topic) {
        bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        out.push_back(ok ? c : '_');
    }
    return out;
}

namespace {
std::string isoUtc()
{
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tm);
    return buf;
}
std::string joinPath(const std::string& dir, const std::string& name)
{
    std::filesystem::path p(dir);
    p /= name;
    return p.string();
}
}  // namespace

std::string defaultDetailPath(const std::string& dir, const std::string& prefix)
{
    std::string p = prefix.empty() ? "bench" : prefix;
    return joinPath(dir, p + "-detail-" + isoUtc() + ".csv");
}
std::string defaultSummaryPath(const std::string& dir, const std::string& prefix)
{
    std::string p = prefix.empty() ? "bench" : prefix;
    return joinPath(dir, p + "-summary-" + isoUtc() + ".csv");
}

// ----- CsvDetailSink ----------------------------------------------------

CsvDetailSink::CsvDetailSink(const std::string& path, const std::vector<std::string>& topics)
{
    f_ = std::fopen(path.c_str(), "w");
    if (!f_) throw std::runtime_error("CsvDetailSink: cannot open " + path);
    topic_slugs_.reserve(topics.size());
    for (auto& t : topics) topic_slugs_.push_back(slugifyTopic(t));
    writeHeader();
}

CsvDetailSink::~CsvDetailSink()
{
    if (f_) std::fclose(f_);
}

void CsvDetailSink::writeHeader()
{
    // Build the header in a deterministic order, capturing column indices as
    // we go.  Schema mirrors spec §5.
    std::vector<std::string> cols = {"time_s", "run_id", "event_kind"};

    auto reg = [&](int& idx, const std::string& name) {
        idx = static_cast<int>(cols.size());
        cols.push_back(name);
    };

    reg(idx_total_cum_bytes_mb_, "total/cum_bytes_mb");
    reg(idx_total_cum_batches_,  "total/cum_batches");
    reg(idx_total_thru_,         "total/throughput_mbps_inst");

    reg(idx_net_cwnd_,    "network/cwnd_segs");
    reg(idx_net_srtt_,    "network/srtt_ms");
    reg(idx_net_retrans_, "network/retrans_total");
    reg(idx_net_acked_,   "network/bytes_acked");
    reg(idx_net_rcv_,     "network/rcv_space_kb");

    reg(idx_proc_rss_,   "process/rss_kb");
    reg(idx_proc_vsz_,   "process/vsz_kb");
    reg(idx_proc_heap_,  "process/heap_kb");
    reg(idx_proc_cpu_,   "process/cpu_pct");
    reg(idx_proc_ratio_, "process/rss_over_recv_ratio");

    reg(idx_phase_dispatch_,    "phase/dispatch");
    reg(idx_phase_first_sch_,   "phase/first_schema_any");
    reg(idx_phase_first_batch_, "phase/first_batch_any");
    reg(idx_phase_last_batch_,  "phase/last_batch_any");
    reg(idx_phase_return_,      "phase/return");

    per_topic_base_.reserve(topic_slugs_.size());
    for (const auto& slug : topic_slugs_) {
        per_topic_base_.push_back(static_cast<int>(cols.size()));
        cols.push_back("topic/" + slug + "/batch_bytes");
        cols.push_back("topic/" + slug + "/batch_rows");
        cols.push_back("topic/" + slug + "/cum_bytes_mb");
        cols.push_back("topic/" + slug + "/gap_ms");
        cols.push_back("topic/" + slug + "/throughput_mbps_inst");
        cols.push_back("topic/" + slug + "/phase/first_schema");
        cols.push_back("topic/" + slug + "/phase/first_batch");
        cols.push_back("topic/" + slug + "/phase/last_batch");
    }
    n_columns_ = static_cast<int>(cols.size());

    for (size_t i = 0; i < cols.size(); ++i) {
        std::fputs(cols[i].c_str(), f_);
        std::fputc(i + 1 == cols.size() ? '\n' : ',', f_);
    }
}

void CsvDetailSink::writeRow(const std::vector<std::pair<int, std::string>>& cells, int run_id)
{
    std::vector<std::string> row(n_columns_, "");
    for (auto& [idx, val] : cells) row[idx] = val;
    row[1] = std::to_string(run_id);
    for (int i = 0; i < n_columns_; ++i) {
        std::fputs(row[i].c_str(), f_);
        std::fputc(i + 1 == n_columns_ ? '\n' : ',', f_);
    }
}

namespace {
std::string fmtD(double v)
{
    char b[32];
    std::snprintf(b, sizeof(b), "%.6f", v);
    return b;
}
}  // namespace

void CsvDetailSink::onPhase(const PhaseEvent& e)
{
    std::vector<std::pair<int, std::string>> cells;
    cells.emplace_back(0, fmtD(e.t_s));
    cells.emplace_back(2, "phase");
    int col = -1;
    if (e.topic.empty()) {
        if      (e.phase == "dispatch")     col = idx_phase_dispatch_;
        else if (e.phase == "first_schema") col = idx_phase_first_sch_;
        else if (e.phase == "first_batch")  col = idx_phase_first_batch_;
        else if (e.phase == "last_batch")   col = idx_phase_last_batch_;
        else if (e.phase == "return")       col = idx_phase_return_;
    } else {
        auto slug = slugifyTopic(e.topic);
        for (size_t i = 0; i < topic_slugs_.size(); ++i) {
            if (topic_slugs_[i] != slug) continue;
            int base = per_topic_base_[i];
            if      (e.phase == "first_schema") col = base + kPhaseSchema;
            else if (e.phase == "first_batch")  col = base + kPhaseFirstBatch;
            else if (e.phase == "last_batch")   col = base + kPhaseLastBatch;
        }
    }
    if (col >= 0) cells.emplace_back(col, "1");
    writeRow(cells, /*run_id=*/0);  // run_id wired in Task 11 — placeholder for now
}

void CsvDetailSink::onBatch(const BatchEvent& e)
{
    cum_bytes_   += e.bytes;
    cum_batches_ += 1;
    double gap_ms = gap_.observe(e.topic, e.t_s);

    std::vector<std::pair<int, std::string>> cells;
    cells.emplace_back(0, fmtD(e.t_s));
    cells.emplace_back(2, "batch");
    cells.emplace_back(idx_total_cum_bytes_mb_, fmtD(cum_bytes_ / (1024.0 * 1024.0)));
    cells.emplace_back(idx_total_cum_batches_, std::to_string(cum_batches_));

    auto slug = slugifyTopic(e.topic);
    for (size_t i = 0; i < topic_slugs_.size(); ++i) {
        if (topic_slugs_[i] != slug) continue;
        int base = per_topic_base_[i];
        cells.emplace_back(base + kBatchBytes, std::to_string(e.bytes));
        cells.emplace_back(base + kBatchRows,  std::to_string(e.rows));
        cells.emplace_back(base + kGapMs,      fmtD(gap_ms));
    }
    writeRow(cells, 0);
}

void CsvDetailSink::onSample(const SampleEvent& s)
{
    if (!has_rss_start_) {
        rss_start_kb_ = static_cast<double>(s.rss_kb);
        has_rss_start_ = true;
    }

    double dt = s.t_s - last_sample_t_s_;
    double thru_mbps = 0.0;
    if (last_sample_t_s_ > 0 && dt > 0) {
        double dB = static_cast<double>(cum_bytes_ - last_sample_cum_bytes_);
        thru_mbps = (dB * 8.0) / (dt * 1e6);
    }
    double ratio = (cum_bytes_ > 0)
        ? (static_cast<double>(s.rss_kb) - rss_start_kb_) / (cum_bytes_ / 1024.0)
        : 0.0;

    std::vector<std::pair<int, std::string>> cells;
    cells.emplace_back(0, fmtD(s.t_s));
    cells.emplace_back(2, "sample");
    cells.emplace_back(idx_total_thru_,    fmtD(thru_mbps));
    cells.emplace_back(idx_net_cwnd_,      std::to_string(s.cwnd_segs));
    cells.emplace_back(idx_net_srtt_,      fmtD(s.srtt_ms));
    cells.emplace_back(idx_net_retrans_,   std::to_string(s.retrans_total));
    cells.emplace_back(idx_net_acked_,     std::to_string(s.bytes_acked));
    cells.emplace_back(idx_net_rcv_,       std::to_string(s.rcv_space_kb));
    cells.emplace_back(idx_proc_rss_,      std::to_string(s.rss_kb));
    cells.emplace_back(idx_proc_vsz_,      std::to_string(s.vsz_kb));
    cells.emplace_back(idx_proc_heap_,     std::to_string(s.heap_kb));
    cells.emplace_back(idx_proc_cpu_,      fmtD(s.cpu_pct));
    cells.emplace_back(idx_proc_ratio_,    fmtD(ratio));
    writeRow(cells, 0);

    last_sample_t_s_       = s.t_s;
    last_sample_cum_bytes_ = cum_bytes_;
}

void CsvDetailSink::onSummary(const BenchSummary&) { /* summary handled by CsvSummarySink */ }

// ----- CsvSummarySink ---------------------------------------------------

CsvSummarySink::CsvSummarySink(const std::string& path)
{
    f_ = std::fopen(path.c_str(), "w");
    if (!f_) throw std::runtime_error("CsvSummarySink: cannot open " + path);
}
CsvSummarySink::~CsvSummarySink() { if (f_) std::fclose(f_); }

void CsvSummarySink::onSummary(const BenchSummary& s)
{
    if (!header_written_) {
        std::fputs(
            "run_id,wall_time_s,cum_bytes_mb,n_batches,n_topics,"
            "avg_mbps,peak_mbps,max_inter_batch_gap_ms,retrans_total,"
            "rss_start_mb,rss_peak_mb,rss_end_mb,rss_growth_mb,"
            "cum_bytes_to_rss_growth_ratio,retention_ratio,verdict,"
            "heap_peak_mb,cpu_avg_pct,cpu_peak_pct,status_ok,error\n", f_);
        header_written_ = true;
    }
    auto kbToMb = [](uint64_t kb) { return static_cast<double>(kb) / 1024.0; };
    auto growth_mb = kbToMb(s.rss_peak_kb) - kbToMb(s.rss_start_kb);
    auto inv_ratio = (growth_mb > 0)
        ? (static_cast<double>(s.cum_bytes) / (1024.0 * 1024.0)) / growth_mb
        : 0.0;
    const char* verdict_s = (s.verdict == BenchSummary::kStreaming)     ? "streaming"
                          : (s.verdict == BenchSummary::kMixed)         ? "mixed"
                                                                        : "retention_bug";
    std::fprintf(f_,
        "%d,%.6f,%.6f,%lld,%d,"
        "%.6f,%.6f,%.6f,%llu,"
        "%.3f,%.3f,%.3f,%.3f,"
        "%.6f,%.6f,%s,"
        "%.3f,%.3f,%.3f,%d,%s\n",
        s.run_id, s.wall_time_s, s.cum_bytes / (1024.0 * 1024.0),
        static_cast<long long>(s.n_batches), s.n_topics,
        s.avg_mbps, s.peak_mbps, s.max_inter_batch_gap_ms,
        static_cast<unsigned long long>(s.retrans_total),
        kbToMb(s.rss_start_kb), kbToMb(s.rss_peak_kb),
        kbToMb(s.rss_end_kb),   growth_mb,
        inv_ratio, s.retention_ratio, verdict_s,
        kbToMb(s.heap_peak_kb), s.cpu_avg_pct, s.cpu_peak_pct,
        s.status.ok() ? 1 : 0, s.error.c_str());
}

}  // namespace mosaico::bench
