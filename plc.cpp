#include <limits.h>
#include "specwidgets.h"
#include "plc.h"
#include "plc_p.h"

extern "C" {
#include "libnodave/nodave.h"
#include "libnodave/openSocket.h"
}

#include <QDebug>

CPLC::CPLC(QObject *parent) :
    QObject(parent),
    dptr(new CPLCPrivate(this))
{
    dptr->ip = QString();
    dptr->rack = 0;
    dptr->slot = 0;
    dptr->fds.rfd = 0;
    dptr->fds.wfd = 0;
    dptr->skippedTicks = 0;
    dptr->recErrorsCount = 0;
    dptr->updateInterval = 0;
    dptr->state = splcDisconnected;
    dptr->netTimeout = 5000000;
    dptr->tmMaxRecErrorCount = 50;
    dptr->tmMaxConnectRetryCount = 1;
    dptr->tmWaitReconnect = 2;

    dptr->daveIntf = nullptr;
    dptr->daveConn = nullptr;

    dptr->watchpoints.clear();

    dptr->mainClock = nullptr;
    dptr->resClock = nullptr;
    dptr->infClock = nullptr;

    dptr->rearrangeWatchpoints();
}

CPLC::~CPLC()
{
    dptr->deleteLater();
}

void CPLC::plcSetWatchpoints(const CWPList &aWatchpoints)
{
    if (dptr->state!=splcDisconnected) {
        emit plcError(trUtf8("You can add variables only in disconnected state."));
        return;
    }

    dptr->watchpoints = aWatchpoints;

    if (!dptr->rearrangeWatchpoints()) {
        dptr->watchpoints.clear();
        dptr->pairings.clear();
        return;
    }
}

void CPLC::plcSetRetryParams(int maxErrorCnt, int maxRetryCnt, int waitReconnect)
{
    dptr->tmMaxRecErrorCount = maxErrorCnt;
    dptr->tmMaxConnectRetryCount = maxRetryCnt;
    dptr->tmWaitReconnect = waitReconnect;
}

bool CPLCPrivate::rearrangeWatchpoints()
{
    if (state != CPLC::splcDisconnected) {
        qDebug() << "ERROR: rearrange called after connection";
        return false;
    }

    // clear pairing
    pairings.clear();

    int maxPDU = 200;

    // pairing
    for (int i=0;i<watchpoints.count();i++) {
        // udefined variable
        if (watchpoints.at(i).varea==CWP::NoArea) {
            pairings.clear();
            return false;
        }
        // add to existing pair
        bool paired = false;
        // do not group timers and counters
        if ((watchpoints.at(i).varea!=CWP::Counters) && (watchpoints.at(i).varea!=CWP::Timers)) {
            for (int j=0;j<pairings.count();j++) {
                if ((watchpoints.at(i).varea!=pairings.at(j).area) || (watchpoints.at(i).vdb!=pairings.at(j).db)) continue;

                if (pairings[j].sizeWith(watchpoints.at(i)) < maxPDU) {
                    pairings[j].items << i;
                    pairings[j].calcSize();
                    paired = true;
                    break;
                }
            }
        }
        // no pair - create new
        if (!paired) {
            pairings << CPairing(&watchpoints,watchpoints.at(i).varea,watchpoints.at(i).vdb);
            pairings.last().items << i;
            pairings.last().calcSize();
        }
    }
    return true;
}

void CPLC::plcSetAddress(const QString &Ip, int Rack, int Slot, int Timeout)
{
    if (dptr->state!=splcDisconnected) {
        emit plcError(trUtf8("Unable to change connection settings in online mode. Disconnect and try again."));
        return;
    }
    dptr->ip = Ip;
    dptr->rack = Rack;
    dptr->slot = Slot;
    dptr->netTimeout = Timeout;
}

void CPLC::plcSetAcqInterval(int Milliseconds)
{
    dptr->mainClock->setInterval(Milliseconds);
}

