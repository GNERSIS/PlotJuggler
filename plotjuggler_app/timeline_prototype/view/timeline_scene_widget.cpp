/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/timeline_scene_widget.h"

#include "view/time_ruler_item.h"
#include "view/topic_item.h"
#include "view/work_range_handle_item.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace PJ::TimelinePrototype
{

TimelineSceneWidget::TimelineSceneWidget(TimelineModel* model, QWidget* parent)
  : QGraphicsView(parent), model_(model), scene_(new QGraphicsScene(this))
{
  setScene(scene_);
  setRenderHint(QPainter::Antialiasing);
  setBackgroundBrush(QColor(0xee, 0xee, 0xee));
  setAlignment(Qt::AlignLeft | Qt::AlignTop);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  setTransformationAnchor(QGraphicsView::NoAnchor);
  setAcceptDrops(true);

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
  // The scene's minimum size now tracks the viewport (so an empty model
  // still fills the visible area). Re-run the layout so a viewport resize
  // re-stretches the scene rect, ruler width, and playhead height. rebuild
  // is gated against drag-in-progress, so it's safe to call here.
  rebuild();
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

  // All sequences are always shown. Selection is rendered as a highlight
  // (non-selected sequences are dimmed) rather than a filter — see
  // isSequenceDimmed() below.
  std::vector<std::pair<int, int>> rows;
  for (int s = 0; s < static_cast<int>(seqs.size()); ++s)
  {
    for (int t = 0; t < static_cast<int>(seqs[s].topics.size()); ++t)
    {
      rows.emplace_back(s, t);
    }
  }

  const qreal rows_h = rows.size() * (kRowHeight + kRowGap) + 16.0;
  // Scene must always be at least as large as the visible viewport so that
  // an empty model (no rows) still fills the panel rather than collapsing
  // to a 100×40 patch in the corner.
  const qreal vp_w = (viewport() && viewport()->width() > 0) ? viewport()->width() : 100.0;
  const qreal vp_h = (viewport() && viewport()->height() > 0) ? viewport()->height() : 100.0;
  const qreal scene_w = std::max<qreal>(vp_w, (ext_hi - ext_lo) * px_per_ns_);
  const qreal scene_h = std::max<qreal>(vp_h, TimeRulerItem::kRulerHeight + 8.0 + rows_h);
  scene_->setSceneRect(0, 0, scene_w, scene_h);

  ruler_->setEpochOffsetNs(ext_lo);
  ruler_->setTimeRange(ext_lo, ext_hi);
  ruler_->setPixelWidth(scene_w);

  playhead_->setHeight(scene_h);
  onPlayheadChanged(model_->playhead());  // re-place after rebuild

  auto [ws, we] = model_->workRange();
  onWorkRangeChanged(ws, we);

  // Add the topic rectangles. Topics whose sequence is not in the current
  // selection (when a selection exists) are rendered dimmed.
  const auto& sel = model_->selection();
  qreal y = TimeRulerItem::kRulerHeight + 8.0;
  for (const auto& [s, t] : rows)
  {
    auto [t_lo, t_hi] = model_->topicDisplayWindow(s, t);
    const qreal x = (t_lo - ext_lo) * px_per_ns_;
    const qreal w = std::max<qreal>(2.0, (t_hi - t_lo) * px_per_ns_);
    const QString label = QString("%1 %2").arg(seqs[s].name).arg(seqs[s].topics[t].name);
    const bool dimmed = !sel.empty() && sel.count(s) == 0;
    auto* item =
        new TopicItem(s, t, label, seqs[s].color, seqs[s].topics[t].topic_offset_overridden,
                      seqs[s].seq_offset_overridden, dimmed);
    item->setRect(0, 0, w, kRowHeight);
    item->setPos(x, y);
    scene_->addItem(item);
    y += kRowHeight + kRowGap;
  }

  updateRulerGeometry();
}

void TimelineSceneWidget::setPlaying(bool playing)
{
  is_playing_ = playing;
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

void TimelineSceneWidget::contextMenuEvent(QContextMenuEvent* e)
{
  QGraphicsItem* item = itemAt(e->pos());
  auto* handle = dynamic_cast<WorkRangeHandleItem*>(item);
  if (!handle)
  {
    QGraphicsView::contextMenuEvent(e);
    return;
  }
  QMenu menu(this);
  const int lead = model_->leading();
  const QString reset_label =
      (lead >= 0) ? "Reset to leading sequence range" : "Reset to scene extent";
  QAction* reset_action = menu.addAction(reset_label);
  if (menu.exec(e->globalPos()) == reset_action)
  {
    qint64 lo, hi;
    if (lead >= 0)
    {
      std::tie(lo, hi) = model_->displayWindow(lead);
    }
    else
    {
      std::tie(lo, hi) = model_->sceneExtent();
    }
    model_->setWorkRange(lo, hi);
  }
  e->accept();
}

void TimelineSceneWidget::onPlayheadChanged(qint64 ns)
{
  auto [ext_lo, ext_hi] = model_->sceneExtent();
  const qreal x = (ns - ext_lo) * px_per_ns_;
  playhead_->setPos(x, 0);

  // While playing, follow the playhead horizontally so it stays on-screen.
  // Suppressed during a manual playhead drag — scrolling the view while the
  // user is dragging would yank the cursor away from their grip.
  if (is_playing_ && !dragging_playhead_)
  {
    auto* hbar = horizontalScrollBar();
    if (!hbar)
    {
      return;
    }
    const int view_w = viewport()->width();
    const int scroll_x = hbar->value();
    const qreal margin = std::min<qreal>(80.0, view_w * 0.1);
    const qreal view_x = x - scroll_x;
    if (view_x < margin || view_x > view_w - margin)
    {
      // Park the playhead near the right edge so the user can see the
      // upcoming timeline as it plays forward. After a loop wrap this also
      // re-anchors the view to wherever the playhead jumped to.
      qreal target = x - (view_w - margin);
      target = std::clamp<qreal>(target, 0.0, std::numeric_limits<int>::max());
      hbar->setValue(static_cast<int>(target));
    }
  }
}

void TimelineSceneWidget::dragEnterEvent(QDragEnterEvent* e)
{
  if (e->mimeData()->hasFormat("curveslist/add_curve"))
  {
    e->acceptProposedAction();
    return;
  }
  QGraphicsView::dragEnterEvent(e);
}

void TimelineSceneWidget::dragMoveEvent(QDragMoveEvent* e)
{
  if (e->mimeData()->hasFormat("curveslist/add_curve"))
  {
    e->acceptProposedAction();
    return;
  }
  QGraphicsView::dragMoveEvent(e);
}

void TimelineSceneWidget::dropEvent(QDropEvent* e)
{
  if (!e->mimeData()->hasFormat("curveslist/add_curve"))
  {
    QGraphicsView::dropEvent(e);
    return;
  }
  // PlotJuggler's curvelist serializes selected curve names as a stream of
  // QString (see curvelist_view.cpp). Decode and forward — the host (e.g.
  // MainWindow) is responsible for resolving names → time ranges.
  QByteArray payload = e->mimeData()->data("curveslist/add_curve");
  QDataStream stream(&payload, QIODevice::ReadOnly);
  QStringList names;
  while (!stream.atEnd())
  {
    QString name;
    stream >> name;
    names << name;
  }
  if (!names.isEmpty())
  {
    emit curvesDropped(names);
  }
  e->acceptProposedAction();
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
