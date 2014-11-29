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
#include "../sc_memory_private.h"

#include <glib.h>

#ifdef ENABLE_HARDWARE_STORAGE

#include "sc_storage_snp/sc_storage_snp_glue.h"

#define PARAM_NOT_USED(__param__) (void)(__param__)

sc_bool sc_storage_initialize(const char *path, sc_bool clear)
{
    return snp_initialize(path, (clear != SC_FALSE)) ? SC_TRUE : SC_FALSE;
}

void sc_storage_shutdown(sc_bool save_state)
{
    snp_shutdown(save_state);
}

sc_bool sc_storage_is_initialized()
{
    return snp_is_initialized() ? SC_TRUE : SC_FALSE;
}

sc_element* sc_storage_append_el_into_segments(const sc_memory_context *ctx, sc_element *element, sc_addr *addr)
{
    PARAM_NOT_USED(ctx);
    PARAM_NOT_USED(element);
    PARAM_NOT_USED(addr);

    g_assert(!"Method is not used in snp storage implementation.");
    return 0;
}

sc_bool sc_storage_is_element(const sc_memory_context *ctx, sc_addr addr)
{
    PARAM_NOT_USED(ctx);
    return snp_element_exists(addr);
}

sc_addr sc_storage_element_new_access(const sc_memory_context *ctx, sc_type type, sc_access_levels access_levels)
{
    PARAM_NOT_USED(ctx);
    PARAM_NOT_USED(type);
    PARAM_NOT_USED(access_levels);

    g_assert(!"Method is not used in snp storage implementation.");

    sc_addr addr;
    return addr;
}

sc_result sc_storage_element_free(const sc_memory_context *ctx, sc_addr addr)
{
    return snp_element_destroy(ctx, addr);
}

sc_addr sc_storage_node_new(const sc_memory_context *ctx, sc_type type)
{
    return sc_storage_node_new_ext(ctx, type, ctx->access_levels);
}

sc_addr sc_storage_node_new_ext(const sc_memory_context *ctx, sc_type type, sc_access_levels access_levels)
{
    PARAM_NOT_USED(ctx);

    g_assert(!(sc_type_arc_mask & type));
    type = sc_flags_remove(sc_type_node | type);

    return snp_element_create_node(type, access_levels);
}

sc_addr sc_storage_link_new(const sc_memory_context *ctx)
{
    return sc_storage_link_new_ext(ctx, ctx->access_levels);
}

sc_addr sc_storage_link_new_ext(const sc_memory_context *ctx, sc_access_levels access_levels)
{
    PARAM_NOT_USED(ctx);

    sc_type type = sc_type_link;
    return snp_element_create_node(type, access_levels);
}

sc_addr sc_storage_arc_new(const sc_memory_context *ctx, sc_type type, sc_addr beg, sc_addr end)
{
    return sc_storage_arc_new_ext(ctx, type, beg, end, ctx->access_levels);
}

sc_addr sc_storage_arc_new_ext(const sc_memory_context *ctx, sc_type type, sc_addr beg, sc_addr end, sc_access_levels access_levels)
{
    PARAM_NOT_USED(ctx);

    g_assert(!(sc_type_node & type));
    type = sc_flags_remove((type & sc_type_arc_mask) ? type : (sc_type_arc_common | type));

    return snp_element_create_arc(type, beg, end, access_levels);
}

/*! Get type of sc-element with specified sc-addr
 * @param addr sc-addr of element to get type
 * @param result Pointer to result container
 * @return If input params are correct and type resolved, then return SC_OK;
 * otherwise return SC_ERROR
 */
sc_result sc_storage_get_element_type(const sc_memory_context *ctx, sc_addr addr, sc_type *result)
{
    (void)ctx;
    (void)addr;
    (void)result;

    return SC_RESULT_ERROR;
}

/*! Change element sub-type
 * @param addr sc-addr of element to set new type
 * @param type New sub-type of sc-element (this type must be: type & sc_type_element_mask == 0)
 * @return If sub-type changed, then returns SC_RESULT_OK; otherwise returns SC_RESULT_ERROR
 */
sc_result sc_storage_change_element_subtype(const sc_memory_context *ctx, sc_addr addr, sc_type type)
{
    (void)ctx;
    (void)addr;
    (void)type;

    return SC_RESULT_ERROR;
}

/*! Returns sc-addr of begin element of specified arc
 * @param addr sc-addr of arc to get begin element
 * @param result Pointer to result container
 * @return If input params are correct and begin element resolved, then return SC_OK.
 * If element with specified addr isn't an arc, then return SC_INVALID_TYPE
 */
