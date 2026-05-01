/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/timeline_scene_widget.h"

#include "view/time_ruler_item.h"
#include "view/topic_item.h"
#include "view/work_range_handle_item.h"

#include <QGraphicsScene>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

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

  playhead_ = new PlayheadItem();
  scene_->addItem(playhead_);
  connect(model_, &TimelineModel::playheadChanged, this, &TimelineSceneWidget::onPlayheadChanged);

  work_start_handle_ = new WorkRangeHandleItem(WorkRangeHandleItem::Side::Start);
  work_end_handle_ = new WorkRangeHandleItem(WorkRangeHandleItem::Side::End);
  scene_->addItem(work_start_handle_);
  scene_->addItem(work_end_handle_);
  connect(model_, &TimelineModel::workRangeChanged, this, &TimelineSceneWidget::onWorkRangeChanged);

  connect(model_, &TimelineModel::sequencesChanged, this, &TimelineSceneWidget::rebuild);
  connect(model_, &TimelineModel::offsetsChanged, this, [this](int) { rebuild(); });
  connect(model_, &TimelineModel::selectionChanged, this,
          [this](const std::set<int>&) { rebuild(); });

  rebuild();

  // If playhead is still at its default 0 (before any external setter ran),
  // park it at the scene's left edge so it's visible at launch. The
  // PlaybackController in Task 12 may overwrite this with a smarter default.
  if (model_->playhead() == 0)
  {
    auto [lo, hi] = model_->sceneExtent();
    model_->setPlayhead(lo);
  }
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
  const qreal target = std::max(0.0, new_x - e->position().x());
  // Clamp to int range — at extreme zoom the scene can exceed INT_MAX pixels
  // and silently wrap the scrollbar value.
  const qreal clamped = std::min<qreal>(target, std::numeric_limits<int>::max());
  horizontalScrollBar()->setValue(static_cast<int>(clamped));
  e->accept();
}

void TimelineSceneWidget::resizeEvent(QResizeEvent* e)
{
  QGraphicsView::resizeEvent(e);
  updateRulerGeometry();
}

void TimelineSceneWidget::rebuild()
{
  // Don't tear down items while a drag is in progress — drag_item_ would
  // dangle and the next mouse event would crash. The drag's own model
  // write at release will trigger a rebuild as a normal follow-up.
  if (drag_item_)
  {
    return;
  }

  // Remove all items except the ruler, playhead, and work-range handles.
  for (QGraphicsItem* item : scene_->items())
  {
    if (item == ruler_ || item == playhead_ || item == work_start_handle_ ||
        item == work_end_handle_)
    {
      continue;
    }
    scene_->removeItem(item);
    delete item;
  }

  auto [ext_lo, ext_hi] = model_->sceneExtent();
  const auto& seqs = model_->sequences();

  // Build a contiguous list of (seq_idx, topic_idx) for visible sequences.
  std::vector<std::pair<int, int>> rows;
  for (int s = 0; s < static_cast<int>(seqs.size()); ++s)
  {
    if (!isSequenceVisible(s))
    {
      continue;
    }
    for (int t = 0; t < static_cast<int>(seqs[s].topics.size()); ++t)
    {
      rows.emplace_back(s, t);
    }
  }

  const qreal rows_h = rows.size() * (kRowHeight + kRowGap) + 16.0;
  const qreal scene_w = std::max<qreal>(100.0, (ext_hi - ext_lo) * px_per_ns_);
  const qreal scene_h = TimeRulerItem::kRulerHeight + 8.0 + rows_h;
  scene_->setSceneRect(0, 0, scene_w, scene_h);

  ruler_->setEpochOffsetNs(ext_lo);
  ruler_->setTimeRange(ext_lo, ext_hi);
  ruler_->setPixelWidth(scene_w);

  playhead_->setHeight(scene_h);
  onPlayheadChanged(model_->playhead());  // re-place after rebuild

  auto [ws, we] = model_->workRange();
  onWorkRangeChanged(ws, we);

  // Add the topic rectangles.
  qreal y = TimeRulerItem::kRulerHeight + 8.0;
  for (const auto& [s, t] : rows)
  {
    auto [t_lo, t_hi] = model_->topicDisplayWindow(s, t);
    const qreal x = (t_lo - ext_lo) * px_per_ns_;
    const qreal w = std::max<qreal>(2.0, (t_hi - t_lo) * px_per_ns_);
    const QString label = QString("%1 %2").arg(seqs[s].name).arg(seqs[s].topics[t].name);
    auto* item =
        new TopicItem(s, t, label, seqs[s].color, seqs[s].topics[t].topic_offset_overridden,
                      seqs[s].seq_offset_overridden);
    item->setRect(0, 0, w, kRowHeight);
    item->setPos(x, y);
    scene_->addItem(item);
    y += kRowHeight + kRowGap;
  }

  updateRulerGeometry();
}

