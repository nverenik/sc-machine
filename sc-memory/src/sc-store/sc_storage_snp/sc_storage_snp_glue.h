#ifndef _sc_storage_snp_glue_h_
#define _sc_storage_snp_glue_h_

#ifdef ENABLE_HARDWARE_STORAGE

#ifdef __cplusplus
extern "C" {
#endif

#include "../sc_types.h"
#include "../sc_stream.h"

sc_bool     snp_initialize(const char *path, sc_bool clear);
void        snp_shutdown(sc_bool save_state);
sc_bool     snp_is_initialized();

sc_bool     snp_element_exists(sc_addr addr);
sc_result   snp_element_destroy(const sc_memory_context *ctx, sc_addr addr);

sc_addr     snp_element_create_node(sc_type type, sc_access_levels access_levels);
sc_addr     snp_element_create_link(sc_access_levels access_levels);
sc_addr     snp_element_create_arc(sc_type type, sc_addr beg, sc_addr end, sc_access_levels access_levels);

sc_result   snp_element_get_type(const sc_memory_context *ctx, sc_addr addr, sc_type *result);
sc_result   snp_element_set_subtype(const sc_memory_context *ctx, sc_addr addr, sc_type type);

sc_result   snp_element_get_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels *result);
sc_result   snp_element_set_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels access_levels, sc_access_levels *new_value);

sc_result   snp_element_get_arc_begin(const sc_memory_context *ctx, sc_addr addr, sc_addr *result);
sc_result   snp_element_get_arc_end(const sc_memory_context *ctx, sc_addr addr, sc_addr *result);

sc_result   snp_element_get_link_content(const sc_memory_context *ctx, sc_addr addr, sc_stream **stream);
sc_result   snp_element_set_link_content(const sc_memory_context *ctx, sc_addr addr, const sc_stream *stream);
sc_result   snp_element_find_link(const sc_memory_context *ctx, const sc_stream *stream, sc_addr **result, sc_uint32 *result_count);

#ifdef __cplusplus
}
#endif

#endif //ENABLE_HARDWARE_STORAGE

#endif //_sc_storage_snp_glue_h_
