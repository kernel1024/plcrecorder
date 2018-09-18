#include <QMessageBox>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "global.h"
#include "varmodel.h"
#include "settingsdialog.h"
#include "specwidgets.h"
#include <limits.h>

#define PLR_VERSION 2

CGlobal *gSet = nullptr;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    appendLog(trUtf8("Initializing."));

    QIcon appicon;
    appicon.addFile(":/icon32");
    appicon.addFile(":/icon40");
    setWindowIcon(appicon);

    gSet = new CGlobal();
    csvPrevTime = QTime::currentTime();
    csvHasHeader = false;
    agcRestartCounter = 0;
    autoOnLogging = false;
    savedCSVActive = false;
    aggregatedStartActive = false;

    lblState = new QLabel(trUtf8("Offline"));
    cbVat = new QCheckBox(trUtf8("Online VAT"));
    cbRec = new QCheckBox(trUtf8("CSV recording"));
    cbPlot = new QCheckBox(trUtf8("Time graph"));
    ui->statusBar->addPermanentWidget(cbPlot);
    ui->statusBar->addPermanentWidget(cbRec);
    ui->statusBar->addPermanentWidget(cbVat);
    ui->statusBar->addPermanentWidget(lblState);

    connect(ui->actionSettings,&QAction::triggered,this,&MainWindow::settingsDlg);
    connect(ui->actionLoadConnection,&QAction::triggered,this,&MainWindow::loadConnection);
    connect(ui->actionSaveConnection,&QAction::triggered,this,&MainWindow::saveConnection);
    connect(ui->actionForceRotateCSV,&QAction::triggered,this,&MainWindow::csvRotateFile);
    connect(ui->actionAbout,&QAction::triggered,this,&MainWindow::aboutMsg);
    connect(ui->actionAboutQt,&QAction::triggered,this,&MainWindow::aboutQtMsg);
    connect(ui->tableVariables,&CTableView:: ctxMenuRequested,this,&MainWindow::variablesCtxMenu);

    connect(cbVat,&QCheckBox::clicked,this,&MainWindow::vatControl);
    connect(cbRec,&QCheckBox::clicked,this,&MainWindow::csvCaptureControl);
    connect(cbPlot,&QCheckBox::clicked,this,&MainWindow::plotControl);

    plc = new CPLC();
    plcThread = new QThread();

    vtmodel = new CVarModel(this,plc);
    vtmodel->mainWnd = this;
    ui->tableVariables->setModel(vtmodel);

    vtdelegate = new CVarDelegate();
    vtdelegate->setVarModel(vtmodel);
    ui->tableVariables->setItemDelegate(vtdelegate);

    ui->btnConnect->setEnabled(true);
    ui->btnDisconnect->setEnabled(false);
    ui->actionLoadConnection->setEnabled(true);
    ui->actionSaveConnection->setEnabled(true);
    ui->actionForceRotateCSV->setEnabled(false);
    cbVat->setEnabled(false);
    cbVat->setChecked(false);
    cbRec->setEnabled(false);
    cbRec->setChecked(false);
    cbPlot->setEnabled(false);
    cbPlot->setChecked(false);

    plc->moveToThread(plcThread);
    plcThread->start();

    connect(this,&MainWindow::plcCorrectToThread,plc,&CPLC::correctToThread,Qt::QueuedConnection);
    emit plcCorrectToThread();

    graph = new CGraphForm(this);
    graph->setWindowFlag(Qt::Window,true);
    graph->hide();

    connect(ui->btnConnect,&QPushButton::clicked,this,&MainWindow::ctlAggregatedStart);
    connect(ui->btnDisconnect,&QPushButton::clicked,this,&MainWindow::ctlStop);
    connect(ui->editAcqInterval,SIGNAL(valueChanged(int)),plc,SLOT(plcSetAcqInterval(int)),Qt::QueuedConnection);

    connect(plc,&CPLC::plcError,this,&MainWindow::plcErrorMsg,Qt::QueuedConnection);
    connect(plc,&CPLC::plcConnectFailed,this,&MainWindow::plcStartFailed,Qt::QueuedConnection);
    connect(plc,&CPLC::plcStartFailed,this,&MainWindow::plcStartFailed,Qt::QueuedConnection);
    connect(plc,&CPLC::plcOnConnect,this,&MainWindow::plcConnected,Qt::QueuedConnection);
    connect(plc,&CPLC::plcOnDisconnect,this,&MainWindow::plcDisconnected,Qt::QueuedConnection);
    connect(plc,&CPLC::plcOnStart,this,&MainWindow::plcStarted,Qt::QueuedConnection);
    connect(plc,&CPLC::plcOnStop,this,&MainWindow::plcStopped,Qt::QueuedConnection);
    connect(plc,&CPLC::plcVariablesUpdatedConsistent,
            this,&MainWindow::plcVariablesUpdatedConsistent,Qt::QueuedConnection);
    connect(plc,&CPLC::plcScanTime,ui->lblActualAcqInterval,&QLabel::setText,Qt::QueuedConnection);

    connect(ui->tableVariables,&CTableView::customContextMenuRequested,this,&MainWindow::variablesCtxMenu);
    connect(graph,&CGraphForm::logMessage,this,&MainWindow::appendLog);
    connect(graph,&CGraphForm::stopGraph,this,&MainWindow::plotStop);

    connect(vtmodel,&CVarModel::syncPLCtoModel,plc,&CPLC::plcSetWatchpoints);
    connect(this,&MainWindow::plcSetAddress,plc,&CPLC::plcSetAddress);
    connect(this,&MainWindow::plcSetRetryParams,plc,&CPLC::plcSetRetryParams);
    connect(this,&MainWindow::plcConnect,plc,&CPLC::plcConnect);
    connect(this,&MainWindow::plcStart,plc,&CPLC::plcStart);
    connect(this,&MainWindow::plcDisconnect,plc,&CPLC::plcDisconnect);

    QTimer* syncTimer = new QTimer(this);
    syncTimer->setInterval(60000);
    connect(syncTimer,&QTimer::timeout,this,&MainWindow::csvSync);
    syncTimer->start();

    bool needToStart = false;
    for (int i=1;i<QApplication::arguments().count();i++) {
        QString s = QApplication::arguments().at(i);
        QFileInfo fi(s);
        if (fi.exists())
            loadConnectionFromFile(s);
        else if (s.toLower().startsWith("-start"))
            needToStart = true;
    }
    if (needToStart && (vtmodel->getCWPCount()>0)) {
        appendLog(trUtf8("Automatic reconnect after 10 sec from command prompt."));
        autoOnLogging = true;
        QTimer::singleShot(10*1000,this,&MainWindow::ctlAggregatedStart);
    }

    gSet->loadSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::appendLog(const QString &msg)
{
    ui->textLog->appendPlainText(trUtf8("%1: %2").
                                 arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss")).
                                 arg(msg));
}

