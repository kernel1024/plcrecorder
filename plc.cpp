#include <QtCore>
#include "specwidgets.h"
#include "plc.h"
#include "limits.h"

CPLC::CPLC(QObject *parent) :
    QObject(parent)
{
    ip = QString();
    rack = 0;
    slot = 0;
    fds.rfd = 0;
    fds.wfd = 0;
    skippedTicks = 0;
    recErrorsCount = 0;
    updateInterval = 0;
    state = splcDisconnected;
    netTimeout = 5000000;
    tmMaxRecErrorCount = 50;
    tmMaxConnectRetryCount = 1;
    tmWaitReconnect = 2;

    daveIntf = nullptr;
    daveConn = nullptr;

    wp.clear();

    mainClock = nullptr;
    resClock = nullptr;
    infClock = nullptr;

    rearrangeWatchpoints();
}

CPLC::~CPLC()
{

}

void CPLC::plcSetWatchpoints(const CWPList &aWatchpoints)
{
    if (state!=splcDisconnected) {
        emit plcError(trUtf8("You can add variables only in disconnected state."));
        return;
    }

    wp = aWatchpoints;

    if (!rearrangeWatchpoints()) {
        wp.clear();
        pairings.clear();
        return;
    }
}

void CPLC::plcSetRetryParams(int maxErrorCnt, int maxRetryCnt, int waitReconnect)
{
    tmMaxRecErrorCount = maxErrorCnt;
    tmMaxConnectRetryCount = maxRetryCnt;
    tmWaitReconnect = waitReconnect;
}

bool CPLC::rearrangeWatchpoints()
{
    if (state != splcDisconnected) {
        qDebug() << "ERROR: rearrange called after connection";
        return false;
    }

    // clear pairing
    pairings.clear();

    int maxPDU = 200;

    // pairing
    for (int i=0;i<wp.count();i++) {
        // udefined variable
        if (wp.at(i).varea==CWP::NoArea) {
            pairings.clear();
            return false;
        }
        // add to existing pair
        bool paired = false;
        // do not group timers and counters
        if ((wp.at(i).varea!=CWP::Counters) && (wp.at(i).varea!=CWP::Timers)) {
            for (int j=0;j<pairings.count();j++) {
                if ((wp.at(i).varea!=pairings.at(j).area) || (wp.at(i).vdb!=pairings.at(j).db)) continue;

                if (pairings[j].sizeWith(wp.at(i)) < maxPDU) {
                    pairings[j].items << i;
                    pairings[j].calcSize();
                    paired = true;
                    break;
                }
            }
        }
        // no pair - create new
        if (!paired) {
            pairings << CPairing(&wp,wp.at(i).varea,wp.at(i).vdb);
            pairings.last().items << i;
            pairings.last().calcSize();
        }
    }
    return true;
}

void CPLC::plcSetAddress(const QString &Ip, int Rack, int Slot, int Timeout)
{
    if (state!=splcDisconnected) {
        emit plcError(trUtf8("Unable to change connection settings in online mode. Disconnect and try again."));
        return;
    }
    ip = Ip;
    rack = Rack;
    slot = Slot;
    netTimeout = Timeout;
}

void CPLC::plcSetAcqInterval(int Milliseconds)
{
    mainClock->setInterval(Milliseconds);
}

