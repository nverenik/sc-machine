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

#include "sc_storage.h"

#include "sc_defines.h"
#include "sc_segment.h"
#include "sc_element.h"
#include "sc_fs_storage.h"
#include "sc_link_helpers.h"
#include "sc_event.h"
#include "sc_config.h"
#include "sc_iterator.h"

#include "sc_event/sc_event_private.h"

#include <memory.h>
#include <glib.h>

// segments array
sc_segment **segments = 0;
// number of segments
sc_uint32 segments_num = 0;

sc_uint32 storage_time_stamp = 1;
sc_bool is_initialized = SC_FALSE;

const sc_uint16 s_max_lock_attempts = 100;


// -----------------------------------------------------------------------------

/* Updates segment information:
 * - Calculate number of stored sc-elements
 * - Free unused cells in segments
 */
//void sc_storage_update_segments()
//{
//    sc_uint32 idx = 0;
//    sc_uint32 elements_count = 0;
//    sc_uint32 element_free_count = 0;
//    sc_uint32 oldest_time_stamp = 0;
//    sc_segment *seg = 0;

//    oldest_time_stamp = sc_iterator_get_oldest_timestamp();
//    if (oldest_time_stamp == 0)
//        oldest_time_stamp = sc_storage_get_time_stamp();

//    for (idx = 0; idx < segments_num; ++idx)
//    {
//        seg = segments[idx];
//        if (seg == 0)
//            continue; // @todo segments load

//        // @todo oldest timestamp
//        sc_segment_free_garbage(seg, oldest_time_stamp);
//    }
//}

// -----------------------------------------------------------------------------


sc_bool sc_storage_initialize(const char *path, sc_bool clear)
{
    g_assert( segments == (sc_segment**)0 );
    g_assert( !is_initialized );

    segments = g_new0(sc_segment*, SC_ADDR_SEG_MAX);
    _sc_storage_segment_cache_init();

    sc_bool res = sc_fs_storage_initialize(path, clear);
    if (res == SC_FALSE)
        return SC_FALSE;

    if (clear == SC_FALSE)
        sc_fs_storage_read_from_path(segments, &segments_num);

    storage_time_stamp = 1;

    is_initialized = SC_TRUE;
    sc_storage_update_segments();

    return SC_TRUE;
}

void sc_storage_shutdown()
{
    sc_uint idx = 0;
    g_assert( segments != (sc_segment**)0 );


    sc_fs_storage_shutdown(segments);

    for (idx = 0; idx < SC_ADDR_SEG_MAX; idx++)
    {
        if (segments[idx] == nullptr) continue; // skip segments, that are not loaded
        g_free(segments[idx]);
    }

    _sc_storage_segment_cache_destroy();

    g_free(segments);
    segments = (sc_segment**)0;

    is_initialized = SC_FALSE;
}

sc_bool sc_storage_is_initialized()
{
    return is_initialized;
}

sc_bool sc_storage_is_element(const sc_memory_context *ctx, sc_addr addr)
{
    sc_bool res = SC_TRUE;

    sc_element *el = sc_storage_element_lock(ctx, addr);

    if (el == 0) return SC_FALSE;

    if (el->flags.type == 0)
        res = SC_FALSE;

    if (el->delete_time_stamp > 0)
        res = SC_FALSE;

    sc_storage_element_unlock(ctx, addr);

    return res;
}

sc_element* sc_storage_append_el_into_segments(const sc_memory_context *ctx, sc_element *element, sc_addr *addr)
{
    sc_segment *segment = 0;

    g_assert( addr != 0 );
    SC_ADDR_MAKE_EMPTY(*addr);

    if (sc_iterator_has_any_timestamp())
        g_atomic_int_inc(&storage_time_stamp);

    if (g_atomic_int_get(&segments_num) >= sc_config_get_max_loaded_segments())
        return nullptr;

    // try to find segment with empty slots
    sc_uint32 i;
    for (i = 0; i < g_atomic_int_get(&segments_num); ++i)
    {
        sc_segment *seg = g_atomic_pointer_get(&segments[i]);

        if (seg == nullptr)
            continue;

        sc_element *el = sc_segment_lock_empty_element(ctx, seg, &addr->offset);
        if (el != nullptr)
        {
            addr->seg = i;
            return el;
        }
    }

    while (SC_TRUE)
    {
        sc_uint32 seg_num = g_atomic_int_get(&segments_num);
        if (seg_num >= SC_ADDR_SEG_MAX)
            return nullptr;

        while (g_atomic_int_compare_and_exchange(&segments_num, seg_num, seg_num + 1) == FALSE) {}

        // if element still not added, then create new segment and append element into it
        segment = sc_segment_new(seg_num);
        addr->seg = seg_num;
        segments[seg_num] = segment;

        sc_element *el = sc_segment_lock_empty_element(ctx, segment, &addr->offset);
        if (el != nullptr)
            return el;
    }

    return nullptr;
}

