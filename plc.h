#ifndef PLC_H
#define PLC_H

#include <QObject>
#include <QDataStream>
#include <QString>
#include <QVariant>
#include <QUuid>
#include <QList>
#include <QTime>

class CVarModel;

class CWP
{
    friend QDataStream &operator<<(QDataStream &out, const CWP &obj);
    friend QDataStream &operator>>(QDataStream &in, CWP &obj);
public:
    friend class CVarModel;
    enum VArea {
        NoArea = -1,
        Inputs = 0x81,
        Outputs = 0x82,
        Merkers = 0x83,
        DB = 0x84,
        IDB = 0x85,
        Counters = 28,
        Timers = 29
    };
    enum VType {
        S7NoType,
        S7BOOL,
        S7BYTE,
        S7WORD,
        S7DWORD,
        S7INT,
        S7DINT,
        S7REAL,
        S7TIME,
        S7DATE,
        S7S5TIME,
        S7TIME_OF_DAY
    };

    QString label;
    VArea varea;
    VType vtype;
    int vdb;
    int offset;
    int bitnum;
    QVariant data;
    bool dataSign;
    CWP();
    CWP(QString aLabel, VArea aArea, VType aType, int aVdb, int aOffset, int aBitnum);
    CWP &operator=(const CWP& other);
    bool operator==(const CWP& ref) const;
    bool operator!=(const CWP& ref) const;
    int size();
private:
    QUuid uuid;
};

typedef QList<CWP> CWPList;

class CPairing {
public:
    QList<int> items;
    CWP::VArea area;
    int db;
    CPairing();
    CPairing(CWPList* Wlist, CWP::VArea Area, int aDB = -1);
    CPairing &operator=(const CPairing& other);
    int size();
    int offset();
    int sizeWith(CWP wp);
    int offsetWith(CWP wp);
    void calcSize();
private:
    int sz;
    int ofs;
    int cnt;
    CWPList* wlist;
};

class CPLCPrivate;

class CPLC : public QObject
{
    Q_OBJECT
public:
    enum PlcState {
        splcDisconnected,
        splcConnected,
        splcRecording
    };
    explicit CPLC(QObject *parent = nullptr);
    virtual ~CPLC();

private:
    CPLCPrivate* dptr;

signals:
    void plcError(const QString& msg, bool critical);
    void plcConnectFailed();
    void plcStartFailed();
    void plcOnConnect();
    void plcOnDisconnect();
    void plcOnStart();
    void plcOnStop();
    void plcVariablesUpdated();
    void plcVariablesUpdatedConsistent(const CWPList& aWatchpoints, const QDateTime& tm);
    void plcScanTime(const QString& msg);
    
public slots:
    void plcSetAddress(const QString& Ip, int Rack, int Slot, int Timeout = 5000000);
    void plcSetAcqInterval(int Milliseconds);
    void plcSetWatchpoints(const CWPList& aWatchpoints);
    void plcSetRetryParams(int maxErrorCnt, int maxRetryCnt, int waitReconnect);
    void plcConnect();
    void plcStart();
    void plcStop();
    void plcDisconnect();
    void correctToThread();

private slots:
    void plcClock();
    void resClock();
    void infClock();
    
};

QString plcGetAddrName(const CWP& aWp);
QString plcGetTypeName(CWP::VType vtype);

#endif // PLC_H
