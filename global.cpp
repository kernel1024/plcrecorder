#include <QSettings>
#include <QStandardPaths>
#include "global.h"
#include "plc.h"

CGlobal::CGlobal(QObject *parent) :
    QObject(parent)
{
    outputCSVDir = QString();
    outputFileTemplate = QString();
    tmTCPTimeout = 5000000;
    tmMaxRecErrorCount = 50;
    plotVerticalSize = 100;
    plotShowScatter = false;
    tmMaxConnectRetryCount = 1;
    tmWaitReconnect = 2;
    tmTotalRetryCount = 1;
    suppressMsgBox = false;
    restoreCSV = false;
    // TODO: save CSV settings, configure plot size, add help for CSV template
}

QString CGlobal::plcGetAddrName(const CWP& aWp) {
    CWP wp = aWp;
    QString res;
    res.clear();
    if (wp.vtype==CWP::S7NoType)
        return trUtf8("Select type ->");

    switch (wp.varea) {
        case CWP::Inputs:   res = "I"; break;
        case CWP::Outputs:  res = "Q"; break;
        case CWP::Merkers:  res = "M"; break;
        case CWP::DB:       res = QString("DB%1.DB").arg(wp.vdb); break;
        case CWP::IDB:      res = QString("IDB%1.DB").arg(wp.vdb); break;
        case CWP::Counters: res = "C"; break;
        case CWP::Timers:   res = "T"; break;
        default: return QString();
    }
    if ((wp.varea!=CWP::Counters) && (wp.varea!=CWP::Timers)) {
        switch (wp.vtype) {
            case CWP::S7BOOL:
                if ((wp.varea==CWP::DB) || (wp.varea==CWP::IDB))
                    res += QString("X%1.%2").arg(wp.offset).arg(wp.bitnum);
                else
                    res += QString("%1.%2").arg(wp.offset).arg(wp.bitnum);
                break;
            case CWP::S7BYTE:
                res += QString("B%1").arg(wp.offset);
                break;
            case CWP::S7WORD:
            case CWP::S7INT:
            case CWP::S7S5TIME:
            case CWP::S7DATE:
                res += QString("W%1").arg(wp.offset);
                break;
            case CWP::S7DWORD:
            case CWP::S7DINT:
            case CWP::S7REAL:
            case CWP::S7TIME:
            case CWP::S7TIME_OF_DAY:
                res += QString("D%1").arg(wp.offset);
                break;

            default: return QString();
        }
    } else {
        res += QString("%1").arg(wp.offset);
    }
    return res;
}

QString CGlobal::plcGetTypeName(const CWP& aWp) {
    if (aWp.varea==CWP::Timers)
        return QString("TIMER");
    if (aWp.varea==CWP::Counters)
        return QString("COUNTER");
    switch (aWp.vtype) {
        case CWP::S7BOOL:   return QString("BOOL");
        case CWP::S7BYTE:   return QString("BYTE");
        case CWP::S7WORD:   return QString("WORD");
        case CWP::S7INT:    return QString("INT");
        case CWP::S7S5TIME: return QString("S5TIME");
        case CWP::S7DATE:   return QString("DATE");
        case CWP::S7DWORD:  return QString("DWORD");
        case CWP::S7DINT:   return QString("DINT");
        case CWP::S7REAL:   return QString("REAL");
        case CWP::S7TIME:   return QString("TIME");
        case CWP::S7TIME_OF_DAY:    return QString("TOD");
        default: return QString("----");
    }
}

QStringList CGlobal::plcAvailableTypeNames() {
    QStringList sl;
    sl << QString("BOOL");
    sl << QString("BYTE");
    sl << QString("WORD");
    sl << QString("INT");
    sl << QString("S5TIME");
    sl << QString("DATE");
    sl << QString("DWORD");
    sl << QString("DINT");
    sl << QString("REAL");
    sl << QString("TIME");
    sl << QString("TOD");
    sl << QString("TIMER");
    sl << QString("COUNTER");
    return sl;
}

