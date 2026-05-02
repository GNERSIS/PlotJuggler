/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "model/timeline_model.h"

#include <gtest/gtest.h>

using namespace PJ::TimelinePrototype;

namespace
{

Sequence makeSeq(const QString& name, qint64 min_ns, qint64 max_ns, std::vector<Topic> topics = {})
{
  Sequence s;
  s.name = name;
  s.min_ts_ns = min_ns;
  s.max_ts_ns = max_ns;
  s.topics = std::move(topics);
  return s;
}

Topic makeTopic(const QString& name, qint64 first, qint64 last)
{
  Topic t;
  t.name = name;
  t.first_sample_ns = first;
  t.last_sample_ns = last;
  return t;
}

}  // namespace

TEST(TimelineModel, DisplayWindowAddsSequenceOffset)
{
  TimelineModel m;
  Sequence s = makeSeq("a", 100, 200);
  s.display_offset_ns = 50;
  m.setSequences({ s });
  auto [start, end] = m.displayWindow(0);
  EXPECT_EQ(start, 150);
  EXPECT_EQ(end, 250);
}

TEST(TimelineModel, TopicDisplayWindowAddsBothOffsets)
{
  TimelineModel m;
  Sequence s = makeSeq("a", 100, 200, { makeTopic("/t", 110, 190) });
  s.display_offset_ns = 50;
  s.topics[0].per_topic_offset_ns = 5;
  m.setSequences({ s });
  auto [start, end] = m.topicDisplayWindow(0, 0);
  EXPECT_EQ(start, 110 + 50 + 5);
  EXPECT_EQ(end, 190 + 50 + 5);
}

TEST(TimelineModel, SceneExtentSpansAllSequencesAfterOffsets)
{
  TimelineModel m;
  Sequence s1 = makeSeq("a", 0, 100);
  Sequence s2 = makeSeq("b", 0, 50);
  s2.display_offset_ns = 200;  // shifted right of s1
  m.setSequences({ s1, s2 });
  auto [start, end] = m.sceneExtent();
  EXPECT_EQ(start, 0);
  EXPECT_EQ(end, 250);
}

