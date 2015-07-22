/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd. <matthew.vogt@jollamobile.com>
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

#include <QSqlQuery>
#include <QSqlError>

#include "databaseio_p.h"
#include "commhistorydatabase.h"
#include "eventmodel_p.h"
#include "contactlistener.h"

#include "recentcontactsmodel.h"
#include "debug.h"

namespace CommHistory {

using namespace CommHistory;

static int eventContact(const Event &event)
{
    return event.recipients().contactIds().value(0);
}

class RecentContactsModelPrivate : public EventModelPrivate {
public:
    Q_DECLARE_PUBLIC(RecentContactsModel);

    RecentContactsModelPrivate(EventModel *model)
        : EventModelPrivate(model),
          requiredProperty(RecentContactsModel::NoPropertyRequired)
    {
        setResolveContacts(true);
    }

    virtual bool acceptsEvent(const Event &event) const;
    virtual bool fillModel(int start, int end, QList<Event> events);
    virtual void prependEvents(QList<Event> events);

    virtual void slotContactInfoChanged(const RecipientList &recipients);
    virtual void slotContactChanged(const RecipientList &recipients);

private:
    int requiredProperty;
};

bool RecentContactsModelPrivate::acceptsEvent(const Event &event) const
{
    // Contact must be resolved before we can do anything, so just accept
    Q_UNUSED(event);
    return true;
}

bool RecentContactsModelPrivate::fillModel(int start, int end, QList<Event> events)
{
    Q_Q(RecentContactsModel);
    Q_UNUSED(start);
    Q_UNUSED(end);

    // This model doesn't fetchMore, so fill is only called once. We can use the prepend logic to get
    // the right contact behaviors.
    prependEvents(events);

    emit q->resolvingChanged();
    modelUpdatedSlot(true);
    return true;
}

void RecentContactsModelPrivate::slotContactInfoChanged(const RecipientList &recipients)
{
    EventModelPrivate::slotContactInfoChanged(recipients);

    // XXX This signal isn't emitted for all changes, and we don't have
    // access to the full list of accounts from here.. ugh.
#if 0
    bool hasAddressType[3] = { false, false, false };
    foreach (const ContactAddress &address, contactAddresses) {
        Q_ASSERT((address.type >= ContactListener::IMAccountType) && (address.type <= ContactListener::EmailAddressType));
        hasAddressType[address.type - 1] = true;
    }

    QSet<quint32> * const typeSet[3] = { &imContacts, &phoneContacts, &emailContacts };
    for (int i = 0; i < 3; ++i) {
        if (hasAddressType[i]) {
            typeSet[i]->insert(localId);
        } else {
            typeSet[i]->remove(localId);
        }
    }
#endif

    // FIXME: Contact updates can result in the model being wrong. But, because
    // unwanted events are discarded, that's difficult to solve without re-querying.
}

void RecentContactsModelPrivate::slotContactChanged(const RecipientList &recipients)
{
    EventModelPrivate::slotContactChanged(recipients);

    // If any of our events no longer resolve to a contact, remove them
    int rowCount = eventRootItem->childCount();
    for (int row = 0; row < rowCount; ) {
        const Event &existing(eventRootItem->eventAt(row));
        if (existing.contactRecipients().isEmpty()) {
            deleteFromModel(existing.id());
            --rowCount;
        } else {
            ++row;
        }
    }
}

void RecentContactsModelPrivate::prependEvents(QList<Event> events)
{
    Q_Q(RecentContactsModel);

    // Ensure the new events represent different contacts
    QList<Event> newEvents;
    QSet<int> newContactIds;
    foreach (const Event &event, events) {
        const int eventContactId = eventContact(event);
        if (eventContactId && !newContactIds.contains(eventContactId)) {
            newContactIds.insert(eventContactId);
            newEvents.append(event);

            // Don't add any more events than we can present
            if (newEvents.count() == queryLimit) {
                break;
            }
        }
    }

    if (newEvents.isEmpty())
        return;

    QSet<int> removeSet;

    // Does the new event replace an existing event?
    const int rowCount = eventRootItem->childCount();
    for (int row = 0; row < rowCount; ++row) {
        const Event &existing(eventRootItem->eventAt(row));
        if (newContactIds.contains(eventContact(existing))) {
            removeSet.insert(row);
        }
    }

    // Do we need to remove the final event(s) to maintain the limit?
    if (queryLimit) {
        int trimCount = rowCount + newEvents.count() - removeSet.count() - queryLimit;
        int removeIndex = rowCount - 1;
        while (trimCount > 0) {
            while (removeSet.contains(removeIndex)) {
                --removeIndex;
            }
            if (removeIndex < 0) {
                break;
            } else {
                removeSet.insert(removeIndex);
                --removeIndex;
                --trimCount;
            }
        }
    }

    // Remove the rows that have been made obsolete
    QList<int> removeIndices = removeSet.toList();
    qSort(removeIndices);

    int count;
    while ((count = removeIndices.count()) != 0) {
        int end = removeIndices.last();
        int consecutiveCount = 1;
        for ( ; (count - consecutiveCount) > 0; ++consecutiveCount) {
            if (removeIndices.at(count - 1 - consecutiveCount) != (end - consecutiveCount)) {
                break;
            }
        }

        removeIndices = removeIndices.mid(0, count - consecutiveCount);

        int start = (end - consecutiveCount + 1);
        q->beginRemoveRows(QModelIndex(), start, end);
        while (end >= start) {
            eventRootItem->removeAt(end);
            --end;
        }
        q->endRemoveRows();
    }

    // Insert the new events at the start
    int start = 0;
    q->beginInsertRows(QModelIndex(), start, newEvents.count() - 1);
    QList<Event>::const_iterator it = newEvents.constBegin(), end = newEvents.constEnd();
    for ( ; it != end; ++it) {
        eventRootItem->insertChildAt(start++, new EventTreeItem(*it, eventRootItem));
    }
    q->endInsertRows();
}

RecentContactsModel::RecentContactsModel(QObject *parent)
    : EventModel(*new RecentContactsModelPrivate(this), parent)
{
}

RecentContactsModel::~RecentContactsModel()
{
}

int RecentContactsModel::requiredProperty() const
{
    Q_D(const RecentContactsModel);
    return d->requiredProperty;
}

void RecentContactsModel::setRequiredProperty(int requiredProperty)
{
    Q_D(RecentContactsModel);
    d->requiredProperty = requiredProperty;
    if (requiredProperty != RecentContactsModel::NoPropertyRequired)
        qWarning() << "requiredProperty not supported in RecentContactsModel";
}

bool RecentContactsModel::resolving() const
{
    Q_D(const RecentContactsModel);

    return !d->isReady || (d->addResolver && d->addResolver->isResolving()) ||
           (d->receiveResolver && d->receiveResolver->isResolving());
}

bool RecentContactsModel::getEvents()
{
    Q_D(RecentContactsModel);

    beginResetModel();
    d->clearEvents();
    endResetModel();

    QString limitClause;
    if (d->queryLimit) {
        // Default to twice the configured limit, because some of the addresses may
        // resolve to the same final contact
        limitClause = QString::fromLatin1(" LIMIT %1").arg(2 * d->queryLimit);
    }

    QString q = DatabaseIOPrivate::eventQueryBase() + QString::fromLatin1(
" WHERE Events.id IN ("
  " SELECT lastId FROM ("
    " SELECT max(id) AS lastId, max(endTime) FROM Events"
    " JOIN ("
      " SELECT remoteUid, localUid, max(endTime) AS lastEventTime FROM Events"
      " GROUP BY remoteUid, localUid"
      " ORDER BY lastEventTime DESC %1"
    " ) AS LastEvent ON Events.endTime = LastEvent.lastEventTime"
                   " AND Events.remoteUid = LastEvent.remoteUid"
                   " AND Events.localUid = LastEvent.localUid"
    " GROUP BY Events.remoteUid, Events.localUid"
    " ORDER BY max(endTime) DESC"
  " )"
" )"
" ORDER BY Events.endTime DESC").arg(limitClause);

    QSqlQuery query = DatabaseIOPrivate::instance()->createQuery();
    if (!query.prepare(q)) {
        qWarning() << "Failed to execute query";
        qWarning() << query.lastError();
        qWarning() << query.lastQuery();
        return false;
    }

    bool re = d->executeQuery(query);
    if (re)
        emit resolvingChanged();
    return re;

}

} // namespace CommHistory
