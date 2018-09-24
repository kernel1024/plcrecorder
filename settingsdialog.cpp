#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "global.h"


CSettingsDialog::CSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CSettingsDialog)
{
    ui->setupUi(this);
    connect(ui->btnCSVDir,SIGNAL(clicked()),this,SLOT(selectDirDlg()));
}

CSettingsDialog::~CSettingsDialog()
{
    delete ui;
}

int CSettingsDialog::getTCPTimeout()
{
    return ui->spinNetTimeout->value()*1000;
}

int CSettingsDialog::getMaxRecErrorCount()
{
    return ui->spinMaxRecErrorCount->value();
}

int CSettingsDialog::getMaxConnectRetryCount()
{
    return ui->spinConnectRetryCount->value();
}

int CSettingsDialog::getWaitReconnect()
{
    return ui->spinWaitBeforeRetryConnect->value();
}

int CSettingsDialog::getTotalRetryCount()
{
    return ui->spinTotalRetryCount->value();
}

bool CSettingsDialog::getSuppressMsgBox()
{
    return ui->checkSuppressMsgBoxes->isChecked();
}

bool CSettingsDialog::getRestoreCSV()
{
    return ui->checkRestoreCSV->isChecked();
}

int CSettingsDialog::getPlotVerticalSize()
{
    return ui->spinPlotVerticalSize->value();
}

bool CSettingsDialog::getPlotShowScatter()
{
    return ui->checkPlotShotScatter->isChecked();
}

bool CSettingsDialog::getPlotAntialiasing()
{
    return ui->checkAntialiasing->isChecked();
}


void CSettingsDialog::setParams(const QString &outputDir, const QString &fileTemplate, int tcpTimeout,
                                int maxRecErrorCount, int maxConnectRetryCount, int waitReconnect,
                                int totalRetryCount, bool suppressMsgBox, bool restoreCSV,
                                int plotVerticalSize, bool plotShowScatter, bool plotAntialiasing)
{
    ui->editCSVDir->setText(outputDir);
    ui->editCSVTemplate->setText(fileTemplate);
    ui->spinNetTimeout->setValue(tcpTimeout/1000);
    ui->spinMaxRecErrorCount->setValue(maxRecErrorCount);
    ui->spinConnectRetryCount->setValue(maxConnectRetryCount);
    ui->spinWaitBeforeRetryConnect->setValue(waitReconnect);
    ui->spinTotalRetryCount->setValue(totalRetryCount);
    ui->checkSuppressMsgBoxes->setChecked(suppressMsgBox);
    ui->checkRestoreCSV->setChecked(restoreCSV);
    ui->spinPlotVerticalSize->setValue(plotVerticalSize);
    ui->checkPlotShotScatter->setChecked(plotShowScatter);
    ui->checkAntialiasing->setChecked(plotAntialiasing);
}

QString CSettingsDialog::getOutputDir() const
{
    return ui->editCSVDir->text();
}

QString CSettingsDialog::getFileTemplate() const
{
    return ui->editCSVTemplate->text();
}

void CSettingsDialog::selectDirDlg()
{
    QString s = getExistingDirectoryD(this,trUtf8("Directory for CSV files"),ui->editCSVDir->text());
    if (!s.isEmpty()) ui->editCSVDir->setText(s);
}
