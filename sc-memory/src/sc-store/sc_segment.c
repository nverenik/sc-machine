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

#include "sc_segment.h"
#include "sc_element.h"
#include "sc_storage.h"
#include "../sc_memory_private.h"

#include <glib.h>

sc_segment* sc_segment_new(sc_addr_seg num)
{
#if USE_SEGMENT_EMPTY_SLOT_BUFFER
    sc_uint idx;
#endif

    sc_segment *segment = g_new0(sc_segment, 1);

    return segment;
}

void sc_segment_free(sc_segment *segment)
{
    g_assert( segment != 0);

    g_free(segment);
}

void sc_segment_set_element(sc_segment *seg, sc_element *el, sc_uint16 offset)
{
    sc_uint slot = 0;
    g_assert( seg != 0 );
    g_assert( el != 0 );

    g_assert(slot < SC_SEGMENT_ELEMENTS_COUNT);

    g_assert(g_atomic_pointer_get(&seg->sections[offset % SC_CONCURRENCY_LEVEL].ctx_lock) != 0);
    g_atomic_int_inc(&seg->elements_count);

    seg->elements[offset] = *el;
}

sc_element* sc_segment_get_element(sc_segment *seg, sc_uint16 offset)
{
    g_assert(offset < SC_SEGMENT_ELEMENTS_COUNT && seg != 0);
    return &(seg->elements[offset]);
}

void sc_segment_erase_element(sc_segment *seg, sc_uint16 offset)
{
    g_assert(g_atomic_pointer_get(&seg->sections[offset % SC_CONCURRENCY_LEVEL].ctx_lock) != 0);
    g_atomic_int_dec_and_test(&seg->elements_count);

    g_assert( seg != (sc_segment*)0 );
    g_assert( offset < SC_SEGMENT_ELEMENTS_COUNT );
    seg->elements[offset].flags.type = 0;
}

sc_uint32 sc_segment_get_elements_count(sc_segment *seg)
{
    g_assert(seg != nullptr);

    return g_atomic_int_get(&seg->elements_count);
}

/*sc_uint32 sc_segment_free_garbage(sc_segment *seg, sc_uint32 oldest_time_stamp)
{

    static sc_memory_context *ctx = (gpointer)0x1;
    sc_uint32 i;
    // lock all segment sections
    for (i = 0; i < SC_CONCURRENCY_LEVEL; ++i)
        sc_segment_section_lock(ctx, &seg->sections[i]);

    sc_uint32 free_count = 0, elements_count = 0;
    sc_uint32 idx = 0;
    sc_element *el = 0, *el2 = 0, *el_arc = 0, *next_el_arc = 0, *prev_el_arc = 0, *b_el = 0, *e_el = 0;
    sc_addr prev_arc, next_arc;
    sc_addr self_addr;


    self_addr.seg = seg->num;

    for (idx = 0; idx < SC_SEGMENT_ELEMENTS_COUNT; ++idx)
    {
        el = &(seg->elements[idx]);
        self_addr.offset = idx;

        if (el->flags.type != 0)
            ++elements_count;
        else
            continue;

        // skip element that wasn't deleted
        if (el->delete_time_stamp <= oldest_time_stamp && el->delete_time_stamp != 0)
        {
            // delete arcs from output and input lists
            // @todo two oriented lists support
            if (el->flags.type & sc_type_arc_mask)
            {
                prev_arc = el->arc.prev_out_arc;
                next_arc = el->arc.next_out_arc;

                if (SC_ADDR_IS_NOT_EMPTY(prev_arc))
                {
                    prev_el_arc = sc_storage_get_element(prev_arc, SC_TRUE);
                    prev_el_arc->arc.next_out_arc = next_arc;
                }

                if (SC_ADDR_IS_NOT_EMPTY(next_arc))
                {
                    next_el_arc = sc_storage_get_element(next_arc, SC_TRUE);
                    next_el_arc->arc.prev_out_arc = prev_arc;
                }

                b_el = sc_storage_get_element(el->arc.begin, SC_TRUE);
                if (SC_ADDR_IS_EQUAL(self_addr, b_el->first_out_arc))
                    b_el->first_out_arc = next_arc;

                prev_arc = el->arc.prev_in_arc;
                next_arc = el->arc.next_in_arc;

                if (SC_ADDR_IS_NOT_EMPTY(prev_arc))
                {
                    prev_el_arc = sc_storage_get_element(prev_arc, SC_TRUE);
                    prev_el_arc->arc.next_in_arc = next_arc;
                }

                if (SC_ADDR_IS_NOT_EMPTY(next_arc))
                {
                    next_el_arc = sc_storage_get_element(next_arc, SC_TRUE);
                    next_el_arc->arc.prev_in_arc = prev_arc;
                }

                e_el = sc_storage_get_element(el->arc.end, SC_TRUE);
                if (SC_ADDR_IS_EQUAL(self_addr, b_el->first_in_arc))
                    e_el->first_in_arc = next_arc;
            }

            free_count ++;
        }

    }

    g_atomic_int_set(&seg->elements_count, elements_count);

    // unlock all sections
    for (i = 0; i < SC_CONCURRENCY_LEVEL; ++i)
        sc_segment_section_unlock(ctx, &seg->sections[i]);

    return free_count;
}
*/

