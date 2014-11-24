#ifndef _sc_storage_snp_glue_h_
#define _sc_storage_snp_glue_h_

#ifdef ENABLE_HARDWARE_STORAGE

#include "sc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

sc_bool snp_initialize(const char *path, sc_bool clear);
void snp_shutdown(sc_bool save_state);
sc_bool snp_is_initialized();

#ifdef __cplusplus
}
#endif

#endif //ENABLE_HARDWARE_STORAGE

#endif //_sc_storage_snp_glue_h_
