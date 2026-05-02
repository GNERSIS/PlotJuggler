/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_PLAYBACK_TOOLBAR_H
#define PJ_TIMELINE_PROTOTYPE_PLAYBACK_TOOLBAR_H

#include "controller/playback_controller.h"
#include "model/timeline_model.h"

#include <QToolBar>

class QAction;
class QActionGroup;
class QDoubleSpinBox;

namespace PJ::TimelinePrototype
{

class PlaybackToolbar : public QToolBar
{
  Q_OBJECT
public:
  PlaybackToolbar(TimelineModel* model, PlaybackController* controller, QWidget* parent = nullptr);

private slots:
  void onPlayingChanged(bool playing);
  void onResetAlignmentClicked();

private:
  TimelineModel* model_;
  PlaybackController* controller_;
  QAction* play_action_ = nullptr;
  QAction* loop_action_ = nullptr;
  QDoubleSpinBox* speed_spin_ = nullptr;

  // Three exclusive alignment buttons (Start = align_left, Middle, Finish =
  // align_right). The QActionGroup enforces single-selection.
  QActionGroup* alignment_group_ = nullptr;
  QAction* align_start_action_ = nullptr;
  QAction* align_middle_action_ = nullptr;
  QAction* align_finish_action_ = nullptr;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_PLAYBACK_TOOLBAR_H
