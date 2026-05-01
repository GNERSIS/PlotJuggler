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
class QComboBox;
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
  void onAlignmentComboChanged(int idx);
  void onResetAlignmentClicked();

private:
  TimelineModel* model_;
  PlaybackController* controller_;
  QAction* play_action_ = nullptr;
  QAction* loop_action_ = nullptr;
  QDoubleSpinBox* speed_spin_ = nullptr;
  QComboBox* alignment_combo_ = nullptr;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_PLAYBACK_TOOLBAR_H
