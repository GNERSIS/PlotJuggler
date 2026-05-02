/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rhs_settings_panel.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

RhsSettingsPanel::RhsSettingsPanel(QWidget* parent) : QWidget(parent)
{
  setMinimumWidth(220);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(4);

  auto* title = new QLabel(tr("Settings"), this);
  QFont f = title->font();
  f.setBold(true);
  title->setFont(f);
  root->addWidget(title);

  subject_label_ = new QLabel(tr("No selection"), this);
  subject_label_->setWordWrap(true);
  root->addWidget(subject_label_);

  auto* controls_container = new QWidget(this);
  controls_layout_ = new QVBoxLayout(controls_container);
  controls_layout_->setContentsMargins(0, 0, 0, 0);
  controls_layout_->setSpacing(2);
  root->addWidget(controls_container, 0, Qt::AlignLeft);

  auto* line = new QFrame(this);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  root->addWidget(line);

  auto* container = new QWidget(this);
  content_layout_ = new QVBoxLayout(container);
  content_layout_->setContentsMargins(0, 0, 0, 0);
  root->addWidget(container, 1);

  root->addStretch(1);
}

void RhsSettingsPanel::installPlotControls(const QList<QWidget*>& controls)
{
  if (!controls_layout_)
  {
    return;
  }
  for (QWidget* w : controls)
  {
    if (!w)
    {
      continue;
    }
    w->setParent(controls_layout_->parentWidget());
    controls_layout_->addWidget(w);
  }
}

void RhsSettingsPanel::setSubject(const QString& name)
{
  if (subject_label_)
  {
    subject_label_->setText(name.isEmpty() ? tr("No selection") : name);
  }
}
