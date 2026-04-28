// SPDX-License-Identifier: MPL-2.0
#include "bench/csv_sinks.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace mosaico::bench;

TEST(Slugify, replaces_non_alnum_with_underscore)
{
  EXPECT_EQ(slugifyTopic("imu_raw"), "imu_raw");
  EXPECT_EQ(slugifyTopic("imu/raw"), "imu_raw");
  EXPECT_EQ(slugifyTopic("/sensors/imu 0"), "_sensors_imu_0");
  EXPECT_EQ(slugifyTopic("topic.with.dots"), "topic_with_dots");
}

TEST(CsvDetailSink, writes_header_then_phase_row_then_batch_row)
{
  auto path = std::filesystem::temp_directory_path() / "bench-detail-test.csv";
  {
    CsvDetailSink sink(path.string(), { "imu_raw", "gps" });
    sink.onPhase(PhaseEvent{ 0.0, "dispatch", "" });
    sink.onBatch(BatchEvent{ 0.1, "imu_raw", 1024, 100, 0 });
  }  // sink destructor flushes
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  auto contents = ss.str();
  // header has the column we care about
  EXPECT_NE(contents.find("time_s,run_id,event_kind"), std::string::npos);
  EXPECT_NE(contents.find("phase/dispatch"), std::string::npos);
  EXPECT_NE(contents.find("topic/imu_raw/batch_bytes"), std::string::npos);
  EXPECT_NE(contents.find("topic/gps/batch_bytes"), std::string::npos);
  // dispatch row → 1 in phase/dispatch column
  EXPECT_NE(contents.find(",phase,"), std::string::npos);
  // batch row → bytes value 1024 appears
  EXPECT_NE(contents.find("1024"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(CsvSummarySink, writes_header_and_one_row_with_verdict_string)
{
  auto path = std::filesystem::temp_directory_path() / "bench-summary-test.csv";
  {
    CsvSummarySink sink(path.string());
    BenchSummary s{};
    s.run_id = 0;
    s.wall_time_s = 12.34;
    s.cum_bytes = 100 * 1024 * 1024;
    s.rss_start_kb = 200 * 1024;
    s.rss_peak_kb = 320 * 1024;
    s.retention_ratio = 1.2;
    s.verdict = BenchSummary::kRetentionBug;
    s.status = arrow::Status::OK();
    sink.onSummary(s);
  }
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  auto contents = ss.str();
  EXPECT_NE(contents.find("retention_ratio,verdict"), std::string::npos);
  EXPECT_NE(contents.find("retention_bug"), std::string::npos);
  EXPECT_NE(contents.find("12.34"), std::string::npos);
  std::filesystem::remove(path);
}
