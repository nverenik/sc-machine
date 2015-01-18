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

#ifdef ENABLE_HARDWARE_STORAGE

#include "../sc_memory_private.h"
#include "sc_storage_snp/sc_storage_snp_glue.h"

#include <glib.h>

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
    return snp_element_create_node(type, access_levels);
}

sc_addr sc_storage_link_new(const sc_memory_context *ctx)
{
    return sc_storage_link_new_ext(ctx, ctx->access_levels);
}

sc_addr sc_storage_link_new_ext(const sc_memory_context *ctx, sc_access_levels access_levels)
{
    PARAM_NOT_USED(ctx);
    return snp_element_create_link(access_levels);
}

sc_addr sc_storage_arc_new(const sc_memory_context *ctx, sc_type type, sc_addr beg, sc_addr end)
{
    return sc_storage_arc_new_ext(ctx, type, beg, end, ctx->access_levels);
}

sc_addr sc_storage_arc_new_ext(const sc_memory_context *ctx, sc_type type, sc_addr beg, sc_addr end, sc_access_levels access_levels)
{
    PARAM_NOT_USED(ctx);
    return snp_element_create_arc(type, beg, end, access_levels);
}

sc_result sc_storage_get_element_type(const sc_memory_context *ctx, sc_addr addr, sc_type *result)
{
    return snp_element_get_type(ctx, addr, result);
}

sc_result sc_storage_change_element_subtype(const sc_memory_context *ctx, sc_addr addr, sc_type type)
{
    return snp_element_set_subtype(ctx, addr, type);
}

sc_result sc_storage_get_arc_begin(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
{
    return snp_element_get_arc_begin(ctx, addr, result);
}

sc_result sc_storage_get_arc_end(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
{
    return snp_element_get_arc_end(ctx, addr, result);
}

sc_result sc_storage_set_link_content(const sc_memory_context *ctx, sc_addr addr, const sc_stream *stream)
{
    return snp_element_set_link_content(ctx, addr, stream);
}

sc_result sc_storage_get_link_content(const sc_memory_context *ctx, sc_addr addr, sc_stream **stream)
{
    return snp_element_get_link_content(ctx, addr, stream);
}

sc_result sc_storage_find_links_with_content(const sc_memory_context *ctx, const sc_stream *stream, sc_addr **result, sc_uint32 *result_count)
{
    return snp_element_find_link(ctx, stream, result, result_count);
}

sc_result sc_storage_set_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels access_levels, sc_access_levels * new_value)
{
    return snp_element_set_access_levels(ctx, addr, access_levels, new_value);
}

sc_result sc_storage_get_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels * result)
{
    return snp_element_get_access_levels(ctx, addr, result);
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

//! Returns pointer to sc-element metainfo
sc_element_meta* sc_storage_get_element_meta(const sc_memory_context *ctx, sc_addr addr)
{
    (void)ctx;
    (void)addr;

    return 0;
}

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
