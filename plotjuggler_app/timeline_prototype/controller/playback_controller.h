/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_PLAYBACK_CONTROLLER_H
#define PJ_TIMELINE_PROTOTYPE_PLAYBACK_CONTROLLER_H

#include "model/timeline_model.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace PJ::TimelinePrototype
{

class PlaybackController : public QObject
{
  Q_OBJECT
public:
  explicit PlaybackController(TimelineModel* model, QObject* parent = nullptr);

  bool playing() const
  {
    return timer_.isActive();
  }
  bool loop() const
  {
    return loop_;
  }
  double speed() const
  {
    return speed_;
  }

public slots:
  void play();
  void pause();
  void togglePlay();
  void setLoop(bool on)
  {
    loop_ = on;
  }
  void setSpeed(double s)
  {
    speed_ = s;
  }

signals:
  void playingChanged(bool playing);

private slots:
  void onTick();

private:
  TimelineModel* model_;
  QTimer timer_;
  QElapsedTimer wall_;
  bool loop_ = false;
  double speed_ = 1.0;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_PLAYBACK_CONTROLLER_H
