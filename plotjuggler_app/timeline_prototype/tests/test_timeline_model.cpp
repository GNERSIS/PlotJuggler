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

TEST(TimelineModel, SceneExtentEmptyReturnsZeroOne)
{
  TimelineModel m;
  m.setSequences({});
  auto [start, end] = m.sceneExtent();
  EXPECT_EQ(start, 0);
  EXPECT_EQ(end, 1);
}
