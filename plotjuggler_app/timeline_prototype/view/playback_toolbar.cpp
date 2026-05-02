/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/playback_toolbar.h"

#include <QAction>
#include <QActionGroup>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>

namespace PJ::TimelinePrototype
{

PlaybackToolbar::PlaybackToolbar(TimelineModel* model, PlaybackController* controller,
                                 QWidget* parent)
  : QToolBar("Playback", parent), model_(model), controller_(controller)
{
  setMovable(false);

  play_action_ = addAction(QIcon(":/resources/svg/play_arrow.svg"), QString());
  play_action_->setCheckable(true);
  play_action_->setToolTip("Play / Pause");
  connect(play_action_, &QAction::triggered, controller_, &PlaybackController::togglePlay);

  loop_action_ = addAction(QIcon(":/resources/svg/loop.svg"), QString());
  loop_action_->setCheckable(true);
  loop_action_->setToolTip("Loop playback");
  connect(loop_action_, &QAction::toggled, controller_, &PlaybackController::setLoop);

  addSeparator();
  addWidget(new QLabel("Speed:"));
  speed_spin_ = new QDoubleSpinBox(this);
  speed_spin_->setRange(0.1, 10.0);
  speed_spin_->setSingleStep(0.1);
  speed_spin_->setValue(1.0);
  connect(speed_spin_, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          controller_, &PlaybackController::setSpeed);
  controller_->setSpeed(speed_spin_->value());  // bind defaults at startup
  addWidget(speed_spin_);

  addSeparator();

  // Three mutually-exclusive alignment buttons. Start = align_left is the
  // default (matches AlignmentMode::Start). The button group enforces that
  // exactly one is checked at any time; AlignmentMode::None is no longer
  // surfaced in the UI but the model still accepts it programmatically.
  alignment_group_ = new QActionGroup(this);
  alignment_group_->setExclusive(true);

  align_start_action_ = addAction(QIcon(":/resources/svg/align_left.svg"), QString());
  align_start_action_->setCheckable(true);
  align_start_action_->setToolTip("Align sequences to the leading sequence's start");
  align_start_action_->setData(static_cast<int>(AlignmentMode::Start));
  alignment_group_->addAction(align_start_action_);

  align_middle_action_ = addAction(QIcon(":/resources/svg/align_middle.svg"), QString());
  align_middle_action_->setCheckable(true);
  align_middle_action_->setToolTip("Align sequences to the leading sequence's middle");
  align_middle_action_->setData(static_cast<int>(AlignmentMode::Middle));
  alignment_group_->addAction(align_middle_action_);

  align_finish_action_ = addAction(QIcon(":/resources/svg/align_right.svg"), QString());
  align_finish_action_->setCheckable(true);
  align_finish_action_->setToolTip("Align sequences to the leading sequence's finish");
  align_finish_action_->setData(static_cast<int>(AlignmentMode::Finish));
  alignment_group_->addAction(align_finish_action_);

  align_start_action_->setChecked(true);  // default
  // Push the default to the model so its state matches the visible UI from
  // the start. The model would otherwise default to AlignmentMode::None.
  model_->setAlignment(AlignmentMode::Start);

  connect(alignment_group_, &QActionGroup::triggered, this, [this](QAction* a) {
    model_->setAlignment(static_cast<AlignmentMode>(a->data().toInt()));
  });

  auto* reset_action = addAction(QIcon(":/resources/svg/reset_settings.svg"), QString());
  reset_action->setToolTip("Reset alignment offsets");
  connect(reset_action, &QAction::triggered, this, &PlaybackToolbar::onResetAlignmentClicked);

  connect(controller_, &PlaybackController::playingChanged, this,
          &PlaybackToolbar::onPlayingChanged);
  connect(model_, &TimelineModel::alignmentChanged, this, [this](AlignmentMode m) {
    // Sync the buttons to whatever the model says without re-firing triggered.
    const QSignalBlocker blocker(alignment_group_);
    for (QAction* a : alignment_group_->actions())
    {
      a->setChecked(static_cast<AlignmentMode>(a->data().toInt()) == m);
    }
  });
}

void PlaybackToolbar::onPlayingChanged(bool playing)
{
  play_action_->setChecked(playing);
  play_action_->setIcon(
      QIcon(playing ? ":/resources/svg/pause.svg" : ":/resources/svg/play_arrow.svg"));
}

void PlaybackToolbar::onResetAlignmentClicked()
{
  // Count distinct sequences with any override. setTopicOffset auto-flags
  // the parent sequence, so iterating per-topic would double-count user
  // actions in a way that's hard to predict.
  int n = 0;
  for (const Sequence& s : model_->sequences())
  {
    if (s.seq_offset_overridden)
    {
      ++n;
    }
  }
  if (n == 0)
  {
    model_->resetAlignment();
    return;
  }
  auto reply = QMessageBox::question(
      this, "Reset Alignment",
      QString("Reset offsets on %1 sequence(s) and re-apply alignment?").arg(n),
      QMessageBox::Ok | QMessageBox::Cancel);
  if (reply == QMessageBox::Ok)
  {
    model_->resetAlignment();
  }
}

}  // namespace PJ::TimelinePrototype
