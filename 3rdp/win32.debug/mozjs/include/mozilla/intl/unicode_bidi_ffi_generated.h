/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.29.1 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen. See RunCbindgen.py */


#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

namespace mozilla {
namespace intl {
namespace ffi {

/// Bidi object to be exposed to Gecko via FFI.
struct UnicodeBidi;

/// Embedding Level
///
/// Embedding Levels are numbers between 0 and 126 (inclusive), where even values denote a
/// left-to-right (LTR) direction and odd values a right-to-left (RTL) direction.
///
/// This struct maintains a *valid* status for level numbers, meaning that creating a new level, or
/// mutating an existing level, with the value smaller than `0` (before conversion to `u8`) or
/// larger than 125 results in an `Error`.
///
/// <http://www.unicode.org/reports/tr9/#BD2>
using Level = uint8_t;

/// LevelRun type to be returned to C++.
/// 32-bit indexes (rather than usize) are sufficient here because Gecko works
/// with 32-bit indexes when collecting the text buffer for a paragraph.
struct LevelRun {
  uint32_t start;
  uint32_t length;
  uint8_t level;
};

extern "C" {

/// Create a new UnicodeBidi object for the given text.
/// NOTE that the text buffer must remain valid for the lifetime of this object!
UnicodeBidi *bidi_new(const uint16_t *text, uintptr_t length, uint8_t level);

/// Destroy the Bidi object.
void bidi_destroy(UnicodeBidi *bidi);

/// Get the length of the text covered by the Bidi object.
int32_t bidi_get_length(const UnicodeBidi *bidi);

/// Get the paragraph direction: LTR=1, RTL=-1, mixed=0.
int8_t bidi_get_direction(const UnicodeBidi *bidi);

/// Get the paragraph level.
uint8_t bidi_get_paragraph_level(const UnicodeBidi *bidi);

/// Get the number of runs present.
int32_t bidi_count_runs(UnicodeBidi *bidi);

/// Get a pointer to the Levels array. The resulting pointer is valid only as long as
/// the UnicodeBidi object exists!
const Level *bidi_get_levels(UnicodeBidi *bidi);

/// Get the extent of the run at the given index in the visual runs array.
/// This would panic!() if run_index is out of range (see bidi_count_runs),
/// or if the run's start or length exceeds u32::MAX (which cannot happen
/// because Gecko can't create such a huge text buffer).
LevelRun bidi_get_visual_run(UnicodeBidi *bidi, uint32_t run_index);

/// Return index map showing the result of reordering using the given levels array.
/// (This is a generic helper that does not use a UnicodeBidi object, it just takes an
/// arbitrary array of levels.)
void bidi_reorder_visual(const uint8_t *levels, uintptr_t length, int32_t *index_map);

/// Get the base direction for the given text, returning 1 for LTR, -1 for RTL,
/// and 0 for neutral. If first_paragraph is true, only the first paragraph will be considered;
/// if false, subsequent paragraphs may be considered until a non-neutral character is found.
int8_t bidi_get_base_direction(const uint16_t *text, uintptr_t length, bool first_paragraph);

}  // extern "C"

}  // namespace ffi
}  // namespace intl
}  // namespace mozilla