bool CGlobal::plcSetTypeForName(const QString &tname, CWP& wp)
{
    if (tname.toUpper().compare("BOOL")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7BOOL;
        return true;
    }
    if (tname.toUpper().compare("BYTE")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7BYTE;
        return true;
    }
    if (tname.toUpper().compare("WORD")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7WORD;
        return true;
    }
    if (tname.toUpper().compare("INT")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7INT;
        return true;
    }
    if (tname.toUpper().compare("S5TIME")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7S5TIME;
        return true;
    }
    if (tname.toUpper().compare("DATE")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7DATE;
        return true;
    }
    if (tname.toUpper().compare("DWORD")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7DWORD;
        return true;
    }
    if (tname.toUpper().compare("DINT")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7DINT;
        return true;
    }
    if (tname.toUpper().compare("REAL")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7REAL;
        return true;
    }
    if (tname.toUpper().compare("TIME")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7TIME;
        return true;
    }
    if (tname.toUpper().compare("TOD")==0) {
        if (wp.offset<0) wp.offset = 0;
        wp.vtype=CWP::S7TIME_OF_DAY;
        return true;
    }
    if (tname.toUpper().compare("TIMER")==0) {
        if (wp.offset<1) wp.offset = 1;
        wp.vtype=CWP::S7WORD;
        wp.varea=CWP::Timers;
        return true;
    }
    if (tname.toUpper().compare("COUNTER")==0) {
        if (wp.offset<1) wp.offset = 1;
        wp.vtype=CWP::S7INT;
        wp.varea=CWP::Counters;
        return true;
    }
    return false;
}

