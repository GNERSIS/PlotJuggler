/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_SEQUENCE_TABLE_WIDGET_H
#define PJ_TIMELINE_PROTOTYPE_SEQUENCE_TABLE_WIDGET_H

#include "model/timeline_model.h"

#include <QAbstractTableModel>
#include <QWidget>

class QTableView;

namespace PJ::TimelinePrototype
{

class SequenceTableQtModel;  // private impl in .cpp

class SequenceTableWidget : public QWidget
{
  Q_OBJECT
public:
  explicit SequenceTableWidget(TimelineModel* model, QWidget* parent = nullptr);

private slots:
  void onSequencesChanged();
  void onOffsetsChanged(int seq_idx);
  void onTableSelectionChanged();

private:
  TimelineModel* model_;
  QTableView* table_;
  SequenceTableQtModel* qt_model_;
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_SEQUENCE_TABLE_WIDGET_H
