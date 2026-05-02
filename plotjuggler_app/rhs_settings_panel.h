/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RHS_SETTINGS_PANEL_H
#define RHS_SETTINGS_PANEL_H

#include <QList>
#include <QWidget>

// Right-hand-side settings column. Currently a skeleton — header label, a
// plot-controls grid (the buttons that used to live in a standalone vertical
// column on the right of the plot area), and a content layout reserved for
// future editors. Wiring of any specific editor (curve style, color, etc.)
// is intentionally not included.
class QGridLayout;
class QLabel;
class QVBoxLayout;

class RhsSettingsPanel : public QWidget
{
  Q_OBJECT
public:
  explicit RhsSettingsPanel(QWidget* parent = nullptr);
  ~RhsSettingsPanel() override = default;

  // Slot for the host to update the title (e.g. selected plot name). Left as
  // a no-op-friendly setter — wiring it up to a selection signal is out of
  // scope here.
  void setSubject(const QString& name);

  // Reparents the supplied widgets into a single-column stack at the top of
  // the panel. Each widget keeps its existing object name and signal
  // connections — only the parent and layout slot change.
  void installPlotControls(const QList<QWidget*>& controls);

  // Layout that future editors can be installed into.
  QVBoxLayout* contentLayout()
  {
    return content_layout_;
  }

private:
  QLabel* subject_label_ = nullptr;
  QVBoxLayout* content_layout_ = nullptr;
  QVBoxLayout* controls_layout_ = nullptr;
};

#endif  // RHS_SETTINGS_PANEL_H
