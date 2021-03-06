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
#include "sctpEventManager.h"
#include "sctpCommand.h"

sctpEventManager* sctpEventManager::msInstance = 0;

sctpEventManager::sctpEventManager()
    : mLastEventId(0)
{
    Q_ASSERT(msInstance == 0);
    msInstance = this;
}

sctpEventManager::~sctpEventManager()
{
    msInstance = 0;
}

sctpEventManager* sctpEventManager::getSingleton()
{
    return msInstance;
}

bool sctpEventManager::initialize()
{
    QMutexLocker locker(&mEventsMutex);
}

void sctpEventManager::shutdown()
{
    QMutexLocker locker(&mEventsMutex);

    tScEventsMap::iterator it, itEnd = mEvents.end();
    for (it = mEvents.begin(); it != itEnd; ++it)
    {
        sEventData *evt = it->second;
        sc_event_destroy(evt->event);
        delete evt;
    }

    mEvents.clear();
}

bool sctpEventManager::createEvent(sc_event_type type, sc_addr addr, sctpCommand *cmd, tEventId &event)
{
    QMutexLocker locker(&mEventsMutex);


    if (!_getAvailableEventId(event))
        return false;

    sEventData *evt = new sEventData();

    evt->cmd = cmd;
    evt->id = event;
    evt->event = sc_event_new(addr, type, event, &sctpEventManager::_eventsCallback, 0);

    Q_ASSERT(mEvents.find(evt->id) == mEvents.end());

    mEvents[evt->id] = evt;

    return true;
}

bool sctpEventManager::destroyEvent(tEventId event)
{
    QMutexLocker locker(&mEventsMutex);

    tScEventsMap::iterator it = mEvents.find(event);

    if (it == mEvents.end())
        return false;

    Q_ASSERT(it->second->id == event);

    if (sc_event_destroy(it->second->event) != SC_RESULT_OK)
        return false;

    mEvents.erase(it);

    return true;
}

bool sctpEventManager::_getAvailableEventId(tEventId &eventId)
{
    tEventId start = mLastEventId;
    eventId = start + 1;

    while (eventId != start && (mEvents.find(eventId) != mEvents.end()))
        ++eventId;

    return eventId != start;
}


sc_result sctpEventManager::_eventsCallback(const sc_event *event, sc_addr arg)
{
    QMutexLocker locker(&sctpEventManager::msInstance->mEventsMutex);

    Q_ASSERT(event != 0);

    tScEventsMap::iterator it = sctpEventManager::msInstance->mEvents.find(sc_event_get_id(event));
    if (it == sctpEventManager::msInstance->mEvents.end())
        return SC_RESULT_ERROR_INVALID_STATE;

    sEventData *evt = it->second;
    Q_ASSERT(evt && evt->cmd);
    Q_ASSERT(event == evt->event);

    evt->cmd->processEventEmit(evt->id, sc_event_get_element(event), arg);

    return SC_RESULT_OK;
}
