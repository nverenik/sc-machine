/*
-----------------------------------------------------------------------------
This source file is part of OSTIS (Open Semantic Technology for Intelligent Systems)
For the latest info, see http://www.ostis.net

Copyright (c) 2010-2014 OSTIS

OSTIS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OSTIS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OSTIS.  If not, see <http://www.gnu.org/licenses/>.
-----------------------------------------------------------------------------
*/

#ifndef _sctpCommand_h_
#define _sctpCommand_h_

#include <QObject>
#include <QMutex>
#include <QByteArray>

#include <set>

#include "sctpTypes.h"


class QIODevice;

/*! Base class for sctp commands.
 * It provide command packing/unpacking to binary data.
 * All types of command processed there in one function by using switch/case
 * statement to provide maximum speed.
 */
class sctpCommand : public QObject
{
    Q_OBJECT

    friend class sctpEventManager;

public:
    explicit sctpCommand(QObject *parent = 0);
    virtual ~sctpCommand();


    void init();
    void shutdown();

    /*! Read and process command from buffer
     * @param inDevice Pointer to device for input data reading
     * @param outDevice Pointer to device for output data writing
     */
    eSctpErrorCode processCommand(QIODevice *inDevice, QIODevice *outDevice);

    /*! Wait while specified number of bytes will be available in specified data stream
     * @param stream Pointer to data stream to wait available bytes
     * @param bytesNum Number of waiting bytes
     * @returns If specified \b bytesNum available in \b stream, then return true; otherwise return false
     */
    bool waitAvailableBytes(QIODevice *stream, quint32 bytesNum);

    /*! Writes result header into output device
     * @param cmdCode Code of processed command
     * @param cmdId Processed command id
     * @param resCode Return result code
     * @param resSize Size of result data (in bytes)
     * @param outDevice Pointer to output device (use to write header)
     */
    void writeResultHeader(eSctpCommandCode cmdCode, quint32 cmdId, eSctpResultCode resCode, quint32 resSize, QIODevice *outDevice);

    //! Return size of command header in bytes
    static quint32 cmdHeaderSize();
    
protected:
    //! Type of command processing function
    typedef eSctpErrorCode (*fProcessCommand)(quint8 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);

private:
    // ------- processing functions ----------
    eSctpErrorCode processCheckElement(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processGetElementType(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processElementErase(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processCreateNode(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processCreateLink(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processCreateArc(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);


    eSctpErrorCode processGetLinkContent(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processFindLinks(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processSetLinkContent(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processIterateElements(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);

    // events
    eSctpErrorCode processCreateEvent(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processDestroyEvent(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processEmitEvent(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);

    eSctpErrorCode processFindElementBySysIdtf(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processSetSysIdtf(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);
    eSctpErrorCode processStatistics(quint32 cmdFlags, quint32 cmdId, QDataStream *params, QIODevice *outDevice);

protected:
    sc_result processEventEmit(quint32 eventId, sc_addr el_addr, sc_addr arg_addr);

private:
    //! Mutex to synchronize data sending to client read/writes
    QMutex mSendMutex;
    //! Array that contains server commands data to send to client
    QByteArray mSendData;
    //! Number of events to send to client
    quint32 mSendEventsCount;

    //! List of event created by this command handler
    typedef std::set<tEventId> tEventsSet;
    tEventsSet mEventsSet;

signals:
    
public slots:
    
};



#endif // SCTP_COMMAND_H
