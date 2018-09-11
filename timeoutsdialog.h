#ifndef TIMEOUTSDIALOG_H
#define TIMEOUTSDIALOG_H

#include <QDialog>
#include <QWidget>

namespace Ui {
class CTimeoutsDialog;
}

class CTimeoutsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit CTimeoutsDialog(QWidget *parent = nullptr);
    ~CTimeoutsDialog();
    void setParams(int tcpTimeout, int maxRecErrorCount, int maxConnectRetryCount, int waitReconnect,
                   int totalRetryCount, bool suppressMsgBox, bool restoreCSV);
    int getTCPTimeout();
    int getMaxRecErrorCount();
    int getMaxConnectRetryCount();
    int getWaitReconnect();
    int getTotalRetryCount();
    bool getSuppressMsgBox();
    bool getRestoreCSV();
    
private:
    Ui::CTimeoutsDialog *ui;
};

#endif // TIMEOUTSDIALOG_H