void CPLC::plcConnect()
{
    if (dptr->state!=splcDisconnected) {
        emit plcError(trUtf8("Unable to connect - connection is still active. Disconnect and try again."));
        return;
    }

    if (!dptr->rearrangeWatchpoints()) {
        emit plcError(trUtf8("Unable to parse and rearrange variable list.\n"
                             "Possible syntax error in one or more variable definitions.\n"
                             "PLC connection was stopped."));
        return;
    }

    for (int i=0;i<dptr->tmMaxConnectRetryCount;i++) {
        dptr->fds.rfd = openSocket(102,dptr->ip.toLatin1().constData());
        dptr->fds.wfd = dptr->fds.rfd;

        if (dptr->fds.rfd>0) {
            char ifname[63];
            strcpy(ifname,"IF1");
            dptr->daveIntf = daveNewInterface(dptr->fds,ifname,0,daveProtoISOTCP, daveSpeed187k);
            if (dptr->daveIntf != nullptr) {
                daveSetTimeout(dptr->daveIntf,dptr->netTimeout);
                dptr->daveConn = daveNewConnection(dptr->daveIntf,0,dptr->rack,dptr->slot);
                if (dptr->daveConn != nullptr) {
                    int res = daveConnectPLC(dptr->daveConn);
                    if (res == 0) {
                        dptr->state = splcConnected;
                        emit plcOnConnect();
                        return;
                    } else {
                        daveDisconnectPLC(dptr->daveConn);
                        daveDisconnectAdapter(dptr->daveIntf);
                        closeSocket(dptr->fds.rfd);
                        dptr->daveConn = nullptr;
                        dptr->daveIntf = nullptr;
                        dptr->fds.rfd = 0; dptr->fds.wfd = 0;
                        emit plcError(trUtf8("Unable to connect to PLC."));
                    }
                } else {
                    daveDisconnectAdapter(dptr->daveIntf);
                    closeSocket(dptr->fds.rfd);
                    dptr->daveConn = nullptr;
                    dptr->daveIntf = nullptr;
                    dptr->fds.rfd = 0; dptr->fds.wfd = 0;
                    emit plcError(trUtf8("Unable to connect to specified IP."));
                }
            } else {
                closeSocket(dptr->fds.rfd);
                dptr->daveConn = nullptr;
                dptr->daveIntf = nullptr;
                dptr->fds.rfd = 0; dptr->fds.wfd = 0;
                emit plcError(trUtf8("Unable to create IF1 interface for PLC connection at specified IP."));
            }
        } else {
            dptr->daveConn = nullptr;
            dptr->daveIntf = nullptr;
            dptr->fds.rfd = 0; dptr->fds.wfd = 0;
            emit plcError(trUtf8("Unable to open socked to specified IP."));
        }
        CSleep::sleep(static_cast<unsigned long>(dptr->tmWaitReconnect));
    }
    emit plcConnectFailed();
}

void CPLC::plcStart()
{
    if (dptr->mainClock==nullptr) return;
    if (dptr->state != splcConnected) {
        emit plcError(trUtf8("Unable to start PLC recording. libnodave is not ready."));
        return;
    }
    if (dptr->pairings.isEmpty()) {
        emit plcError(trUtf8("Variables is not parsed and prepared. Recording failure."));
        emit plcStartFailed();
        return;
    }
    dptr->updateClock.start();
    dptr->mainClock->start();
    dptr->state = splcRecording;
    emit plcOnStart();
}

void CPLC::plcStop()
{
    if (dptr->state != splcRecording) {
        emit plcError(trUtf8("Unable to stop PLC recording, recording was not even started."));
        return;
    }
    dptr->state = splcConnected;
    dptr->mainClock->stop();
    emit plcOnStop();
}

void CPLC::plcDisconnect()
{
    if (dptr->state == splcDisconnected) {
        emit plcError(trUtf8("Not connected to PLC."));
        return;
    }

    if (dptr->state == splcRecording)
        plcStop();

    if (dptr->state != splcConnected) {
        emit plcError(trUtf8("Unable to stop PLC recording."));
        return;
    }

    daveDisconnectPLC(dptr->daveConn);
    daveDisconnectAdapter(dptr->daveIntf);
    closeSocket(dptr->fds.rfd);
    dptr->daveConn = nullptr;
    dptr->daveIntf = nullptr;
    dptr->fds.rfd = 0; dptr->fds.wfd = 0;

    dptr->state = splcDisconnected;

    emit plcOnDisconnect();
}

void CPLC::correctToThread()
{
    dptr->mainClock = new QTimer(this);
    dptr->resClock = new QTimer(this);
    dptr->infClock = new QTimer(this);

    dptr->mainClock->setInterval(100);
    dptr->resClock->setInterval(120*60*1000); // 2 min for reset accumulated record errors
    dptr->infClock->setInterval(2000);

    connect(dptr->mainClock,&QTimer::timeout,this,&CPLC::plcClock);
    connect(dptr->resClock,&QTimer::timeout,[this](){
        dptr->recErrorsCount = 0;
    });
    connect(dptr->infClock,&QTimer::timeout,[this](){
        if (dptr->state==splcRecording)
            emit plcScanTime(trUtf8("%1 ms").arg(dptr->updateInterval));
        else
            emit plcScanTime(trUtf8("- ms"));
    });

    dptr->resClock->start();
    dptr->infClock->start();
}

