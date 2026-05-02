/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "timeline_window.h"
#include "view/playback_toolbar.h"
#include "view/timeline_scene_widget.h"

#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace PJ::TimelinePrototype
{

TimelineWindow::TimelineWindow(QWidget* parent)
  : QMainWindow(parent), model_(std::make_unique<TimelineModel>())
{
  setWindowTitle("Timeline Prototype");
  resize(1280, 720);

  controller_ = std::make_unique<PlaybackController>(model_.get());
  auto* toolbar = new PlaybackToolbar(model_.get(), controller_.get(), this);
  addToolBar(Qt::TopToolBarArea, toolbar);

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* scene = new TimelineSceneWidget(model_.get(), central);
  connect(controller_.get(), &PlaybackController::playingChanged, scene,
          &TimelineSceneWidget::setPlaying);
  layout->addWidget(scene, 1);

  setCentralWidget(central);

  statusBar()->showMessage("Drag = move sequence  |  Ctrl+drag = move single topic  "
                           "|  Wheel = zoom");
}

TimelineWindow::~TimelineWindow() = default;

}  // namespace PJ::TimelinePrototype
