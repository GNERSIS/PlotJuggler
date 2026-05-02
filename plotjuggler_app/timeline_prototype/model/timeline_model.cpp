/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "model/timeline_model.h"

#include <QtGlobal>
#include <algorithm>
#include <limits>

namespace PJ::TimelinePrototype
{

TimelineModel::TimelineModel(QObject* parent) : QObject(parent)
{
}

void TimelineModel::setSequences(std::vector<Sequence> seqs)
{
  sequences_ = std::move(seqs);
  if (leading_idx_ >= static_cast<int>(sequences_.size()))
  {
    leading_idx_ = -1;
  }
  emit sequencesChanged();
}

void TimelineModel::addSequence(Sequence seq)
{
  sequences_.push_back(std::move(seq));
  emit sequencesChanged();
}

void TimelineModel::setLeading(int idx)
{
  if (idx < -1 || idx >= static_cast<int>(sequences_.size()))
  {
    Q_ASSERT_X(false, "setLeading", "leading index out of range");
    idx = -1;
  }
  if (idx == leading_idx_)
  {
    return;
  }
  leading_idx_ = idx;
  emit leadingChanged(idx);
  applyAlignmentToNonOverridden();
}

void TimelineModel::setAlignment(AlignmentMode mode)
{
  if (mode == alignment_)
  {
    return;
  }
  alignment_ = mode;
  emit alignmentChanged(mode);
  applyAlignmentToNonOverridden();
}

void TimelineModel::setSequenceOffset(int seq_idx, qint64 ns)
{
  Q_ASSERT(seq_idx >= 0 && seq_idx < static_cast<int>(sequences_.size()));
  Sequence& s = sequences_[seq_idx];
  s.display_offset_ns = ns;
  s.seq_offset_overridden = true;
  emit offsetsChanged(seq_idx);
}

void TimelineModel::setTopicOffset(int seq_idx, int topic_idx, qint64 ns)
{
  Q_ASSERT(seq_idx >= 0 && seq_idx < static_cast<int>(sequences_.size()));
  Sequence& s = sequences_[seq_idx];
  Q_ASSERT(topic_idx >= 0 && topic_idx < static_cast<int>(s.topics.size()));
  s.topics[topic_idx].per_topic_offset_ns = ns;
  s.topics[topic_idx].topic_offset_overridden = true;
  s.seq_offset_overridden = true;  // asterisks parent sequence too (spec §3)
  emit offsetsChanged(seq_idx);
}

void TimelineModel::setSelection(std::set<int> selected)
{
  if (selected == selection_)
  {
    return;
  }
  selection_ = std::move(selected);
  emit selectionChanged(selection_);
}

void TimelineModel::setPlayhead(qint64 ns)
{
  if (ns == playhead_ns_)
  {
    return;
  }
  playhead_ns_ = ns;
  emit playheadChanged(ns);
}

void TimelineModel::setWorkRange(qint64 start_ns, qint64 end_ns)
{
  if (start_ns > end_ns)
  {
    std::swap(start_ns, end_ns);
  }
  if (start_ns == work_start_ns_ && end_ns == work_end_ns_)
  {
    return;
  }
  work_start_ns_ = start_ns;
  work_end_ns_ = end_ns;
  emit workRangeChanged(start_ns, end_ns);
}

void TimelineModel::resetAlignment()
{
  for (Sequence& s : sequences_)
  {
    s.display_offset_ns = 0;
    s.seq_offset_overridden = false;
    for (Topic& t : s.topics)
    {
      t.per_topic_offset_ns = 0;
      t.topic_offset_overridden = false;
    }
  }
  // applyAlignmentToNonOverridden emits offsetsChanged(-1) when alignment
  // is active; only emit ourselves when it would not have, to avoid a
  // double-rebuild downstream.
  const bool helper_will_emit = leading_idx_ >= 0 && alignment_ != AlignmentMode::None;
  applyAlignmentToNonOverridden();
  if (!helper_will_emit)
  {
    emit offsetsChanged(-1);
  }
}

std::pair<qint64, qint64> TimelineModel::displayWindow(int seq_idx) const
{
  Q_ASSERT(seq_idx >= 0 && seq_idx < static_cast<int>(sequences_.size()));
  const Sequence& s = sequences_[seq_idx];
  return { s.min_ts_ns + s.display_offset_ns, s.max_ts_ns + s.display_offset_ns };
}

std::pair<qint64, qint64> TimelineModel::topicDisplayWindow(int seq_idx, int topic_idx) const
{
  Q_ASSERT(seq_idx >= 0 && seq_idx < static_cast<int>(sequences_.size()));
  const Sequence& s = sequences_[seq_idx];
  Q_ASSERT(topic_idx >= 0 && topic_idx < static_cast<int>(s.topics.size()));
  const Topic& t = s.topics[topic_idx];
  qint64 base = s.display_offset_ns + t.per_topic_offset_ns;
  return { t.first_sample_ns + base, t.last_sample_ns + base };
}

std::pair<qint64, qint64> TimelineModel::sceneExtent() const
{
  if (sequences_.empty())
  {
    // 60-second default so an empty timeline still has a usable ruler,
    // playhead, and work range. The previous {0,1} ns default left the
    // ruler with no room for a single tick step (tick candidates start at
    // 1 ms) and the playhead as a single-pixel sliver.
    return { 0, 60'000'000'000LL };
  }
  qint64 lo = std::numeric_limits<qint64>::max();
  qint64 hi = std::numeric_limits<qint64>::min();
  for (size_t i = 0; i < sequences_.size(); ++i)
  {
    auto [s, e] = displayWindow(static_cast<int>(i));
    lo = std::min(lo, s);
    hi = std::max(hi, e);
    // Per-topic offsets (ε_topic, set via Ctrl+drag) can push individual
    // topics outside the parent sequence window; widen the extent so they
    // remain reachable in the scene's scrollable area.
    const Sequence& seq = sequences_[i];
    for (size_t j = 0; j < seq.topics.size(); ++j)
    {
      auto [ts, te] = topicDisplayWindow(static_cast<int>(i), static_cast<int>(j));
      lo = std::min(lo, ts);
      hi = std::max(hi, te);
    }
  }
  return { lo, hi };
}

qint64 TimelineModel::snapToGrid(qint64 ns, qint64 interval_ns)
{
  if (interval_ns <= 0)
  {
    return ns;
  }
  // round to nearest, ties round away from zero
  qint64 half = interval_ns / 2;
  if (ns >= 0)
  {
    return ((ns + half) / interval_ns) * interval_ns;
  }
  else
  {
    return -(((-ns + half) / interval_ns) * interval_ns);
  }
}

void TimelineModel::applyAlignmentToNonOverridden()
{
  if (leading_idx_ < 0 || alignment_ == AlignmentMode::None)
  {
    return;
  }
  if (leading_idx_ >= static_cast<int>(sequences_.size()))
  {
    return;
  }
  const Sequence& L = sequences_[leading_idx_];
  // Anchor to the leading sequence's *displayed* edges (raw timestamps shifted
  // by its current display offset). Using raw L.min/max here would silently
  // ignore any previous offset L carried — e.g. an offset L picked up while it
  // was a follower of a prior leader — so others would snap to where L *would*
  // sit at zero offset rather than where it is actually drawn. The visible
  // symptom is "everyone aligns around the previous leader instead of the
  // newly-chosen one."
  const qint64 L_min = L.min_ts_ns + L.display_offset_ns;
  const qint64 L_max = L.max_ts_ns + L.display_offset_ns;
  for (size_t i = 0; i < sequences_.size(); ++i)
  {
    if (static_cast<int>(i) == leading_idx_)
    {
      continue;
    }
    Sequence& S = sequences_[i];
    if (S.seq_offset_overridden)
    {
      continue;
    }
    switch (alignment_)
    {
      case AlignmentMode::Start:
        S.display_offset_ns = L_min - S.min_ts_ns;
        break;
      case AlignmentMode::Finish:
        S.display_offset_ns = L_max - S.max_ts_ns;
        break;
      case AlignmentMode::Middle: {
        qint64 lc = (L_min + L_max) / 2;
        qint64 sc = (S.min_ts_ns + S.max_ts_ns) / 2;
        S.display_offset_ns = lc - sc;
        break;
      }
      case AlignmentMode::None:
        break;
    }
  }
  emit offsetsChanged(-1);
}

}  // namespace PJ::TimelinePrototype
