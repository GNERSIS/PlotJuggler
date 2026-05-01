/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "view/topic_item.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace PJ::TimelinePrototype
{

TopicItem::TopicItem(int seq_idx, int topic_idx, const QString& label, const QColor& fill,
                     bool topic_overridden, bool seq_overridden)
  : seq_idx_(seq_idx)
  , topic_idx_(topic_idx)
  , label_(label)
  , fill_(fill)
  , topic_overridden_(topic_overridden)
  , seq_overridden_(seq_overridden)
{
  // ItemIsSelectable lets QGraphicsScene track per-item selection so
  // paint() can flip the border on State_Selected. The actual sync between
  // scene selection and the model's LHS-table selection is currently
  // dormant — Task 10's mouse handlers swallow the press, and a future
  // task will need to either restore the click→select fallthrough or wire
  // selection through the model directly.
  setFlag(ItemIsSelectable, true);
  setAcceptHoverEvents(true);
  setCursor(Qt::OpenHandCursor);
  setZValue(10);
}

void TopicItem::paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget*)
{
  QRectF r = rect().translated(ghost_dx_px_, 0);

  QColor c = fill_;
  c.setAlphaF(qFuzzyIsNull(ghost_dx_px_) ? 0.8 : 0.5);
  p->setBrush(c);

  QPen border((opt->state & QStyle::State_Selected) ? Qt::white : QColor(20, 20, 20));
  border.setWidth((opt->state & QStyle::State_Selected) ? 2 : 1);
  p->setPen(border);
  p->drawRect(r);

  QString text = label_;
  if (topic_overridden_)
  {
    text += " *";
  }
  QFontMetricsF fm(p->font());
  text = fm.elidedText(text, Qt::ElideRight, r.width() - 6);
  p->setPen(Qt::white);
  p->drawText(r.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}

}  // namespace PJ::TimelinePrototype
