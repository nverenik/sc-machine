#include "sc_storage_snp_glue.h"

#ifdef ENABLE_HARDWARE_STORAGE

#include <string>
#include <vector>
#include <glib.h>

#include <snp/snp.h>
using snp::snpErrorCode;

#include "sc_storage_snp_types.h"

extern "C" {
#include "../../sc_memory_private.h"
#include "../sc_storage.h"
#include "../sc_fs_storage.h"
#include "../sc_link_helpers.h"
#include "../sc_stream_memory.h"
}

// create static device instance with predefined bitwidth
static sc_storage_snp::Device s_Device;

static std::string s_sDumpFilePath;
static std::string s_sDumpFileName = "snp.dump";

// forward declaration for private methods
void snp_enter_critical_section();
void snp_leave_critical_section();

bool snp_data_storage_init(const std::string &path, bool clear);
void snp_data_storage_shutdown();

sc_storage_snp::VertexID snp_vertex_id_obtain();
void snp_vertex_id_release(const sc_storage_snp::VertexID &vertex_id);

bool snp_vertex_find(const sc_storage_snp::VertexID &vertex_id);
bool snp_vertex_read(const sc_memory_context *ctx, const sc_storage_snp::VertexID &vertex_id, sc_storage_snp::Cell &cell, sc_result &result);

bool snp_vertex_create(const sc_storage_snp::VertexID &vertex_id, sc_type type, sc_access_levels access_levels);
bool snp_edge_create(const sc_storage_snp::VertexID &vertex_id1, const sc_storage_snp::VertexID &vertex_id2);

bool snp_link_create(const sc_storage_snp::VertexID &vertex_id, uint32 checksum, uint8 index);
bool snp_link_destroy(const sc_storage_snp::VertexID &vertex_id);
bool snp_link_read(const sc_storage_snp::VertexID &vertex_id, uint8 size, std::vector<char> &output);

#define SNP_BITFIELD_DATA_SET                   snpBitfieldSet
#define SNP_BITFIELD_MASK_SET(__bitfield__)     snpBitfieldSet((__bitfield__), -1)
#define SNP_BITFIELD_MASK_RESET(__bitfield__)   snpBitfieldSet((__bitfield__),  0)

//
// Storage consists of two parts:
// - storage for payload data (it's external file memory storage);
// - storage for semantic network based on this data (snp device)
//

// Initialize both file storage and snp device.
// If path is not presented or clear flag is True created storage is empty.
// Otherwise try to load content from file system.
sc_bool snp_initialize(const char *path, sc_bool clear)
{
    snp_enter_critical_section();

    bool bResult = false;
    do
    {
        // dump file is mandatory
        if (!path || !strlen(path))
            break;

        // initialize file storage for payload
        if (!snp_data_storage_init(path, (clear != SC_FALSE)))
            break;

        // initialize/configure snp device (single GPU or cluster)
        snpErrorCode eResult = s_Device.configure(g_iCellsPerPU, g_iNumberOfPU);
        if (eResult != snpErrorCode::SUCCEEDED)
        {
            // print error log accordingly to returned code
            // for now just assert in any case
            assert(0);
            break;
        }

        if (/*clear*/ true /* ... for now */)
        {
            // todo: remove file if it exists
            // ...

            // clear device memory
            sc_storage_snp::Device::snpInstruction Instruction;
            SNP_BITFIELD_MASK_RESET(Instruction.field.addressMask.raw);
            SNP_BITFIELD_MASK_SET(Instruction.field.dataMask.raw);
            SNP_BITFIELD_DATA_SET(Instruction.field.dataData.raw, 0);
            s_Device.exec(false, snp::snpAssign, Instruction);
        }
        else
        {
            // todo: try to load file into device memory as is (note
            // that device is not support dumping yet)
        }

        s_sDumpFilePath = path;
        bResult = true;
    }
    while(0);

    snp_leave_critical_section();
    return bResult ? SC_TRUE : SC_FALSE;
}

void snp_shutdown(sc_bool save_state)
{
    snp_enter_critical_section();
    if (save_state && !s_sDumpFilePath.empty())
    {
        // dump device memory content onto disk
        // in case of cluster should we separate the data on
        // several dump files? like 20 files * 512Mb or smth.

        // note that data size is not related to graph state,
        // it always equals to the size of device memory

        // but we can implement some optimization to store zero fields
        // (skip unused cells and store just used cells)
    }

    // release device
    s_sDumpFilePath.clear();
    s_Device.end();

    snp_leave_critical_section();
}

sc_bool snp_is_initialized()
{
    snp_enter_critical_section();
    bool bReady = s_Device.isReady();

    snp_leave_critical_section();
    return bReady ? SC_TRUE : SC_FALSE;
}

// Check if sc_element with this address exists.
sc_bool snp_element_exists(sc_addr addr)
{
    snp_enter_critical_section();

    sc_storage_snp::VertexID VertexID;
    VertexID.m_asAddr = addr;

    bool bExists = snp_vertex_find(VertexID);

    snp_leave_critical_section();
    return bExists ? SC_TRUE : SC_FALSE;
}

