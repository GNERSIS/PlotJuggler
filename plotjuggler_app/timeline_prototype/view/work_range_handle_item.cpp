/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/work_range_handle_item.h"

#include <QCursor>
#include <QPainter>

namespace PJ::TimelinePrototype
{

namespace
{
constexpr qreal kHalfWidth = 7.0;
constexpr qreal kHeight = 14.0;
}  // namespace

WorkRangeHandleItem::WorkRangeHandleItem(Side side) : side_(side)
{
  setZValue(150);
  setCursor(QCursor(Qt::SizeHorCursor));
  setAcceptHoverEvents(true);
}

QRectF WorkRangeHandleItem::boundingRect() const
{
  return QRectF(-kHalfWidth, 0, 2 * kHalfWidth, kHeight);
}

void WorkRangeHandleItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
  QPolygonF tri;
  if (side_ == Side::Start)
  {
    // Triangle pointing right (start of range).
    tri << QPointF(-kHalfWidth, 0) << QPointF(kHalfWidth, kHeight / 2.0)
        << QPointF(-kHalfWidth, kHeight);
  }
  else
  {
    // Triangle pointing left (end of range).
    tri << QPointF(kHalfWidth, 0) << QPointF(-kHalfWidth, kHeight / 2.0)
        << QPointF(kHalfWidth, kHeight);
  }
  p->setBrush(QColor(80, 200, 255));
  p->setPen(QPen(QColor(220, 220, 220), 1));
  p->drawPolygon(tri);
}

}  // namespace PJ::TimelinePrototype