void MainWindow::loadConnectionFromFile(const QString &fname)
{
    QFile f(fname);
    if (f.open(QIODevice::ReadOnly)) {
        QDataStream in(&f);
        in.setVersion(QDataStream::Qt_5_2);
        QString aip;
        int acqInt,arack,aslot, v;
        in >> v;
        if (v==PLR_VERSION) {
            in >> aip >> arack >> aslot >> acqInt;
            ui->editIP->setText(aip);
            ui->editRack->setValue(arack);
            ui->editSlot->setValue(aslot);
            ui->editAcqInterval->setValue(acqInt);
            vtmodel->loadWPList(in);
            f.close();
            appendLog(trUtf8("File %1 loaded.").arg(fname));
            return;
        } else {
            f.close();
            QMessageBox::critical(this,trUtf8("PLC recorder error"),
                                  trUtf8("Unable to load file %1.\n"
                                         "Incompatible version.").arg(fname));
        }
    } else
        QMessageBox::critical(this,trUtf8("PLC recorder error"),
                              trUtf8("Unable to load file %1.").arg(fname));
    appendLog(trUtf8("Unable to load file %1.").arg(fname));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (gSet!=nullptr)
        gSet->saveSettings();
    event->accept();
}

void MainWindow::plcConnected()
{
    ui->btnConnect->setEnabled(false);
    vtdelegate->setEditEnabled(false);
    vtmodel->setEditEnabled(false);
    ui->btnDisconnect->setEnabled(true);
    cbVat->setEnabled(false);
    cbVat->setChecked(false);
    cbVat->setStyleSheet(QString());
    cbRec->setEnabled(false);
    cbRec->setChecked(false);
    cbRec->setStyleSheet(QString());
    cbPlot->setEnabled(false);
    cbPlot->setChecked(false);
    cbPlot->setStyleSheet(QString());

    ui->actionLoadConnection->setEnabled(false);

    lblState->setText(trUtf8("Online"));
    appendLog(trUtf8("Connected to PLC."));
    csvStopClose();
}

