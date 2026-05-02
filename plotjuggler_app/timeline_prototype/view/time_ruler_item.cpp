/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/time_ruler_item.h"

#include <QFontMetricsF>
#include <QPainter>
#include <array>
#include <cmath>

namespace PJ::TimelinePrototype
{

namespace
{
constexpr qint64 kSec = 1'000'000'000LL;
constexpr qint64 kMin = 60 * kSec;
constexpr qint64 kHour = 60 * kMin;

// Round-number tick steps in ns: 1ms, 2ms, 5ms, 10ms, ... up to 10min.
const std::array<qint64, 18> kCandidateSteps = {
  1'000'000LL,   2'000'000LL,   5'000'000LL,   10'000'000LL, 20'000'000LL, 50'000'000LL,
  100'000'000LL, 200'000'000LL, 500'000'000LL, 1 * kSec,     2 * kSec,     5 * kSec,
  10 * kSec,     30 * kSec,     1 * kMin,      5 * kMin,     10 * kMin,    1 * kHour,
};
}  // namespace

TimeRulerItem::TimeRulerItem()
{
  setZValue(100);
}

void TimeRulerItem::setTimeRange(qint64 start_ns, qint64 end_ns)
{
  if (start_ns == start_ns_ && end_ns == end_ns_)
  {
    return;
  }
  prepareGeometryChange();
  start_ns_ = start_ns;
  end_ns_ = end_ns;
  recomputeTickInterval();
  update();
}

void TimeRulerItem::setPixelWidth(qreal w)
{
  if (qFuzzyCompare(w, width_px_))
  {
    return;
  }
  prepareGeometryChange();
  width_px_ = w;
  recomputeTickInterval();
  update();
}

void TimeRulerItem::setEpochOffsetNs(qint64 e)
{
  if (e == epoch_ns_)
  {
    return;
  }
  epoch_ns_ = e;
  update();
}

QRectF TimeRulerItem::boundingRect() const
{
  return QRectF(0, 0, width_px_, kRulerHeight);
}

void TimeRulerItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
  painter->fillRect(boundingRect(), QColor(0xdd, 0xdd, 0xdd));
  painter->setPen(QColor(0x33, 0x33, 0x33));
  painter->drawLine(QLineF(0, kRulerHeight - 1, width_px_, kRulerHeight - 1));

  if (end_ns_ <= start_ns_ || tick_interval_ns_ <= 0)
  {
    return;
  }
  const qreal px_per_ns = width_px_ / static_cast<qreal>(end_ns_ - start_ns_);

  // Snap first tick to a multiple of the interval.
  qint64 first = ((start_ns_ + tick_interval_ns_ - 1) / tick_interval_ns_) * tick_interval_ns_;
  for (qint64 t = first; t <= end_ns_; t += tick_interval_ns_)
  {
    qreal x = (t - start_ns_) * px_per_ns;
    painter->drawLine(QLineF(x, kRulerHeight - 8, x, kRulerHeight - 1));
    QString label = formatLabel(t - epoch_ns_);
    QFontMetricsF fm(painter->font());
    qreal tw = fm.horizontalAdvance(label);
    painter->drawText(QPointF(x - tw / 2.0, kRulerHeight - 10), label);
  }
}

void TimeRulerItem::recomputeTickInterval()
{
  if (end_ns_ <= start_ns_ || width_px_ <= 0)
  {
    tick_interval_ns_ = 1'000'000'000LL;
    return;
  }
  // Aim for ~80 px between major ticks.
  constexpr qreal kTargetPx = 80.0;
  const qreal px_per_ns = width_px_ / static_cast<qreal>(end_ns_ - start_ns_);
  const qreal target_ns = kTargetPx / px_per_ns;
  for (qint64 step : kCandidateSteps)
  {
    if (static_cast<qreal>(step) >= target_ns)
    {
      tick_interval_ns_ = step;
      return;
    }
  }
  tick_interval_ns_ = kCandidateSteps.back();
}

QString TimeRulerItem::formatLabel(qint64 ns_rel)
{
  // Choose format based on absolute magnitude of the value, not the interval —
  // this keeps labels readable even when the range spans hours.
  qint64 abs_ns = std::abs(ns_rel);
  if (abs_ns >= kHour)
  {
    qint64 sec = ns_rel / kSec;
    qint64 hh = sec / 3600;
    qint64 mm = (sec / 60) % 60;
    return QString("%1:%2").arg(hh).arg(mm, 2, 10, QChar('0'));
  }
  if (abs_ns >= kMin)
  {
    qint64 sec = ns_rel / kSec;
    qint64 mm = sec / 60;
    qint64 ss = sec % 60;
    return QString("%1:%2").arg(mm).arg(ss, 2, 10, QChar('0'));
  }
  if (abs_ns >= kSec)
  {
    double sec = ns_rel / 1e9;
    return QString::number(sec, 'f', 1) + "s";
  }
  double ms = ns_rel / 1e6;
  return QString::number(ms, 'f', 1) + "ms";
}

}  // namespace PJ::TimelinePrototype
