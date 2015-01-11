#ifndef _sc_storage_snp_types_h_
#define _sc_storage_snp_types_h_

#ifdef ENABLE_HARDWARE_STORAGE

#include "sc_storage_snp_config.h"
#include "../sc_types.h"

namespace sc_storage_snp {

union VertexID
{
    sc_addr m_asAddr;   // 32bit
    struct {
        uint8 raw[4];
    } m_asBitfield;     // 32bit
};

struct Vertex
{
    VertexID            m_ID;               // 32bit
    sc_access_levels    m_scAccessLevels;   // 8bit
    sc_type             m_scType;           // 16bit
    uint8               m_uLock;            // 8bit
    uint8               m_uType     : 2;    // 2bit
    uint8               m_uLinkSize : 5;    // 5bit
    uint16              _reserved   : 9;    // 9bit
    uint16              m_uSearch;          // 16bit
};

struct Edge
{
    VertexID    m_ID1;              // 32bit
    VertexID    m_ID2;              // 32bit
    uint8       m_uType     : 2;    // 2bit
    uint16      _reserved   : 14;   // 11bit
    uint16      m_uSearch;          // 16bit
};

struct Link
{
    VertexID    m_ID;                   // 32bit
    uint32      m_uChecksum;            // 32bit
    uint8       m_uType         : 2;    // 2bit
    uint8       m_uLinkIndex    : 3;    // 3bit
    uint16      _reserved       : 11;   // 11bit
    uint16      m_uSearch;              // 16bit
};

enum class CellType : uint8
{
    EMPTY = 0,
    VERTEX,
    EDGE,
    LINK
};

union Cell
{
    Device::snpBitfield m_asBitfield;
    Vertex              m_asVertex;
    Edge                m_asEdge;
    Link                m_asLink;
};

}

#endif //ENABLE_HARDWARE_STORAGE

#endif //_sc_storage_snp_types_h_
