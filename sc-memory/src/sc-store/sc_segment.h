/*
-----------------------------------------------------------------------------
This source file is part of OSTIS (Open Semantic Technology for Intelligent Systems)
For the latest info, see http://www.ostis.net

Copyright (c) 2010-2014 OSTIS

OSTIS is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OSTIS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with OSTIS.  If not, see <http://www.gnu.org/licenses/>.
-----------------------------------------------------------------------------
*/

#ifndef _sc_segment_h_
#define _sc_segment_h_

#include "sc_types.h"
#include "sc_defines.h"
#include "sc_element.h"

#include <glib.h>

//! Structure to store segment locks
typedef struct _sc_segment_section
{
    const sc_memory_context *ctx_lock;    // pointer to context, that locked section
    const sc_memory_context *ctx;
} sc_segment_section;

/*! Structure for segment storing
 */
struct _sc_segment
{
    sc_element elements[SC_SEGMENT_ELEMENTS_COUNT];
    sc_bool has_empty_slots;
    sc_addr_seg num; // number of this segment in memory
    sc_segment_section sections[SC_CONCURRENCY_LEVEL];
    sc_uint32 elements_count;
};

/*! Create new segment with specified size.
 * @param num Number of created intance in sc-memory
 */
sc_segment* sc_segment_new(sc_addr_seg num);

void sc_segment_free(sc_segment *segment);


/*! Append element into segment at first empty position.
 * @param seg Pointer to segment, that will be contains element
 * @param el Pointer to sc-element data (will be just copied)
 * @param offset Offset of sc-element in segment.
 * @note sc-element with @p offset must to be locked!
 * @return Return pointer to created sc-element data. If element wasn't append into segment, then return 0.
 */
void sc_segment_set_element(sc_segment *seg, sc_element *el, sc_uint16 offset);

//! Remove element from specified segment. @note sc-element with @p offset need to bew locked
void sc_segment_erase_element(sc_segment *seg, sc_uint16 offset);

//! Returns number of stored sc-elements in segment
sc_uint32 sc_segment_get_elements_count(sc_segment *seg);

/*! Deletes garbage in specified segment
 * @param oldet_time_stamp Oldest timestamp, that can be used
 * @param seg Poitnet to segment to delete garbage
 * @returns Returns number of freed cells
 */
//sc_uint32 sc_segment_free_garbage(sc_segment *seg, sc_uint32 oldest_time_stamp);

/*! Check if segment has any empty slots
 * @param segment Pointer to segment for check
 * @returns If \p segment has any empty slots, then return SC_TRUE; otherwise return SC_FALSE
 */
sc_bool sc_segment_has_empty_slot(sc_segment *segment);


// ---------------------- locks --------------------------
/*! Function to lock any empty element
 * @param seg Pointer to segment where to lock empty element
 * @param offset Poitner to container for locked element offset
 * @returns Returns pointer to locked empty element. If there are no any empty element found,
 * then returns 0
 */
sc_element* sc_segment_lock_empty_element(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 *offset);

/*! Function to lock specified element in segment
 * @param seg Pointer to segment to lock element
 * @param offset Offset of element to lock
 * @returns Returns pointer to locked sc-element
 */
sc_element* sc_segment_lock_element(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 offset);

//! Try to lock sc-element with maximum attempts
sc_element* sc_segment_lock_element_try(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 offset, sc_uint16 max_attempts);

/*! Function to unlock specified element in segment
 * @param seg Pointer to segment for element unlocking
 * @param offset Offset of sc-element in segment
 */
void sc_segment_unlock_element(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 offset);

//! Locks segment section. This funciton doesn't returns control, while part wouldn't be locked.
void sc_segment_section_lock(const sc_memory_context *ctx, sc_segment_section *section);
/*! Try to lock segment section. If section already locked, then this function returns false; otherwise it locks section and returns true
 * @params section Pointer to segment section to lock
 * @param max_attempts Maximum number of lock attempts
 */
sc_bool sc_segment_section_lock_try(const sc_memory_context *ctx, sc_segment_section *section, sc_uint16 max_attempts);
//! Unlocks specified segment part
void sc_segment_section_unlock(const sc_memory_context *ctx, sc_segment_section *section);

#endif
