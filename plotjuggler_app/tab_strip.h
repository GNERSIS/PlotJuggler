/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef TAB_STRIP_H
#define TAB_STRIP_H

#include <QStringList>
#include <QWidget>
#include <vector>

class QButtonGroup;
class QHBoxLayout;
class QPushButton;
class QScrollArea;
class QToolButton;

// Horizontal tab strip backed by plain QPushButtons (mutually exclusive via
// a QButtonGroup) inside a horizontal scroll area, with a trailing "+"
// button that is always visible. MainWindow uses this in the top bar to
// surface TabbedPlotWidget's tabs without the QTabWidget chrome.
class TabStrip : public QWidget
{
  Q_OBJECT
public:
  explicit TabStrip(QWidget* parent = nullptr);

  // Repopulate the strip from `names`, marking `current` as checked.
  void setTabs(const QStringList& names, int current);

  // Sync the checked-state without rebuilding.
  void setCurrentIndex(int index);

protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

signals:
  void tabSelected(int index);
  void tabCloseRequested(int index);
  void addTabRequested();

private:
  QScrollArea* scroll_ = nullptr;
  QWidget* row_ = nullptr;
  QHBoxLayout* row_layout_ = nullptr;
  QButtonGroup* group_ = nullptr;
  QToolButton* add_button_ = nullptr;
  // Each tab is a single QPushButton with a label + close QToolButton
  // installed in its child layout, so the button's hover/checked border
  // wraps the entire composite.
  std::vector<QPushButton*> buttons_;
};

#endif  // TAB_STRIP_H