sc_addr sc_storage_element_new(const sc_memory_context *ctx, sc_type type)
{
    sc_element el;
    sc_addr addr;
    sc_element *res = 0;

    memset(&el, 0, sizeof(el));
    el.flags.type = type;
    el.create_time_stamp = storage_time_stamp;

    res = sc_storage_append_el_into_segments(ctx, &el, &addr);
    sc_storage_element_unlock(ctx, addr);
    g_assert(res != 0);
    return addr;
}

sc_result sc_storage_element_free(const sc_memory_context *ctx, sc_addr addr)
{
    GHashTable *lock_table = 0;
    GSList *remove_list = 0;

    // first of all we need to collect and lock all elements
    sc_element *el = sc_storage_element_lock(ctx, addr);
    if (el == nullptr || el->flags.type == 0)
        return SC_RESULT_ERROR;

    lock_table = g_hash_table_new(g_int_hash, g_int_equal);
    g_hash_table_insert(lock_table, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(addr)), el);

    remove_list = g_slist_append(remove_list, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(addr)));
    while (remove_list != 0)
    {
        // get sc-addr for removing
        sc_uint32 addr_int = GPOINTER_TO_UINT(remove_list->data);
        sc_addr _addr;
        _addr.seg = SC_ADDR_LOCAL_SEG_FROM_INT(addr_int);
        _addr.offset = SC_ADDR_LOCAL_OFFSET_FROM_INT(addr_int);

        // go to next sc-addr in list
        remove_list = g_slist_delete_link(remove_list, remove_list);

        el = sc_storage_element_lock(ctx, _addr);

        g_assert(el != 0 && el->flags.type != 0);
        g_hash_table_insert(lock_table, GUINT_TO_POINTER(addr_int), el);

        // remove registered events before deletion
        sc_event_notify_element_deleted(_addr);

        el->delete_time_stamp = storage_time_stamp;

        if (el->flags.type & sc_type_arc_mask)
        {
            sc_event_emit(el->arc.begin, SC_EVENT_REMOVE_OUTPUT_ARC, _addr);
            sc_event_emit(el->arc.end, SC_EVENT_REMOVE_INPUT_ARC, _addr);
        }

        // Iterate all connectors for deleted element and append them into remove_list
        _addr = el->first_out_arc;
        while (SC_ADDR_IS_NOT_EMPTY(_addr))
        {
            sc_element *el2 = sc_storage_element_lock(ctx, _addr);
            g_assert(el2 != 0 && el2->flags.type != 0);
            gpointer p_addr = GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(_addr));

            // do not append elements, that have delete_time_stamp != 0
            if (g_hash_table_lookup(lock_table, p_addr) == nullptr)
                remove_list = g_slist_append(remove_list, p_addr);

            g_hash_table_insert(lock_table, p_addr, el2);

            _addr = el2->arc.next_out_arc;
        }

        _addr = el->first_in_arc;
        while (SC_ADDR_IS_NOT_EMPTY(_addr))
        {
            sc_element *el2 = sc_storage_element_lock(ctx, _addr);
            g_assert(el2 != 0 && el2->flags.type != 0);
            gpointer p_addr = GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(_addr));

            // do not append elements, that have delete_time_stamp != 0
            if (g_hash_table_lookup(lock_table, p_addr) == nullptr)
                remove_list = g_slist_append(remove_list, p_addr);

            g_hash_table_insert(lock_table, p_addr, el2);

            _addr = el2->arc.next_in_arc;
        }

        // clean temp addr
        SC_ADDR_MAKE_EMPTY(_addr);
    }

    // now we need to erase all elements
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, lock_table);
    gpointer key, value;
    while (g_hash_table_iter_next(&iter, &key, &value) == TRUE)
    {
        el = value;
        sc_uint32 uint_addr = GPOINTER_TO_UINT(key);
        addr.offset = SC_ADDR_LOCAL_OFFSET_FROM_INT(uint_addr);
        addr.seg = SC_ADDR_LOCAL_SEG_FROM_INT(uint_addr);

        el->flags.type = 0;

        // delete arcs from output and input lists
        if (el->flags.type & sc_type_arc_mask)
        {
            // output arcs
            sc_addr prev_arc = el->arc.prev_out_arc;
            sc_addr next_arc = el->arc.next_out_arc;

            if (SC_ADDR_IS_NOT_EMPTY(prev_arc))
            {
                sc_element *prev_el_arc = g_hash_table_lookup(lock_table, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(prev_arc)));
                if (prev_el_arc == nullptr)
                {
                    prev_el_arc = sc_storage_element_lock(ctx, prev_arc);
                    prev_el_arc->arc.next_out_arc = next_arc;
                    sc_storage_element_unlock(ctx, prev_arc);
                }else
                    prev_el_arc->arc.next_out_arc = next_arc;

            }

            if (SC_ADDR_IS_NOT_EMPTY(next_arc))
            {
                sc_element *next_el_arc = g_hash_table_lookup(lock_table, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(next_arc)));
                if (next_el_arc == nullptr)
                {
                    next_el_arc = sc_storage_element_lock(ctx, next_arc);
                    next_el_arc->arc.prev_out_arc = prev_arc;
                    sc_storage_element_unlock(ctx, next_arc);
                }else
                    next_el_arc->arc.prev_out_arc = prev_arc;
            }

            sc_element *b_el = g_hash_table_lookup(lock_table, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(el->arc.begin)));
            sc_bool need_unlock = SC_FALSE;
            if (b_el == nullptr)
            {
                b_el = sc_storage_element_lock(ctx, el->arc.begin);
                need_unlock = SC_TRUE;
            }
            if (SC_ADDR_IS_EQUAL(addr, b_el->first_out_arc))
                b_el->first_out_arc = next_arc;

            if (need_unlock)
                sc_storage_element_unlock(ctx, el->arc.begin);

            // input arcs
            prev_arc = el->arc.prev_in_arc;
            next_arc = el->arc.next_in_arc;

            if (SC_ADDR_IS_NOT_EMPTY(prev_arc))
            {
                sc_element *prev_el_arc = g_hash_table_lookup(lock_table, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(prev_arc)));
                if (prev_el_arc == nullptr)
                {
                    prev_el_arc = sc_storage_element_lock(ctx, prev_arc);
                    prev_el_arc->arc.next_in_arc = next_arc;
                    sc_storage_element_unlock(ctx, prev_arc);
                }else
                    prev_el_arc->arc.next_in_arc = next_arc;
            }

            if (SC_ADDR_IS_NOT_EMPTY(next_arc))
            {
                sc_element *next_el_arc = g_hash_table_lookup(lock_table, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(next_arc)));
                if (next_el_arc == nullptr)
                {
                    next_el_arc = sc_storage_element_lock(ctx, next_arc);
                    next_el_arc->arc.prev_in_arc = prev_arc;
                    sc_storage_element_unlock(ctx, next_arc);
                }else
                    next_el_arc->arc.prev_in_arc = prev_arc;
            }

            need_unlock = SC_FALSE;
            sc_element *e_el = g_hash_table_lookup(lock_table, GUINT_TO_POINTER(SC_ADDR_LOCAL_TO_INT(el->arc.end)));
            if (e_el == nullptr)
            {
                e_el = sc_storage_element_lock(ctx, el->arc.end);
                need_unlock = SC_TRUE;
            }
            if (SC_ADDR_IS_EQUAL(addr, b_el->first_in_arc))
                e_el->first_in_arc = next_arc;
            if (need_unlock)
                sc_storage_element_unlock(ctx, el->arc.end);
        }

    }

    // now unlock elements
    g_hash_table_iter_init(&iter, lock_table);
    while (g_hash_table_iter_next(&iter, &key, &value) == TRUE)
    {
        sc_uint32 uint_addr = GPOINTER_TO_UINT(key);
        addr.offset = SC_ADDR_LOCAL_OFFSET_FROM_INT(uint_addr);
        addr.seg = SC_ADDR_LOCAL_SEG_FROM_INT(uint_addr);

        sc_storage_element_unlock(ctx, addr);
    }

    g_slist_free(remove_list);
    g_hash_table_destroy(lock_table);

    storage_time_stamp++;

    sc_event_emit(addr, SC_EVENT_REMOVE_ELEMENT, addr);

    return SC_RESULT_OK;
}