void MainWindow::plcDisconnected()
{
    ui->btnConnect->setEnabled(true);
    vtdelegate->setEditEnabled(true);
    vtmodel->setEditEnabled(true);
    ui->btnDisconnect->setEnabled(false);
    cbVat->setEnabled(false);
    cbVat->setChecked(false);
    cbVat->setStyleSheet(QString());
    cbRec->setEnabled(false);
    cbRec->setChecked(false);
    cbRec->setStyleSheet(QString());
    cbPlot->setEnabled(false);
    cbPlot->setChecked(false);
    cbPlot->setStyleSheet(QString());

    ui->actionLoadConnection->setEnabled(true);
    lblState->setText(trUtf8("Offline"));
    appendLog(trUtf8("Disconnected from PLC."));
    csvStopClose();
}

void MainWindow::plcStarted()
{
    agcRestartCounter = 0;
    aggregatedStartActive = false;
    ui->btnConnect->setEnabled(false);
    vtdelegate->setEditEnabled(false);
    vtmodel->setEditEnabled(false);
    ui->btnDisconnect->setEnabled(true);
    cbVat->setEnabled(true);
    cbVat->setChecked(false);
    cbVat->setStyleSheet(QString());
    cbRec->setEnabled(true);
    cbRec->setChecked(false);
    cbRec->setStyleSheet(QString());
    cbPlot->setEnabled(true);
    cbPlot->setChecked(false);
    cbPlot->setStyleSheet(QString());

    ui->actionLoadConnection->setEnabled(false);
    lblState->setText(trUtf8("<b>ONLINE</b>"));
    appendLog(trUtf8("Activating ONLINE."));

    graph->setupGraphs(vtmodel->getCWPList());

    if (autoOnLogging || (gSet->restoreCSV && savedCSVActive)) {
        appendLog(trUtf8("Restore CSV recording."));
        cbRec->setChecked(true);
        csvCaptureControl();
    }
}

void MainWindow::plcStopped()
{
    ui->btnConnect->setEnabled(false);
    vtdelegate->setEditEnabled(false);
    vtmodel->setEditEnabled(false);
    ui->btnDisconnect->setEnabled(true);
    cbVat->setEnabled(false);
    cbVat->setChecked(false);
    cbVat->setStyleSheet(QString());
    cbRec->setEnabled(false);
    savedCSVActive = cbRec->isChecked();
    cbRec->setChecked(false);
    cbRec->setStyleSheet(QString());
    cbPlot->setEnabled(false);
    cbPlot->setChecked(false);
    cbPlot->setStyleSheet(QString());

    ui->actionLoadConnection->setEnabled(false);
    lblState->setText(trUtf8("Online"));
    appendLog(trUtf8("Deactivating ONLINE."));
    csvStopClose();
}

