#ifndef CGLOBAL_H
#define CGLOBAL_H

#include <QObject>
#include <QString>
#include <QFileDialog>
#include "plc.h"

class CGlobal : public QObject
{
    Q_OBJECT
public:
    // global settings ---------------------------------
    QString outputCSVDir, outputFileTemplate;
    int tmTCPTimeout, tmMaxRecErrorCount, tmMaxConnectRetryCount;
    int tmWaitReconnect, tmTotalRetryCount;
    bool suppressMsgBox, restoreCSV;
    int plotVerticalSize;
    bool plotShowScatter;
    bool plotAntialiasing;

    QString savedAuxDir;


    // -------------------------------------------------

    explicit CGlobal(QObject *parent = 0);

    QString plcGetAddrName(const CWP& aWp);
    QString plcGetTypeName(const CWP& aWp);
    QStringList plcAvailableTypeNames();
    bool plcSetTypeForName(const QString& tname, CWP &wp);
    bool plcParseAddr(const QString& addr, CWP &wp);
    QString plcFormatActualValue(const CWP& wp);
    bool plcIsPlottableType(const CWP& aWp);

    void loadSettings();
    void saveSettings();
};

extern CGlobal* gSet;

QString getOpenFileNameD ( QWidget * parent = NULL, const QString & caption = QString(),
                           const QString & dir = QString(), const QString & filter = QString(),
                           QString * selectedFilter = NULL);

QStringList getOpenFileNamesD ( QWidget * parent = NULL, const QString & caption = QString(),
                               const QString & dir = QString(), const QString & filter = QString(),
                               QString * selectedFilter = NULL);

QString getSaveFileNameD (QWidget * parent = NULL, const QString & caption = QString(),
                          const QString & dir = QString(), const QString & filter = QString(),
                          QString * selectedFilter = NULL, QString preselectFileName = QString());

QString	getExistingDirectoryD ( QWidget * parent = NULL, const QString & caption = QString(),
                                const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::ShowDirsOnly);
#endif // CGLOBAL_H