void CPLC::plcConnect()
{
    if (state!=splcDisconnected) {
        emit plcError(trUtf8("Unable to connect - connection is still active. Disconnect and try again."));
        return;
    }

    if (!rearrangeWatchpoints()) {
        emit plcError(trUtf8("Unable to parse and rearrange variable list.\n"
                             "Possible syntax error in one or more variable definitions.\n"
                             "PLC connection was stopped."));
        return;
    }

    for (int i=0;i<tmMaxConnectRetryCount;i++) {
        fds.rfd = openSocket(102,ip.toLatin1().constData());
        fds.wfd = fds.rfd;

        if (fds.rfd>0) {
            char ifname[63];
            strcpy(ifname,"IF1");
            daveIntf = daveNewInterface(fds,ifname,0,daveProtoISOTCP, daveSpeed187k);
            if (daveIntf != nullptr) {
                daveSetTimeout(daveIntf,netTimeout);
                daveConn = daveNewConnection(daveIntf,0,rack,slot);
                if (daveConn != nullptr) {
                    int res = daveConnectPLC(daveConn);
                    if (res == 0) {
                        state = splcConnected;
                        emit plcOnConnect();
                        return;
                    } else {
                        daveDisconnectPLC(daveConn);
                        daveDisconnectAdapter(daveIntf);
                        closeSocket(fds.rfd);
                        daveConn = nullptr;
                        daveIntf = nullptr;
                        fds.rfd = 0; fds.wfd = 0;
                        emit plcError(trUtf8("Unable to connect to PLC."));
                    }
                } else {
                    daveDisconnectAdapter(daveIntf);
                    closeSocket(fds.rfd);
                    daveConn = nullptr;
                    daveIntf = nullptr;
                    fds.rfd = 0; fds.wfd = 0;
                    emit plcError(trUtf8("Unable to connect to specified IP."));
                }
            } else {
                closeSocket(fds.rfd);
                daveConn = nullptr;
                daveIntf = nullptr;
                fds.rfd = 0; fds.wfd = 0;
                emit plcError(trUtf8("Unable to create IF1 interface for PLC connection at specified IP."));
            }
        } else {
            daveConn = nullptr;
            daveIntf = nullptr;
            fds.rfd = 0; fds.wfd = 0;
            emit plcError(trUtf8("Unable to open socked to specified IP."));
        }
        CSleep::sleep(static_cast<unsigned long>(tmWaitReconnect));
    }
    emit plcConnectFailed();
}

void CPLC::plcStart()
{
    if (mainClock==nullptr) return;
    if (state != splcConnected) {
        emit plcError(trUtf8("Unable to start PLC recording. libnodave is not ready."));
        return;
    }
    if (pairings.isEmpty()) {
        emit plcError(trUtf8("Variables is not parsed and prepared. Recording failure."));
        emit plcStartFailed();
        return;
    }
    updateClock.start();
    mainClock->start();
    state = splcRecording;
    emit plcOnStart();
}

void CPLC::plcStop()
{
    if (state != splcRecording) {
        emit plcError(trUtf8("Unable to stop PLC recording, recording was not even started."));
        return;
    }
    state = splcConnected;
    mainClock->stop();
    emit plcOnStop();
}

void CPLC::plcDisconnect()
{
    if (state == splcDisconnected) {
        emit plcError(trUtf8("Not connected to PLC."));
        return;
    }

    if (state == splcRecording)
        plcStop();

    if (state != splcConnected) {
        emit plcError(trUtf8("Unable to stop PLC recording."));
        return;
    }

    daveDisconnectPLC(daveConn);
    daveDisconnectAdapter(daveIntf);
    closeSocket(fds.rfd);
    daveConn = nullptr;
    daveIntf = nullptr;
    fds.rfd = 0; fds.wfd = 0;

    state = splcDisconnected;

    emit plcOnDisconnect();
}

void CPLC::correctToThread()
{
    mainClock = new QTimer(this);
    resClock = new QTimer(this);
    infClock = new QTimer(this);

    mainClock->setInterval(250);
    resClock->setInterval(120*60*1000); // 2 min for reset accumulated record errors
    infClock->setInterval(2000);

    connect(mainClock,SIGNAL(timeout()),this,SLOT(plcClock()));
    connect(resClock,SIGNAL(timeout()),this,SLOT(resetClock()));
    connect(infClock,SIGNAL(timeout()),this,SLOT(infoClock()));

    resClock->start();
    infClock->start();
}

