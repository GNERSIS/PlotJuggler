/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_SEQUENCE_H
#define PJ_TIMELINE_PROTOTYPE_SEQUENCE_H

#include <QColor>
#include <QString>
#include <cstdint>
#include <vector>

namespace PJ::TimelinePrototype
{

struct Topic
{
  QString name;
  qint64 first_sample_ns = 0;
  qint64 last_sample_ns = 0;
  qint64 per_topic_offset_ns = 0;
  bool topic_offset_overridden = false;
};

struct Sequence
{
  QString name;
  qint64 min_ts_ns = 0;
  qint64 max_ts_ns = 0;
  qint64 display_offset_ns = 0;
  bool seq_offset_overridden = false;
  QColor color;
  std::vector<Topic> topics;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_SEQUENCE_H
