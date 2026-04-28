// SPDX-License-Identifier: MPL-2.0
#include "bench/bench_core.hpp"
#include <gtest/gtest.h>

using namespace mosaico::bench;

TEST(BenchTypes, default_constructed_params_are_safe)
{
  BenchParams p{};
  EXPECT_EQ(p.start_ns, 0);
  EXPECT_EQ(p.end_ns, 0);
  EXPECT_FALSE(p.retain_batches);
  EXPECT_FALSE(p.use_cache);
  EXPECT_EQ(p.run_id, 0);
  EXPECT_EQ(p.sample_interval_ms, 100u);
  EXPECT_DOUBLE_EQ(p.retention_warn, 0.15);
  EXPECT_DOUBLE_EQ(p.retention_fail, 0.70);
  EXPECT_EQ(p.interrupted, nullptr);
}

TEST(BenchTypes, summary_default_verdict_is_streaming)
{
  BenchSummary s{};
  EXPECT_EQ(s.verdict, BenchSummary::kStreaming);
  EXPECT_EQ(s.run_id, 0);
}

TEST(Verdict, default_thresholds_streaming_below_warn)
{
  EXPECT_EQ(classifyVerdict(0.00, 0.15, 0.70), BenchSummary::kStreaming);
  EXPECT_EQ(classifyVerdict(0.10, 0.15, 0.70), BenchSummary::kStreaming);
  EXPECT_EQ(classifyVerdict(0.149, 0.15, 0.70), BenchSummary::kStreaming);
}

TEST(Verdict, at_warn_threshold_promoted_to_mixed)
{
  EXPECT_EQ(classifyVerdict(0.150, 0.15, 0.70), BenchSummary::kMixed);
  EXPECT_EQ(classifyVerdict(0.500, 0.15, 0.70), BenchSummary::kMixed);
  EXPECT_EQ(classifyVerdict(0.699, 0.15, 0.70), BenchSummary::kMixed);
}

TEST(Verdict, at_fail_threshold_promoted_to_retention_bug)
{
  EXPECT_EQ(classifyVerdict(0.700, 0.15, 0.70), BenchSummary::kRetentionBug);
  EXPECT_EQ(classifyVerdict(1.100, 0.15, 0.70), BenchSummary::kRetentionBug);
  EXPECT_EQ(classifyVerdict(2.000, 0.15, 0.70), BenchSummary::kRetentionBug);
}

TEST(Verdict, custom_thresholds_honored)
{
  EXPECT_EQ(classifyVerdict(0.10, 0.05, 0.30), BenchSummary::kMixed);
  EXPECT_EQ(classifyVerdict(0.40, 0.05, 0.30), BenchSummary::kRetentionBug);
}

TEST(GapTracker, first_batch_per_topic_has_zero_gap)
{
  GapTracker g;
  EXPECT_DOUBLE_EQ(g.observe("a", 0.10), 0.0);
  EXPECT_DOUBLE_EQ(g.observe("b", 0.15), 0.0);
}

TEST(GapTracker, subsequent_batches_per_topic_get_correct_gap)
{
  GapTracker g;
  g.observe("a", 0.10);
  g.observe("b", 0.20);
  EXPECT_NEAR(g.observe("a", 0.30), 200.0, 1e-9);  // ms
  EXPECT_NEAR(g.observe("b", 0.50), 300.0, 1e-9);
}

TEST(GapTracker, interleaved_topics_track_independently)
{
  GapTracker g;
  g.observe("a", 1.0);
  g.observe("b", 1.1);
  g.observe("a", 1.5);  // a-gap = 500ms
  g.observe("b", 1.7);  // b-gap = 600ms
  EXPECT_NEAR(g.observe("a", 1.6), 100.0, 1e-9);
  EXPECT_NEAR(g.observe("b", 2.0), 300.0, 1e-9);
}

