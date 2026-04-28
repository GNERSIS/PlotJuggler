// SPDX-License-Identifier: MPL-2.0
#include "bench/bench_core.hpp"

#include "flight/mosaico_client.hpp"
#include "bench/proc_self.hpp"
#include "bench/ss_poller.hpp"
#include <arrow/util/byte_size.h>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace mosaico::bench {

BenchSummary::Verdict classifyVerdict(double ratio, double warn, double fail)
{
    if (ratio < warn) return BenchSummary::kStreaming;
    if (ratio < fail) return BenchSummary::kMixed;
    return BenchSummary::kRetentionBug;
}

double GapTracker::observe(const std::string& topic, double t_s)
{
    auto it = last_t_s_.find(topic);
    double gap_ms = (it == last_t_s_.end()) ? 0.0 : (t_s - it->second) * 1000.0;
    last_t_s_[topic] = t_s;
    return gap_ms;
}

namespace {
double nowSeconds(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}
}  // namespace

arrow::Status runInstrumentedFetch(MosaicoClient& client,
                                   const BenchParams& params,
                                   const std::vector<MetricsSink*>& sinks)
{
    auto start = std::chrono::steady_clock::now();
    SinkDispatcher dispatcher(sinks);
    SummaryAccumulator acc(params.run_id, params.retention_warn, params.retention_fail);

    std::atomic<bool> sampler_running{true};
    std::atomic<uint64_t> latest_retrans{0};
    std::atomic<uint32_t> latest_cwnd{0};
    std::atomic<uint64_t> latest_acked{0};
    std::atomic<uint32_t> latest_rcv{0};
    std::atomic<bool>     latest_srtt_set{false};
    std::atomic<double>   latest_srtt{0.0};

    // Parse host:port out of the client URI for ss filtering.
    std::string ss_host;
    uint16_t    ss_port = 0;
    {
        const std::string& uri = client.uri();
        auto last_colon = uri.rfind(':');
        if (last_colon != std::string::npos) {
            auto host_start = uri.find("://");
            host_start = (host_start == std::string::npos) ? 0 : host_start + 3;
            ss_host = uri.substr(host_start, last_colon - host_start);
            try {
                ss_port = static_cast<uint16_t>(std::stoi(uri.substr(last_colon + 1)));
            } catch (...) {
                ss_port = 0;
            }
        }
    }

    SsPoller ss_poller(ss_host, ss_port, params.sample_interval_ms,
        [&](const SsSample& s) {
            latest_cwnd     = s.cwnd_segs;
            latest_srtt     = s.srtt_ms;
            latest_srtt_set = true;
            latest_retrans  = s.retrans_total;
            latest_acked    = s.bytes_acked;
            latest_rcv      = s.rcv_space_kb;
        });
    if (ss_port != 0) ss_poller.start();

    ProcStat last_stat = readProcSelfStat();
    auto last_stat_t = std::chrono::steady_clock::now();
    long clk_tck = sysconfClkTck();

    std::thread sampler([&]{
        while (sampler_running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(params.sample_interval_ms));
            auto status = readProcSelfStatus();
            auto stat   = readProcSelfStat();
            auto now    = std::chrono::steady_clock::now();
            double dt   = std::chrono::duration<double>(now - last_stat_t).count();
            uint64_t ticks_used = (stat.utime_ticks + stat.stime_ticks) -
                                  (last_stat.utime_ticks + last_stat.stime_ticks);
            double cpu_pct = (dt > 0 && clk_tck > 0)
                ? 100.0 * static_cast<double>(ticks_used) / (dt * clk_tck)
                : 0.0;
            last_stat = stat; last_stat_t = now;

            SampleEvent e{};
            e.t_s = nowSeconds(start);
            e.cwnd_segs     = latest_cwnd.load();
            e.srtt_ms       = latest_srtt_set.load() ? latest_srtt.load() : 0.0;
            e.retrans_total = latest_retrans.load();
            e.bytes_acked   = latest_acked.load();
            e.rcv_space_kb  = latest_rcv.load();
            e.rss_kb  = status.vm_rss_kb;
            e.vsz_kb  = status.vm_size_kb;
            e.heap_kb = status.vm_data_kb;
            e.cpu_pct = cpu_pct;
            acc.onSample(e.t_s, e);
            dispatcher.dispatchSample(e);
        }
    });

    auto emit_phase = [&](const std::string& name, const std::string& topic) {
        PhaseEvent p{nowSeconds(start), name, topic};
        if (topic.empty()) {
            if (name == "dispatch") acc.onPhaseDispatch(p.t_s);
            if (name == "return")   acc.onPhaseReturn(p.t_s);
        }
        dispatcher.dispatchPhase(p);
    };

    emit_phase("run_start", "");
    emit_phase("dispatch",  "");

    std::unordered_set<std::string> first_schema_seen, first_batch_seen, last_batch_seen;
    std::mutex seen_mu;
    bool first_schema_any_emitted = false;
    bool first_batch_any_emitted  = false;

    auto schema_cb = [&](const std::string& topic, const std::shared_ptr<arrow::Schema>&) {
        std::lock_guard<std::mutex> g(seen_mu);
        if (first_schema_seen.insert(topic).second) emit_phase("first_schema", topic);
        if (!first_schema_any_emitted) { first_schema_any_emitted = true; emit_phase("first_schema", ""); }
    };
    auto batch_cb = [&](const std::string& topic, const std::shared_ptr<arrow::RecordBatch>& batch) {
        if (!batch) return;
        BatchEvent be;
        be.t_s   = nowSeconds(start);
        be.topic = topic;
        be.bytes = static_cast<int64_t>(arrow::util::TotalBufferSize(*batch));
        be.rows  = batch->num_rows();
        acc.onBatch(be.t_s, topic, be.bytes, be.rows);
        dispatcher.dispatchBatch(be);

        std::lock_guard<std::mutex> g(seen_mu);
        if (first_batch_seen.insert(topic).second) emit_phase("first_batch", topic);
        if (!first_batch_any_emitted) { first_batch_any_emitted = true; emit_phase("first_batch", ""); }
    };
    auto on_done = [&](const std::string& topic, arrow::Result<PullResult> /*result*/) {
        std::lock_guard<std::mutex> g(seen_mu);
        if (last_batch_seen.insert(topic).second) emit_phase("last_batch", topic);
    };

    std::vector<std::string> topics_std = params.topics;
    TimeRange range;
    if (params.start_ns > 0) range.start_ns = params.start_ns;
    if (params.end_ns   > 0) range.end_ns   = params.end_ns;

    arrow::Status status = client.pullTopics(
        params.sequence_name, topics_std, range,
        on_done, /*progress=*/nullptr, params.interrupted,
        batch_cb, schema_cb, params.retain_batches);

    emit_phase("last_batch", "");
    emit_phase("return", "");

    sampler_running.store(false);
    sampler.join();
    if (ss_port != 0) ss_poller.stop();

    BenchSummary summary = acc.build();
    summary.status = status;
    if (!status.ok()) summary.error = status.ToString();
    dispatcher.dispatchSummary(summary);
    return status;
}

