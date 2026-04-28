// SPDX-License-Identifier: MPL-2.0
#include "bench/ss_poller.hpp"
#include <gtest/gtest.h>

using mosaico::bench::parseSsBlock;
using mosaico::bench::SsSample;

TEST(SsPoller, parses_typical_ss_tinH_block)
{
  // One ESTAB line + one detail line — the format `ss -tinH` produces.
  const char* sample = "ESTAB 0 0 192.168.1.6:44321 46.224.172.254:6726\n"
                       "\t cubic wscale:7,7 rto:312 rtt:107.832/2.234 ato:40 mss:1452 "
                       "pmtu:1500 cwnd:1093 bytes_acked:486234567 retrans:0/4 "
                       "rcv_space:14600 minrtt:62.354 send 117500291bps\n";
  auto s = parseSsBlock(sample);
  ASSERT_TRUE(s.has_value());
  EXPECT_EQ(s->cwnd_segs, 1093u);
  EXPECT_NEAR(s->srtt_ms, 107.832, 0.001);
  EXPECT_EQ(s->retrans_total, 4u);  // total retrans, not the unacked count
  EXPECT_EQ(s->bytes_acked, 486234567u);
  EXPECT_EQ(s->rcv_space_kb, 14u);  // 14600 bytes / 1024 rounded down
}

TEST(SsPoller, no_socket_returns_nullopt)
{
  auto s = parseSsBlock("");
  EXPECT_FALSE(s.has_value());
}

TEST(SsPoller, retrans_field_missing_treated_as_zero)
{
  const char* sample = "ESTAB 0 0 192.168.1.6:44321 46.224.172.254:6726\n"
                       "\t cubic wscale:7,7 rtt:62.5/1.0 cwnd:10 bytes_acked:1024 send 1000bps\n";
  auto s = parseSsBlock(sample);
  ASSERT_TRUE(s.has_value());
  EXPECT_EQ(s->retrans_total, 0u);
  EXPECT_EQ(s->cwnd_segs, 10u);
}