void MainWindow::plcStartFailed()
{
    if (agcRestartCounter!=INT_MAX) {
        if (gSet->tmTotalRetryCount==0)
            agcRestartCounter = 0;
        if (agcRestartCounter<=gSet->tmTotalRetryCount) {
            QTimer::singleShot(gSet->tmWaitReconnect*1000,this,&MainWindow::ctlAggregatedStart);
        } else
            ctlStop();
    }
}

void MainWindow::plcErrorMsg(const QString &msg)
{
    appendLog(msg);
    if (!gSet->suppressMsgBox)
        QMessageBox::critical(this,trUtf8("PLC recorder error"),msg);
}

void MainWindow::plcVariablesUpdatedConsistent(const CWPList &wp, const QDateTime &stm)
{
    // Updating VAT
    if (cbVat->isChecked()) {
        vtmodel->loadActualsFromPLC(wp);
    }

    // Updating CSV
    if ((cbRec->isChecked()) && (csvLog.device()!=nullptr)) {
        if (!csvHasHeader) {
            QString hdr = trUtf8("\"Time\"; ");
            for (int i=0;i<wp.count();i++) {
                hdr += QString("\"%1 (%2)\"; ").
                        arg(wp.at(i).label).
                        arg(gSet->plcGetAddrName(wp.at(i)));
            }
            csvLog << hdr << QString("\r\n");
            csvHasHeader = true;
        }
        QString s = QString("\"%1\"; ").arg(stm.toString("yyyy-MM-dd hh:mm:ss.zzz"));
        for (int i=0;i<wp.count();i++) {
            s += QString("%1; ").arg(gSet->plcFormatActualValue(wp.at(i)));
        }
        csvLog << s << QString("\r\n");
    }

    // Updating Plot
    if (cbPlot->isChecked()) {
        graph->addData(wp,stm);
    }
}

void MainWindow::connectPLC()
{
    vtmodel->syncPLC();
    emit plcSetRetryParams(gSet->tmMaxRecErrorCount, gSet->tmMaxConnectRetryCount, gSet->tmWaitReconnect);
    emit plcSetAddress(ui->editIP->text(), ui->editRack->value(), ui->editSlot->value(), gSet->tmTCPTimeout);
    emit plcConnect();
}

void MainWindow::aboutMsg()
{
    QMessageBox::about(this,trUtf8("PLC recorder"),
                       trUtf8("PLC Recorder is a recording and debugging tool for Siemens S7 300/400 PLCs.\n\n"
                              "Written by kernel1024, 2012-2018, under GPLv2 license.\n\n"
                              "Includes partial libnodave snapshot.\n"
                              "libnodave (c) Thomas Hergenhahn (thomas.hergenhahn@web.de) "
                              "2002-2005, under license GPLv2.\n\n\n"
                              "BIG FAT WARNING:\n"
                              "This is beta code and information. You assume all responsibility for its use.\n"
                              "DANGER: DON'T connect to a PLC unless you are certain it is safe to do so!!!\n"
                              "It is assumed that you are experienced in PLC programming/troubleshooting and that\n"
                              "you know EXACTLY what you are doing. PLCs are used to control industrial processes,\n"
                              "motors, steam valves, hydraulic presses, etc.\n\n"
                              "You are ABSOLUTELY RESPONSIBLE for ensuring that NO-ONE is in danger of being injured or killed\n"
                              "because you affected the operation of a running PLC.\n\n"
                              "Also expect that buggy drivers could write data even when you expect that they will read only!!!"));
}

void MainWindow::aboutQtMsg()
{
    QMessageBox::aboutQt(this,trUtf8("About Qt"));
}

