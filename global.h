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


    // -------------------------------------------------

    explicit CGlobal(QObject *parent = nullptr);

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

QString getOpenFileNameD ( QWidget * parent, const QString & caption, const QString & dir = QString(),
                           const QString & filter = QString(), QString * selectedFilter = nullptr,
                           QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QStringList getOpenFileNamesD ( QWidget * parent, const QString & caption, const QString & dir = QString(),
                                const QString & filter = QString(), QString * selectedFilter = nullptr,
                                QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QString getSaveFileNameD ( QWidget * parent, const QString & caption, const QString & dir = QString(),
                           const QString & filter = QString(), QString * selectedFilter = nullptr,
                           QFileDialog::Options options = QFileDialog::DontUseNativeDialog );
QString	getExistingDirectoryD ( QWidget * parent, const QString & caption, const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::DontUseNativeDialog );

#endif // CGLOBAL_H