sc_bool sc_segment_has_empty_slot(sc_segment *segment)
{
    g_assert(segment != nullptr);
    return g_atomic_int_get(&segment->elements_count) < SC_SEGMENT_ELEMENTS_COUNT;
}

// ---------------------------
sc_element* sc_segment_lock_empty_element(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 *offset)
{
    sc_uint16 max_attempts = 1;
    while (sc_segment_has_empty_slot(seg) == SC_TRUE)
    {
        sc_uint32 i;
        for (i = 0; i < SC_CONCURRENCY_LEVEL; ++i)
        {
            sc_segment_section * section = &seg->sections[i];
            sc_bool locked = sc_segment_section_lock_try(ctx, section, max_attempts);

            if (locked)
            {
                // trying to find empty element in section
                *offset = i % SC_CONCURRENCY_LEVEL;
                while (*offset < SC_SEGMENT_ELEMENTS_COUNT)
                {
                    if (seg->elements[i].flags.type == 0)
                        return &seg->elements[*offset];

                    *offset += SC_CONCURRENCY_LEVEL;
                }
            }
        }
        if (max_attempts < 100)
            ++max_attempts;
    }

    return nullptr;
}

sc_element* sc_segment_lock_element(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 offset)
{
    g_assert(offset < SC_SEGMENT_ELEMENTS_COUNT && seg != nullptr);
    sc_segment_section *section = &seg->sections[offset % SC_CONCURRENCY_LEVEL];
    sc_segment_section_lock(ctx, section);
    return &seg->elements[offset];
}

sc_element* sc_segment_lock_element_try(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 offset, sc_uint16 max_attempts)
{
    g_assert(offset < SC_SEGMENT_ELEMENTS_COUNT && seg != nullptr);
    sc_segment_section *section = &seg->sections[offset % SC_CONCURRENCY_LEVEL];
    if (sc_segment_section_lock_try(ctx, section, max_attempts) == SC_TRUE)
        return &seg->elements[offset];

    return (sc_element*)0;
}

void sc_segment_unlock_element(const sc_memory_context *ctx, sc_segment *seg, sc_uint16 offset)
{
    g_assert(offset < SC_SEGMENT_ELEMENTS_COUNT && seg != nullptr);
    sc_segment_section *section = &seg->sections[offset % SC_CONCURRENCY_LEVEL];
    sc_segment_section_unlock(ctx, section);
}

void sc_segment_section_lock(const sc_memory_context *ctx, sc_segment_section *section)
{
    g_assert(section != nullptr);
    while (g_atomic_pointer_compare_and_exchange(&section->ctx_lock, 0, ctx) == FALSE || g_atomic_pointer_get(&section->ctx_lock) != ctx) {}
}

sc_bool sc_segment_section_lock_try(const sc_memory_context *ctx, sc_segment_section *section, sc_uint16 max_attempts)
{
    g_assert(section != nullptr);
    sc_uint16 attempt = 0;
    while (g_atomic_pointer_compare_and_exchange(&section->ctx_lock, 0, ctx) == FALSE || g_atomic_pointer_get(&section->ctx_lock) != ctx)
    {
        if (++attempt >= max_attempts)
            return SC_FALSE;
    }
    return SC_TRUE;
}

void sc_segment_section_unlock(const sc_memory_context *ctx, sc_segment_section *section)
{
    g_assert(section != nullptr);
    g_assert(g_atomic_pointer_get(&section->ctx_lock) == ctx);
    g_atomic_pointer_set(&section->ctx_lock, 0);
}
