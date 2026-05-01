/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_TIMELINE_WINDOW_H
#define PJ_TIMELINE_PROTOTYPE_TIMELINE_WINDOW_H

#include "controller/playback_controller.h"
#include "model/timeline_model.h"

#include <QMainWindow>
#include <memory>

class QSplitter;

namespace PJ::TimelinePrototype
{

class TimelineWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit TimelineWindow(QWidget* parent = nullptr);
  ~TimelineWindow() override;

  TimelineModel* model()
  {
    return model_.get();
  }

private:
  std::unique_ptr<TimelineModel> model_;
  std::unique_ptr<PlaybackController> controller_;
  QSplitter* splitter_ = nullptr;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_TIMELINE_WINDOW_H
