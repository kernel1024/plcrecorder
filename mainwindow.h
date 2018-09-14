#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCheckBox>
#include <QLabel>
#include <QString>
#include <QTextStream>
#include "plc.h"
#include "graphform.h"

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

    explicit MainWindow(QWidget *parent = nullptr);
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
    QTextStream csvLog;
    QTime csvPrevTime;
    bool csvHasHeader;
    int agcRestartCounter;
    bool autoOnLogging;
    bool savedCSVActive;
    bool aggregatedStartActive;

    void loadSettingsFromFile(const QString& fname);

public slots:
    void plcConnected();
    void plcDisconnected();
    void plcStarted();
    void plcStopped();
    void plcStartFailed();
    void plcErrorMsg(const QString &msg);
    void plcVariablesUpdatedConsistent(const CWPList& wp, const QDateTime& stm);

    void connectPLC();
    void aboutMsg();
    void aboutQtMsg();
    void variablesCtxMenu(QPoint pos);
    void setupOutputSettings();
    void setupTimeouts();
    void loadSettings();
    void saveSettings();
    void csvCaptureControl();
    void csvRotateFile();
    void csvSync();
    void csvStopClose();

    void plotControl();
    void plotStop();

    void vatControl();

    void ctlAggregatedStart();
    void ctlStop();

    void sysSIGPIPE();

    void appendLog(const QString& msg);

signals:
    void plcSetAddress(const QString& Ip, int Rack, int Slot, int Timeout);
    void plcSetRetryParams(int maxErrorCnt, int maxRetryCnt, int waitReconnect);
    void plcConnect();
    void plcStart();
    void plcDisconnect();
    void plcCorrectToThread();

};

#endif // MAINWINDOW_H
