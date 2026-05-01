/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_PLAYHEAD_ITEM_H
#define PJ_TIMELINE_PROTOTYPE_PLAYHEAD_ITEM_H

#include <QGraphicsItem>

namespace PJ::TimelinePrototype
{

class PlayheadItem : public QGraphicsItem
{
public:
  static constexpr qreal kHandleHalfWidth = 6.0;
  static constexpr qreal kHandleHeight = 14.0;

  PlayheadItem();

  void setHeight(qreal h);

  QRectF boundingRect() const override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget* w) override;

private:
  qreal height_ = 200.0;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_PLAYHEAD_ITEM_H
