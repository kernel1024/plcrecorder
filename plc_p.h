#ifndef PLC_P_H
#define PLC_P_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QTimer>
#include "global.h"
#include "plc.h"
#include "libnodave/nodave.h"
#include "libnodave/openSocket.h"

class CPLCPrivate : public QObject
{
    Q_OBJECT

public:
    CPLC* qptr;

    QString ip;
    int rack;
    int slot;
    int netTimeout;
    _daveOSserialType fds;
    CPLC::PlcState state;

    daveInterface* daveIntf;
    daveConnection* daveConn;

    QMutex clockInterlock;
    int skippedTicks;
    int recErrorsCount;
    QTimer* mainClock;
    QTimer* resClock;
    QTimer* infClock;
    int updateInterval;
    QTime updateClock;

    int tmMaxRecErrorCount;
    int tmMaxConnectRetryCount;
    int tmWaitReconnect;

    CWPList watchpoints;

    QList<CPairing> pairings;

    CPLCPrivate(CPLC* q) : QObject(q), qptr(q) { }
    virtual ~CPLCPrivate() { }

    bool rearrangeWatchpoints();
};

#endif // PLC_P_H
