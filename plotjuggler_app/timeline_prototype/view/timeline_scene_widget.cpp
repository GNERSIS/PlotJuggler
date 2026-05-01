/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/timeline_scene_widget.h"

#include "view/time_ruler_item.h"

#include <QGraphicsScene>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>

namespace PJ::TimelinePrototype
{

TimelineSceneWidget::TimelineSceneWidget(TimelineModel* model, QWidget* parent)
  : QGraphicsView(parent), model_(model), scene_(new QGraphicsScene(this))
{
  setScene(scene_);
  setRenderHint(QPainter::Antialiasing);
  setBackgroundBrush(QColor(35, 35, 35));
  setAlignment(Qt::AlignLeft | Qt::AlignTop);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  setTransformationAnchor(QGraphicsView::NoAnchor);

  ruler_ = new TimeRulerItem();
  scene_->addItem(ruler_);
  ruler_->setEpochOffsetNs(0);  // task 9 will set this from sceneExtent

  connect(model_, &TimelineModel::sequencesChanged, this, &TimelineSceneWidget::rebuild);
  connect(model_, &TimelineModel::offsetsChanged, this, [this](int) { rebuild(); });
  connect(model_, &TimelineModel::selectionChanged, this,
          [this](const std::set<int>&) { rebuild(); });

  rebuild();
}

void TimelineSceneWidget::wheelEvent(QWheelEvent* e)
{
  // Horizontal zoom anchored at the cursor's x position in ns.
  const qreal angle = e->angleDelta().y();
  if (angle == 0)
  {
    QGraphicsView::wheelEvent(e);
    return;
  }
  const qreal factor = (angle > 0) ? 1.25 : 1.0 / 1.25;

  const QPointF scene_pos = mapToScene(e->position().toPoint());
  const qreal cursor_ns = (px_per_ns_ > 0) ? (scene_pos.x() / px_per_ns_) : 0.0;

  const qreal new_px = std::clamp(px_per_ns_ * factor, kMinPxPerNs, kMaxPxPerNs);
  if (qFuzzyCompare(new_px, px_per_ns_))
  {
    e->accept();
    return;
  }
  px_per_ns_ = new_px;
  rebuild();

  // Recenter so the same ns sits under the cursor.
  const qreal new_x = cursor_ns * px_per_ns_;
  horizontalScrollBar()->setValue(static_cast<int>(std::max(0.0, new_x - e->position().x())));
  e->accept();
}

void TimelineSceneWidget::resizeEvent(QResizeEvent* e)
{
  QGraphicsView::resizeEvent(e);
  updateRulerGeometry();
}

void TimelineSceneWidget::rebuild()
{
  // Remove all items except the ruler, then re-add per current model state.
  for (QGraphicsItem* item : scene_->items())
  {
    if (item == ruler_)
    {
      continue;
    }
    scene_->removeItem(item);
    delete item;
  }

  auto [ext_lo, ext_hi] = model_->sceneExtent();
  const qreal scene_w = (ext_hi - ext_lo) * px_per_ns_;
  const qreal scene_h = TimeRulerItem::kRulerHeight + 8.0 +
                        std::max<qreal>(200.0, kRowHeight * 20);  // placeholder height
  scene_->setSceneRect(0, 0, scene_w, scene_h);

  ruler_->setEpochOffsetNs(ext_lo);
  ruler_->setTimeRange(ext_lo, ext_hi);
  ruler_->setPixelWidth(scene_w);
  ruler_->setPos(0, 0);

  updateRulerGeometry();
}

void TimelineSceneWidget::updateRulerGeometry()
{
  // Keep ruler visually pinned to the top of the viewport during vertical
  // scroll. We do this by parking the ruler at scene-y = scrollbar value.
  if (!ruler_)
  {
    return;
  }
  const qreal y = verticalScrollBar() ? verticalScrollBar()->value() : 0;
  ruler_->setPos(0, y);
}

}  // namespace PJ::TimelinePrototype
