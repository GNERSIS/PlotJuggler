/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/playhead_item.h"

#include <QCursor>
#include <QPainter>

namespace PJ::TimelinePrototype
{

PlayheadItem::PlayheadItem()
{
  setZValue(200);
  setFlag(ItemIsSelectable, false);
  setCursor(QCursor(Qt::SizeHorCursor));
  setAcceptHoverEvents(true);
}

void PlayheadItem::setHeight(qreal h)
{
  if (qFuzzyCompare(h, height_))
  {
    return;
  }
  prepareGeometryChange();
  height_ = h;
}

QRectF PlayheadItem::boundingRect() const
{
  return QRectF(-kHandleHalfWidth, 0, 2 * kHandleHalfWidth, height_);
}

void PlayheadItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
  // Vertical line.
  p->setPen(QPen(QColor(255, 80, 80), 1.5));
  p->drawLine(QLineF(0, kHandleHeight, 0, height_));
  // Triangular handle at the top.
  QPolygonF tri;
  tri << QPointF(-kHandleHalfWidth, 0) << QPointF(kHandleHalfWidth, 0) << QPointF(0, kHandleHeight);
  p->setBrush(QColor(255, 80, 80));
  p->setPen(Qt::NoPen);
  p->drawPolygon(tri);
}

}  // namespace PJ::TimelinePrototype
