/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/playback_toolbar.h"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>

namespace PJ::TimelinePrototype
{

PlaybackToolbar::PlaybackToolbar(TimelineModel* model, PlaybackController* controller,
                                 QWidget* parent)
  : QToolBar("Playback", parent), model_(model), controller_(controller)
{
  setMovable(false);

  play_action_ = addAction(style()->standardIcon(QStyle::SP_MediaPlay), "Play");
  play_action_->setCheckable(true);
  connect(play_action_, &QAction::triggered, controller_, &PlaybackController::togglePlay);

  loop_action_ = addAction("Loop");
  loop_action_->setCheckable(true);
  connect(loop_action_, &QAction::toggled, controller_, &PlaybackController::setLoop);

  addSeparator();
  addWidget(new QLabel("Speed:"));
  speed_spin_ = new QDoubleSpinBox(this);
  speed_spin_->setRange(0.1, 10.0);
  speed_spin_->setSingleStep(0.1);
  speed_spin_->setValue(1.0);
  connect(speed_spin_, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          controller_, &PlaybackController::setSpeed);
  addWidget(speed_spin_);

  addSeparator();
  addWidget(new QLabel("Align:"));
  alignment_combo_ = new QComboBox(this);
  alignment_combo_->addItem("—", static_cast<int>(AlignmentMode::None));
  alignment_combo_->addItem("Start", static_cast<int>(AlignmentMode::Start));
  alignment_combo_->addItem("Finish", static_cast<int>(AlignmentMode::Finish));
  alignment_combo_->addItem("Middle", static_cast<int>(AlignmentMode::Middle));
  connect(alignment_combo_, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &PlaybackToolbar::onAlignmentComboChanged);
  addWidget(alignment_combo_);

  auto* reset_btn = new QPushButton("Reset Alignment", this);
  connect(reset_btn, &QPushButton::clicked, this, &PlaybackToolbar::onResetAlignmentClicked);
  addWidget(reset_btn);

  connect(controller_, &PlaybackController::playingChanged, this,
          &PlaybackToolbar::onPlayingChanged);
  connect(model_, &TimelineModel::alignmentChanged, this, [this](AlignmentMode m) {
    const int idx = alignment_combo_->findData(static_cast<int>(m));
    if (idx >= 0 && idx != alignment_combo_->currentIndex())
    {
      const QSignalBlocker blocker(alignment_combo_);
      alignment_combo_->setCurrentIndex(idx);
    }
  });
}

void PlaybackToolbar::onPlayingChanged(bool playing)
{
  play_action_->setChecked(playing);
  play_action_->setIcon(
      style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
  play_action_->setText(playing ? "Pause" : "Play");
}

void PlaybackToolbar::onAlignmentComboChanged(int idx)
{
  const int v = alignment_combo_->itemData(idx).toInt();
  model_->setAlignment(static_cast<AlignmentMode>(v));
}

void PlaybackToolbar::onResetAlignmentClicked()
{
  // Count overrides for the prompt.
  int n = 0;
  for (const Sequence& s : model_->sequences())
  {
    if (s.seq_offset_overridden)
    {
      ++n;
    }
    for (const Topic& t : s.topics)
    {
      if (t.topic_offset_overridden)
      {
        ++n;
      }
    }
  }
  if (n == 0)
  {
    model_->resetAlignment();
    return;
  }
  auto reply = QMessageBox::question(
      this, "Reset Alignment", QString("Reset %1 manual offset(s) and re-apply alignment?").arg(n),
      QMessageBox::Ok | QMessageBox::Cancel);
  if (reply == QMessageBox::Ok)
  {
    model_->resetAlignment();
  }
}

}  // namespace PJ::TimelinePrototype