sc_addr sc_storage_node_new(const sc_memory_context *ctx, sc_type type )
{
    sc_element el;
    sc_addr addr;

    g_assert( !(sc_type_arc_mask & type) );
    memset(&el, 0, sizeof(el));

    el.flags.type = sc_type_node | type;

    sc_element *locked_el = sc_storage_append_el_into_segments(ctx, &el, &addr);
    if (locked_el == nullptr)
    {
        SC_ADDR_MAKE_EMPTY(addr);
    }
    else
        sc_storage_element_unlock(ctx, addr);
    return addr;
}

sc_addr sc_storage_link_new(const sc_memory_context *ctx)
{
    sc_element el;
    sc_addr addr;

    memset(&el, 0, sizeof(el));
    el.flags.type = sc_type_link;

    sc_element *locked_el = sc_storage_append_el_into_segments(ctx, &el, &addr);
    if (locked_el == nullptr)
    {
        SC_ADDR_MAKE_EMPTY(addr);
    }
    else
        sc_storage_element_unlock(ctx, addr);
    return addr;
}

sc_addr sc_storage_arc_new(const sc_memory_context *ctx, sc_type type, sc_addr beg, sc_addr end)
{
    sc_addr addr;
    sc_element el, *beg_el, *end_el, *tmp_el, tmp_arc;

    memset(&el, 0, sizeof(el));
    g_assert( !(sc_type_node & type) );
    el.flags.type = (type & sc_type_arc_mask) ? type : (sc_type_arc_common | type);

    el.arc.begin = beg;
    el.arc.end = end;

    // get new element
    tmp_el = sc_storage_append_el_into_segments(&el, &addr);

    g_assert(tmp_el != 0);

    // get begin and end elements
    beg_el = sc_storage_get_element(beg, SC_TRUE);
    end_el = sc_storage_get_element(end, SC_TRUE);

    // emit events
    sc_event_emit(beg, SC_EVENT_ADD_OUTPUT_ARC, addr);
    sc_event_emit(end, SC_EVENT_ADD_INPUT_ARC, addr);
//    if (type & sc_type_edge_common)
//    {
//        sc_event_emit(end, SC_EVENT_ADD_OUTPUT_ARC, addr);
//        sc_event_emit(beg, SC_EVENT_ADD_INPUT_ARC, addr);
//    }

    // check values
    g_assert(beg_el != nullptr && end_el != nullptr);
    g_assert(beg_el->flags.type != 0 && end_el->flags.type != 0);

    // set next output arc for our created arc
    tmp_el->arc.next_out_arc = beg_el->first_out_arc;
    tmp_el->arc.next_in_arc = end_el->first_in_arc;

    if (SC_ADDR_IS_NOT_EMPTY(beg_el->first_out_arc))
    {
        tmp_arc = sc_storage_get_element(beg_el->first_out_arc, SC_TRUE);
        tmp_arc->arc.prev_out_arc = addr;
    }

    if (SC_ADDR_IS_NOT_EMPTY(end_el->first_in_arc))
    {
        tmp_arc = sc_storage_get_element(end_el->first_in_arc, SC_TRUE);
        tmp_arc->arc.prev_in_arc = addr;
    }

    // set our arc as first output/input at begin/end elements
    beg_el->first_out_arc = addr;
    end_el->first_in_arc = addr;

    return addr;
}

