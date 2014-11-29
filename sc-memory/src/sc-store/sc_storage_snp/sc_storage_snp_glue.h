#ifndef _sc_storage_snp_glue_h_
#define _sc_storage_snp_glue_h_

#include "../sc_types.h"

#ifdef ENABLE_HARDWARE_STORAGE

#ifdef __cplusplus
extern "C" {
#endif

sc_bool     snp_initialize(const char *path, sc_bool clear);
void        snp_shutdown(sc_bool save_state);
sc_bool     snp_is_initialized();

sc_bool     snp_element_exists(sc_addr addr);
sc_result   snp_element_destroy(const sc_memory_context *ctx, sc_addr addr);

sc_addr     snp_element_create_node(sc_type type, sc_access_levels access_levels);
sc_addr     snp_element_create_arc(sc_type type, sc_addr beg, sc_addr end, sc_access_levels access_levels);

#ifdef __cplusplus
}
#endif

#endif //ENABLE_HARDWARE_STORAGE

#endif //_sc_storage_snp_glue_h_
