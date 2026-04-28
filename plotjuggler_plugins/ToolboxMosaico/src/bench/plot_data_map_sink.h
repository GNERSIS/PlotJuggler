// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "bench/bench_core.hpp"

#include <QObject>
#include <QString>

class PlotDataMapSink : public QObject, public mosaico::bench::MetricsSink
{
  Q_OBJECT

public:
  explicit PlotDataMapSink(QObject* parent = nullptr);

  void onPhase(const mosaico::bench::PhaseEvent&) override;
  void onBatch(const mosaico::bench::BatchEvent&) override;
  void onSample(const mosaico::bench::SampleEvent&) override;
  void onSummary(const mosaico::bench::BenchSummary&) override;

signals:
  // Marshalled to MainWindow via Qt::QueuedConnection. Plain-data payloads
  // cross threads safely; MainWindow performs PlotDataMapRef updates in the
  // GUI thread.
  void phaseFired(int run_id, double t_s, QString phase, QString topic);
  void batchObserved(int run_id, double t_s, QString topic, qint64 bytes, qint64 rows,
                     double gap_ms);
  void sampleObserved(int run_id, double t_s, quint32 cwnd_segs, double srtt_ms,
                      quint64 retrans_total, quint64 bytes_acked, quint32 rcv_space_kb,
                      quint64 rss_kb, quint64 vsz_kb, quint64 heap_kb, double cpu_pct);
  void summaryReady(int run_id, double wall_time_s, qint64 cum_bytes, double avg_mbps,
                    double peak_mbps, double max_inter_batch_gap_ms, quint64 rss_start_kb,
                    quint64 rss_peak_kb, quint64 rss_end_kb, double retention_ratio, int verdict,
                    QString error);

public:
  void setRunId(int run_id)
  {
    run_id_ = run_id;
  }

private:
  int run_id_ = 0;
};
