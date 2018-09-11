#include "timeoutsdialog.h"
#include "ui_timeoutsdialog.h"

CTimeoutsDialog::CTimeoutsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CTimeoutsDialog)
{
    ui->setupUi(this);
}

CTimeoutsDialog::~CTimeoutsDialog()
{
    delete ui;
}

void CTimeoutsDialog::setParams(int tcpTimeout, int maxRecErrorCount, int maxConnectRetryCount,
                                int waitReconnect, int totalRetryCount, bool suppressMsgBox,
                                bool restoreCSV)
{
    ui->spinNetTimeout->setValue(tcpTimeout/1000);
    ui->spinMaxRecErrorCount->setValue(maxRecErrorCount);
    ui->spinConnectRetryCount->setValue(maxConnectRetryCount);
    ui->spinWaitBeforeRetryConnect->setValue(waitReconnect);
    ui->spinTotalRetryCount->setValue(totalRetryCount);
    ui->checkSuppressMsgBoxes->setChecked(suppressMsgBox);
    ui->checkRestoreCSV->setChecked(restoreCSV);
}

int CTimeoutsDialog::getTCPTimeout()
{
    return ui->spinNetTimeout->value()*1000;
}

int CTimeoutsDialog::getMaxRecErrorCount()
{
    return ui->spinMaxRecErrorCount->value();
}

int CTimeoutsDialog::getMaxConnectRetryCount()
{
    return ui->spinConnectRetryCount->value();
}

int CTimeoutsDialog::getWaitReconnect()
{
    return ui->spinWaitBeforeRetryConnect->value();
}

int CTimeoutsDialog::getTotalRetryCount()
{
    return ui->spinTotalRetryCount->value();
}

bool CTimeoutsDialog::getSuppressMsgBox()
{
    return ui->checkSuppressMsgBoxes->isChecked();
}

bool CTimeoutsDialog::getRestoreCSV()
{
    return ui->checkRestoreCSV->isChecked();
}
