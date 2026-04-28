// SPDX-License-Identifier: MPL-2.0
#include "bench/ss_poller.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <regex>
#include <string>
#include <utility>

namespace mosaico::bench {

std::optional<SsSample> parseSsBlock(std::string_view block)
{
    if (block.find("ESTAB") == std::string_view::npos) return std::nullopt;

    SsSample s{};
    std::string str(block);

    static const std::regex re_cwnd(R"(cwnd:(\d+))");
    static const std::regex re_rtt (R"(rtt:([0-9.]+))");
    static const std::regex re_ack (R"(bytes_acked:(\d+))");
    static const std::regex re_rcv (R"(rcv_space:(\d+))");
    // ss prints retrans:UNACKED/TOTAL — we want TOTAL.
    static const std::regex re_retr(R"(retrans:\d+/(\d+))");

    std::smatch m;
    if (std::regex_search(str, m, re_cwnd)) s.cwnd_segs     = static_cast<uint32_t>(std::stoul(m[1]));
    if (std::regex_search(str, m, re_rtt))  s.srtt_ms       = std::stod(m[1]);
    if (std::regex_search(str, m, re_ack))  s.bytes_acked   = std::stoull(m[1]);
    if (std::regex_search(str, m, re_rcv))  s.rcv_space_kb  = static_cast<uint32_t>(std::stoul(m[1]) / 1024);
    if (std::regex_search(str, m, re_retr)) s.retrans_total = std::stoull(m[1]);
    return s;
}

SsPoller::SsPoller(std::string host, uint16_t port, uint32_t interval_ms, Callback cb)
    : host_(std::move(host)), port_(port), interval_ms_(interval_ms), cb_(std::move(cb)) {}

SsPoller::~SsPoller() { stop(); }

void SsPoller::start()
{
    running_ = true;
    thread_ = std::thread([this]{ loop(); });
}

void SsPoller::stop()
{
    bool was = running_.exchange(false);
    if (was && thread_.joinable()) thread_.join();
}

void SsPoller::loop()
{
    char cmd[256];
    std::snprintf(cmd, sizeof(cmd), "ss -tinH dst %s:%u 2>/dev/null", host_.c_str(), port_);
    while (running_.load(std::memory_order_relaxed)) {
        FILE* p = ::popen(cmd, "r");
        std::string buf;
        if (p) {
            std::array<char, 4096> chunk{};
            while (auto n = std::fread(chunk.data(), 1, chunk.size(), p)) buf.append(chunk.data(), n);
            ::pclose(p);
        }
        if (auto s = parseSsBlock(buf); s) cb_(*s);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }
}

}  // namespace mosaico::bench
