#include "sc_storage_snp_glue.h"

#ifdef ENABLE_HARDWARE_STORAGE

#include <string>
#include <snp/snp.h>
using snp::snpErrorCode;

#include "sc_storage_snp_types.h"
#include "../../sc_memory_private.h"

// create static device instance with predefined bitwidth
static sc_storage_snp::Device s_Device;

static std::string s_sDumpFilePath;
static std::string s_sDumpFileName = "snp.dump";

#define snp_clear_struct(__struct__) memset(&__struct__, 0, sizeof(__struct__))

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
            snpBitfieldSet(Instruction.field.addressMask.raw, 0);
            snpBitfieldSet(Instruction.field.dataMask.raw, ~0);
            snpBitfieldSet(Instruction.field.dataData.raw, 0);
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
        // all its output edges
        //
        // IMPORTANT: if cell layout is changed that it's possible we will need
        // separated instruction execution to delete output edges

        sc_storage_snp::Cell AddressMask;
        snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
        snpBitfieldSet(AddressMask.m_asVertex.m_ID.m_asBitfield.raw, ~0);

        sc_storage_snp::Cell AddressData;
        AddressData.m_asVertex.m_ID.m_asBitfield = VertexID.m_asBitfield;

        // fill matched cells with zeros
        sc_storage_snp::Device::snpInstruction Instruction;
        snpBitfieldSet(Instruction.raw, 0);
        snpBitfieldSet(Instruction.field.dataMask.raw, ~0);
        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;

        s_Device.exec(false, snp::snpAssign, Instruction, &eErrorCode);
        if (eErrorCode != snpErrorCode::SUCCEEDED)
        {
            assert(0);
            break;
        }

        // now delete all input edges
        snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
        snpBitfieldSet(AddressMask.m_asEdge.m_ID2.m_asBitfield.raw, ~0);
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
        snpBitfieldSet(VertexID.m_asBitfield.raw, 0);
    }

    snp_leave_critical_section();
    return VertexID.m_asAddr;
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
        snpBitfieldSet(VertexID.m_asBitfield.raw, 0);
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
        snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
        snpBitfieldSet(AddressMask.m_asVertex.m_ID.m_asBitfield.raw, ~0);
        AddressMask.m_uiType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
        AddressData.m_asVertex.m_ID.m_asBitfield = VertexID.m_asBitfield;

        sc_storage_snp::Cell DataMask;
        snpBitfieldSet(DataMask.m_asBitfield.raw, 0);
        DataMask.m_asVertex.m_scType = ~0;

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

sc_result snp_element_arc_get_begin(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
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
        snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
        AddressMask.m_uiType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::EDGE);

        sc_storage_snp::Cell DataMask;
        snpBitfieldSet(DataMask.m_asBitfield.raw, 0);
        DataMask.m_uiSearch[0] = snpBit(0);

        sc_storage_snp::Cell DataData;
        DataData.m_uiSearch[0] = 0;

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
        snpBitfieldSet(AddressMask.m_asEdge.m_ID2.m_asBitfield.raw, ~0);
        AddressData.m_asEdge.m_ID2.m_asBitfield = VertexID.m_asBitfield;
        DataData.m_uiSearch[0] = snpBit(0);

        Instruction.field.addressMask = AddressMask.m_asBitfield;
        Instruction.field.addressData = AddressData.m_asBitfield;
        Instruction.field.dataData = DataData.m_asBitfield;

        bResult = s_Device.exec(true, snp::snpAssign, Instruction, &eErrorCode);
        assert(bResult && eErrorCode == snpErrorCode::SUCCEEDED);

        // there's no input edges in memory, it's impossible if we initially had arc element
        if (!bResult || eErrorCode != snpErrorCode::SUCCEEDED)
            break;

        // 3. read them until find vertex with node type (it must be only one with this type)
        snpBitfieldSet(AddressMask.m_asEdge.m_ID2.m_asBitfield.raw, 0);
        AddressMask.m_uiSearch[0] = snpBit(0);
        AddressData.m_uiSearch[0] = snpBit(0);
        DataData.m_uiSearch[0] = 0;

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

sc_result snp_element_arc_get_end(const sc_memory_context *ctx, sc_addr addr, sc_addr *result)
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
        snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
        snpBitfieldSet(AddressMask.m_asEdge.m_ID1.m_asBitfield.raw, ~0);
        AddressMask.m_uiType = 0b11;

        sc_storage_snp::Cell AddressData;
        AddressData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::EDGE);
        AddressData.m_asEdge.m_ID1.m_asBitfield = VertexID.m_asBitfield;

        sc_storage_snp::Device::snpInstruction Instruction;
        snpBitfieldSet(Instruction.raw, 0);
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

////////////////////////////////////////////////////////////////////////////////
// Private methods, no multithread guard is needed

void snp_enter_critical_section()
{
    // nothing for now
}

void snp_leave_critical_section()
{
    // nothing for now
}

bool snp_data_storage_init(const std::string &path, bool clear)
{
    (void)path;
    (void)clear;
    return true;
}

void snp_data_storage_shutdown()
{
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
    snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
    snpBitfieldSet(AddressMask.m_asVertex.m_ID.m_asBitfield.raw, ~0);
    AddressMask.m_uiType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
    AddressData.m_asVertex.m_ID.m_asBitfield = vertex_id.m_asBitfield;

    sc_storage_snp::Device::snpInstruction Instruction;
    snpBitfieldSet(Instruction.raw, 0);
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
    snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
    AddressMask.m_uiType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::EMPTY);

    sc_storage_snp::Cell DataMask;
    snpBitfieldSet(DataMask.m_asBitfield.raw, ~0);

    sc_storage_snp::Cell DataData;
    snpBitfieldSet(DataData.m_asBitfield.raw, 0);
    DataData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::VERTEX);
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
    snpBitfieldSet(AddressMask.m_asBitfield.raw, 0);
    AddressMask.m_uiType = 0b11;

    sc_storage_snp::Cell AddressData;
    AddressData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::EMPTY);

    sc_storage_snp::Cell DataMask;
    snpBitfieldSet(DataMask.m_asBitfield.raw, ~0);

    sc_storage_snp::Cell DataData;
    snpBitfieldSet(DataData.m_asBitfield.raw, 0);
    DataData.m_uiType = static_cast<uint8>(sc_storage_snp::CellType::EDGE);
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

#endif //ENABLE_HARDWARE_STORAGE
