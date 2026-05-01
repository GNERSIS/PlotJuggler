/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_TIMELINE_SCENE_WIDGET_H
#define PJ_TIMELINE_PROTOTYPE_TIMELINE_SCENE_WIDGET_H

#include "model/timeline_model.h"
#include "view/playhead_item.h"
#include "view/topic_item.h"
#include "view/work_range_handle_item.h"

#include <QGraphicsView>

class QGraphicsScene;

namespace PJ::TimelinePrototype
{

class TimeRulerItem;

class TimelineSceneWidget : public QGraphicsView
{
  Q_OBJECT
public:
  static constexpr qreal kRowHeight = 24.0;
  static constexpr qreal kRowGap = 2.0;
  static constexpr qreal kMinPxPerNs = 1e-9;
  static constexpr qreal kMaxPxPerNs = 1e-1;
  static constexpr qreal kDefaultPxPerNs = 1e-7;  // ~100 ns per pixel

  explicit TimelineSceneWidget(TimelineModel* model, QWidget* parent = nullptr);

  // ns-pixel mapping (single source of truth for all child items).
  qreal pxPerNs() const
  {
    return px_per_ns_;
  }

protected:
  void wheelEvent(QWheelEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;

private slots:
  void rebuild();
  void onPlayheadChanged(qint64 ns);
  void onWorkRangeChanged(qint64 start_ns, qint64 end_ns);

private:
  void updateRulerGeometry();
  bool isSequenceVisible(int seq_idx) const;  // honors selection-as-filter

  TimelineModel* model_;
  QGraphicsScene* scene_;
  TimeRulerItem* ruler_;
  qreal px_per_ns_ = kDefaultPxPerNs;

  // Drag state. rebuild() is gated against running while drag_item_ is set,
  // so this raw pointer can't dangle from a model-driven re-populate.
  // QPointer would be ideal but TopicItem is not a QObject.
  TopicItem* drag_item_ = nullptr;
  bool drag_topic_only_ = false;
  QPointF drag_start_scene_;
  qreal drag_dx_px_ = 0.0;

  PlayheadItem* playhead_ = nullptr;
  bool dragging_playhead_ = false;

  WorkRangeHandleItem* work_start_handle_ = nullptr;
  WorkRangeHandleItem* work_end_handle_ = nullptr;
  WorkRangeHandleItem* dragging_handle_ = nullptr;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_TIMELINE_SCENE_WIDGET_H