void CPLC::plcClock()
{
    if (state!=splcRecording) return;
    if (mainClock==nullptr) return;
    if (!clockInterlock.tryLock()) {
        if (mainClock->interval()>0)
            skippedTicks++;
        return;
    }

    updateInterval = updateClock.restart();
    QDateTime tms = QDateTime::currentDateTime();

    for (int i=0;i<pairings.count();i++) {
        int ofs = pairings[i].offset();
        int sz = pairings[i].size();
        int area = pairings.at(i).area;
        int db = pairings.at(i).db;
        if ((area!=daveDB) && (area!=daveDI))
            db = 0;

        int res = daveReadBytes(daveConn,area,db,ofs,sz,nullptr);
        if (res==0) {
            for (int j=0;j<pairings.at(i).items.count();j++) {
                int idx = pairings.at(i).items.at(j);
                int iofs = wp.at(idx).offset;
                CWP::VType t = wp.at(idx).vtype;
                if (area==CWP::Counters) {
                    wp[idx].data = QVariant(daveGetCounterValueAt(daveConn,0));
                } else if (area==CWP::Timers) {
                    wp[idx].data = QVariant(daveGetSecondsAt(daveConn,0));
                } else {
                    switch (t) {
                        case CWP::S7BOOL:
                            wp[idx].data = QVariant(((daveGetU8At(daveConn,iofs-ofs) &
                                                         (0x01 << wp.at(idx).bitnum)) > 0));
                            break;
                        case CWP::S7BYTE:
                            wp[idx].data = QVariant(static_cast<uint>(daveGetU8At(daveConn,iofs-ofs)));
                            break;
                        case CWP::S7WORD:
                            wp[idx].data = QVariant(static_cast<uint>(daveGetU16At(daveConn,iofs-ofs)));
                            break;
                        case CWP::S7DWORD:
                            wp[idx].data = QVariant(static_cast<uint>(daveGetU32At(daveConn,iofs-ofs)));
                            break;
                        case CWP::S7INT:
                            wp[idx].data = QVariant(daveGetS16At(daveConn,iofs-ofs));
                            break;
                        case CWP::S7DINT:
                            wp[idx].data = QVariant(daveGetS32At(daveConn,iofs-ofs));
                            break;
                        case CWP::S7REAL:
                            wp[idx].data = QVariant(daveGetFloatAt(daveConn,iofs-ofs));
                            break;
                        case CWP::S7TIME:
                            if (true) {
                                QTime t = QTime(0,0,0);
                                int tm = daveGetS32At(daveConn,iofs-ofs);
                                t = t.addMSecs(abs(tm));
                                wp[idx].data = QVariant(t);
                                wp[idx].dataSign = (tm>=0);
                            }
                            break;
                        case CWP::S7DATE:
                            if (true) {
                                QDate d = QDate(1990,1,1);
                                d = d.addDays(static_cast<uint>(daveGetU16At(daveConn,iofs-ofs)));
                                wp[idx].data = QVariant(d);
                            }
                            break;
                        case CWP::S7S5TIME:
                            if (true) {
                                uint s5 = static_cast<uint>(daveGetU16At(daveConn,iofs-ofs));
                                uint s5mode = (s5 >> 12) & 0x03;
                                s5 = s5 & 0x0fff;
                                uint mult;
                                if (s5mode == 0) mult=10;
                                else if (s5mode == 1) mult=100;
                                else if (s5mode == 2) mult=1000;
                                else mult = 10000;
                                uint msecs = ((s5 >> 8) & 0x0f)*100 + ((s5 >> 4) & 0x0f)*10 + (s5 & 0x0f);
                                QTime t = QTime(0,0,0,0);
                                t = t.addMSecs(static_cast<int>(msecs*mult));
                                wp[idx].data = QVariant(t);
                            }
                            break;
                        case CWP::S7TIME_OF_DAY:
                            if (true) {
                                QTime t = QTime();
                                t = t.addMSecs(static_cast<int>(daveGetU32At(daveConn,iofs-ofs)));
                                wp[idx].data = QVariant(t);
                            }
                            break;
                        default:
                            wp[idx].data = QVariant();
                            break;
                    }
                }
            }
        } else {
            if (tmMaxRecErrorCount>0) {
                recErrorsCount++;
                if (recErrorsCount>tmMaxRecErrorCount) {
                    QString strMsg(daveStrerror(res));
                    plcStop();
                    emit plcError(trUtf8("Too many errors on active connection. Recording stopped. %1")
                                  .arg(strMsg));
                }
            } else
                recErrorsCount = 0;
        }
    }
    emit plcVariablesUpdatedConsistent(wp,tms);
    emit plcVariablesUpdated();
    clockInterlock.unlock();
}

void CPLC::resetClock()
{
    recErrorsCount = 0;
}

void CPLC::infoClock()
{
    if (state==splcRecording)
        emit plcScanTime(trUtf8("%1 ms").arg(updateInterval));
    else
        emit plcScanTime(trUtf8("- ms"));
}

CWP::CWP()
{
    label = QString();
    varea = NoArea;
    vtype = S7NoType;
    vdb = -1;
    offset = -1;
    bitnum = -1;
    data = QVariant();
    dataSign = true;
    uuid = QUuid::createUuid();
}

