#include "sc_storage_snp_glue.h"

#include <snp/snp.h>
using snp::snpErrorCode;

// create static device instance with predefined bitwidth
typedef snp::snpDevice<128> Device;
static Device s_Device;

/*! Initialize sc storage in specified path
 * @param path Path to repository
 * @param clear Flag to clear initialize empty storage
 */
sc_bool snp_initialize(const char *path, sc_bool clear)
{
    (void)path;
    (void)clear;
    return (s_Device.configure(4, 4) != snpErrorCode::SUCCEEDED) ? SC_FALSE : SC_TRUE;
}

//! Shutdown sc storage
void snp_shutdown(sc_bool save_state)
{
    (void)save_state;
    s_Device.end();
}

//! Check if storage initialized
sc_bool snp_is_initialized()
{
    return s_Device.isReady() ? SC_TRUE : SC_FALSE;
}

