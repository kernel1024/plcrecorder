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

    gSet = new CGlobal(this);
    csvHandler = new CCSVHandler(this);

    agcRestartCounter = 0;
    autoOnLogging = false;
    savedCSVActive = false;
    aggregatedStartActive = false;

    lblState = new QLabel(trUtf8("Offline"));
    cbVat = new QCheckBox(trUtf8("Online VAT"));
    cbRec = new QCheckBox(trUtf8("CSV recording"));
    cbPlot = new QCheckBox(trUtf8("Signal plot"));
    ui->statusBar->addPermanentWidget(cbPlot);
    ui->statusBar->addPermanentWidget(cbRec);
    ui->statusBar->addPermanentWidget(cbVat);
    ui->statusBar->addPermanentWidget(lblState);

    connect(ui->actionSettings,SIGNAL(triggered()),this,SLOT(settingsDlg()));
    connect(ui->actionLoadConnection,SIGNAL(triggered()),this,SLOT(loadConnection()));
    connect(ui->actionSaveConnection,SIGNAL(triggered()),this,SLOT(saveConnection()));
    connect(ui->actionForceRotateCSV,SIGNAL(triggered()),csvHandler,SLOT(rotateFile()));
    connect(ui->actionAbout,SIGNAL(triggered()),this,SLOT(aboutMsg()));
    connect(ui->actionAboutQt,SIGNAL(triggered()),this,SLOT(aboutQtMsg()));
    connect(ui->tableVariables,SIGNAL(ctxMenuRequested(QPoint)),this,SLOT(variablesCtxMenu(QPoint)));

    connect(cbVat,SIGNAL(clicked()),this,SLOT(vatControl()));
    connect(cbRec,SIGNAL(clicked()),this,SLOT(csvControl()));
    connect(cbPlot,SIGNAL(clicked()),this,SLOT(plotControl()));

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

    connect(this,SIGNAL(plcCorrectToThread()),plc,SLOT(correctToThread()),Qt::QueuedConnection);
    emit plcCorrectToThread();

    graph = new CGraphForm(this);
    graph->setWindowFlags(graph->windowFlags() | Qt::Window);
    graph->hide();
    connect(ui->actionShowPlot,SIGNAL(triggered()),graph,SLOT(show()));

    connect(ui->btnConnect,SIGNAL(clicked()),this,SLOT(ctlAggregatedStartForce()));
    connect(ui->btnDisconnect,SIGNAL(clicked()),this,SLOT(ctlStop()));
    connect(ui->editAcqInterval,SIGNAL(valueChanged(int)),
            plc,SLOT(plcSetAcqInterval(int)),Qt::QueuedConnection);

    connect(plc,SIGNAL(plcError(QString,bool)),this,SLOT(plcErrorMsg(QString,bool)),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcConnectFailed()),this,SLOT(plcStartFailed()),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcStartFailed()),this,SLOT(plcStartFailed()),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcOnConnect()),this,SLOT(plcConnected()),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcOnDisconnect()),this,SLOT(plcDisconnected()),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcOnStart()),this,SLOT(plcStarted()),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcOnStop()),this,SLOT(plcStopped()),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcVariablesUpdatedConsistent(CWPList,QDateTime)),
            this,SLOT(plcVariablesUpdatedConsistent(CWPList,QDateTime)),Qt::QueuedConnection);
    connect(plc,SIGNAL(plcScanTime(QString)),ui->lblActualAcqInterval,SLOT(setText(QString)),Qt::QueuedConnection);

    connect(ui->tableVariables,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(variablesCtxMenu(QPoint)));
    connect(graph,SIGNAL(logMessage(QString)),this,SLOT(appendLog(QString)));
    connect(graph,SIGNAL(stopGraph()),this,SLOT(plotStop()));

    connect(vtmodel,SIGNAL(syncPLCtoModel(CWPList)),plc,SLOT(plcSetWatchpoints(CWPList)));
    connect(this,SIGNAL(plcSetAddress(QString,int,int,int)),plc,SLOT(plcSetAddress(QString,int,int,int)));
    connect(this,SIGNAL(plcSetRetryParams(int,int,int)),plc,SLOT(plcSetRetryParams(int,int,int)));
    connect(this,SIGNAL(plcConnect()),plc,SLOT(plcConnect()));
    connect(this,SIGNAL(plcStart()),plc,SLOT(plcStart()));
    connect(this,SIGNAL(plcDisconnect()),plc,SLOT(plcDisconnect()));

    QTimer* syncTimer = new QTimer(this);
    syncTimer->setInterval(60000);
    connect(syncTimer,SIGNAL(timeout()),this,SLOT(syncTimer()));
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
        QTimer::singleShot(10*1000,this,SLOT(ctlAggregatedStartForce()));
    }

    connect(csvHandler,SIGNAL(errorMessage(QString)),this,SLOT(csvError(QString)));
    connect(csvHandler,SIGNAL(recordingStopped()),this,SLOT(recordingStopped()));
    connect(csvHandler,SIGNAL(appendLog(QString)),this,SLOT(appendLog(QString)));
    connect(this,SIGNAL(csvSync()),csvHandler,SLOT(timerSync()));

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

void MainWindow::syncTimer()
{
    if (cbRec->isChecked())
        emit csvSync();
}

void MainWindow::csvError(const QString &msg)
{
    if (!gSet->suppressMsgBox)
        QMessageBox::critical(this,trUtf8("PLC recorder error"),msg);
}

void MainWindow::recordingStopped()
{
    cbRec->setChecked(false);
    cbRec->setStyleSheet(QString());
    ui->actionForceRotateCSV->setEnabled(false);
}

