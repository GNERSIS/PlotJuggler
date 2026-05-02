/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "tab_strip.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>

TabStrip::TabStrip(QWidget* parent) : QWidget(parent)
{
  auto* outer = new QHBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  scroll_ = new QScrollArea(this);
  scroll_->setWidgetResizable(true);
  scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll_->setFrameShape(QFrame::NoFrame);

  row_ = new QWidget(scroll_);
  row_layout_ = new QHBoxLayout(row_);
  row_layout_->setContentsMargins(0, 0, 0, 0);
  row_layout_->setSpacing(2);
  // Trailing stretch keeps tabs left-aligned when they don't fill the row.
  // setTabs() inserts new buttons before this stretch (at index = count-1).
  row_layout_->addStretch(1);

  scroll_->setWidget(row_);
  outer->addWidget(scroll_, 1);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);

  add_button_ = new QToolButton(this);
  add_button_->setText("+");
  add_button_->setAutoRaise(true);
  add_button_->setToolTip("Add tab");
  connect(add_button_, &QToolButton::clicked, this, &TabStrip::addTabRequested);
  outer->addWidget(add_button_);
}

void TabStrip::setTabs(const QStringList& names, int current)
{
  // The tab itself is the QPushButton — the label and close button live
  // inside its child layout. That way the button's :hover and :checked
  // borders (defined in the global stylesheet) wrap the whole composite
  // rather than just the label half.
  for (QPushButton* b : buttons_)
  {
    group_->removeButton(b);
    row_layout_->removeWidget(b);
    b->deleteLater();
  }
  buttons_.clear();

  for (int i = 0; i < names.size(); ++i)
  {
    auto* btn = new QPushButton(row_);
    btn->setCheckable(true);
    btn->setChecked(i == current);
    btn->setFlat(true);
    btn->setFocusPolicy(Qt::NoFocus);

    auto* hl = new QHBoxLayout(btn);
    // 2 px top/bottom margin gives the :checked border its own gutter,
    // otherwise the content sits flush with the button rect and the border
    // gets drawn over (or clipped by) the label/close button.
    hl->setContentsMargins(6, 2, 2, 2);
    hl->setSpacing(4);

    auto* label = new QLabel(names.at(i), btn);
    // Let the parent QPushButton see the click and the :hover state — the
    // QLabel intercepts mouse events otherwise.
    label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hl->addWidget(label);

    auto* close_btn = new QToolButton(btn);
    close_btn->setIcon(QIcon(":/resources/svg/close-button.svg"));
    close_btn->setIconSize(QSize(12, 12));
    close_btn->setAutoRaise(true);
    close_btn->setFocusPolicy(Qt::NoFocus);
    close_btn->setFixedSize(16, 16);
    close_btn->setToolTip("Close tab");
    // Override the global QToolButton hover/checked border (defined in the
    // app stylesheet). The hover signal here is "icon swap" instead.
    close_btn->setStyleSheet("QToolButton, QToolButton:hover, QToolButton:pressed,"
                             " QToolButton:checked, QToolButton:checked:hover {"
                             " border: none; background: transparent; }");
    // Watch hover-enter/leave so the icon can swap to its filled variant.
    close_btn->installEventFilter(this);
    hl->addWidget(close_btn);

    // Lock the button to its sizeHint vertically, otherwise QHBoxLayout
    // stretches it to the row's full height and the :checked border ends
    // up flush with the row edge (clipped on the bottom).
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    group_->addButton(btn, i);
    row_layout_->insertWidget(i, btn, 0, Qt::AlignVCenter);

    connect(btn, &QPushButton::clicked, this, [this, i]() { emit tabSelected(i); });
    connect(close_btn, &QToolButton::clicked, this, [this, i]() { emit tabCloseRequested(i); });

    buttons_.push_back(btn);
  }
}

bool TabStrip::eventFilter(QObject* obj, QEvent* event)
{
  // Swap the close-button icon between outline (idle) and filled (hover)
  // variants. Installed only on per-tab close buttons in setTabs.
  if (auto* tb = qobject_cast<QToolButton*>(obj))
  {
    if (event->type() == QEvent::Enter)
    {
      tb->setIcon(QIcon(":/resources/svg/close_filled.svg"));
    }
    else if (event->type() == QEvent::Leave)
    {
      tb->setIcon(QIcon(":/resources/svg/close-button.svg"));
    }
  }
  return QWidget::eventFilter(obj, event);
}

void TabStrip::setCurrentIndex(int index)
{
  for (size_t i = 0; i < buttons_.size(); ++i)
  {
    if (buttons_[i]->isChecked() != (static_cast<int>(i) == index))
    {
      buttons_[i]->setChecked(static_cast<int>(i) == index);
    }
  }
}
