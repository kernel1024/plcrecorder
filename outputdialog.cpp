#include <QtCore>
#include <QtGui>
#include "outputdialog.h"
#include "ui_outputdialog.h"
#include "global.h"


COutputDialog::COutputDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::COutputDialog)
{
    ui->setupUi(this);
    connect(ui->btnCSVDir,&QPushButton::clicked,this,&COutputDialog::selectDirDlg);
}

COutputDialog::~COutputDialog()
{
    delete ui;
}

void COutputDialog::setParams(const QString &outputDir, const QString &fileTemplate)
{
    ui->editCSVDir->setText(outputDir);
    ui->editCSVTemplate->setText(fileTemplate);
}

QString COutputDialog::getOutputDir() const
{
    return ui->editCSVDir->text();
}

QString COutputDialog::getFileTemplate() const
{
    return ui->editCSVTemplate->text();
}

void COutputDialog::selectDirDlg()
{
    QString s = getExistingDirectoryD(this,trUtf8("Directory for CSV files"),ui->editCSVDir->text(), nullptr);
    if (!s.isEmpty()) ui->editCSVDir->setText(s);
}
