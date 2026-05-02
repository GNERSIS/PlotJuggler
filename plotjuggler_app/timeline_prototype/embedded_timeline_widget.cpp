/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "embedded_timeline_widget.h"

#include "view/playback_toolbar.h"
#include "view/timeline_scene_widget.h"

#include <QVBoxLayout>

namespace PJ::TimelinePrototype
{

EmbeddedTimelineWidget::EmbeddedTimelineWidget(QWidget* parent)
  : QWidget(parent), model_(std::make_unique<TimelineModel>())
{
  controller_ = std::make_unique<PlaybackController>(model_.get());

  auto* toolbar = new PlaybackToolbar(model_.get(), controller_.get(), this);

  scene_widget_ = new TimelineSceneWidget(model_.get(), this);
  connect(controller_.get(), &PlaybackController::playingChanged, scene_widget_,
          &TimelineSceneWidget::setPlaying);
  connect(scene_widget_, &TimelineSceneWidget::curvesDropped, this,
          &EmbeddedTimelineWidget::curvesDropped);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(toolbar);
  layout->addWidget(scene_widget_, 1);
}

EmbeddedTimelineWidget::~EmbeddedTimelineWidget() = default;

}  // namespace PJ::TimelinePrototype
