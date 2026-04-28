# mosaico_fetch_bench — usage

Instrumented Mosaico Flight benchmark. Two consumers share the same
instrumentation core (`mosaico_bench_core` static lib):

- **CLI binary** `mosaico_fetch_bench` — for CI / regression runs / scripted
  benchmarks. Writes a detail CSV (per-event) and a summary CSV (one row
  per run) that PlotJuggler's `DataLoadCSV` plugin opens directly.
- **"Run benchmark" button in the ToolboxMosaico plugin** — for live
  visualization while the fetch happens. Series appear in the timeseries
  tree under `mosaico_bench/run<k>/...` so re-running adds new branches
  without clobbering earlier traces.

## Six metric layers (collected in both modes)

1. **Phase timing** — dispatch → first schema → first batch → last batch → return.
2. **Per-batch events** — arrival time, topic, bytes, rows, gap-since-prev-on-same-topic.
3. **Per-topic cumulative** — bytes_cum, batches_cum, instantaneous Mbps.
4. **Kernel TCP socket info** — cwnd, srtt, retrans, rcv_space (sampled every 100 ms via `ss -tinH`).
5. **Process resources** — RSS, VSZ, heap, CPU % (sampled every 100 ms via `/proc/self/{status,stat}`).
6. **Downstream decode cost** — wall-clock spent in batch_cb (the wrapper that the bench passes to `MosaicoClient::pullTopics`).

## Retention-bug detector

The bench's headline number is `retention_ratio = rss_growth / cum_bytes_received`.
Verdict thresholds (configurable; defaults from spec):

- `retention_ratio < 0.15` → **streaming** (exit 0)
- `< 0.70` → **mixed** (exit 1)
- `≥ 0.70` → **retention_bug** (exit 2)

Running the same fetch with `--retain-batches=0` (default, streaming SDK path)
then `--retain-batches=1` (legacy retain-everything path) is the canonical
regression test — the verdicts must differ.

## CLI usage

```
mosaico_fetch_bench --uri grpc+tls://HOST:PORT \
                    --sequence NAME \
                    (--topics A,B,C | --topics-file PATH) \
                    --start-ns NS --end-ns NS \
                    [--repeat N=1] [--use-cache] [--retain-batches=0|1] \
                    [--sample-interval-ms N=100] \
                    [--output-dir DIR=.] [--output-prefix NAME] [--no-csv] \
                    [--retention-warn 0.15] [--retention-fail 0.70] \
                    [-q | -v]
```

Exit codes:

```
0 — all runs streaming
1 — at least one run mixed
2 — at least one run retention_bug
3 — at least one run failed (Flight RPC error)
4 — argument / setup error
```

## Smoke test against `demo.mosaico.dev:6726`

Replace `<SEQ>` with a known sequence and `<TOPICS>` with a comma-separated
list (or use `--topics-file=...`). Replace `<NS_START>`/`<NS_END>` with a
nanosecond range that exists for the sequence:

```bash
BIN=$(find cmake-build-debug -name 'mosaico_fetch_bench' -type f -executable | head -1)

# Streaming run — should produce verdict=streaming
"$BIN" --uri=grpc+tls://demo.mosaico.dev:6726 \
       --sequence=<SEQ> --topics=<TOPICS> \
       --start-ns=<NS_START> --end-ns=<NS_END> \
       --repeat=2 --output-dir=/tmp

# Regression check — must produce verdict=retention_bug
"$BIN" --uri=grpc+tls://demo.mosaico.dev:6726 \
       --sequence=<SEQ> --topics=<TOPICS> \
       --start-ns=<NS_START> --end-ns=<NS_END> \
       --retain-batches=1 --output-dir=/tmp
```

The first command exits 0; the second exits 2. Both write
`bench-detail-<UTC>.csv` and `bench-summary-<UTC>.csv` to `/tmp`.

Open in PlotJuggler:

```bash
File → Open → /tmp/bench-detail-<UTC>.csv
(DataLoadCSV will prompt for the time column → pick `time_s`)
```

Drag `process/rss_kb` and `total/cum_bytes_mb` onto the same plot —
in streaming mode `rss_kb` stays roughly flat while `cum_bytes_mb`
climbs; in retention-bug mode the two climb together.

## Plugin usage

1. Open PlotJuggler.
2. Tools → Mosaico (or however the plugin is launched in your build).
3. Connect to `demo.mosaico.dev:6726`, pick a sequence and topics.
4. Click **Run benchmark** (next to **Fetch**).
5. Accept the popover defaults (repeat=1, no cache, no CSV) or fill them in.
6. New series appear under `mosaico_bench/run0/` in the left tree as data
   streams in. Drag them onto plots.
7. The status label at the end reads:

```
bench run 0: ok in N.Ns — avg X Mbps, peak Y, ratio Z → streaming
```

## Artifacts produced

- `bench-detail-<UTC>.csv` — wide CSV; one row per phase / batch / sample event.
- `bench-summary-<UTC>.csv` — one row per `--repeat` iteration, suitable for
  a regression dashboard.

See `docs/superpowers/specs/2026-04-27-mosaico-fetch-bench-design.md` for
the full schema and design rationale.
