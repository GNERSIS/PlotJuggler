/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_TOPIC_ITEM_H
#define PJ_TIMELINE_PROTOTYPE_TOPIC_ITEM_H

#include <cmath>

#include <QColor>
#include <QGraphicsRectItem>
#include <QString>

namespace PJ::TimelinePrototype
{

class TopicItem : public QGraphicsRectItem
{
public:
  TopicItem(int seq_idx, int topic_idx, const QString& label, const QColor& fill,
            bool topic_overridden, bool seq_overridden);

  int sequenceIndex() const
  {
    return seq_idx_;
  }
  int topicIndex() const
  {
    return topic_idx_;
  }

  void setGhostDx(qreal dx_px)
  {
    ghost_dx_px_ = dx_px;
    update();
  }

  QRectF boundingRect() const override
  {
    QRectF r = QGraphicsRectItem::boundingRect();
    qreal extra = std::abs(ghost_dx_px_);
    return r.adjusted(-extra, 0, extra, 0);
  }

  void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget* w) override;

private:
  int seq_idx_;
  int topic_idx_;
  QString label_;
  QColor fill_;
  bool topic_overridden_;
  bool seq_overridden_;
  qreal ghost_dx_px_ = 0.0;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_TOPIC_ITEM_H
