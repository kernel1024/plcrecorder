#ifndef OUTPUTDIALOG_H
#define OUTPUTDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class CSettingsDialog;
}

class CSettingsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit CSettingsDialog(QWidget *parent = nullptr);
    ~CSettingsDialog();

    void setParams(const QString& outputDir, const QString& fileTemplate, int tcpTimeout,
                   int maxRecErrorCount, int maxConnectRetryCount, int waitReconnect, int totalRetryCount,
                   bool suppressMsgBox, bool restoreCSV, int plotVerticalSize, bool plotShowScatter, bool plotAntialiasing);
    QString getOutputDir() const;
    QString getFileTemplate() const;
    int getTCPTimeout();
    int getMaxRecErrorCount();
    int getMaxConnectRetryCount();
    int getWaitReconnect();
    int getTotalRetryCount();
    bool getSuppressMsgBox();
    bool getRestoreCSV();
    int getPlotVerticalSize();
    bool getPlotShowScatter();
    bool getPlotAntialiasing();


private:
    Ui::CSettingsDialog *ui;

private slots:
    void selectDirDlg();
};

#endif // OUTPUTDIALOG_H