void MainWindow::variablesCtxMenu(QPoint pos)
{
    if (!vtmodel->isEditEnabled()) return;

    QMenu cm(ui->tableVariables);

    QAction* acm;
    acm = cm.addAction(QIcon(":/new"),trUtf8("Add variable"));
    connect(acm,&QAction::triggered,this,[this](){
        vtmodel->insertRow(vtmodel->getCWPCount());
    });

    acm = cm.addAction(QIcon(":/delete"),trUtf8("Remove variable"));
    connect(acm,&QAction::triggered,this,[this](){
        QModelIndexList mi = ui->tableVariables->selectionModel()->selectedIndexes();
        QList<int> rows;
        for (int i=0;i<mi.count();i++) {
            if (!mi.at(i).isValid()) continue;
            rows << mi.at(i).row();
        }
        if (!rows.isEmpty())
            vtmodel->removeMultipleRows(rows);
    });
    acm->setEnabled(!(ui->tableVariables->selectionModel()->selectedIndexes().isEmpty()));

    cm.addSeparator();

    acm = cm.addAction(trUtf8("Remove all"));
    connect(acm,&QAction::triggered,this,[this](){
        vtmodel->removeRows(0,vtmodel->getCWPCount());
    });

    QPoint p = pos;
    p.setY(p.y()+ui->tableVariables->horizontalHeader()->height());
    p.setX(p.x()+ui->tableVariables->verticalHeader()->width());
    cm.exec(ui->tableVariables->mapToGlobal(p));
}

void MainWindow::settingsDlg()
{
    CSettingsDialog dlg;
    // TODO: add plot parameters
    dlg.setParams(gSet->outputCSVDir,gSet->outputFileTemplate,gSet->tmTCPTimeout,gSet->tmMaxRecErrorCount,
                  gSet->tmMaxConnectRetryCount,gSet->tmWaitReconnect,gSet->tmTotalRetryCount,gSet->suppressMsgBox,
                                        gSet->restoreCSV);
    if (dlg.exec()) {
        gSet->tmTCPTimeout = dlg.getTCPTimeout();
        gSet->tmMaxRecErrorCount = dlg.getMaxRecErrorCount();
        gSet->tmMaxConnectRetryCount = dlg.getMaxConnectRetryCount();
        gSet->tmWaitReconnect = dlg.getWaitReconnect();
        gSet->tmTotalRetryCount = dlg.getTotalRetryCount();
        gSet->suppressMsgBox = dlg.getSuppressMsgBox();
        gSet->restoreCSV = dlg.getRestoreCSV();
        gSet->outputCSVDir = dlg.getOutputDir();
        gSet->outputFileTemplate = dlg.getFileTemplate();
    }
}

void MainWindow::loadConnection()
{
    QString s = getOpenFileNameD(this,trUtf8("Load connection settings file"),QString(),
                                 trUtf8("PLC recorder files (*.plr)"));
    if (s.isEmpty()) return;

    loadConnectionFromFile(s);
}

void MainWindow::saveConnection()
{
    QString s = getSaveFileNameD(this,trUtf8("Save connection settings file"),QString(),
                                 trUtf8("PLC recorder files (*.plr)"));
    if (s.isEmpty()) return;
    QFileInfo fi(s);
    if (fi.suffix().isEmpty()) s+=trUtf8(".plr");

    QFile f(s);
    if (f.open(QIODevice::WriteOnly)) {
        QDataStream out(&f);
        out.setVersion(QDataStream::Qt_5_2);
        int v = PLR_VERSION;
        out << v << ui->editIP->text() << ui->editRack->value() <<
               ui->editSlot->value() << ui->editAcqInterval->value();
        vtmodel->saveWPList(out);
        f.flush();
        f.close();
        appendLog(trUtf8("File was saved %1.").arg(s));
    } else {
        appendLog(trUtf8("Unable to save file %1.").arg(s));
        QMessageBox::critical(this,trUtf8("PLC recorder error"),trUtf8("Unable to save file %1.").arg(s));
    }
}

void MainWindow::csvCaptureControl()
{
    if (cbRec->isChecked()) {
        csvRotateFile();
        if (csvLog.device()!=nullptr)
            cbRec->setStyleSheet("background-color: red; color: white; font: bold;");
    } else {
        csvStopClose();
        cbRec->setStyleSheet(QString());
    }
    ui->actionForceRotateCSV->setEnabled(cbRec->isChecked() && (csvLog.device()!=nullptr));
}