namespace
{
struct RecordingSink : public MetricsSink
{
  std::vector<std::string> log;
  void onPhase(const PhaseEvent& e) override
  {
    log.push_back("P:" + e.phase + ":" + e.topic);
  }
  void onBatch(const BatchEvent& e) override
  {
    log.push_back("B:" + e.topic);
  }
  void onSample(const SampleEvent&) override
  {
    log.push_back("S");
  }
  void onSummary(const BenchSummary&) override
  {
    log.push_back("D");
  }
};
}  // namespace

TEST(SinkDispatcher, fanout_in_order_to_every_sink)
{
  RecordingSink a, b;
  SinkDispatcher d({ &a, &b });
  d.dispatchPhase(PhaseEvent{ 0, "dispatch", "" });
  d.dispatchBatch(BatchEvent{ 0.1, "imu", 100, 10, 0 });
  d.dispatchSample(SampleEvent{ 0.2 });
  d.dispatchSummary(BenchSummary{});

  const std::vector<std::string> expected = { "P:dispatch:", "B:imu", "S", "D" };
  EXPECT_EQ(a.log, expected);
  EXPECT_EQ(b.log, expected);
}

TEST(SummaryAccumulator, streaming_path_low_retention_ratio)
{
  SummaryAccumulator acc(/*run_id=*/0, /*warn=*/0.15, /*fail=*/0.70);
  acc.onPhaseDispatch(0.0);
  // Receive 100 MB total across the run.
  acc.onBatch(0.1, "imu", /*bytes=*/100 * 1024 * 1024, /*rows=*/0);
  // RSS climbs only ~5 MB — typical streaming.
  acc.onSample(0.0, SampleEvent{ /*t_s=*/0.0, /*cwnd*/ 0, /*srtt*/ 0, /*retrans*/ 0, /*ack*/ 0,
                                 /*rcv*/ 0,
                                 /*rss_kb=*/200 * 1024, /*vsz=*/0, /*heap=*/0, /*cpu=*/0 });
  acc.onSample(1.0, SampleEvent{ 1.0, 0, 0, 0, 0, 0, 205 * 1024, 0, 0, 0 });
  acc.onPhaseReturn(1.0);
  BenchSummary s = acc.build();
  EXPECT_EQ(s.cum_bytes, 100 * 1024 * 1024);
  EXPECT_EQ(s.rss_start_kb, 200u * 1024u);
  EXPECT_EQ(s.rss_peak_kb, 205u * 1024u);
  EXPECT_NEAR(s.retention_ratio, 5.0 / 100.0, 0.01);
  EXPECT_EQ(s.verdict, BenchSummary::kStreaming);
}

TEST(SummaryAccumulator, retain_path_pegs_retention_ratio_to_one)
{
  SummaryAccumulator acc(0, 0.15, 0.70);
  acc.onPhaseDispatch(0.0);
  acc.onBatch(0.1, "imu", 100 * 1024 * 1024, 0);
  acc.onSample(0.0, SampleEvent{ 0.0, 0, 0, 0, 0, 0, 200 * 1024, 0, 0, 0 });
  // RSS climbs by ~110 MB — slightly over the received bytes (decode scratch).
  acc.onSample(1.0, SampleEvent{ 1.0, 0, 0, 0, 0, 0, 310 * 1024, 0, 0, 0 });
  acc.onPhaseReturn(1.0);
  BenchSummary s = acc.build();
  EXPECT_NEAR(s.retention_ratio, 110.0 / 100.0, 0.05);
  EXPECT_EQ(s.verdict, BenchSummary::kRetentionBug);
}

TEST(SummaryAccumulator, max_inter_batch_gap_per_topic_aggregated_globally)
{
  SummaryAccumulator acc(0, 0.15, 0.70);
  acc.onPhaseDispatch(0.0);
  acc.onBatch(0.1, "imu", 1000, 0);
  acc.onBatch(0.5, "imu", 1000, 0);  // 400 ms gap
  acc.onBatch(0.6, "gps", 1000, 0);
  acc.onBatch(2.0, "gps", 1000, 0);  // 1400 ms gap (largest)
  acc.onPhaseReturn(2.0);
  BenchSummary s = acc.build();
  EXPECT_NEAR(s.max_inter_batch_gap_ms, 1400.0, 0.001);
}
