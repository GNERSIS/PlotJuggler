/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/sequence_table_widget.h"

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

namespace PJ::TimelinePrototype
{

namespace
{
constexpr int kColColor = 0;
constexpr int kColName = 1;
constexpr int kColLead = 2;
constexpr int kColAlignment = 3;
constexpr int kColOffsetMs = 4;
constexpr int kColCount = 5;

QString modeText(AlignmentMode m)
{
  switch (m)
  {
    case AlignmentMode::None:
      return "—";
    case AlignmentMode::Start:
      return "Start";
    case AlignmentMode::Finish:
      return "Finish";
    case AlignmentMode::Middle:
      return "Middle";
  }
  return "?";
}
}  // namespace

class SequenceTableQtModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  explicit SequenceTableQtModel(TimelineModel* m, QObject* parent = nullptr)
    : QAbstractTableModel(parent), model_(m)
  {
  }

  int rowCount(const QModelIndex& = {}) const override
  {
    return static_cast<int>(model_->sequences().size());
  }
  int columnCount(const QModelIndex& = {}) const override
  {
    return kColCount;
  }

  QVariant headerData(int section, Qt::Orientation orient, int role) const override
  {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole)
    {
      return {};
    }
    switch (section)
    {
      case kColColor:
        return "";
      case kColName:
        return "Name";
      case kColLead:
        return "Lead";
      case kColAlignment:
        return "Align";
      case kColOffsetMs:
        return "δ (ms)";
    }
    return {};
  }

  QVariant data(const QModelIndex& idx, int role) const override
  {
    if (!idx.isValid())
    {
      return {};
    }
    const Sequence& s = model_->sequences()[idx.row()];
    if (role == Qt::DisplayRole)
    {
      switch (idx.column())
      {
        case kColName:
          return s.seq_offset_overridden ? s.name + " *" : s.name;
        case kColLead:
          return (model_->leading() == idx.row()) ? "●" : "";
        case kColAlignment:
          return modeText(model_->alignment());
        case kColOffsetMs: {
          QString val = QString::number(static_cast<double>(s.display_offset_ns) / 1e6, 'f', 1);
          if (s.seq_offset_overridden)
          {
            val += " *";
          }
          return val;
        }
        default:
          return {};
      }
    }
    if (role == Qt::BackgroundRole && idx.column() == kColColor)
    {
      return s.color;
    }
    return {};
  }

  void refresh()
  {
    beginResetModel();
    endResetModel();
  }

private:
  TimelineModel* model_;
};

SequenceTableWidget::SequenceTableWidget(TimelineModel* model, QWidget* parent)
  : QWidget(parent), model_(model)
{
  qt_model_ = new SequenceTableQtModel(model_, this);
  table_ = new QTableView(this);
  table_->setModel(qt_model_);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  table_->verticalHeader()->setVisible(false);
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setColumnWidth(kColColor, 18);
  table_->setColumnWidth(kColName, 180);
  table_->setColumnWidth(kColLead, 40);
  table_->setColumnWidth(kColAlignment, 60);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(table_);

  connect(model_, &TimelineModel::sequencesChanged, this, &SequenceTableWidget::onSequencesChanged);
  connect(model_, &TimelineModel::offsetsChanged, this, &SequenceTableWidget::onOffsetsChanged);
  connect(model_, &TimelineModel::leadingChanged, this, [this](int) { qt_model_->refresh(); });
  connect(model_, &TimelineModel::alignmentChanged, this,
          [this](AlignmentMode) { qt_model_->refresh(); });
  connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &SequenceTableWidget::onTableSelectionChanged);

  // Double-click on Lead column to set leading.
  connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex& i) {
    if (i.column() == kColLead)
    {
      const int new_lead = (model_->leading() == i.row()) ? -1 : i.row();
      model_->setLeading(new_lead);
    }
  });
}

void SequenceTableWidget::onSequencesChanged()
{
  qt_model_->refresh();
}

void SequenceTableWidget::onOffsetsChanged(int /*seq_idx*/)
{
  qt_model_->refresh();
}

void SequenceTableWidget::onTableSelectionChanged()
{
  std::set<int> sel;
  const auto rows = table_->selectionModel()->selectedRows();
  for (const QModelIndex& r : rows)
  {
    sel.insert(r.row());
  }
  model_->setSelection(std::move(sel));
}

}  // namespace PJ::TimelinePrototype

#include "sequence_table_widget.moc"
