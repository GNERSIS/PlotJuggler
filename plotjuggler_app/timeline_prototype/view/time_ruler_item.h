/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_TIME_RULER_ITEM_H
#define PJ_TIMELINE_PROTOTYPE_TIME_RULER_ITEM_H

#include <QGraphicsItem>
#include <QtGlobal>

namespace PJ::TimelinePrototype
{

class TimeRulerItem : public QGraphicsItem
{
public:
  static constexpr qreal kRulerHeight = 28.0;

  TimeRulerItem();

  // The displayed time range in nanoseconds (drives the labels).
  // Width in scene pixels is set separately via setPixelWidth().
  void setTimeRange(qint64 start_ns, qint64 end_ns);
  void setPixelWidth(qreal width_px);
  void setEpochOffsetNs(qint64 epoch_ns);  // ns subtracted from displayed times
                                           // so labels start at 00:00 instead
                                           // of huge wall-clock numbers
  qint64 currentTickIntervalNs() const
  {
    return tick_interval_ns_;
  }

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
  void recomputeTickInterval();
  static QString formatLabel(qint64 ns_relative);

  qint64 start_ns_ = 0;
  qint64 end_ns_ = 1;
  qreal width_px_ = 800.0;
  qint64 epoch_ns_ = 0;
  qint64 tick_interval_ns_ = 1'000'000'000LL;  // 1 sec default
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_TIME_RULER_ITEM_H