bool CGlobal::plcParseAddr(const QString &addr, CWP &wp)
{
    // plcSetTypeForName must be called before this on same wp
    QString s = addr.toUpper().trimmed();
    if (wp.varea==CWP::Timers) {
        if (!s.startsWith('T')) return false;
        s.remove('T');
        bool okconv;
        int t = s.toInt(&okconv);
        if (!okconv) return false;
        if (t<0) return false;
        wp.offset = t;
        wp.bitnum = -1;
        wp.vdb = -1;
    } else if (wp.varea==CWP::Counters) {
        if (!s.startsWith('C')) return false;
        s.remove('C');
        bool okconv;
        int t = s.toInt(&okconv);
        if (!okconv) return false;
        if (t<0) return false;
        wp.offset = t;
        wp.bitnum = -1;
        wp.vdb = -1;
    } else {
        CWP::VArea tarea;
        if (s.startsWith("IDB")) tarea = CWP::IDB;
        else if (s.startsWith('I')) tarea = CWP::Inputs;
        else if (s.startsWith('Q')) tarea = CWP::Outputs;
        else if (s.startsWith('M')) tarea = CWP::Merkers;
        else if (s.startsWith("DB")) tarea = CWP::DB;
        else return false;
        bool okconv1,okconv2;
        int ofs, bit;
        QStringList sl;

        if ((tarea==CWP::Inputs) || (tarea==CWP::Outputs) || (tarea==CWP::Merkers)) {
            if ((tarea==CWP::Inputs) && (!s.startsWith('I'))) return false;
            if ((tarea==CWP::Outputs) && (!s.startsWith('Q'))) return false;
            if ((tarea==CWP::Merkers) && (!s.startsWith('M'))) return false;
            s.remove(0,1);
            switch (wp.vtype) {
                case CWP::S7BOOL:
                    if ((!s.at(0).isDigit()) || (!s.contains('.')) || (!s.at(s.length()-1).isDigit())) return false;
                    sl = s.split('.');
                    if (sl.count()!=2) return false;
                    ofs = sl.first().toInt(&okconv1);
                    bit = sl.last().toInt(&okconv2);
                    if ((!okconv1) || (!okconv2)) return false;
                    if ((bit>7) || (bit<0) || (ofs<0)) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = bit;
                    wp.vdb = -1;
                    break;
                case CWP::S7BYTE:
                    if (!s.startsWith('B')) return false;
                    s.remove(0,1);
                    ofs = s.toInt(&okconv1);
                    if (!okconv1) return false;
                    if (ofs<0) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = -1;
                    wp.vdb = -1;
                    break;
                case CWP::S7WORD:
                case CWP::S7INT:
                case CWP::S7S5TIME:
                case CWP::S7DATE:
                    if (!s.startsWith('W')) return false;
                    s.remove(0,1);
                    ofs = s.toInt(&okconv1);
                    if (!okconv1) return false;
                    if (ofs<0) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = -1;
                    wp.vdb = -1;
                    break;
                case CWP::S7DWORD:
                case CWP::S7DINT:
                case CWP::S7REAL:
                case CWP::S7TIME:
                case CWP::S7TIME_OF_DAY:
                    if (!s.startsWith('D')) return false;
                    s.remove(0,1);
                    ofs = s.toInt(&okconv1);
                    if (!okconv1) return false;
                    if (ofs<0) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = -1;
                    wp.vdb = -1;
                    break;
                default:
                    return false;
            }
        } else if ((tarea==CWP::DB) || (tarea==CWP::IDB)) {
            if ((tarea==CWP::DB) && (!s.startsWith("DB"))) return false;
            if ((tarea==CWP::IDB) && (!s.startsWith("IDB"))) return false;
            if (tarea==CWP::DB) s.remove(0,2);
            if (tarea==CWP::IDB) s.remove(0,3);

            QString dotDB = QString(".DB");
            if (!s.contains(dotDB)) return false;
            int tdb = s.left(s.indexOf(dotDB)).toInt(&okconv1);
            if (!okconv1) return false;
            s.remove(0,s.indexOf(dotDB)+dotDB.length());

            switch (wp.vtype) {
                case CWP::S7BOOL:
                    if (!s.startsWith('X')) return false;
                    s.remove(0,1);
                    if ((!s.at(0).isDigit()) ||
                            (!s.contains('.')) ||
                            (!s.at(s.length()-1).isDigit())) return false;
                    sl = s.split('.');
                    if (sl.count()!=2) return false;
                    ofs = sl.first().toInt(&okconv1);
                    bit = sl.last().toInt(&okconv2);
                    if ((!okconv1) || (!okconv2)) return false;
                    if ((bit>7) || (bit<0) || (ofs<0)) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = bit;
                    wp.vdb = tdb;
                    break;
                case CWP::S7BYTE:
                    if (!s.startsWith('B')) return false;
                    s.remove(0,1);
                    ofs = s.toInt(&okconv1);
                    if (!okconv1) return false;
                    if (ofs<0) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = -1;
                    wp.vdb = tdb;
                    break;
                case CWP::S7WORD:
                case CWP::S7INT:
                case CWP::S7S5TIME:
                case CWP::S7DATE:
                    if (!s.startsWith('W')) return false;
                    s.remove(0,1);
                    ofs = s.toInt(&okconv1);
                    if (!okconv1) return false;
                    if (ofs<0) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = -1;
                    wp.vdb = tdb;
                    break;
                case CWP::S7DWORD:
                case CWP::S7DINT:
                case CWP::S7REAL:
                case CWP::S7TIME:
                case CWP::S7TIME_OF_DAY:
                    if (!s.startsWith('D')) return false;
                    s.remove(0,1);
                    ofs = s.toInt(&okconv1);
                    if (!okconv1) return false;
                    if (ofs<0) return false;
                    wp.varea = tarea;
                    wp.offset = ofs;
                    wp.bitnum = -1;
                    wp.vdb = tdb;
                    break;
                default:
                    return false;
            }
        } else
            return false;
    }
    wp.data=QVariant();
    return true;
}

QString CGlobal::plcFormatActualValue(const CWP &wp)
{
    switch (wp.vtype) {
        case CWP::S7BYTE:
            return trUtf8("0x%1").arg(wp.data.toInt(),2,16,QChar('0'));
        case CWP::S7WORD:
            return trUtf8("0x%1").arg(wp.data.toInt(),4,16,QChar('0'));
        case CWP::S7DWORD:
            return trUtf8("0x%1").arg(wp.data.toInt(),8,16,QChar('0'));
        case CWP::S7S5TIME:
        case CWP::S7TIME_OF_DAY:
            return trUtf8("%1").arg(wp.data.toTime().toString("hh:mm:ss.zzz"));
        case CWP::S7TIME:
            if (wp.dataSign)
                return trUtf8("%1").arg(wp.data.toTime().toString("hh:mm:ss.zzz"));
            else
                return trUtf8("-%1").arg(wp.data.toTime().toString("hh:mm:ss.zzz"));
        default:
            return wp.data.toString();
    }
}

