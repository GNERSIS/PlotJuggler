/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "model/fake_sequence_provider.h"
#include "timeline_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  PJ::TimelinePrototype::TimelineWindow window;
  window.model()->setSequences(PJ::TimelinePrototype::FakeSequenceProvider::generate());
  auto [ext_lo, ext_hi] = window.model()->sceneExtent();
  window.model()->setWorkRange(ext_lo, ext_hi);
  window.show();
  return app.exec();
}
