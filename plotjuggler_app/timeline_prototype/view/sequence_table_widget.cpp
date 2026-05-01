/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/sequence_table_widget.h"

#include <QAbstractTableModel>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

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

  Qt::ItemFlags flags(const QModelIndex& idx) const override
  {
    Qt::ItemFlags f = QAbstractTableModel::flags(idx);
    if (idx.isValid() && idx.column() == kColOffsetMs)
    {
      f |= Qt::ItemIsEditable;
    }
    return f;
  }

  bool setData(const QModelIndex& idx, const QVariant& value, int role) override
  {
    if (!idx.isValid() || role != Qt::EditRole)
    {
      return false;
    }
    if (idx.column() != kColOffsetMs)
    {
      return false;
    }
    bool ok = false;
    double ms = value.toString().remove(" *").toDouble(&ok);
    if (!ok || !std::isfinite(ms))
    {
      return false;
    }
    // Guard the cast — static_cast<qint64> of an out-of-range double is UB
    // per C++17 [conv.fpint]. Reject silently; the user re-types.
    constexpr double kMaxMs = static_cast<double>(std::numeric_limits<qint64>::max()) / 1e6;
    if (std::abs(ms) >= kMaxMs)
    {
      return false;
    }
    qint64 ns = static_cast<qint64>(ms * 1e6);
    model_->setSequenceOffset(idx.row(), ns);
    return true;
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
  table_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(table_, &QTableView::customContextMenuRequested, this, [this](const QPoint& pos) {
    QModelIndex idx = table_->indexAt(pos);
    if (!idx.isValid())
    {
      return;
    }
    QMenu menu(this);
    QAction* set_lead = menu.addAction("Set as Leading");
    QAction* clear_lead = menu.addAction("Clear Leading");
    menu.addSeparator();
    QAction* a_none = menu.addAction("Align: \xe2\x80\x94");
    QAction* a_start = menu.addAction("Align: Start");
    QAction* a_finish = menu.addAction("Align: Finish");
    QAction* a_middle = menu.addAction("Align: Middle");
    QAction* picked = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (picked == set_lead)
    {
      model_->setLeading(idx.row());
    }
    else if (picked == clear_lead)
    {
      model_->setLeading(-1);
    }
    else if (picked == a_none)
    {
      model_->setAlignment(AlignmentMode::None);
    }
    else if (picked == a_start)
    {
      model_->setAlignment(AlignmentMode::Start);
    }
    else if (picked == a_finish)
    {
      model_->setAlignment(AlignmentMode::Finish);
    }
    else if (picked == a_middle)
    {
      model_->setAlignment(AlignmentMode::Middle);
    }
  });
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