void CPLC::plcClock()
{
    if (dptr->state!=splcRecording) return;
    if (dptr->mainClock==nullptr) return;
    if (!dptr->clockInterlock.tryLock()) {
        if (dptr->mainClock->interval()>0)
            dptr->skippedTicks++;
        return;
    }

    dptr->updateInterval = dptr->updateClock.restart();
    QDateTime tms = QDateTime::currentDateTime();

    for (int i=0;i<dptr->pairings.count();i++) {
        int ofs = dptr->pairings[i].offset();
        int sz = dptr->pairings[i].size();
        int area = dptr->pairings.at(i).area;
        int db = dptr->pairings.at(i).db;
        if ((area!=daveDB) && (area!=daveDI))
            db = 0;

        int res = daveReadBytes(dptr->daveConn,area,db,ofs,sz,nullptr);
        if (res==0) {
            for (int j=0;j<dptr->pairings.at(i).items.count();j++) {
                int idx = dptr->pairings.at(i).items.at(j);
                int iofs = dptr->watchpoints.at(idx).offset;
                CWP::VType t = dptr->watchpoints.at(idx).vtype;
                if (area==CWP::Counters) {
                    dptr->watchpoints[idx].data = QVariant(daveGetCounterValueAt(dptr->daveConn,0));
                } else if (area==CWP::Timers) {
                    dptr->watchpoints[idx].data = QVariant(daveGetSecondsAt(dptr->daveConn,0));
                } else {
                    switch (t) {
                        case CWP::S7BOOL:
                            dptr->watchpoints[idx].data = QVariant(((daveGetU8At(dptr->daveConn,iofs-ofs) &
                                                         (0x01 << dptr->watchpoints.at(idx).bitnum)) > 0));
                            break;
                        case CWP::S7BYTE:
                            dptr->watchpoints[idx].data = QVariant(static_cast<uint>(daveGetU8At(dptr->daveConn,iofs-ofs)));
                            break;
                        case CWP::S7WORD:
                            dptr->watchpoints[idx].data = QVariant(static_cast<uint>(daveGetU16At(dptr->daveConn,iofs-ofs)));
                            break;
                        case CWP::S7DWORD:
                            dptr->watchpoints[idx].data = QVariant(static_cast<uint>(daveGetU32At(dptr->daveConn,iofs-ofs)));
                            break;
                        case CWP::S7INT:
                            dptr->watchpoints[idx].data = QVariant(daveGetS16At(dptr->daveConn,iofs-ofs));
                            break;
                        case CWP::S7DINT:
                            dptr->watchpoints[idx].data = QVariant(daveGetS32At(dptr->daveConn,iofs-ofs));
                            break;
                        case CWP::S7REAL:
                            dptr->watchpoints[idx].data = QVariant(static_cast<double>(daveGetFloatAt(dptr->daveConn,iofs-ofs)));
                            break;
                        case CWP::S7TIME:
                            if (true) {
                                QTime t = QTime(0,0,0);
                                int tm = daveGetS32At(dptr->daveConn,iofs-ofs);
                                t = t.addMSecs(abs(tm));
                                dptr->watchpoints[idx].data = QVariant(t);
                                dptr->watchpoints[idx].dataSign = (tm>=0);
                            }
                            break;
                        case CWP::S7DATE:
                            if (true) {
                                QDate d = QDate(1990,1,1);
                                d = d.addDays(static_cast<uint>(daveGetU16At(dptr->daveConn,iofs-ofs)));
                                dptr->watchpoints[idx].data = QVariant(d);
                            }
                            break;
                        case CWP::S7S5TIME:
                            if (true) {
                                uint s5 = static_cast<uint>(daveGetU16At(dptr->daveConn,iofs-ofs));
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
                                dptr->watchpoints[idx].data = QVariant(t);
                            }
                            break;
                        case CWP::S7TIME_OF_DAY:
                            if (true) {
                                QTime t = QTime();
                                t = t.addMSecs(static_cast<int>(daveGetU32At(dptr->daveConn,iofs-ofs)));
                                dptr->watchpoints[idx].data = QVariant(t);
                            }
                            break;
                        default:
                            dptr->watchpoints[idx].data = QVariant();
                            break;
                    }
                }
            }
        } else {
            if (dptr->tmMaxRecErrorCount>0) {
                dptr->recErrorsCount++;
                if (dptr->recErrorsCount>dptr->tmMaxRecErrorCount) {
                    QString strMsg(daveStrerror(res));
                    plcStop();
                    emit plcError(trUtf8("Too many errors on active connection. Recording stopped. %1")
                                  .arg(strMsg));
                }
            } else
                dptr->recErrorsCount = 0;
        }
    }
    emit plcVariablesUpdatedConsistent(dptr->watchpoints,tms);
    emit plcVariablesUpdated();
    dptr->clockInterlock.unlock();
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