bool TimelineSceneWidget::isSequenceVisible(int seq_idx) const
{
  const auto& sel = model_->selection();
  if (sel.empty())
  {
    return true;
  }
  return sel.count(seq_idx) > 0;
}

void TimelineSceneWidget::updateRulerGeometry()
{
  // Keep ruler visually pinned to the top of the viewport during vertical
  // scroll. We do this by parking the ruler at scene-y = scrollbar value.
  // TODO(task-11+): also connect verticalScrollBar()::valueChanged so the
  // ruler tracks live during scroll, not just on resize/rebuild.
  if (!ruler_)
  {
    return;
  }
  const qreal y = verticalScrollBar() ? verticalScrollBar()->value() : 0;
  ruler_->setPos(0, y);
}

void TimelineSceneWidget::mousePressEvent(QMouseEvent* e)
{
  if (e->button() == Qt::LeftButton)
  {
    QGraphicsItem* item = itemAt(e->pos());

    // Clicking empty scene background clears LHS selection. We don't
    // accept the event so QGraphicsView still handles potential pan.
    if (item == nullptr)
    {
      model_->setSelection({});
    }

    // Work-range handle drag (checked first — handles sit on the ruler, so
    // this must win over the plain ruler-click / seek handler below).
    if (auto* h = dynamic_cast<WorkRangeHandleItem*>(item))
    {
      dragging_handle_ = h;
      e->accept();
      return;
    }

    // Playhead-or-ruler click: seek to that position.
    if (item == playhead_ || (item == ruler_ && e->pos().y() < TimeRulerItem::kRulerHeight))
    {
      dragging_playhead_ = true;
      QPointF s = mapToScene(e->pos());
      auto [lo, hi] = model_->sceneExtent();
      qint64 ns = static_cast<qint64>(s.x() / px_per_ns_) + lo;
      ns = std::clamp(ns, lo, hi);
      model_->setPlayhead(ns);
      e->accept();
      return;
    }

    // Topic drag.
    auto* topic = dynamic_cast<TopicItem*>(item);
    if (topic)
    {
      drag_item_ = topic;
      drag_topic_only_ = (e->modifiers() & Qt::ControlModifier);
      drag_start_scene_ = mapToScene(e->pos());
      drag_dx_px_ = 0.0;
      setCursor(drag_topic_only_ ? Qt::SizeHorCursor : Qt::ClosedHandCursor);
      e->accept();
      return;
    }
  }
  QGraphicsView::mousePressEvent(e);
}