void MainWindow::loadConnectionFromFile(const QString &fname)
{
    QFile f(fname);
    if (f.open(QIODevice::ReadOnly)) {
        QDataStream in(&f);
        in.setVersion(QDataStream::Qt_4_8);
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

    csvHandler->stopClose();
    ui->actionForceRotateCSV->setEnabled(false);
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

    csvHandler->stopClose();
    ui->actionForceRotateCSV->setEnabled(false);
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

    if (autoOnLogging || (gSet->restoreCSV && savedCSVActive)) {
        appendLog(trUtf8("Restore CSV recording."));
        cbRec->setChecked(true);
        csvControl();
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

    csvHandler->stopClose();
    ui->actionForceRotateCSV->setEnabled(false);
}

void MainWindow::plcStartFailed()
{
    if (agcRestartCounter!=INT_MAX) {
        if (gSet->tmTotalRetryCount==0)
            agcRestartCounter = 0;
        if (agcRestartCounter<=gSet->tmTotalRetryCount) {
            QTimer::singleShot(gSet->tmWaitReconnect*1000,this,SLOT(ctlAggregatedStart()));
        } else
            ctlStop();
    }
}

void MainWindow::plcErrorMsg(const QString &msg, bool critical)
{
    appendLog(msg);
    if (critical && !gSet->suppressMsgBox)
        QMessageBox::critical(this,trUtf8("PLC recorder error"),msg);
}

void MainWindow::plcVariablesUpdatedConsistent(const CWPList &wp, const QDateTime &stm)
{
    // Updating VAT
    if (cbVat->isChecked())
        vtmodel->loadActualsFromPLC(wp);

    // Updating CSV
    if (cbRec->isChecked())
        csvHandler->addData(wp,stm);

    // Updating Plot
    if (cbPlot->isChecked())
        graph->addData(wp,stm);
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
    connect(acm,SIGNAL(triggered()),this,SLOT(ctxNew()));

    acm = cm.addAction(QIcon(":/delete"),trUtf8("Remove variable"));
    connect(acm,SIGNAL(triggered()),this,SLOT(ctxRemove()));
    acm->setEnabled(!(ui->tableVariables->selectionModel()->selectedIndexes().isEmpty()));

    cm.addSeparator();

    acm = cm.addAction(trUtf8("Remove all"));
    connect(acm,SIGNAL(triggered()),this,SLOT(ctxRemoveAll()));

    QPoint p = pos;
    p.setY(p.y()+ui->tableVariables->horizontalHeader()->height());
    p.setX(p.x()+ui->tableVariables->verticalHeader()->width());
    cm.exec(ui->tableVariables->mapToGlobal(p));
}

void MainWindow::ctxNew()
{
    vtmodel->insertRow(vtmodel->getCWPCount());
}

void MainWindow::ctxRemove()
{
    QModelIndexList mi = ui->tableVariables->selectionModel()->selectedIndexes();
    QList<int> rows;
    for (int i=0;i<mi.count();i++) {
        if (!mi.at(i).isValid()) continue;
        rows << mi.at(i).row();
    }
    if (!rows.isEmpty())
        vtmodel->removeMultipleRows(rows);
}

void MainWindow::ctxRemoveAll()
{
    vtmodel->removeRows(0,vtmodel->getCWPCount());
}

void MainWindow::settingsDlg()
{
    CSettingsDialog dlg;
    dlg.setParams(gSet->outputCSVDir,gSet->outputFileTemplate,gSet->tmTCPTimeout,gSet->tmMaxRecErrorCount,
                  gSet->tmMaxConnectRetryCount,gSet->tmWaitReconnect,gSet->tmTotalRetryCount,gSet->suppressMsgBox,
                  gSet->restoreCSV,gSet->plotVerticalSize,gSet->plotShowScatter,gSet->plotAntialiasing);
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
        gSet->plotVerticalSize = dlg.getPlotVerticalSize();
        gSet->plotShowScatter = dlg.getPlotShowScatter();
        gSet->plotAntialiasing = dlg.getPlotAntialiasing();
    }
}

void MainWindow::loadConnection()
{
    QString s = getOpenFileNameD(this,trUtf8("Load connection settings file"),gSet->savedAuxDir,
                                 trUtf8("PLC recorder files (*.plr)"));
    if (s.isEmpty()) return;
    gSet->savedAuxDir = QFileInfo(s).absolutePath();

    loadConnectionFromFile(s);
}

void MainWindow::saveConnection()
{
    QString s = getSaveFileNameD(this,trUtf8("Save connection settings file"),gSet->savedAuxDir,
                                 trUtf8("PLC recorder files (*.plr)"));
    if (s.isEmpty()) return;
    gSet->savedAuxDir = QFileInfo(s).absolutePath();

    QFile f(s);
    if (f.open(QIODevice::WriteOnly)) {
        QDataStream out(&f);
        out.setVersion(QDataStream::Qt_4_8);
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

void MainWindow::csvControl()
{
    bool fileOpened = false;
    if (cbRec->isChecked()) {
        fileOpened = csvHandler->rotateFile();
        if (fileOpened)
            cbRec->setStyleSheet("background-color: red; color: white; font: bold;");
    } else {
        csvHandler->stopClose();
        cbRec->setStyleSheet(QString());
    }
    ui->actionForceRotateCSV->setEnabled(cbRec->isChecked() && fileOpened);
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

void MainWindow::ctlAggregatedStartForce()
{
    agcRestartCounter = 0;
    ctlAggregatedStart();
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
    ctlAggregatedStartForce();
}