sc_result sc_storage_get_element_type(sc_addr addr, sc_type *result)
{
    sc_element *el = sc_storage_get_element(addr, SC_TRUE);
    if (el == 0)
        return SC_RESULT_ERROR;

    *result = el->flags.type;

    return SC_RESULT_OK;
}

sc_result sc_storage_change_element_subtype(sc_addr addr, sc_type type)
{
    if (type & sc_type_element_mask)
        return SC_RESULT_ERROR_INVALID_PARAMS;

    sc_element *el = sc_storage_get_element(addr, SC_TRUE);
    if (el == 0)
        return SC_RESULT_ERROR;

    el->flags.type = (el->flags.type & sc_type_element_mask) | (type & ~sc_type_element_mask);

    return SC_RESULT_OK;
}

sc_result sc_storage_get_arc_begin(sc_addr addr, sc_addr *result)
{
    sc_element *el = sc_storage_get_element(addr, SC_TRUE);
    if (el->flags.type & sc_type_arc_mask)
    {
        *result = el->arc.begin;
        return SC_RESULT_OK;
    }

    return SC_RESULT_ERROR_INVALID_TYPE;
}

sc_result sc_storage_get_arc_end(sc_addr addr, sc_addr *result)
{
    sc_element *el = sc_storage_get_element(addr, SC_TRUE);
    if (el->flags.type & sc_type_arc_mask)
    {
        *result = el->arc.end;
        return SC_RESULT_OK;
    }

    return SC_RESULT_ERROR_INVALID_TYPE;
}

