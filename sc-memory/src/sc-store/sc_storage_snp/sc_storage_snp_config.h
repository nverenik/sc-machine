#ifndef _sc_storage_snp_config_h_
#define _sc_storage_snp_config_h_

#ifdef ENABLE_HARDWARE_STORAGE

#include <snp/snp.h>

// device configuration
const int g_iNumberOfPU     = 1024;
const int g_iCellsPerPU     = 64 * 1024;
const int g_iCellBitwidth   = 96;

namespace sc_storage_snp {

typedef snp::snpDevice<g_iCellBitwidth> Device;

}

//
// Scheme of data-mapping:
//
//            +---------+-------------------+-----------------------------+---------+---------+
//            |Cell Type|     Identifier    |    Type-based Attributes    | reserved|  Search |
//            +---------+-------------------+-----------------------------+---------+---------+
//            |   2bit  |       32bit       |           'A'bit            | 'B'bit  |  16bit  |
//            +---------+-------------------+-----------------------------+---------+---------+
//
//            A + B = 46 =>
//            cell size = 12 byte = 3 * uint32 = 96 bit
//
// Graph vertex:
// + optional 'link size' field
//            +---------+-------------------+-----------------------------+---------+---------+
//            |   type  |      vertexID     |          attributes         | reserved|  Search |
//            +---------+-------------------+-----------------------------+---------+---------+
//            |    01   | segment | offset  |accs.lvl.| sc_type |   lock  |#########|    -    |
//            +---------+-------------------+-----------------------------+---------+---------+
//            |   2bit  |  16bit  |  16bit  |   8bit  |  16bit  |   8bit  |  14bit  |  16bit  |
//            +---------+-------------------+-----------------------------+---------+---------+
//
// Graph directed edge (connection):
//            +---------+-------------------+-----------------------------+---------+---------+
//            |   type  |     vertexID1     |     vertexID2     |      reserved     |  Search |
//            +---------+-------------------+-----------------------------+---------+---------+
//            |    10   | segment | offset  | segment | offset  |###################|    -    |
//            +---------+-------------------+-----------------------------+---------+---------+
//            |   2bit  |  16bit  |  16bit  |  16bit  |  16bit  |       14bit       |  16bit  |
//            +---------+-------------------+-----------------------------+---------+---------+
//
// Vertex content link:
//            +---------+-------------------+-----------------------------+---------+---------+
//            |   type  | linkID = vertexID |          attributes         | reserved|  Search |
//            +---------+-------------------+-----------------------------+---------+---------+
//            |    11   | segment | offset  |  checksum[index]  |  index  |#########|    -    |
//            +---------+-------------------+-----------------------------+---------+---------+
//            |   2bit  |  16bit  |  16bit  |       32bit       |   3bit  |  11bit  |  16bit  |
//            +---------+-------------------+-----------------------------+---------+---------+
//
// For instance: 1Gb of memory => 89 478 485 cells
//

#endif //ENABLE_HARDWARE_STORAGE

#endif //_sc_storage_snp_config_h_
