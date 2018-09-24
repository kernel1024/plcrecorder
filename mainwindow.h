#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCheckBox>
#include <QLabel>
#include <QString>
#include "plc.h"
#include "graphform.h"
#include "csvhandler.h"

class CVarModel;
class CVarDelegate;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    CPLC* plc;
    CGraphForm* graph;
    CCSVHandler* csvHandler;

    explicit MainWindow(QWidget *parent = NULL);
    ~MainWindow();
    
private:
    Ui::MainWindow *ui;
    CVarModel *vtmodel;
    CVarDelegate *vtdelegate;
    QCheckBox* cbVat;
    QCheckBox* cbRec;
    QCheckBox* cbPlot;
    QThread* plcThread;
    QLabel* lblState;
    QLabel* lblScanTime;
    int agcRestartCounter;
    bool autoOnLogging;
    bool savedCSVActive;
    bool aggregatedStartActive;

    void loadConnectionFromFile(const QString& fname);

protected:
    virtual void closeEvent(QCloseEvent * event);

signals:
    void csvSync();

public slots:
    void plcConnected();
    void plcDisconnected();
    void plcStarted();
    void plcStopped();
    void plcStartFailed();
    void plcErrorMsg(const QString &msg, bool critical);
    void plcVariablesUpdatedConsistent(const CWPList& wp, const QDateTime& stm);

    void connectPLC();
    void aboutMsg();
    void aboutQtMsg();
    void variablesCtxMenu(QPoint pos);
    void settingsDlg();
    void loadConnection();
    void saveConnection();

    void plotControl();
    void plotStop();

    void vatControl();

    void csvControl();

    void ctlAggregatedStart();
    void ctlAggregatedStartForce();
    void ctlStop();

    void sysSIGPIPE();

    void appendLog(const QString& msg);

    void syncTimer();
    void csvError(const QString& msg);
    void recordingStopped();

    void ctxNew();
    void ctxRemove();
    void ctxRemoveAll();

signals:
    void plcSetAddress(const QString& Ip, int Rack, int Slot, int Timeout);
    void plcSetRetryParams(int maxErrorCnt, int maxRetryCnt, int waitReconnect);
    void plcConnect();
    void plcStart();
    void plcDisconnect();
    void plcCorrectToThread();

};

#endif // MAINWINDOW_H
