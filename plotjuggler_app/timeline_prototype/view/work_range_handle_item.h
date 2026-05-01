/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_WORK_RANGE_HANDLE_ITEM_H
#define PJ_TIMELINE_PROTOTYPE_WORK_RANGE_HANDLE_ITEM_H

#include <QGraphicsItem>

namespace PJ::TimelinePrototype
{

class WorkRangeHandleItem : public QGraphicsItem
{
public:
  enum class Side
  {
    Start,
    End,
  };

  explicit WorkRangeHandleItem(Side side);
  Side side() const
  {
    return side_;
  }

  QRectF boundingRect() const override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget* w) override;

private:
  Side side_;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_WORK_RANGE_HANDLE_ITEM_H
