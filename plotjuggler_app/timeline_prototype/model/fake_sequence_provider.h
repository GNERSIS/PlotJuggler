/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PJ_TIMELINE_PROTOTYPE_FAKE_SEQUENCE_PROVIDER_H
#define PJ_TIMELINE_PROTOTYPE_FAKE_SEQUENCE_PROVIDER_H

#include "model/sequence.h"

namespace PJ::TimelinePrototype
{

class FakeSequenceProvider
{
public:
  // Returns 4 hard-coded fixtures chosen to exercise alignment, zoom,
  // and selection. See spec §7 for the table.
  static std::vector<Sequence> generate();
};

}  // namespace PJ::TimelinePrototype

#endif  // PJ_TIMELINE_PROTOTYPE_FAKE_SEQUENCE_PROVIDER_H
