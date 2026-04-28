#include "bench/proc_self.hpp"
#include <gtest/gtest.h>

using mosaico::bench::parseProcStatus;
using mosaico::bench::ProcStatus;

TEST(ProcSelf, parses_vmrss_vmsize_vmdata_kb)
{
  const char* sample = "Name:\ttest\n"
                       "Pid:\t12345\n"
                       "VmPeak:\t  500000 kB\n"
                       "VmSize:\t  450000 kB\n"
                       "VmRSS:\t   123456 kB\n"
                       "VmData:\t   78901 kB\n"
                       "Threads:\t8\n";
  ProcStatus s = parseProcStatus(sample);
  EXPECT_EQ(s.vm_rss_kb, 123456u);
  EXPECT_EQ(s.vm_size_kb, 450000u);
  EXPECT_EQ(s.vm_data_kb, 78901u);
}

TEST(ProcSelf, missing_fields_default_to_zero)
{
  ProcStatus s = parseProcStatus("Name:\tx\nPid:\t1\n");
  EXPECT_EQ(s.vm_rss_kb, 0u);
  EXPECT_EQ(s.vm_size_kb, 0u);
  EXPECT_EQ(s.vm_data_kb, 0u);
}

TEST(ProcSelf, parses_proc_stat_utime_stime)
{
  // (comm) can contain spaces and parentheses inside; the parser must
  // anchor at the trailing ')'.
  const char* sample = "12345 (some weird (comm) name) S 1 12345 12345 0 -1 4194304 "
                       "100 0 0 0 1234 5678 0 0 20 0 8 0 999 12345678 1024 18446744073709551615\n";
  auto s = mosaico::bench::parseProcStat(sample);
  EXPECT_EQ(s.utime_ticks, 1234u);
  EXPECT_EQ(s.stime_ticks, 5678u);
}
