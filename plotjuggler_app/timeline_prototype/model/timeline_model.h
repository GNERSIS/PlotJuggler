/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_TIMELINE_MODEL_H
#define PJ_TIMELINE_PROTOTYPE_TIMELINE_MODEL_H

#include "model/sequence.h"

#include <QObject>
#include <set>
#include <utility>
#include <vector>

namespace PJ::TimelinePrototype
{

enum class AlignmentMode
{
  None,
  Start,
  Finish,
  Middle,
};

class TimelineModel : public QObject
{
  Q_OBJECT
public:
  explicit TimelineModel(QObject* parent = nullptr);

  // ---- mutators (each emits the matching signal) -------------------------
  void setSequences(std::vector<Sequence> seqs);
  void addSequence(Sequence seq);                  // appends; emits sequencesChanged
  void setLeading(int idx);                        // -1 = no leading
  void setAlignment(AlignmentMode mode);           // re-applies to non-overridden
  void setSequenceOffset(int seq_idx, qint64 ns);  // marks sequence overridden
  void setTopicOffset(int seq_idx, int topic_idx,
                      qint64 ns);  // marks topic + sequence overridden
  void setSelection(std::set<int> selected);
  void setPlayhead(qint64 ns);
  void setWorkRange(qint64 start_ns, qint64 end_ns);
  void resetAlignment();  // clears overrides + re-applies

  // ---- accessors ---------------------------------------------------------
  const std::vector<Sequence>& sequences() const
  {
    return sequences_;
  }
  int leading() const
  {
    return leading_idx_;
  }
  AlignmentMode alignment() const
  {
    return alignment_;
  }
  const std::set<int>& selection() const
  {
    return selection_;
  }
  qint64 playhead() const
  {
    return playhead_ns_;
  }
  std::pair<qint64, qint64> workRange() const
  {
    return { work_start_ns_, work_end_ns_ };
  }

  // ---- pure derived helpers ---------------------------------------------
  std::pair<qint64, qint64> displayWindow(int seq_idx) const;
  std::pair<qint64, qint64> topicDisplayWindow(int seq_idx, int topic_idx) const;
  std::pair<qint64, qint64> sceneExtent() const;

  // Round `ns` to the nearest multiple of `interval_ns`. Static so tests
  // can call without instantiating.
  static qint64 snapToGrid(qint64 ns, qint64 interval_ns);

signals:
  void sequencesChanged();
  void offsetsChanged(int seq_idx);  // -1 = all
  void leadingChanged(int idx);
  void alignmentChanged(AlignmentMode mode);
  void selectionChanged(const std::set<int>& selected);
  void playheadChanged(qint64 ns);
  void workRangeChanged(qint64 start_ns, qint64 end_ns);

private:
  void applyAlignmentToNonOverridden();  // helper for setLeading/setAlignment

  std::vector<Sequence> sequences_;
  int leading_idx_ = -1;
  AlignmentMode alignment_ = AlignmentMode::None;
  std::set<int> selection_;
  qint64 playhead_ns_ = 0;
  qint64 work_start_ns_ = 0;
  qint64 work_end_ns_ = 60'000'000'000LL;  // 60s — see sceneExtent()
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_TIMELINE_MODEL_H
