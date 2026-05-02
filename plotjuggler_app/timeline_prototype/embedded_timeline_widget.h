/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_EMBEDDED_TIMELINE_WIDGET_H
#define PJ_TIMELINE_PROTOTYPE_EMBEDDED_TIMELINE_WIDGET_H

#include "controller/playback_controller.h"
#include "model/timeline_model.h"

#include <QStringList>
#include <QWidget>
#include <memory>

namespace PJ::TimelinePrototype
{

class TimelineSceneWidget;

// Embeddable variant of TimelineWindow. The standalone prototype keeps using
// TimelineWindow (a QMainWindow with menu/status bar); the main PlotJuggler
// app uses this so the timeline can drop into an existing layout. The LHS
// sequence table was removed in favor of accepting drops from PlotJuggler's
// own curve tree directly into the scene.
class EmbeddedTimelineWidget : public QWidget
{
  Q_OBJECT
public:
  explicit EmbeddedTimelineWidget(QWidget* parent = nullptr);
  ~EmbeddedTimelineWidget() override;

  TimelineModel* model()
  {
    return model_.get();
  }
  PlaybackController* controller()
  {
    return controller_.get();
  }

signals:
  // Re-emitted from the inner scene widget. The host owns the timeseries data
  // and is responsible for resolving curve names → time ranges and feeding
  // them back via the model.
  void curvesDropped(const QStringList& curve_names);

private:
  std::unique_ptr<TimelineModel> model_;
  std::unique_ptr<PlaybackController> controller_;
  TimelineSceneWidget* scene_widget_ = nullptr;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_EMBEDDED_TIMELINE_WIDGET_H
