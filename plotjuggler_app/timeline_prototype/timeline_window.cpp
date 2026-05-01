/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "timeline_window.h"
#include "view/playback_toolbar.h"
#include "view/sequence_table_widget.h"
#include "view/timeline_scene_widget.h"

#include <QSplitter>
#include <QStatusBar>

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

  // Central splitter: LHS table | RHS scene.
  splitter_ = new QSplitter(Qt::Horizontal, this);

  auto* lhs = new SequenceTableWidget(model_.get(), splitter_);
  lhs->setMinimumWidth(280);
  splitter_->addWidget(lhs);

  auto* rhs = new TimelineSceneWidget(model_.get(), splitter_);
  splitter_->addWidget(rhs);

  splitter_->setStretchFactor(0, 0);
  splitter_->setStretchFactor(1, 1);
  splitter_->setSizes({ 320, 960 });

  setCentralWidget(splitter_);

  statusBar()->showMessage("Drag = move sequence  |  Ctrl+drag = move single topic  "
                           "|  Wheel = zoom  |  Right-click LHS row for alignment");
}

TimelineWindow::~TimelineWindow() = default;

}  // namespace PJ::TimelinePrototype
