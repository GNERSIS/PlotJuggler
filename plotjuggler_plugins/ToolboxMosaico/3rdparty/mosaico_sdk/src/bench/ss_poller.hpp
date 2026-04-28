// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace mosaico::bench {

struct SsSample {
    uint32_t cwnd_segs     = 0;
    double   srtt_ms       = 0.0;
    uint64_t retrans_total = 0;
    uint64_t bytes_acked   = 0;
    uint32_t rcv_space_kb  = 0;
};

// Parse a single `ss -tinH dst H:P` block. Returns nullopt when the block
// has no ESTAB line (no socket open yet, or just torn down).
std::optional<SsSample> parseSsBlock(std::string_view block);

// Polls `ss -tinH dst HOST:PORT` every interval_ms and invokes on_sample.
// Owns a background thread; stop() joins it.
class SsPoller {
public:
    using Callback = std::function<void(const SsSample&)>;

    SsPoller(std::string host, uint16_t port, uint32_t interval_ms, Callback cb);
    ~SsPoller();
    SsPoller(const SsPoller&) = delete;
    SsPoller& operator=(const SsPoller&) = delete;

    void start();
    void stop();   // safe to call multiple times

private:
    void loop();
    std::string host_;
    uint16_t    port_;
    uint32_t    interval_ms_;
    Callback    cb_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace mosaico::bench
