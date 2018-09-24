#ifndef CSVHANDLER_H
#define CSVHANDLER_H

#include <QObject>
#include <QTextStream>
#include <QTime>
#include "plc.h"

class CCSVHandler : public QObject
{
    Q_OBJECT
private:
    QTextStream csvLog;
    QTime csvPrevTime;
    bool csvHasHeader;

public:
    explicit CCSVHandler(QObject *parent = 0);

    void addData(const CWPList& wp, const QDateTime& stm);

signals:
    void appendLog(const QString& message);
    void errorMessage(const QString& message);
    void recordingStopped();

public slots:
    bool rotateFile();
    void timerSync();
    void stopClose();
};

#endif // CSVHANDLER_H
