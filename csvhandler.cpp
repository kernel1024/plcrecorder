#include <QBuffer>
#include "global.h"
#include "csvhandler.h"

CCSVHandler::CCSVHandler(QObject *parent) : QObject(parent)
{
    csvPrevTime = QTime::currentTime();
    csvHasHeader = false;
}

void CCSVHandler::addData(const CWPList &wp, const QDateTime &stm)
{
    if (csvLog.device()!=NULL) {
        if (!csvHasHeader) {
            QString hdr = trUtf8("\"Time\"; ");
            for (int i=0;i<wp.count();i++) {
                hdr += QString("\"%1 (%2)\"; ").
                       arg(wp.at(i).label).
                       arg(gSet->plcGetAddrName(wp.at(i)));
            }
            hdr += QString("\"Scan dump\"; ");
            csvLog << hdr << QString("\r\n");
            csvHasHeader = true;
        }
        QString s = QString("\"%1\"; ").arg(stm.toString("yyyy-MM-dd hh:mm:ss.zzz"));
        for (int i=0;i<wp.count();i++) {
            s += QString("%1; ").arg(gSet->plcFormatActualValue(wp.at(i)));
        }

        // write wp dump for graph visualization
        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        QDataStream out(&buf);
        out << stm << wp;
        buf.close();
        s += QString("%1; ").arg(QString::fromLatin1(qCompress(ba,1).toBase64()));

        csvLog << s << QString("\r\n");
    }
}

bool CCSVHandler::rotateFile()
{
    csvPrevTime = QTime::currentTime();
    if (gSet->outputCSVDir.isEmpty()) {
        emit recordingStopped();
        emit appendLog(trUtf8("CSV rotation failure. Directory for creating CSV files not configured."));
        emit errorMessage(trUtf8("Directory for creating CSV files not configured."));
        return false;
    }
    stopClose();

    QDir d(gSet->outputCSVDir);
    QString fname = d.filePath(QString("%1_%2.csv").
                               arg(gSet->outputFileTemplate).
                               arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss")));
    QFile *f = new QFile(fname);
    if (!f->open(QIODevice::WriteOnly)) {
        emit recordingStopped();
        emit appendLog(trUtf8("Unable to save CSV file %1.").arg(fname));
        emit errorMessage(trUtf8("Unable to save file '%1'").arg(fname));
        return false;
    }

    csvLog.setDevice(f);
    csvLog.setCodec("Windows-1251");
    csvHasHeader = false;
    emit appendLog(trUtf8("CSV rotation was successful."));
    return (csvLog.device()!=NULL);
}

void CCSVHandler::timerSync()
{
    // reset cache
    if (csvLog.device()!=NULL)
        csvLog.flush();

    // rotate csv at 0:00
    QTime curTime = QTime::currentTime();
    bool needToRotate = (csvPrevTime.hour()>curTime.hour());
    csvPrevTime = curTime;
    if (needToRotate)
        rotateFile();
}

void CCSVHandler::stopClose()
{
    if (csvLog.device()!=NULL) {
        csvLog.flush();
        csvLog.device()->close();
        csvLog.setDevice(NULL);
        emit appendLog(trUtf8("CSV recording stopped. File closed."));
    }
}
