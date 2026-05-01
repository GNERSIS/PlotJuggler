/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "controller/playback_controller.h"

#include <tuple>

namespace PJ::TimelinePrototype
{

PlaybackController::PlaybackController(TimelineModel* model, QObject* parent)
  : QObject(parent), model_(model)
{
  timer_.setInterval(16);  // ~60 Hz
  connect(&timer_, &QTimer::timeout, this, &PlaybackController::onTick);
}

void PlaybackController::play()
{
  if (timer_.isActive())
  {
    return;
  }
  wall_.start();
  timer_.start();
  emit playingChanged(true);
}

void PlaybackController::pause()
{
  if (!timer_.isActive())
  {
    return;
  }
  timer_.stop();
  emit playingChanged(false);
}

void PlaybackController::togglePlay()
{
  timer_.isActive() ? pause() : play();
}

void PlaybackController::onTick()
{
  const qint64 elapsed_ms = wall_.restart();
  // 1 ms wall-time = 1e6 ns playback-time × speed.
  const qint64 dt_ns = static_cast<qint64>(elapsed_ms * 1'000'000.0 * speed_);
  qint64 next = model_->playhead() + dt_ns;

  auto [ws, we] = model_->workRange();
  // If work range collapses to nothing, fall back to scene extent.
  if (ws >= we)
  {
    std::tie(ws, we) = model_->sceneExtent();
  }

  if (next > we)
  {
    if (loop_)
    {
      next = ws + (next - we);
    }
    else
    {
      next = we;
      pause();
    }
  }
  model_->setPlayhead(next);
}

}  // namespace PJ::TimelinePrototype
