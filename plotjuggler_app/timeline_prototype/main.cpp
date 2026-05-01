/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <QApplication>
#include <QLabel>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QLabel placeholder("timeline_prototype — empty window placeholder");
  placeholder.setMinimumSize(800, 400);
  placeholder.show();
  return app.exec();
}