void MainWindow::csvRotateFile()
{
    csvPrevTime = QTime::currentTime();
    if (gSet->outputCSVDir.isEmpty()) {
        cbRec->setChecked(false);
        cbRec->setStyleSheet(QString());
        ui->actionForceRotateCSV->setEnabled(false);
        appendLog(trUtf8("CSV rotation failure. Directory for creating CSV files not configured."));
        if (!gSet->suppressMsgBox)
            QMessageBox::warning(this,trUtf8("PLC recorder error"),
                                 trUtf8("Directory for creating CSV files not configured."));
        return;
    }
    csvStopClose();

    QDir d(gSet->outputCSVDir);
    QString fname = d.filePath(QString("%1_%2.csv").
                               arg(gSet->outputFileTemplate).
                               arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss")));
    QFile *f = new QFile(fname);
    if (!f->open(QIODevice::WriteOnly)) {
        cbRec->setChecked(false);
        cbRec->setStyleSheet(QString());
        ui->actionForceRotateCSV->setEnabled(false);
        appendLog(trUtf8("Unable to save CSV file %1.").arg(fname));
        if (!gSet->suppressMsgBox)
            QMessageBox::critical(this,trUtf8("PLC recorder error"),
                                  trUtf8("Unable to save file '%1'").arg(fname));
        return;
    }

    csvLog.setDevice(f);
    csvLog.setCodec("Windows-1251");
    csvHasHeader = false;
    ui->actionForceRotateCSV->setEnabled(cbRec->isChecked() && (csvLog.device()!=nullptr));
    appendLog(trUtf8("CSV rotation was successful."));
}

void MainWindow::csvSync()
{
    if (!cbRec->isChecked()) return;

    // reset cache
    if (csvLog.device()!=nullptr)
        csvLog.flush();

    // rotate csv at 0:00
    QTime curTime = QTime::currentTime();
    bool needToRotate = (csvPrevTime.hour()>curTime.hour());
    csvPrevTime = curTime;
    if (needToRotate)
        csvRotateFile();
}

void MainWindow::csvStopClose()
{
    if (csvLog.device()!=nullptr) {
        csvLog.flush();
        csvLog.device()->close();
        csvLog.setDevice(nullptr);
        appendLog(trUtf8("CSV recording stopped. File closed."));
    }
    ui->actionForceRotateCSV->setEnabled(false);
}

void MainWindow::plotControl()
{
    if (cbPlot->isChecked()) {
        graph->show();
        cbPlot->setStyleSheet("background-color: blue; color: yellow; font: bold;");
    } else {
        graph->hide();
        cbPlot->setStyleSheet(QString());
    }
}

void MainWindow::plotStop()
{
    cbPlot->setChecked(false);
    cbPlot->setStyleSheet(QString());
    graph->hide();
}

void MainWindow::vatControl()
{
    if (cbVat->isChecked())
        cbVat->setStyleSheet("background-color: cyan; color: white; font: bold;");
    else
        cbVat->setStyleSheet(QString());
}

void MainWindow::ctlAggregatedStart()
{
    if (agcRestartCounter==INT_MAX) {
        agcRestartCounter = 0;
        return;
    }
    aggregatedStartActive = true;
    ui->btnDisconnect->setEnabled(true);
    connectPLC();
    emit plcStart();
    agcRestartCounter++;
    appendLog(trUtf8("Activating PLC connection request %1.").arg(agcRestartCounter));
}

void MainWindow::ctlStop()
{
    if (qobject_cast<QAbstractButton *>(sender())!=nullptr)
        autoOnLogging = false;

    if (aggregatedStartActive)
        agcRestartCounter=INT_MAX;
    emit plcDisconnect();
    appendLog(trUtf8("Activation PLC disconnection request."));
    ui->btnDisconnect->setEnabled(false);
}

void MainWindow::sysSIGPIPE()
{
    appendLog(trUtf8("SIGPIPE received. Force reconnect to PLC."));
    emit plcDisconnect();
    ctlAggregatedStart();
}