bool CGlobal::plcIsPlottableType(const CWP &aWp)
{
    switch (aWp.vtype) {
        case CWP::S7BOOL:
        case CWP::S7BYTE:
        case CWP::S7WORD:
        case CWP::S7DWORD:
        case CWP::S7INT:
        case CWP::S7DINT:
        case CWP::S7REAL:
            return true;
        default:
            return false;
    }
}

void CGlobal::loadSettings()
{
    QSettings settings("kernel1024", "plcrecorder");
    settings.beginGroup("Settings");
    gSet->outputCSVDir = settings.value("outputCSVDir",
                                        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    gSet->outputFileTemplate = settings.value("outputFileTemplate",QString()).toString();
    gSet->tmTCPTimeout = settings.value("timeTCPTimeout",5000000).toInt();
    gSet->tmMaxRecErrorCount = settings.value("timeMaxRecErrorCount",50).toInt();
    gSet->tmMaxConnectRetryCount = settings.value("timeMaxConnectRetryCount",1).toInt();
    gSet->tmTotalRetryCount = settings.value("timeTotalRetryCount",1).toInt();
    gSet->tmWaitReconnect = settings.value("timeWaitReconnect",2).toInt();
    gSet->suppressMsgBox = settings.value("suppressMsgBox",false).toBool();
    gSet->restoreCSV = settings.value("restoreCSV",false).toBool();
    gSet->plotVerticalSize = settings.value("plotVerticalSize",100).toInt();
    gSet->plotShowScatter = settings.value("plotShowScatter",false).toBool();
    settings.endGroup();
}

void CGlobal::saveSettings()
{
    QSettings settings("kernel1024", "plcrecorder");
    settings.beginGroup("Settings");
    settings.setValue("outputCSVDir",gSet->outputCSVDir);
    settings.value("outputFileTemplate",gSet->outputFileTemplate);
    settings.value("timeTCPTimeout",gSet->tmTCPTimeout);
    settings.value("timeMaxRecErrorCount",gSet->tmMaxRecErrorCount);
    settings.value("timeMaxConnectRetryCount",gSet->tmMaxConnectRetryCount);
    settings.value("timeTotalRetryCount",gSet->tmTotalRetryCount);
    settings.value("timeWaitReconnect",gSet->tmWaitReconnect);
    settings.value("suppressMsgBox",gSet->suppressMsgBox);
    settings.value("restoreCSV",gSet->restoreCSV);
    settings.value("plotVerticalSize",gSet->plotVerticalSize);
    settings.value("plotShowScatter",gSet->plotShowScatter);
    settings.endGroup();
}

QString getOpenFileNameD ( QWidget * parent, const QString & caption, const QString & dir,
                           const QString & filter, QString * selectedFilter,
                           QFileDialog::Options options)
{
    return QFileDialog::getOpenFileName(parent,caption,dir,filter,selectedFilter,options);
}

QStringList getOpenFileNamesD ( QWidget * parent, const QString & caption, const QString & dir,
                                const QString & filter, QString * selectedFilter,
                                QFileDialog::Options options)
{
    return QFileDialog::getOpenFileNames(parent,caption,dir,filter,selectedFilter,options);
}

QString getSaveFileNameD ( QWidget * parent, const QString & caption, const QString & dir,
                           const QString & filter, QString * selectedFilter,
                           QFileDialog::Options options)
{
    return QFileDialog::getSaveFileName(parent,caption,dir,filter,selectedFilter,options);
}

QString	getExistingDirectoryD ( QWidget * parent, const QString & caption, const QString & dir,
                                QFileDialog::Options options)
{
    return QFileDialog::getExistingDirectory(parent,caption,dir,options);
}