void TimelineSceneWidget::mouseMoveEvent(QMouseEvent* e)
{
  if (dragging_handle_)
  {
    QPointF s = mapToScene(e->pos());
    auto [lo, hi] = model_->sceneExtent();
    qint64 ns = static_cast<qint64>(s.x() / px_per_ns_) + lo;
    auto [ws, we] = model_->workRange();
    if (dragging_handle_->side() == WorkRangeHandleItem::Side::Start)
    {
      model_->setWorkRange(ns, we);
    }
    else
    {
      model_->setWorkRange(ws, ns);
    }
    e->accept();
    return;
  }
  if (dragging_playhead_)
  {
    QPointF s = mapToScene(e->pos());
    auto [lo, hi] = model_->sceneExtent();
    qint64 ns = static_cast<qint64>(s.x() / px_per_ns_) + lo;
    ns = std::clamp(ns, lo, hi);
    model_->setPlayhead(ns);
    e->accept();
    return;
  }
  if (drag_item_)
  {
    const QPointF cur = mapToScene(e->pos());
    const qreal dx_px = cur.x() - drag_start_scene_.x();
    drag_dx_px_ = dx_px;

    if (drag_topic_only_)
    {
      drag_item_->setGhostDx(dx_px);
    }
    else
    {
      // Move all topic items in the same sequence.
      const int s = drag_item_->sequenceIndex();
      for (QGraphicsItem* it : scene_->items())
      {
        if (auto* ti = dynamic_cast<TopicItem*>(it))
        {
          if (ti->sequenceIndex() == s)
          {
            ti->setGhostDx(dx_px);
          }
        }
      }
    }
    e->accept();
    return;
  }
  QGraphicsView::mouseMoveEvent(e);
}

void TimelineSceneWidget::mouseReleaseEvent(QMouseEvent* e)
{
  if (dragging_handle_ && e->button() == Qt::LeftButton)
  {
    dragging_handle_ = nullptr;
    e->accept();
    return;
  }
  if (dragging_playhead_ && e->button() == Qt::LeftButton)
  {
    dragging_playhead_ = false;
    e->accept();
    return;
  }
  if (drag_item_ && e->button() == Qt::LeftButton)
  {
    const qint64 tick = ruler_->currentTickIntervalNs();
    // llround for symmetric rounding — static_cast truncates toward zero
    // and would lose sub-tick negative drags at high zoom.
    qint64 dx_ns = std::llround(drag_dx_px_ / px_per_ns_);
    dx_ns = TimelineModel::snapToGrid(dx_ns, tick);

    const int s = drag_item_->sequenceIndex();
    const int t = drag_item_->topicIndex();

    // Clear ghosts before model write so rebuild() paints from the new state.
    for (QGraphicsItem* it : scene_->items())
    {
      if (auto* ti = dynamic_cast<TopicItem*>(it))
      {
        ti->setGhostDx(0.0);
      }
    }

    if (dx_ns != 0)
    {
      if (drag_topic_only_)
      {
        const auto& topic = model_->sequences()[s].topics[t];
        model_->setTopicOffset(s, t, topic.per_topic_offset_ns + dx_ns);
      }
      else
      {
        const auto& seq = model_->sequences()[s];
        model_->setSequenceOffset(s, seq.display_offset_ns + dx_ns);
      }
    }

    drag_item_ = nullptr;
    drag_dx_px_ = 0.0;
    // unsetCursor (not setCursor(ArrowCursor)) so per-item hover cursors
    // (TopicItem's OpenHandCursor) become active again on the next hover.
    unsetCursor();
    e->accept();
    return;
  }
  QGraphicsView::mouseReleaseEvent(e);
}

void TimelineSceneWidget::onPlayheadChanged(qint64 ns)
{
  auto [ext_lo, ext_hi] = model_->sceneExtent();
  const qreal x = (ns - ext_lo) * px_per_ns_;
  playhead_->setPos(x, 0);
}

void TimelineSceneWidget::onWorkRangeChanged(qint64 start_ns, qint64 end_ns)
{
  // Same scroll-tracking caveat as the ruler — see TODO(task-11+) in
  // updateRulerGeometry. When that fix lands, also call this slot so the
  // handles re-pin atomically with the ruler.
  auto [ext_lo, ext_hi] = model_->sceneExtent();
  const qreal y = ruler_->pos().y();
  work_start_handle_->setPos((start_ns - ext_lo) * px_per_ns_, y);
  work_end_handle_->setPos((end_ns - ext_lo) * px_per_ns_, y);
}

}  // namespace PJ::TimelinePrototype