// Remove element (ie. vertex) and all it's connections (ie. edges)
sc_result snp_element_destroy(const sc_memory_context *ctx, sc_addr addr)
{
    // todo: try to remove everything despite of error
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // does this element exist?
        if (!snp_vertex_find(VertexID))
            break;

        sc_storage_snp::Cell Cell;

        // if we are here it exists, right? just read it.
        snpErrorCode eErrorCode;
        bool bExists = s_Device.read(Cell.m_asBitfield, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        // but to be sure...
        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // check write access
        if (!sc_access_lvl_check_write(ctx->access_levels, Cell.m_asVertex.m_scAccessLevels))
        {
            eResult = SC_RESULT_ERROR_NO_WRITE_RIGHTS;
            break;
        }

        // release all cells with specified ID (this includes found vertex and
        // all its output edges)
        //
        // IMPORTANT: if cell layout is changed that it's possible we will need
        // separated instruction execution to delete output edges

        sc_storage_snp::Cell AddressMask;
        SNP_BITFIELD_MASK_RESET(AddressMask.m_asBitfield.raw);
        SNP_BITFIELD_MASK_SET(AddressMask.m_asVertex.m_ID.m_asBitfield.raw);

        sc_storage_snp::Cell AddressData;
        AddressData.m_asVertex.m_ID.m_asBitfield = VertexID.m_asBitfield;

        // fill matched cells with zeros
        sc_storage_snp::Cell DataMask;
        SNP_BITFIELD_MASK_SET(DataMask.m_asBitfield.raw);

        sc_storage_snp::Cell DataData;
        SNP_BITFIELD_DATA_SET(DataData.m_asBitfield.raw, 0);

        // run instruction
        sc_storage_snp::Device::snpInstruction Instruction;
        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataMask = DataMask.m_asBitfield;
        Instruction.field.dataData = DataData.m_asBitfield;

        s_Device.exec(false, snp::snpAssign, Instruction, &eErrorCode);
        if (eErrorCode != snpErrorCode::SUCCEEDED)
        {
            assert(0);
            break;
        }

        // now delete all input edges
        SNP_BITFIELD_MASK_RESET(AddressMask.m_asBitfield.raw);
        SNP_BITFIELD_MASK_SET(AddressMask.m_asEdge.m_ID2.m_asBitfield.raw);
        AddressData.m_asEdge.m_ID2.m_asBitfield = VertexID.m_asBitfield;

        s_Device.exec(false, snp::snpAssign, Instruction, &eErrorCode);
        if (eErrorCode != snpErrorCode::SUCCEEDED)
        {
            assert(0);
            break;
        }

        eResult = SC_RESULT_OK;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_addr snp_element_create_node(sc_type type, sc_access_levels access_levels)
{
    snp_enter_critical_section();

    assert(!(sc_type_arc_mask & type));
    type = sc_flags_remove(sc_type_node | type);

    sc_storage_snp::VertexID VertexID = snp_vertex_id_obtain();
    bool bCreated = snp_vertex_create(VertexID, type, access_levels);
    if (!bCreated)
    {
        assert(0);
        SNP_BITFIELD_DATA_SET(VertexID.m_asBitfield.raw, 0);
    }

    snp_leave_critical_section();
    return VertexID.m_asAddr;
}

sc_addr snp_element_create_link(sc_access_levels access_levels)
{
    return snp_element_create_node(sc_type_link, access_levels);
}

sc_addr snp_element_create_arc(sc_type type, sc_addr beg, sc_addr end, sc_access_levels access_levels)
{
    snp_enter_critical_section();

    assert(!(sc_type_node & type));
    type = sc_flags_remove((type & sc_type_arc_mask) ? type : (sc_type_arc_common | type));

    // each arc element is transformed into vertex (which stores metadata)
    // and 2 edges to save direction of connection

    bool bError = false;
    sc_storage_snp::VertexID VertexID = snp_vertex_id_obtain();
    do
    {
        // create vertex at first
        bool bCreated = snp_vertex_create(VertexID, type, access_levels);
        if (!bCreated)
        {
            assert(0);
            bError = true;
            break;
        }

        // then create edges to not lose connection

        sc_storage_snp::VertexID VertexBegin;
        VertexBegin.m_asAddr = beg;

        bCreated = snp_edge_create(VertexBegin, VertexID);
        if (!bCreated)
        {
            assert(0);
            bError = true;
            break;
        }

        sc_storage_snp::VertexID VertexEnd;
        VertexEnd.m_asAddr = end;

        bCreated = snp_edge_create(VertexID, VertexEnd);
        if (!bCreated)
        {
            assert(0);
            bError = true;
            break;
        }
    }
    while(0);

    if (bError)
    {
        // todo: remove element and release ID
        SNP_BITFIELD_DATA_SET(VertexID.m_asBitfield.raw, 0);
    }

    snp_leave_critical_section();
    return VertexID.m_asAddr;
}

sc_result snp_element_get_type(const sc_memory_context *ctx, sc_addr addr, sc_type *result)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        // prevent working with device as soon as possible
        if (!result)
        {
            assert(0);
            break;
        }

        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // try to read this cell (including read access check)
        sc_storage_snp::Cell Cell;
        if (!snp_vertex_read(ctx, VertexID, Cell, eResult))
            break;

        (*result) = Cell.m_asVertex.m_scType;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_set_subtype(const sc_memory_context *ctx, sc_addr addr, sc_type type)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        if (type & sc_type_element_mask)
        {
            eResult = SC_RESULT_ERROR_INVALID_PARAMS;
            break;
        }

        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // does this element exist?
        if (!snp_vertex_find(VertexID))
            break;

        sc_storage_snp::Cell Cell;

        // if we are here it exists, right? just read it.
        snpErrorCode eErrorCode;
        bool bExists = s_Device.read(Cell.m_asBitfield, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        // but to be sure...
        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // check write access
        if (!sc_access_lvl_check_write(ctx->access_levels, Cell.m_asVertex.m_scAccessLevels))
        {
            eResult = SC_RESULT_ERROR_NO_WRITE_RIGHTS;
            break;
        }

        // change type        
        sc_storage_snp::Cell AddressMask;
        SNP_BITFIELD_MASK_RESET(AddressMask.m_asBitfield.raw);
        SNP_BITFIELD_MASK_SET(AddressMask.m_asVertex.m_ID.m_asBitfield.raw);
        AddressMask.m_asVertex.m_uType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_asVertex.m_uType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
        AddressData.m_asVertex.m_ID.m_asBitfield = VertexID.m_asBitfield;

        sc_storage_snp::Cell DataMask;
        SNP_BITFIELD_MASK_RESET(DataMask.m_asBitfield.raw);
        DataMask.m_asVertex.m_scType = -1;

        sc_storage_snp::Cell DataData;
        DataData.m_asVertex.m_scType = (Cell.m_asVertex.m_scType & sc_type_element_mask) | (type & ~sc_type_element_mask);

        sc_storage_snp::Device::snpInstruction Instruction;
        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataMask = DataMask.m_asBitfield;
        Instruction.field.dataData = DataData.m_asBitfield;

        bExists = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        // but to be sure...
        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        eResult = SC_RESULT_OK;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_get_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels *result)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        // prevent working with device as soon as possible
        if (!result)
        {
            assert(0);
            break;
        }

        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // try to read this cell (including read access check)
        sc_storage_snp::Cell Cell;
        if (!snp_vertex_read(ctx, VertexID, Cell, eResult))
            break;

        (*result) = Cell.m_asVertex.m_scAccessLevels;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_set_access_levels(const sc_memory_context *ctx, sc_addr addr, sc_access_levels access_levels, sc_access_levels *new_value)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // does this element exist?
        if (!snp_vertex_find(VertexID))
            break;

        sc_storage_snp::Cell Cell;

        // if we are here it exists, right? just read it.
        snpErrorCode eErrorCode;
        bool bExists = s_Device.read(Cell.m_asBitfield, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        // but to be sure...
        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // check write access
        if (!sc_access_lvl_check_write(ctx->access_levels, Cell.m_asVertex.m_scAccessLevels))
        {
            eResult = SC_RESULT_ERROR_NO_WRITE_RIGHTS;
            break;
        }

        // change access level
        sc_storage_snp::Cell AddressMask;
        SNP_BITFIELD_DATA_SET(AddressMask.m_asBitfield.raw, 0);
        SNP_BITFIELD_DATA_SET(AddressMask.m_asVertex.m_ID.m_asBitfield.raw, ~0);
        AddressMask.m_asVertex.m_uType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_asVertex.m_uType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
        AddressData.m_asVertex.m_ID.m_asBitfield = VertexID.m_asBitfield;

        sc_storage_snp::Cell DataMask;
        SNP_BITFIELD_DATA_SET(DataMask.m_asBitfield.raw, 0);
        DataMask.m_asVertex.m_scAccessLevels = ~0;

        sc_storage_snp::Cell DataData;
        DataData.m_asVertex.m_scAccessLevels = sc_access_lvl_min(ctx->access_levels, access_levels);

        sc_storage_snp::Device::snpInstruction Instruction;
        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataMask = DataMask.m_asBitfield;
        Instruction.field.dataData = DataData.m_asBitfield;

        bExists = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        // but to be sure...
        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        if (new_value) (*new_value) = DataData.m_asVertex.m_scAccessLevels;
        eResult = SC_RESULT_OK;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_get_arc_begin(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        // prevent working with device as soon as possible
        assert(!result);
        if (!result)
        {
            eResult = SC_RESULT_ERROR_INVALID_PARAMS;
            break;
        }

        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // try to read this cell (including read access check)
        sc_storage_snp::Cell Cell;
        if (!snp_vertex_read(ctx, VertexID, Cell, eResult))
            break;

        if (!(Cell.m_asVertex.m_scType & sc_type_arc_mask))
        {
            eResult = SC_RESULT_ERROR_INVALID_TYPE;
            break;
        }

        // SC arc elements is transformed into vertex and 2 connected edges.
        //
        // Thus vertex element with arc type can has several input edges, but
        // only one from which is node type. Other ones with arc type represents
        // arc-in-arc connection.

        // 1. reset search flag for all edges
        sc_storage_snp::Cell AddressMask;
        SNP_BITFIELD_DATA_SET(AddressMask.m_asBitfield.raw, 0);
        AddressMask.m_asEdge.m_uType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_asEdge.m_uType = static_cast<uint8>(sc_storage_snp::CellType::EDGE);

        sc_storage_snp::Cell DataMask;
        SNP_BITFIELD_DATA_SET(DataMask.m_asBitfield.raw, 0);
        DataMask.m_asEdge.m_uSearch = snpBit(0);

        sc_storage_snp::Cell DataData;
        DataData.m_asEdge.m_uSearch = 0;

        sc_storage_snp::Device::snpInstruction Instruction;
        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataMask = DataMask.m_asBitfield;
        Instruction.field.dataData = DataData.m_asBitfield;

        snpErrorCode eErrorCode;
        bool bResult = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
        assert(bResult && eErrorCode == snpErrorCode::SUCCEEDED);

        // there's no edges in memory, it's impossible if we initially had arc element
        if (!bResult || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // 2. mark all edges which have specified ID2 (ie. all input edges)
        SNP_BITFIELD_DATA_SET(AddressMask.m_asEdge.m_ID2.m_asBitfield.raw, ~0);
        AddressData.m_asEdge.m_ID2.m_asBitfield = VertexID.m_asBitfield;
        DataData.m_asEdge.m_uSearch = snpBit(0);

        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataData = DataData.m_asBitfield;

        bResult = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
        assert(bResult && eErrorCode == snpErrorCode::SUCCEEDED);

        // there's no input edges in memory, it's impossible if we initially had arc element
        if (!bResult || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // 3. read them until find vertex with node type (it must be only one with this type)
        SNP_BITFIELD_DATA_SET(AddressMask.m_asEdge.m_ID2.m_asBitfield.raw, 0);
        AddressMask.m_asEdge.m_uSearch = snpBit(0);
        AddressData.m_asEdge.m_uSearch = snpBit(0);
        DataData.m_asEdge.m_uSearch = 0;

        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataData = DataData.m_asBitfield;

        while(s_Device.exec(true, snp::snpAssign, Instruction))
        {
            bool bRead = s_Device.read(Cell.m_asBitfield, &eErrorCode);
            assert(bRead && eErrorCode == snpErrorCode::SUCCEEDED);

            if (!bRead || eErrorCode != snpErrorCode::SUCCEEDED)
                continue;

            if (!snp_vertex_find(Cell.m_asEdge.m_ID1))
            {
                assert(!"error in graph structure, edge start vertex does not exist");
                continue;
            }

            bRead = s_Device.read(Cell.m_asBitfield, &eErrorCode);
            assert(bRead && eErrorCode == snpErrorCode::SUCCEEDED);

            if (!bRead || eErrorCode != snpErrorCode::SUCCEEDED)
                continue;

            // we are looking for vertex with node type
            if (sc_type_arc_mask & Cell.m_asVertex.m_scType)
                continue;

            (*result) = Cell.m_asVertex.m_ID.m_asAddr;
            eResult = SC_RESULT_OK;
            break;
        }
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_get_arc_end(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        // prevent working with device as soon as possible
        assert(!result);
        if (!result)
        {
            eResult = SC_RESULT_ERROR_INVALID_PARAMS;
            break;
        }

        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // try to read this cell (including read access check)
        sc_storage_snp::Cell Cell;
        if (!snp_vertex_read(ctx, VertexID, Cell, eResult))
            break;

        if (!(Cell.m_asVertex.m_scType & sc_type_arc_mask))
        {
            eResult = SC_RESULT_ERROR_INVALID_TYPE;
            break;
        }

        // SC arc elements is transformed into vertex and 2 connected edges.
        //
        // Thus vertex element with arc type can has only one output edge,
        // but it can be either node or arc vertex (represents arc-in-node and
        // arc-in-arc connections respectively)

        // 1. find this single edge
        sc_storage_snp::Cell AddressMask;
        SNP_BITFIELD_DATA_SET(AddressMask.m_asBitfield.raw, 0);
        SNP_BITFIELD_DATA_SET(AddressMask.m_asEdge.m_ID1.m_asBitfield.raw, ~0);
        AddressMask.m_asEdge.m_uType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_asEdge.m_uType = static_cast<uint8>(sc_storage_snp::CellType::EDGE);
        AddressData.m_asEdge.m_ID1.m_asBitfield = VertexID.m_asBitfield;

        sc_storage_snp::Device::snpInstruction Instruction;
        SNP_BITFIELD_DATA_SET(Instruction.raw, 0);
        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;

        snpErrorCode eErrorCode;
        bool bExists = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        // there's no edges in memory, it's impossible if we initially had arc element
        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // 2. read edge to obtain ID2
        bool bRead = s_Device.read(Cell.m_asBitfield, &eErrorCode);
        assert(bRead && eErrorCode == snpErrorCode::SUCCEEDED);

        if (!bRead || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        (*result) = Cell.m_asEdge.m_ID2.m_asAddr;
        eResult = SC_RESULT_OK;
        break;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_get_link_content(const sc_memory_context *ctx, sc_addr addr, sc_stream **stream)
{
    snp_enter_critical_section();

    if (*stream)
        *stream = nullptr;

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        // check input params
        if (!stream)
        {
            eResult = SC_RESULT_ERROR_INVALID_PARAMS;
            break;
        }

        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // try to find vertex by the specified identifier
        if (!snp_vertex_find(VertexID))
            break;

        sc_storage_snp::Cell Cell;

        // read it if exists
        snpErrorCode eErrorCode;
        bool bExists = s_Device.read(Cell.m_asBitfield, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // check vertex type - it must be link
        if (!(Cell.m_asVertex.m_scType & sc_type_link))
        {
            eResult = SC_RESULT_ERROR_INVALID_TYPE;
            break;
        }

        // check read access
        if (!sc_access_lvl_check_read(ctx->access_levels, Cell.m_asVertex.m_scAccessLevels))
        {
            eResult = SC_RESULT_ERROR_NO_READ_RIGHTS;
            break;
        }

        if (!Cell.m_asVertex.m_uLinkSize)
            break;

        std::vector<char> aLinkData;
        if (!snp_link_read(VertexID, Cell.m_asVertex.m_uLinkSize, aLinkData))
            break;

        if (Cell.m_asVertex.m_scType & sc_flag_link_self_container)
        {
            // the data from the device memory is the requested data itself
            *stream = sc_stream_memory_new(&aLinkData[0], aLinkData.size(), SC_STREAM_READ, SC_TRUE);
            eResult = SC_RESULT_OK;
        }
        else
        {
            sc_check_sum CheckSum;
            CheckSum.len = aLinkData.size();

            for (uint32 iIndex = 0; iIndex < aLinkData.size(); iIndex++)
                CheckSum.data[iIndex] = aLinkData[iIndex];

            eResult = sc_fs_storage_get_checksum_content(&CheckSum, stream);
        }
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_set_link_content(const sc_memory_context *ctx, sc_addr addr, const sc_stream *stream)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        // check input params
        if (!stream)
        {
            eResult = SC_RESULT_ERROR_INVALID_PARAMS;
            break;
        }

        sc_storage_snp::VertexID VertexID;
        VertexID.m_asAddr = addr;

        // try to find vertex by the specified identifier
        if (!snp_vertex_find(VertexID))
            break;

        sc_storage_snp::Cell Cell;

        // read it if exists
        snpErrorCode eErrorCode;
        bool bExists = s_Device.read(Cell.m_asBitfield, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // check vertex type - it must be link
        if (!(Cell.m_asVertex.m_scType & sc_type_link))
        {
            eResult = SC_RESULT_ERROR_INVALID_TYPE;
            break;
        }

        // check write access
        if (!sc_access_lvl_check_write(ctx->access_levels, Cell.m_asVertex.m_scAccessLevels))
        {
            eResult = SC_RESULT_ERROR_NO_WRITE_RIGHTS;
            break;
        }

        // at first we have to delete content if link already has any
        if (Cell.m_asVertex.m_uLinkSize)
        {
            std::vector<char> aLinkData;
            if (!snp_link_read(VertexID, Cell.m_asVertex.m_uLinkSize, aLinkData))
                break;

            // delete data from device memory (it can be data itself or just checksum, no matter)
            if (!snp_link_destroy(VertexID))
                break;

            // obtain checksum
            sc_check_sum CheckSum;
            if (Cell.m_asVertex.m_scType & sc_flag_link_self_container)
            {
                // the data from the device memory is the requested data itself
                // so we calculate checksum from it
                sc_stream *pStream = sc_stream_memory_new(&aLinkData[0], aLinkData.size(), SC_STREAM_READ, SC_FALSE);
                bool bChecksum = sc_link_calculate_checksum(stream, &CheckSum);
                sc_stream_free(pStream);

                if (!bChecksum)
                {
                    // link data was already destroyed
                    assert(0);
                    break;
                }
            }
            else
            {
                CheckSum.len = aLinkData.size();
                for (uint32 iIndex = 0; iIndex < aLinkData.size(); iIndex++)
                    CheckSum.data[iIndex] = aLinkData[iIndex];
            }

            STORAGE_CHECK_CALL(sc_fs_storage_remove_content_addr(VertexID.m_asAddr, &CheckSum));
            Cell.m_asVertex.m_uLinkSize = 0;
        }

        // then store data somewhere

        // find checksum for data from the stream
        sc_check_sum CheckSum;
        if (!sc_link_calculate_checksum(stream, &CheckSum))
        {
            // note that current content of the link was destroyed
            assert(0);
            break;
        }

        // get the data size
        uint32 iLength = 0;
        STORAGE_CHECK_CALL(sc_stream_get_length(stream, &iLength));

        std::vector<char> aDataToWrite;

        // if it's small (less than size of check sum)
        if (iLength < SC_CHECKSUM_LEN)
        {
            // then store it directly in device memory
            aDataToWrite.reserve(iLength);

            sc_uint32 iRead = 0;
            STORAGE_CHECK_CALL(sc_stream_read_data(stream, &aDataToWrite[0], iLength, &iRead));
            if (iRead != iLength)
            {
                // again, the current content of the link was destroyed - so there's assert
                assert(0);
                break;
            }

            // Nick: why should we do it? what purpose?
            sc_fs_storage_add_content_addr(VertexID.m_asAddr, &CheckSum);

            // update vertex type
            Cell.m_asVertex.m_scType |= sc_flag_link_self_container;
        }
        else
        {
            // otherwise calculate check sum, which will be stored in device memory
            // but actual data is going to external database
            eResult = sc_fs_storage_write_content(VertexID.m_asAddr, &CheckSum, stream);
            if (eResult != SC_RESULT_OK)
            {
                // again, the current content of the link was destroyed - so there's assert
                assert(0);
                break;
            }

            // and write checksum in device memory
            for (int iIndex = 0; iIndex < CheckSum.len; iIndex++)
                aDataToWrite.push_back(CheckSum.data[iIndex]);

            // update vertex type
            Cell.m_asVertex.m_scType &= ~sc_flag_link_self_container;
        }

        Cell.m_asVertex.m_uLinkSize = aDataToWrite.size();

        // actual writing
        const uint8 iSize = 4;
        union
        {
            uint32  m_asUint32;
            uint8   m_asUint8[iSize];
        } Value;

        bool bCreateError = false;
        uint8 iIndex = 0;

        while(true)
        {
            uint8 iByteIndex = iIndex % iSize;
            Value.m_asUint8[iByteIndex] = aDataToWrite[iIndex];

            bool bBreakLoop = (iIndex >= aDataToWrite.size() - 1);
            if (bBreakLoop || (iByteIndex == iSize - 1))
            {
                bCreateError = !snp_link_create(VertexID, Value.m_asUint32, iIndex / iSize);
                if (bCreateError)
                    break;

                Value.m_asUint32 = 0;
            }

            if (bBreakLoop)
                break;

            iIndex += 1;
        }

        if (bCreateError)
        {
            // note that there're data in external database was created.
            // also some of link vertices could be created earlier
            assert(0);
            break;
        }

        // update vertex type
        sc_storage_snp::Cell AddressMask;
        SNP_BITFIELD_MASK_RESET(AddressMask.m_asBitfield.raw);
        SNP_BITFIELD_MASK_SET(AddressMask.m_asVertex.m_ID.m_asBitfield.raw);
        AddressMask.m_asVertex.m_uType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_asVertex.m_uType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
        AddressData.m_asVertex.m_ID.m_asBitfield = VertexID.m_asBitfield;

        sc_storage_snp::Cell DataMask;
        SNP_BITFIELD_MASK_RESET(DataMask.m_asBitfield.raw);
        DataMask.m_asVertex.m_scType = -1;

        sc_storage_snp::Device::snpInstruction Instruction;
        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataMask = DataMask.m_asBitfield;
        Instruction.field.dataData = Cell.m_asBitfield;

        bExists = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        // but to be sure...
        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        eResult = SC_RESULT_OK;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

sc_result snp_element_find_link(const sc_memory_context *ctx, const sc_stream *stream, sc_addr **result, sc_uint32 *result_count)
{
    snp_enter_critical_section();

    sc_result eResult = SC_RESULT_ERROR;
    do
    {
        if (!stream || !result || !result_count)
        {
            eResult = SC_RESULT_ERROR_INVALID_PARAMS;
            break;
        }

        *result = nullptr;
        *result_count = 0;

        sc_check_sum CheckSum;
        if (!sc_link_calculate_checksum(stream, &CheckSum))
            break;

        sc_addr *pResult = nullptr;
        sc_uint32 iResultCount = 0;

        // Nick: who must delete pResult?
        if (sc_fs_storage_find_links_with_content(&CheckSum, &pResult, &iResultCount) != SC_RESULT_OK)
            break;

        if (!pResult || !iResultCount)
        {
            eResult = SC_RESULT_ERROR_NOT_FOUND;
            break;
        }

        sc_storage_snp::Cell Cell;
        sc_storage_snp::VertexID VertexID;

        // need to check read access
        std::vector<sc_addr> aPassedAddr;

        bool bReadError = false;
        for (uint32 iIndex = 0; iIndex < iResultCount; iIndex++)
        {
            VertexID.m_asAddr = pResult[iIndex];
            if (!snp_vertex_find(VertexID))
            {
                // data in fs_storage is corrupted
                assert(0);
                bReadError = true;
                break;
            }

            snpErrorCode eErrorCode;
            bool bExists = s_Device.read(Cell.m_asBitfield, &eErrorCode);
            assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

            if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
                break;

            // check read access
            if (sc_access_lvl_check_read(ctx->access_levels, Cell.m_asVertex.m_scAccessLevels))
                aPassedAddr.push_back(VertexID.m_asAddr);
        }

        if (bReadError)
            break;

        if (!aPassedAddr.size())
        {
            eResult = SC_RESULT_ERROR_NO_READ_RIGHTS;
            break;
        }

        *result = g_new0(sc_addr, aPassedAddr.size());
        for (uint32 iIndex = 0; iIndex < aPassedAddr.size(); iIndex++)
            (*result)[iIndex] = aPassedAddr[iIndex];

        (*result_count) = aPassedAddr.size();
        eResult = SC_RESULT_OK;
    }
    while(0);

    snp_leave_critical_section();
    return eResult;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods, no multithread guard is needed

void snp_enter_critical_section()
{
    // not implemented yet
}

void snp_leave_critical_section()
{
    // not implemented yet
}

bool snp_data_storage_init(const std::string &path, bool clear)
{
    (void)path;
    (void)clear;

    // not implemented yet
    return true;
}

void snp_data_storage_shutdown()
{
    // not implemented yet
}

sc_storage_snp::VertexID snp_vertex_id_obtain()
{
    union
    {
        sc_storage_snp::VertexID m_asVertexID;
        uint32 m_asUint32;
    } ID;

    static uint32 s_vertexID = 0;
    ID.m_asUint32 = s_vertexID++;

    return ID.m_asVertexID;
}

void snp_vertex_id_release(const sc_storage_snp::VertexID &vertex_id)
{
    (void)vertex_id;
    // nothing for now
}

bool snp_vertex_find(const sc_storage_snp::VertexID &vertex_id)
{
    // all elements are represented as vertices in snp graph
    // so just perform search instruction

    sc_storage_snp::Cell AddressMask;
    SNP_BITFIELD_MASK_RESET(AddressMask.m_asBitfield.raw);
    SNP_BITFIELD_MASK_SET(AddressMask.m_asVertex.m_ID.m_asBitfield.raw);
    AddressMask.m_asVertex.m_uType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_asVertex.m_uType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
    AddressData.m_asVertex.m_ID.m_asBitfield = vertex_id.m_asBitfield;

    sc_storage_snp::Device::snpInstruction Instruction;
    SNP_BITFIELD_DATA_SET(Instruction.raw, 0);
    Instruction.field.addressMask = AddressMask.m_asBitfield;
    Instruction.field.addressData = AddressData.m_asBitfield;

    snpErrorCode eErrorCode;
    bool bResult = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
    if (eErrorCode != snpErrorCode::SUCCEEDED)
    {
        // print error log accordingly to returned code
        // for now just assert in any case
        assert(0);
        return false;
    }
    return bResult;
}

bool snp_vertex_read(const sc_memory_context *ctx, const sc_storage_snp::VertexID &vertex_id, sc_storage_snp::Cell &cell, sc_result &result)
{
    bool bRead = false;
    do
    {
        result = SC_RESULT_ERROR;
        if (!snp_vertex_find(vertex_id))
            break;

        snpErrorCode eErrorCode;
        bool bExists = s_Device.read(cell.m_asBitfield, &eErrorCode);
        assert(bExists && eErrorCode == snpErrorCode::SUCCEEDED);

        if (!bExists || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // check read access
        if (!sc_access_lvl_check_read(ctx->access_levels, cell.m_asVertex.m_scAccessLevels))
        {
            result = SC_RESULT_ERROR_NO_READ_RIGHTS;
            break;
        }

        result = SC_RESULT_OK;
        bRead = true;
    }
    while(0);
    return bRead;
}

bool snp_vertex_create(const sc_storage_snp::VertexID &vertex_id, sc_type type, sc_access_levels access_levels)
{
    sc_storage_snp::Cell AddressMask;
    SNP_BITFIELD_DATA_SET(AddressMask.m_asBitfield.raw, 0);
    AddressMask.m_asVertex.m_uType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_asVertex.m_uType = static_cast<uint8>(sc_storage_snp::CellType::EMPTY);

    sc_storage_snp::Cell DataMask;
    SNP_BITFIELD_DATA_SET(DataMask.m_asBitfield.raw, ~0);

    sc_storage_snp::Cell DataData;
    SNP_BITFIELD_DATA_SET(DataData.m_asBitfield.raw, 0);
    DataData.m_asVertex.m_uType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
    DataData.m_asVertex.m_ID.m_asBitfield = vertex_id.m_asBitfield;
    DataData.m_asVertex.m_scType = type;
    DataData.m_asVertex.m_scAccessLevels = access_levels;

    sc_storage_snp::Device::snpInstruction Instruction;
    Instruction.field.addressMask = AddressMask.m_asBitfield;
    Instruction.field.addressData = AddressData.m_asBitfield;
    Instruction.field.dataMask = DataMask.m_asBitfield;
    Instruction.field.dataData = DataData.m_asBitfield;

    snpErrorCode eErrorCode;
    bool bCreated = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
    if (eErrorCode != snpErrorCode::SUCCEEDED)
    {
        // print error log accordingly to returned code
        // for now just assert in any case
        assert(0);
        return false;
    }
    return bCreated;
}

bool snp_edge_create(const sc_storage_snp::VertexID &vertex_id1, const sc_storage_snp::VertexID &vertex_id2)
{
    sc_storage_snp::Cell AddressMask;
    SNP_BITFIELD_DATA_SET(AddressMask.m_asBitfield.raw, 0);
    AddressMask.m_asEdge.m_uType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_asEdge.m_uType = static_cast<uint8>(sc_storage_snp::CellType::EMPTY);

    sc_storage_snp::Cell DataMask;
    SNP_BITFIELD_DATA_SET(DataMask.m_asBitfield.raw, ~0);

    sc_storage_snp::Cell DataData;
    SNP_BITFIELD_DATA_SET(DataData.m_asBitfield.raw, 0);
    DataData.m_asEdge.m_uType = static_cast<uint8>(sc_storage_snp::CellType::EDGE);
    DataData.m_asEdge.m_ID1.m_asBitfield = vertex_id1.m_asBitfield;
    DataData.m_asEdge.m_ID2.m_asBitfield = vertex_id2.m_asBitfield;

    sc_storage_snp::Device::snpInstruction Instruction;
    Instruction.field.addressMask = AddressMask.m_asBitfield;
    Instruction.field.addressData = AddressData.m_asBitfield;
    Instruction.field.dataMask = DataMask.m_asBitfield;
    Instruction.field.dataData = DataData.m_asBitfield;

    snpErrorCode eErrorCode;
    bool bCreated = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
    if (eErrorCode != snpErrorCode::SUCCEEDED)
    {
        // print error log accordingly to returned code
        // for now just assert in any case
        assert(0);
        return false;
    }
    return bCreated;
}

bool snp_link_create(const sc_storage_snp::VertexID &vertex_id, uint32 checksum, uint8 index)
{
    sc_storage_snp::Cell AddressMask;
    SNP_BITFIELD_DATA_SET(AddressMask.m_asBitfield.raw, 0);
    AddressMask.m_asLink.m_uType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_asLink.m_uType = static_cast<uint8>(sc_storage_snp::CellType::EMPTY);

    sc_storage_snp::Cell DataMask;
    SNP_BITFIELD_DATA_SET(DataMask.m_asBitfield.raw, ~0);

    sc_storage_snp::Cell DataData;
    SNP_BITFIELD_DATA_SET(DataData.m_asBitfield.raw, 0);
    DataData.m_asLink.m_uType = static_cast<uint8>(sc_storage_snp::CellType::LINK);
    DataData.m_asLink.m_ID = vertex_id;
    DataData.m_asLink.m_uChecksum = checksum;
    DataData.m_asLink.m_uLinkIndex = index;

    sc_storage_snp::Device::snpInstruction Instruction;
    Instruction.field.addressMask = AddressMask.m_asBitfield;
    Instruction.field.addressData = AddressData.m_asBitfield;
    Instruction.field.dataMask = DataMask.m_asBitfield;
    Instruction.field.dataData = DataData.m_asBitfield;

    snpErrorCode eErrorCode;
    bool bCreated = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
    if (eErrorCode != snpErrorCode::SUCCEEDED)
    {
        // print error log accordingly to returned code
        // for now just assert in any case
        assert(0);
        return false;
    }
    return bCreated;
}

bool snp_link_destroy(const sc_storage_snp::VertexID &vertex_id)
{
    sc_storage_snp::Cell AddressMask;
    SNP_BITFIELD_MASK_RESET(AddressMask.m_asBitfield.raw);
    SNP_BITFIELD_MASK_SET(AddressMask.m_asLink.m_ID.m_asBitfield.raw);
    AddressMask.m_asLink.m_uType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_asLink.m_uType = static_cast<uint8>(sc_storage_snp::CellType::LINK);
    AddressData.m_asLink.m_ID.m_asBitfield = vertex_id.m_asBitfield;

    sc_storage_snp::Cell DataMask;
    SNP_BITFIELD_MASK_RESET(DataMask.m_asBitfield.raw);
    DataMask.m_asLink.m_uType = 0b11;

    // only reset cell type to 'empty' - it's enough to release cell
    sc_storage_snp::Cell DataData;
    DataData.m_asVertex.m_uType = static_cast<uint8>(sc_storage_snp::CellType::EMPTY);

    // run instruction
    sc_storage_snp::Device::snpInstruction Instruction;
    Instruction.field.addressMask = AddressMask.m_asBitfield;
    Instruction.field.addressData = AddressData.m_asBitfield;
    Instruction.field.dataMask = DataMask.m_asBitfield;
    Instruction.field.dataData = DataData.m_asBitfield;

    snpErrorCode eErrorCode;
    bool bFound = s_Device.exec(false, snp::snpAssign, Instruction, &eErrorCode);
    if (eErrorCode != snpErrorCode::SUCCEEDED)
    {
        assert(0);
        return false;
    }
    return bFound;
}

bool snp_link_read(const sc_storage_snp::VertexID &vertex_id, uint8 size, std::vector<char> &output)
{
    if (!size) return false;
    output.clear();

    sc_storage_snp::Cell AddressMask;
    SNP_BITFIELD_MASK_RESET(AddressMask.m_asBitfield.raw);
    SNP_BITFIELD_MASK_SET(AddressMask.m_asLink.m_ID.m_asBitfield.raw);
    AddressMask.m_asLink.m_uType = -1;
    AddressMask.m_asLink.m_uLinkIndex = -1;

    sc_storage_snp::Cell AddressData;
    AddressData.m_asLink.m_uType = static_cast<uint8>(sc_storage_snp::CellType::LINK);
    AddressData.m_asLink.m_ID.m_asBitfield = vertex_id.m_asBitfield;

    sc_storage_snp::Cell DataMask;
    SNP_BITFIELD_MASK_RESET(DataMask.m_asBitfield.raw);

    // run instruction (we can skip data-data field as it's not used)
    sc_storage_snp::Device::snpInstruction Instruction;
    Instruction.field.addressMask = AddressMask.m_asBitfield;
    Instruction.field.dataMask = DataMask.m_asBitfield;

    const uint8 iSize = 4;
    union
    {
        uint32  m_asUint32;
        uint8   m_asUint8[iSize];
    } Value;

    sc_storage_snp::Cell Cell;
    bool bReadError = false;

    uint8 iNumberOfCells = (size - 1) / iSize + 1;
    for (uint8 iIndex = 0; iIndex < iNumberOfCells; iIndex++)
    {
        AddressData.m_asLink.m_uLinkIndex = iIndex;
        Instruction.field.addressData = AddressData.m_asBitfield;

        snpErrorCode eErrorCode;
        bReadError = !s_Device.exec(false, snp::snpAssign, Instruction, &eErrorCode);
        if (bReadError || eErrorCode != snpErrorCode::SUCCEEDED)
        {
            assert(0);
            break;
        }

        bReadError = !s_Device.read(Cell.m_asBitfield, &eErrorCode);
        if (bReadError || eErrorCode != snpErrorCode::SUCCEEDED)
        {
            assert(0);
            break;
        }

        Value.m_asUint32 = Cell.m_asLink.m_uChecksum;
        for (uint8 iByteIndex = 0; iByteIndex < iSize; iByteIndex++)
            output.push_back(Value.m_asUint8[iByteIndex]);
    }

    if (bReadError)
    {
        output.clear();
        return false;
    }

    // delete redundant bytes
    output.erase(output.begin() + size, output.end());
    return true;
}

#endif //ENABLE_HARDWARE_STORAGE