sc_result sc_storage_set_link_content(sc_addr addr, const sc_stream *stream)
{
    sc_element *el = sc_storage_get_element(addr, SC_TRUE);
    sc_check_sum check_sum;
    sc_result result = SC_RESULT_ERROR;

    g_assert(stream != nullptr);

    if (el == nullptr)
        return SC_RESULT_ERROR_INVALID_PARAMS;

    if (!(el->flags.type & sc_type_link))
        return SC_RESULT_ERROR_INVALID_TYPE;

    // calculate checksum for data
    if (sc_link_calculate_checksum(stream, &check_sum) == SC_TRUE)
    {
        result = sc_fs_storage_write_content(addr, &check_sum, stream);
        memcpy(el->content.data, check_sum.data, check_sum.len);

        //sc_event_emit(addr, SC_EVENT_CHANGE_LINK_CONTENT, addr);
        result = SC_RESULT_OK;
    }

    g_assert(result == SC_RESULT_OK);

    return result;
}

sc_result sc_storage_get_link_content(sc_addr addr, sc_stream **stream)
{
    sc_element *el = sc_storage_get_element(addr, SC_TRUE);
    sc_check_sum checksum;

    if (el == nullptr)
        return SC_RESULT_ERROR_INVALID_PARAMS;

    if (!(el->flags.type & sc_type_link))
        return SC_RESULT_ERROR_INVALID_TYPE;


    // prepare checksum
    memcpy(checksum.data, el->content.data, checksum.len);

    return sc_fs_storage_get_checksum_content(&checksum, stream);
}

sc_result sc_storage_find_links_with_content(const sc_stream *stream, sc_addr **result, sc_uint32 *result_count)
{
    g_assert(stream != 0);
    sc_check_sum check_sum;
    if (sc_link_calculate_checksum(stream, &check_sum) == SC_TRUE)
        return sc_fs_storage_find_links_with_content(&check_sum, result, result_count);

    return SC_RESULT_ERROR;
}


sc_result sc_storage_get_elements_stat(sc_stat *stat)
{
    sc_uint s_idx, e_idx;
    sc_segment *segment;
    sc_type type;
    sc_uint32 delete_stamp;
    g_assert( stat != (sc_stat*)0 );

    memset(stat, 0, sizeof(sc_stat));

    //! TODO: add loading of segment

    // iterate all elements and calculate statistics
    for (s_idx = 0; s_idx < segments_num; s_idx++)
    {
        segment = segments[s_idx];
        g_assert( segment != (sc_segment*)0 );
        for (e_idx = 0; e_idx < SC_SEGMENT_ELEMENTS_COUNT; e_idx++)
        {
            type = segment->elements[e_idx].flags.type;
            delete_stamp = segment->elements[e_idx].delete_time_stamp;
            if (type == 0)
            {
                stat->empty_count++;
            }
            else
            {
                if (type & sc_type_node)
                {
                    stat->node_count++;
                    if (delete_stamp == 0)
                        stat->node_live_count++;
                }
                else
                {
                    if (type & sc_type_arc_mask)
                    {
                        stat->arc_count++;
                        if (delete_stamp == 0)
                            stat->arc_live_count++;
                    }else
                    {
                        if (type & sc_type_link)
                        {
                            stat->link_count++;
                            if (delete_stamp == 0)
                                stat->link_live_count++;
                        }
                    }
                }
            }
        }
    }

    return SC_TRUE;
}

sc_uint sc_storage_get_time_stamp()
{
    return storage_time_stamp;
}

unsigned int sc_storage_get_segments_count()
{
    return segments_num;
}

sc_element* sc_storage_element_lock(const sc_memory_context *ctx, sc_addr addr)
{
    if (addr.seg >= SC_ADDR_SEG_MAX)
        return (sc_element*)0;

    sc_segment *segment = g_atomic_pointer_get(&segments[addr.seg]);
    if (segment == 0)
        return (sc_element*)0;

    return sc_segment_lock_element(ctx, segment, addr.offset);
}

sc_element* sc_storage_element_lock_try(const sc_memory_context *ctx, sc_addr addr, sc_uint16 max_attempts)
{
    if (addr.seg >= SC_ADDR_SEG_MAX)
        return (sc_element*)0;

    sc_segment *segment = g_atomic_pointer_get(&segments[addr.seg]);
    if (segment == 0)
        return (sc_element*)0;

    return sc_segment_lock_element_try(ctx, segment, addr.offset, max_attempts);
}

void sc_storage_element_unlock(const sc_memory_context *ctx, sc_addr addr)
{
    if (addr.seg >= SC_ADDR_SEG_MAX)
        return (sc_element*)0;

    sc_segment *segment = g_atomic_pointer_get(&segments[addr.seg]);
    if (segment == 0)
        return (sc_element*)0;

    return sc_segment_unlock_element(ctx, segment, addr.offset);
}