sc_result sc_storage_get_arc_begin(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
{
    (void)ctx;
    (void)addr;
    (void)result;

    return SC_RESULT_ERROR;
}

/*! Returns sc-addr of end element of specified arc
 * @param addr sc-addr of arc to get end element
 * @param result PoOinter to result container
 * @return If input params are correct and end element resolved, then return SC_OK.
 * If element with specified addr isn't an arc, then return SC_INVALID_TYPE
 */
sc_result sc_storage_get_arc_end(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
{
    (void)ctx;
    (void)addr;
    (void)result;

    return SC_RESULT_ERROR;
}

/*! Setup content data for specified sc-link
 * @param addr sc-addr of sc-link to setup content
 * @param stream Pointer to stream
 * @return If content of specified link changed without any errors, then return SC_OK; otherwise
 * returns on of error codes:
 * <ul>
 * <li>SC_INVALID_TYPE - element with \p addr isn't a sc-link</li>
 * <li>SC_ERROR_INVALID_PARAMS - element with specifed \p addr doesn't exist
 * <li>SC_ERROR - unknown error</li>
 * </ul>
 */
sc_result sc_storage_set_link_content(const sc_memory_context *ctx, sc_addr addr, const sc_stream *stream)
{
    // Calculate crc32 from input stream and push it into device memory
    // In case if crc32 itself is bigger than content we can just store content
    // instead.
    //
    // Where should we store streamed data? Does it initially use the same DB as for graph?
    // So do we need to startup the DB again?
    //
    // sc_link_calculate_checksum()
    // sc_fs_storage_write_content()

    (void)ctx;
    (void)addr;
    (void)stream;

    return SC_RESULT_ERROR;
}

/*! Returns content data from specified sc-link
 * @param addr sc-addr of sc-link to get content data
 * @param stream Pointer to returned data stream
 * @return If content of specified link returned without any errors, then return SC_OK; otherwise
 * returns on of error codes:
 * <ul>
 * <li>SC_INVALID_TYPE - element with \p addr isn't a sc-link</li>
 * <li>SC_ERROR_INVALID_PARAMS - element with specifed \p addr doesn't exist
 * <li>SC_ERROR - unknown error</li>
 * </ul>
 */
sc_result sc_storage_get_link_content(const sc_memory_context *ctx, sc_addr addr, sc_stream **stream)
{
    // sc_link_calculate_checksum()
    // sc_fs_storage_get_checksum_content()

    (void)ctx;
    (void)addr;
    (void)stream;

    return SC_RESULT_ERROR;
}

/*! Search sc-link addrs by specified data
 * @param stream Pointert to stream that contains data for search
 * @param result Pointer to result container
 * @param result_count Container for results count
 * @return If sc-links with specified checksum found, then sc-addrs of found link
 * writes into \p result array and function returns SC_OK; otherwise \p result will contain
 * empty sc-addr and function returns SC_OK. In any case \p result_count contains number of found
 * sc-addrs
 * @attention \p result array need to be free after usage
 */
sc_result sc_storage_find_links_with_content(const sc_memory_context *ctx, const sc_stream *stream, sc_addr **result, sc_uint32 *result_count)
{
    // calculate crc32 from the stream, find the cell which contains such crc32
    // => get ID of link node
    //
    // (!) we can get number of matched nodes without reading from device, but
    // we need to execute kernel the same number of times as the matched nodes amount

    (void)ctx;
    (void)stream;
    (void)result;
    (void)result_count;

    return SC_RESULT_ERROR;
}

/*! Setup new access levels to sc-element. New access levels will be a minimum from context access levels and parameter \b access_levels
 * @param addr sc-addr of sc-element to change access levels
 * @param access_levels new access levels
 * @param new_value new value of access levels for sc-element. This parameter can be NULL
 *
 * @return Returns SC_RESULT_OK, when access level changed; otherwise it returns error code
 */
sc_result sc_storage_set_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels access_levels, sc_access_levels * new_value)
{
    (void)ctx;
    (void)addr;
    (void)access_levels;
    (void)new_value;

    return SC_RESULT_ERROR;
}

//! Get access levels of sc-element
sc_result sc_storage_get_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels * result)
{
    (void)ctx;
    (void)addr;
    (void)result;

    return SC_RESULT_ERROR;
}

sc_uint sc_storage_get_segments_count()
{
    g_assert(!"Method is not used in snp storage implementation.");
    return 0;
}

/*! Get statistics information about elements
 * @param stat Pointer to structure that store statistic
 * @return If statictics info collect without any errors, then return SC_OK;
 * otherwise return SC_ERROR
 */
sc_result sc_storage_get_elements_stat(const sc_memory_context *ctx, sc_stat *stat)
{
    (void)ctx;
    (void)stat;

    return SC_RESULT_ERROR;
}

sc_result sc_storage_erase_element_from_segment(sc_addr addr)
{
    (void)addr;
    return SC_RESULT_ERROR;
}

// ----- Locks -----

//! Locks specified sc-element. Pointer to locked sc-element stores in el
sc_result sc_storage_element_lock(const sc_memory_context *ctx, sc_addr addr, sc_element **el)
{
    (void)ctx;
    (void)addr;
    (void)el;

    return SC_RESULT_ERROR;
}

//! Try to lock sc-element by maximum attempts. If element wasn't locked and there are no errors, then el pointer will have null value.
sc_result sc_storage_element_lock_try(const sc_memory_context *ctx, sc_addr addr, sc_uint16 max_attempts, sc_element **el)
{
    (void)ctx;
    (void)addr;
    (void)max_attempts;
    (void)el;

    return SC_RESULT_ERROR;
}

//! Unlocks specified sc-element
sc_result sc_storage_element_unlock(const sc_memory_context *ctx, sc_addr addr)
{
    (void)ctx;
    (void)addr;

    return SC_RESULT_ERROR;
}

#if SC_PROFILE_MODE

void sc_storage_reset_profile()
{
}

void sc_storage_print_profile()
{
}

#endif

#endif //ENABLE_HARDWARE_STORAGE
