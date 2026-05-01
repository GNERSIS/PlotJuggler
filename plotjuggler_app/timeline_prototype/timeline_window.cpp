/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "timeline_window.h"

#include <QLabel>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace PJ::TimelinePrototype
{

TimelineWindow::TimelineWindow(QWidget* parent)
  : QMainWindow(parent), model_(std::make_unique<TimelineModel>())
{
  setWindowTitle("Timeline Prototype");
  resize(1280, 720);

  // Top toolbar placeholder.
  auto* toolbar = new QToolBar("Playback", this);
  toolbar->addWidget(new QLabel("[ PlaybackToolbar placeholder ]"));
  addToolBar(Qt::TopToolBarArea, toolbar);

  // Central splitter: LHS table | RHS scene.
  splitter_ = new QSplitter(Qt::Horizontal, this);

  auto* lhs = new QLabel("[ SequenceTableWidget placeholder ]", splitter_);
  lhs->setAlignment(Qt::AlignCenter);
  lhs->setMinimumWidth(280);
  splitter_->addWidget(lhs);

  auto* rhs = new QLabel("[ TimelineSceneWidget placeholder ]", splitter_);
  rhs->setAlignment(Qt::AlignCenter);
  splitter_->addWidget(rhs);

  splitter_->setStretchFactor(0, 0);
  splitter_->setStretchFactor(1, 1);
  splitter_->setSizes({ 320, 960 });

  setCentralWidget(splitter_);
}

TimelineWindow::~TimelineWindow() = default;

}  // namespace PJ::TimelinePrototype
