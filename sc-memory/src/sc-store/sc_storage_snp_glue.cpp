#include "sc_storage_snp_glue.h"

#ifdef ENABLE_HARDWARE_STORAGE

#include <string>
#include <snp/snp.h>
using snp::snpErrorCode;

// device configuration
static const int s_iNumberOfPU = 1024;
static const int s_iCellsPerPU = 1024;
static const int s_iCellBitwidth = 352; // (bits) = 44Byte = 11*int32

// create static device instance with predefined bitwidth
typedef snp::snpDevice<s_iCellBitwidth> Device;
static Device s_Device;

//
// Scheme of data-mapping:
//
//            +-------------------+-------------------+---------+---------+
//            |      sc_addr      | sc_memory_context |         |         |
//            +---------+---------+---------+---------+ sc_type |  CRC32  |
// sc-machine | segment | offset  |    id   |accs.lvl.|         |         |
//            +---------+---------+---------+---------+---------+---------+---------+
//            |  16bit  |  16bit  |  16bit  |  8bit   |  16bit  |  256bit |  24bit  |
//            +---------+---------+---------+---------+---------+---------+---------+
//        snp |    Unique Identifier (ID)   |           Attributes        | Reserved|
//            +-----------------------------+-------------------+---------+---------+
//
// For instance: 1Gb of memory => 24 403 223 elements of graph
//
// ! Note: seems we shouldn't store sc_memory_context:access_level field,
// but access level from sc_element:sc_element_flags
//

static std::string s_sDumpFilePath;

// todo: Initialize device instance. If path is not presented or
// clear flag is True. Otherwise try to load device memory content
// from dump-file located by specified path.
sc_bool snp_initialize(const char *path, sc_bool clear)
{
    // allocate all needed memory on device (single GPU or cluster) at once
    snpErrorCode eResult = s_Device.configure(s_iCellsPerPU, s_iNumberOfPU);
    if (eResult != snpErrorCode::SUCCEEDED)
    {
        // print error log accordingly to returned code
        // for now just assert in any case
        assert(0);
        return SC_FALSE;
    }

    if (!path)
    {
        // is dump file support is mandatory or we can just
        // work without saving result onto disk?
        assert(0);
        return SC_FALSE;
    }

    if (clear)
    {
        // remove file if it exists
        // clear device memory
    }
    else
    {
        // try to load file into device memory as is (note
        // that device is not support dumping yet)

        // what should we do if some error occured?
        // return false or just continue with empty database?
    }

    s_sDumpFilePath = path;
    return SC_TRUE;
}

void snp_shutdown(sc_bool save_state)
{
    if (save_state && !s_sDumpFilePath.empty())
    {
        // dump device memory content onto disk
        // in case of cluster should we separate the data on
        // several dump files? like 20 files * 512Mb or smth.

        // note that data size is not related to graph state,
        // it always equals to the size of device memory
    }

    // release device
    s_sDumpFilePath.clear();
    s_Device.end();
}

sc_bool snp_is_initialized()
{
    return s_Device.isReady() ? SC_TRUE : SC_FALSE;
}

#endif //ENABLE_HARDWARE_STORAGE
