/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef COMMHISTORY_CONVERSATIONMODEL_P_H
#define COMMHISTORY_CONVERSATIONMODEL_P_H

#include "eventmodel_p.h"
#include "conversationmodel.h"
#include "group.h"
#include <QSet>

class QSqlQuery;

namespace CommHistory
{

class ConversationModelPrivate : public EventModelPrivate {
public:
    Q_OBJECT
    Q_DECLARE_PUBLIC(ConversationModel)

    ConversationModelPrivate(EventModel *model);

    bool acceptsEvent(const Event &event) const;
    QSqlQuery buildQuery() const;
    bool isModelReady() const;

public Q_SLOTS:
    virtual void eventsReceivedSlot(int start, int end, QList<CommHistory::Event> events);
    virtual void modelUpdatedSlot(bool successful);
    void groupsAddedSlot(const QList<int> &groupIds);
    void groupsDeletedSlot(const QList<int> &groupIds);

public:
    QSet<int> filterGroupIds;
    Event::EventType filterType;
    QString filterAccount;
    Event::EventDirection filterDirection;
    bool allGroups;
};

}

#endif
