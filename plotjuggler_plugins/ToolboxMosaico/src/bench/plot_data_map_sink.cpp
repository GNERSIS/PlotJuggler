// SPDX-License-Identifier: MPL-2.0
#include "bench/plot_data_map_sink.h"

PlotDataMapSink::PlotDataMapSink(QObject* parent) : QObject(parent)
{
}

void PlotDataMapSink::onPhase(const mosaico::bench::PhaseEvent& e)
{
  emit phaseFired(run_id_, e.t_s, QString::fromStdString(e.phase), QString::fromStdString(e.topic));
}
void PlotDataMapSink::onBatch(const mosaico::bench::BatchEvent& e)
{
  emit batchObserved(run_id_, e.t_s, QString::fromStdString(e.topic), static_cast<qint64>(e.bytes),
                     static_cast<qint64>(e.rows), e.gap_ms_same_topic);
}
void PlotDataMapSink::onSample(const mosaico::bench::SampleEvent& e)
{
  emit sampleObserved(run_id_, e.t_s, e.cwnd_segs, e.srtt_ms, static_cast<quint64>(e.retrans_total),
                      static_cast<quint64>(e.bytes_acked), e.rcv_space_kb,
                      static_cast<quint64>(e.rss_kb), static_cast<quint64>(e.vsz_kb),
                      static_cast<quint64>(e.heap_kb), e.cpu_pct);
}
void PlotDataMapSink::onSummary(const mosaico::bench::BenchSummary& s)
{
  emit summaryReady(s.run_id, s.wall_time_s, static_cast<qint64>(s.cum_bytes), s.avg_mbps,
                    s.peak_mbps, s.max_inter_batch_gap_ms, static_cast<quint64>(s.rss_start_kb),
                    static_cast<quint64>(s.rss_peak_kb), static_cast<quint64>(s.rss_end_kb),
                    s.retention_ratio, static_cast<int>(s.verdict),
                    QString::fromStdString(s.error));
}
