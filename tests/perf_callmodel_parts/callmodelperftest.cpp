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

#include <QtTest/QtTest>
#include <QDateTime>
#include <QDBusConnection>
#include <cstdlib>
#include "callmodelperftest.h"
#include "common.h"
#include <iterator>
#include <QElapsedTimer>
#include <QThread>

namespace {

template <class RandomAccessIterator>
void random_shuffle (RandomAccessIterator first, RandomAccessIterator last)
{
    typename std::iterator_traits<RandomAccessIterator>::difference_type i, n;
    n = (last-first);
    for (i=n-1; i>0; --i) {
        qSwap(first[i],first[qrand() % (i+1)]);
    }
}

}

using namespace CommHistory;

void CallModelPerfTest::initTestCase()
{
    logFile = 0;

    qsrand( QDateTime::currentDateTime().toTime_t() );
}

void CallModelPerfTest::init()
{
}

void CallModelPerfTest::prepare()
{
    const int events = 3000;
    const int contacts = 500;
    const int selected = 500;

    int commitBatchSize = 100;

    EventModel addModel;
    QDateTime when = QDateTime::currentDateTime();

    qDebug() << __FUNCTION__ << "- Creating" << contacts << "new contacts";

    QList<QPair<QString, QPair<QString, QString> > > contactDetails;

    int ci = remoteUids.count();
    while(ci < contacts) {
        QString phoneNumber;
        do {
            phoneNumber = QString().setNum(qrand() % 10000000);
        } while (remoteUids.contains(phoneNumber));
        remoteUids << phoneNumber;
        contactIndices << ci;
        ci++;

        contactDetails.append(qMakePair(QString("Test Contact %1").arg(ci), qMakePair(phoneNumber, QString())));

        if(ci % commitBatchSize == 0 && ci < contacts) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "contacts (" << ci << "/" << contacts << ")";
            addTestContacts(contactDetails);
            contactDetails.clear();
        }
    }
    if (!contactDetails.isEmpty()) {
        qDebug() << __FUNCTION__ << "- adding rest of the contacts ("
                 << ci << "/" << contacts << ")";
        addTestContacts(contactDetails);
        contactDetails.clear();
    }

    // Randomize the contact indices
    random_shuffle(contactIndices.begin(), contactIndices.end());

    qDebug() << __FUNCTION__ << "- Creating" << events << "new events";

    QList<Event> eventList;

    int ei = 0;
    while(ei < events) {
        ei++;

        Event::EventDirection direction;
        bool isMissed = false;

        if(qrand() % 2 > 0) {
            direction = Event::Inbound;
            isMissed = (qrand() % 2 > 0);
        } else {
            direction = Event::Outbound;
        }

        Event e;
        e.setType(Event::CallEvent);
        e.setDirection(direction);
        e.setGroupId(-1);
        e.setStartTime(when.addSecs(ei));
        e.setEndTime(when.addSecs(ei));
        e.setLocalUid(RING_ACCOUNT);
        e.setRecipients(Recipient(RING_ACCOUNT, remoteUids.at(contactIndices.at(qrand() % selected))));
        e.setFreeText("");
        e.setIsDraft(false);
        e.setIsMissedCall(isMissed);

        eventList << e;

        if(ei % commitBatchSize == 0 && ei != events) {
            qDebug() << __FUNCTION__ << "- adding" << commitBatchSize
                << "events (" << ei << "/" << events << ")";
            QVERIFY(addModel.addEvents(eventList, false));
            eventList.clear();
        }
    }

    QVERIFY(addModel.addEvents(eventList, false));
    qDebug() << __FUNCTION__ << "- adding rest of the events ("
        << ei << "/" << events << ")";
    eventList.clear();
}

void CallModelPerfTest::execute()
{
    const bool resolve = true;

    int sum = 0;
    QList<int> times;

    int iterations = 1;

    logFile = new QFile("libcommhistory-performance-test.log");
    if(!logFile->open(QIODevice::Append)) {
        qDebug() << "!!!! Failed to open log file !!!!";
        logFile = 0;
    }

    QDateTime startTime = QDateTime::currentDateTime();

    qDebug() << __FUNCTION__ << "- Fetching events." << iterations << "iterations";
    for(int i = 0; i < iterations; i++) {

        CallModel fetchModel;
        bool result = false;

        fetchModel.setResolveContacts(resolve);
        fetchModel.setFilter(CallModel::SortByContact);

        QTime time;
        time.start();

        result = fetchModel.getEvents();
        QVERIFY(result);

        if (!fetchModel.isReady()) {
            QEventLoop loop;
            connect(&fetchModel, SIGNAL(modelReady(bool)), &loop, SLOT(quit()));
            loop.exec();
        }

        int elapsed = time.elapsed();
        times << elapsed;
        sum += elapsed;
        qDebug("Time elapsed: %d ms", elapsed);

        QVERIFY(fetchModel.rowCount() > 0);

        waitForIdle();
    }

    if(logFile) {
        QTextStream out(logFile);

        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << ": "
            << metaObject()->className() << "::" << QTest::currentTestFunction() << "("
            << QTest::currentDataTag() << ", " << iterations << " iterations)"
            << "\n";

        for (int i = 0; i < times.size(); i++) {
            out << times.at(i) << " ";
        }
        out << "\n";
    }

    qSort(times);
    float median = 0.0;
    if(iterations % 2 > 0) {
        median = times[(int)(iterations / 2)];
    } else {
        median = (times[iterations / 2] + times[iterations / 2 - 1]) / 2.0f;
    }

    float mean = sum / (float)iterations;
    int testSecs = startTime.secsTo(QDateTime::currentDateTime());

    qDebug("##### Mean: %.1f; Median: %.1f; Test time: %dsec", mean, median, testSecs);

    if(logFile) {
        QTextStream out(logFile);
        out << "Median average: " << (int)median << " ms. Test time: ";
        if (testSecs > 3600) { out << (testSecs / 3600) << "h "; }
        if (testSecs > 60) { out << ((testSecs % 3600) / 60) << "m "; }
        out << ((testSecs % 3600) % 60) << "s\n";
    }
}

void CallModelPerfTest::finalise()
{
    deleteAll();
}

void CallModelPerfTest::cleanupTestCase()
{
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = 0;
    }
}

QTEST_MAIN(CallModelPerfTest)