SummaryAccumulator::SummaryAccumulator(int run_id, double warn, double fail)
    : run_id_(run_id), warn_(warn), fail_(fail) {}

void SummaryAccumulator::onPhaseDispatch(double t_s) { t_dispatch_s_ = t_s; }
void SummaryAccumulator::onPhaseReturn  (double t_s) { t_return_s_   = t_s; }

void SummaryAccumulator::onBatch(double t_s, const std::string& topic, int64_t bytes, int64_t /*rows*/)
{
    cum_bytes_ += bytes;
    n_batches_ += 1;
    topics_.insert(topic);
    double gap_ms = gap_.observe(topic, t_s);
    if (gap_ms > max_gap_ms_) max_gap_ms_ = gap_ms;
}

void SummaryAccumulator::onSample(double /*t_s*/, const SampleEvent& s)
{
    if (!has_first_sample_) {
        rss_start_kb_ = s.rss_kb;
        has_first_sample_ = true;
    }
    rss_end_kb_ = s.rss_kb;
    if (s.rss_kb > rss_peak_kb_)  rss_peak_kb_  = s.rss_kb;
    if (s.heap_kb > heap_peak_kb_) heap_peak_kb_ = s.heap_kb;
    if (s.retrans_total > retrans_total_) retrans_total_ = s.retrans_total;
    cpu_sum_ += s.cpu_pct;
    cpu_n_   += 1;
    if (s.cpu_pct > cpu_peak_) cpu_peak_ = s.cpu_pct;

    // Per-window Mbps (delta cum_bytes / delta t)
    double dt = s.t_s - last_sample_t_s_;
    if (last_sample_t_s_ > 0 && dt > 0) {
        double dB = static_cast<double>(cum_bytes_ - last_sample_cum_bytes_);
        double mbps = (dB * 8.0) / (dt * 1e6);
        if (mbps > peak_window_mbps_) peak_window_mbps_ = mbps;
    }
    last_sample_t_s_       = s.t_s;
    last_sample_cum_bytes_ = cum_bytes_;
}

BenchSummary SummaryAccumulator::build() const
{
    BenchSummary out;
    out.run_id = run_id_;
    out.wall_time_s = std::max(0.0, t_return_s_ - t_dispatch_s_);
    out.cum_bytes  = cum_bytes_;
    out.n_batches  = n_batches_;
    out.n_topics   = static_cast<int>(topics_.size());

    if (out.wall_time_s > 0)
        out.avg_mbps = (static_cast<double>(cum_bytes_) * 8.0) / (out.wall_time_s * 1e6);
    out.peak_mbps = peak_window_mbps_;
    out.max_inter_batch_gap_ms = max_gap_ms_;
    out.retrans_total = retrans_total_;

    out.rss_start_kb = rss_start_kb_;
    out.rss_peak_kb  = rss_peak_kb_;
    out.rss_end_kb   = rss_end_kb_;
    out.heap_peak_kb = heap_peak_kb_;

    uint64_t growth_kb = (rss_peak_kb_ > rss_start_kb_) ? rss_peak_kb_ - rss_start_kb_ : 0;
    uint64_t bytes_kb  = static_cast<uint64_t>(cum_bytes_ / 1024);
    out.retention_ratio = (bytes_kb > 0)
        ? static_cast<double>(growth_kb) / static_cast<double>(bytes_kb)
        : 0.0;
    out.verdict = classifyVerdict(out.retention_ratio, warn_, fail_);

    out.cpu_avg_pct  = (cpu_n_ > 0) ? cpu_sum_ / cpu_n_ : 0.0;
    out.cpu_peak_pct = cpu_peak_;
    return out;
}

}  // namespace mosaico::bench