TEST(TimelineModel, SceneExtentEmptyReturns60sDefault)
{
  // The empty default needs to be wide enough that a freshly-launched
  // timeline (no series dropped yet) renders a visible ruler, playhead, and
  // work range. 60s was chosen so the ruler can lay out at least a few ticks
  // at the default zoom; previously the {0, 1 ns} default left the ruler
  // and playhead invisible.
  TimelineModel m;
  m.setSequences({});
  auto [start, end] = m.sceneExtent();
  EXPECT_EQ(start, 0);
  EXPECT_EQ(end, 60'000'000'000LL);
}

TEST(TimelineModel, AlignStartShiftsNonLeadingByMinDelta)
{
  TimelineModel m;
  m.setSequences({ makeSeq("L", 1000, 2000), makeSeq("S", 5000, 5500) });
  m.setLeading(0);
  m.setAlignment(AlignmentMode::Start);
  // S.min should land at L.min: δ = 1000 - 5000 = -4000
  EXPECT_EQ(m.sequences()[1].display_offset_ns, -4000);
  // L untouched
  EXPECT_EQ(m.sequences()[0].display_offset_ns, 0);
}

TEST(TimelineModel, AlignFinishShiftsNonLeadingByMaxDelta)
{
  TimelineModel m;
  m.setSequences({ makeSeq("L", 1000, 2000), makeSeq("S", 5000, 5500) });
  m.setLeading(0);
  m.setAlignment(AlignmentMode::Finish);
  // S.max should land at L.max: δ = 2000 - 5500 = -3500
  EXPECT_EQ(m.sequences()[1].display_offset_ns, -3500);
}

TEST(TimelineModel, AlignMiddleShiftsNonLeadingByCenterDelta)
{
  TimelineModel m;
  m.setSequences({ makeSeq("L", 1000, 2000), makeSeq("S", 5000, 5500) });
  m.setLeading(0);
  m.setAlignment(AlignmentMode::Middle);
  // L center = 1500, S center = 5250, δ = 1500 - 5250 = -3750
  EXPECT_EQ(m.sequences()[1].display_offset_ns, -3750);
}

TEST(TimelineModel, OverriddenSequenceIgnoredByAlignment)
{
  TimelineModel m;
  m.setSequences({ makeSeq("L", 0, 1000), makeSeq("A", 0, 1000), makeSeq("B", 0, 1000) });
  m.setSequenceOffset(2, 9999);  // B is now overridden
  m.setLeading(0);
  m.setAlignment(AlignmentMode::Start);
  EXPECT_EQ(m.sequences()[1].display_offset_ns, 0);     // A re-aligned
  EXPECT_EQ(m.sequences()[2].display_offset_ns, 9999);  // B untouched
  EXPECT_TRUE(m.sequences()[2].seq_offset_overridden);
}

TEST(TimelineModel, ChangingLeadingPreservesOverrides)
{
  TimelineModel m;
  m.setSequences({ makeSeq("L1", 0, 100), makeSeq("L2", 0, 100), makeSeq("S", 0, 100) });
  m.setSequenceOffset(2, 5000);  // S overridden
  m.setLeading(0);
  m.setAlignment(AlignmentMode::Start);
  m.setLeading(1);                                      // change leading
  EXPECT_EQ(m.sequences()[2].display_offset_ns, 5000);  // still untouched
}

TEST(TimelineModel, ChangingLeadingAlignsToNewLeadDisplayedPosition)
{
  // Regression: when a new leading sequence is picked, others must align to
  // its *displayed* edges, not its raw min/max. Otherwise the new leader keeps
  // the offset it had as a follower and everyone snaps to where it would be at
  // zero offset — visually, the previous leader stays put and the picked one
  // looks like it's not actually leading.
  TimelineModel m;
  m.setSequences({ makeSeq("A", 0, 100), makeSeq("B", 200, 300) });
  m.setLeading(0);
  m.setAlignment(AlignmentMode::Start);
  // After this: A.offset=0 (still at [0,100]), B.offset=-200 (now at [0,100]).
  EXPECT_EQ(m.sequences()[0].display_offset_ns, 0);
  EXPECT_EQ(m.sequences()[1].display_offset_ns, -200);

  m.setLeading(1);
  // B must stay where it was drawn (display window starting at 0). A must
  // re-align to that display position, i.e., also start at 0 → offset 0.
  EXPECT_EQ(m.sequences()[1].display_offset_ns, -200);
  EXPECT_EQ(m.sequences()[0].display_offset_ns, 0);
}

TEST(TimelineModel, ResetAlignmentClearsOverridesAndReapplies)
{
  TimelineModel m;
  m.setSequences({ makeSeq("L", 1000, 2000), makeSeq("S", 5000, 5500) });
  m.setSequenceOffset(1, 9999);  // override
  m.setLeading(0);
  m.setAlignment(AlignmentMode::Start);
  // Override stuck:
  EXPECT_EQ(m.sequences()[1].display_offset_ns, 9999);
  m.resetAlignment();
  // After reset: override cleared, alignment re-applied → δ = -4000
  EXPECT_FALSE(m.sequences()[1].seq_offset_overridden);
  EXPECT_EQ(m.sequences()[1].display_offset_ns, -4000);
}

TEST(TimelineModel, SetTopicOffsetMarksBothOverridden)
{
  TimelineModel m;
  m.setSequences({ makeSeq("S", 0, 1000, { makeTopic("/t", 100, 900) }) });
  m.setTopicOffset(0, 0, 50);
  EXPECT_TRUE(m.sequences()[0].topics[0].topic_offset_overridden);
  EXPECT_TRUE(m.sequences()[0].seq_offset_overridden);
  EXPECT_EQ(m.sequences()[0].topics[0].per_topic_offset_ns, 50);
}

TEST(TimelineModel, SnapToGridRoundsToNearest)
{
  EXPECT_EQ(TimelineModel::snapToGrid(123, 100), 100);
  EXPECT_EQ(TimelineModel::snapToGrid(150, 100), 200);  // ties round away from 0
  EXPECT_EQ(TimelineModel::snapToGrid(-150, 100), -200);
  EXPECT_EQ(TimelineModel::snapToGrid(0, 100), 0);
  EXPECT_EQ(TimelineModel::snapToGrid(42, 0), 42);  // 0 interval = identity
}
