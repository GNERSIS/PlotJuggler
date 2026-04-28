// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstdint>
#include <string_view>

namespace mosaico::bench {

struct ProcStatus {
    uint64_t vm_rss_kb  = 0;
    uint64_t vm_size_kb = 0;
    uint64_t vm_data_kb = 0;  // heap proxy
};

// Parses /proc/self/status content. Missing fields stay 0.
ProcStatus parseProcStatus(std::string_view content);

// Reads /proc/self/status from disk. On failure (e.g. non-Linux), returns zeros.
ProcStatus readProcSelfStatus();

struct ProcStat {
    uint64_t utime_ticks = 0;
    uint64_t stime_ticks = 0;
};
ProcStat parseProcStat(std::string_view content);
ProcStat readProcSelfStat();

// Sample value for clk_tck. Cached on first call.
long sysconfClkTck();

}  // namespace mosaico::bench