CWP::CWP(QString aLabel, CWP::VArea aArea, VType aType, int aVdb, int aOffset, int aBitnum)
{
    label = aLabel;
    varea = aArea;
    vtype = aType;
    vdb = aVdb;
    offset = aOffset;
    bitnum = aBitnum;
    data = QVariant();
    dataSign = true;
    uuid = QUuid::createUuid();
}

CWP &CWP::operator =(const CWP &other)
{
    label = other.label;
    varea = other.varea;
    vtype = other.vtype;
    vdb = other.vdb;
    offset = other.offset;
    bitnum = other.bitnum;
    data = other.data;
    dataSign = other.dataSign;
    uuid = other.uuid;
    return *this;
}

bool CWP::operator ==(const CWP &ref) const
{
    return (uuid==ref.uuid);
}

bool CWP::operator !=(const CWP &ref) const
{
    return (uuid!=ref.uuid);
}

int CWP::size()
{
    if ((varea==Counters) || (varea==Timers)) return 2;
    switch (vtype) {
        case S7BOOL:
        case S7BYTE:
            return 1;
        case S7WORD:
        case S7INT:
        case S7S5TIME:
        case S7DATE:
            return 2;
        case S7DWORD:
        case S7DINT:
        case S7REAL:
        case S7TIME:
        case S7TIME_OF_DAY:
            return 4;
        default:
            return 0;
    }
}


CPairing::CPairing()
{
    items.clear();
    cnt = -1;
    sz = 0;
    ofs = -1;
    db = -1;
    wlist = nullptr;
    area = CWP::NoArea;
}

CPairing::CPairing(CWPList *Wlist, CWP::VArea Area, int aDB)
{
    items.clear();
    cnt = -1;
    sz = 0;
    ofs = -1;
    db = aDB;
    wlist = Wlist;
    area = Area;
}

CPairing &CPairing::operator =(const CPairing &other)
{
    items = other.items;
    cnt = other.cnt;
    ofs = other.ofs;
    sz = other.sz;
    db = other.db;
    wlist = other.wlist;
    area = other.area;
    return *this;
}

int CPairing::size()
{
    if (cnt != items.count())
        calcSize();
    return sz;
}

int CPairing::offset()
{
    if (cnt != items.count())
        calcSize();
    return ofs;
}

int CPairing::sizeWith(CWP wp)
{
    if ((wp.varea!=area) || (wp.vdb!=db) || (wp.varea==CWP::Counters) ||
            (wp.varea==CWP::Timers)) return INT_MAX;

    if (cnt != items.count())
        calcSize();
    int start1 = ofs;
    int stop1 = ofs + sz;
    int start2 = wp.offset;
    int stop2 = wp.offset + wp.size();
    if (start2<start1) start1 = start2;
    if (stop2>stop1) stop1 = stop2;
    return (stop1-start1);
}

int CPairing::offsetWith(CWP wp)
{
    if (cnt != items.count())
        calcSize();
    int start1 = ofs;
    int stop1 = ofs + sz;
    int start2 = wp.offset;
    int stop2 = wp.offset + wp.size();
    if (start2<start1) start1 = start2;
    if (stop2>stop1) stop1 = stop2;
    return start1;
}

void CPairing::calcSize()
{
    sz = 0;
    ofs = -1;
    cnt = items.count();
    if (wlist==nullptr) {
        qDebug() << "ERROR: call calcSize on uninitialized pairing list";
        return;
    }

    int start1 = ofs;
    int stop1 = ofs + sz;
    for (int i=0;i<cnt;i++) {
        int start2 = wlist->at(items.at(i)).offset;
        int stop2 = (*wlist)[i].size() + start2;
        if (start1 == -1) {
            start1 = start2;
            stop1 = stop2;
        } else {
            if (start2<start1) start1 = start2;
            if (stop2>stop1) stop1 = stop2;
        }
    }
    ofs = start1;
    sz = stop1-start1;
}

QDataStream &operator <<(QDataStream &out, const CWP &obj)
{
    int a = static_cast<int>(obj.varea);
    int b = static_cast<int>(obj.vtype);
    out << obj.uuid << a << b <<  obj.vdb << obj.offset << obj.bitnum << obj.label;
    return out;
}

QDataStream &operator >>(QDataStream &in, CWP &obj)
{
    int a,b;
    in >> obj.uuid >> a >> b >> obj.vdb >> obj.offset >> obj.bitnum >> obj.label;
    obj.varea = static_cast<CWP::VArea>(a);
    obj.vtype = static_cast<CWP::VType>(b);
    return in;
}
