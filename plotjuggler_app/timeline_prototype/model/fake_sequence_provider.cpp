/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "model/fake_sequence_provider.h"

#include <algorithm>
#include <random>

namespace PJ::TimelinePrototype
{

namespace
{

constexpr qint64 kNs = 1;
constexpr qint64 kMs = 1'000'000LL * kNs;
constexpr qint64 kSec = 1'000LL * kMs;
constexpr qint64 kMin = 60LL * kSec;
constexpr qint64 kHour = 60LL * kMin;
constexpr qint64 kDay = 24LL * kHour;

// 8-entry palette, Tableau-style hues.
const std::vector<QColor> kPalette = {
  QColor(31, 119, 180),   // blue
  QColor(255, 127, 14),   // orange
  QColor(44, 160, 44),    // green
  QColor(214, 39, 40),    // red
  QColor(148, 103, 189),  // purple
  QColor(140, 86, 75),    // brown
  QColor(227, 119, 194),  // pink
  QColor(127, 127, 127),  // grey
};

Sequence makeSequence(const QString& name, qint64 abs_start_ns, qint64 duration_ns, int color_idx,
                      const std::vector<QString>& topic_names, std::mt19937& rng)
{
  Sequence s;
  s.name = name;
  s.min_ts_ns = abs_start_ns;
  s.max_ts_ns = abs_start_ns + duration_ns;
  s.color = kPalette[color_idx % kPalette.size()];

  // Each topic occupies a random sub-interval of the sequence window
  // so rectangles do not all start/end at the same edges.
  std::uniform_int_distribution<qint64> start_dist(0, duration_ns / 4);
  std::uniform_int_distribution<qint64> len_dist(duration_ns / 2, duration_ns);

  for (const QString& topic_name : topic_names)
  {
    Topic t;
    t.name = topic_name;
    qint64 start_off = start_dist(rng);
    qint64 length = std::min(len_dist(rng), duration_ns - start_off);
    t.first_sample_ns = abs_start_ns + start_off;
    t.last_sample_ns = abs_start_ns + start_off + length;
    s.topics.push_back(t);
  }
  return s;
}

}  // namespace

std::vector<Sequence> FakeSequenceProvider::generate()
{
  std::mt19937 rng(42);  // deterministic

  // Reference epoch: 2026-04-28 06:00:00 UTC, in ns.
  // (Value is arbitrary — the prototype only cares about relative offsets.)
  constexpr qint64 kRefEpochNs = 1'777'356'000'000'000'000LL;

  std::vector<Sequence> out;

  // 1) flight_2026_04_28_morning — baseline, t=0..60s
  out.push_back(makeSequence(
      "flight_2026_04_28_morning", kRefEpochNs, 60 * kSec, 0,
      { "/imu/accel", "/imu/gyro", "/gps/fix", "/cam/front", "/odom", "/battery" }, rng));

  // 2) flight_2026_04_28_evening — same length-ish, +6h absolute
  out.push_back(makeSequence("flight_2026_04_28_evening", kRefEpochNs + 6 * kHour, 45 * kSec, 1,
                             { "/imu/accel", "/imu/gyro", "/gps/fix", "/cam/front", "/odom" },
                             rng));

  // 3) bench_test_short — much shorter, +1d absolute
  out.push_back(makeSequence("bench_test_short", kRefEpochNs + 1 * kDay, 12 * kSec, 2,
                             { "/torque", "/voltage", "/current" }, rng));

  // 4) bench_test_long — much longer, -2h absolute
  out.push_back(makeSequence("bench_test_long", kRefEpochNs - 2 * kHour, 180 * kSec, 3,
                             { "/imu/accel", "/imu/gyro", "/temperature", "/pressure", "/altitude",
                               "/heading", "/speed", "/control" },
                             rng));

  return out;
}

}  // namespace PJ::TimelinePrototype
