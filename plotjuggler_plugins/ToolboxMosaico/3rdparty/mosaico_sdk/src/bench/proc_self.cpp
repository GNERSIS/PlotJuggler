// SPDX-License-Identifier: MPL-2.0
#include "bench/proc_self.hpp"

#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace mosaico::bench {

namespace {
uint64_t parseKbAfterColon(std::string_view line)
{
    auto colon = line.find(':');
    if (colon == std::string_view::npos) return 0;
    auto rest = line.substr(colon + 1);
    while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.remove_prefix(1);
    uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
    (void)ptr;
    if (ec != std::errc()) return 0;
    return value;  // already in kB per the kernel's status format
}
}  // namespace

ProcStatus parseProcStatus(std::string_view content)
{
    ProcStatus out;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        std::string_view line = content.substr(pos, (nl == std::string_view::npos ? content.size() : nl) - pos);
        if      (line.rfind("VmRSS:",  0) == 0) out.vm_rss_kb  = parseKbAfterColon(line);
        else if (line.rfind("VmSize:", 0) == 0) out.vm_size_kb = parseKbAfterColon(line);
        else if (line.rfind("VmData:", 0) == 0) out.vm_data_kb = parseKbAfterColon(line);
        if (nl == std::string_view::npos) break;
        pos = nl + 1;
    }
    return out;
}

ProcStatus readProcSelfStatus()
{
    std::ifstream f("/proc/self/status");
    if (!f) return {};
    std::ostringstream ss; ss << f.rdbuf();
    return parseProcStatus(ss.str());
}

// /proc/self/stat fields are space-separated; we want utime (14) and stime (15)
// after the (comm) field which can itself contain spaces. Locate the trailing
// ')' and tokenize from there.
ProcStat parseProcStat(std::string_view content)
{
    ProcStat out;
    auto rparen = content.rfind(')');
    if (rparen == std::string_view::npos) return out;
    auto rest = content.substr(rparen + 1);
    std::string s(rest);
    std::istringstream iss(s);
    std::string tok;
    int field = 2;  // we are now at field 3 (state)
    while (iss >> tok) {
        ++field;
        if (field == 14) out.utime_ticks = std::stoull(tok);
        else if (field == 15) { out.stime_ticks = std::stoull(tok); break; }
    }
    return out;
}

ProcStat readProcSelfStat()
{
    std::ifstream f("/proc/self/stat");
    if (!f) return {};
    std::ostringstream ss; ss << f.rdbuf();
    return parseProcStat(ss.str());
}

long sysconfClkTck()
{
    static const long ticks = ::sysconf(_SC_CLK_TCK);
    return ticks > 0 ? ticks : 100;
}

}  // namespace mosaico::bench
