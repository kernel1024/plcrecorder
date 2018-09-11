#ifndef OUTPUTDIALOG_H
#define OUTPUTDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class COutputDialog;
}

class COutputDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit COutputDialog(QWidget *parent = nullptr);
    ~COutputDialog();

    void setParams(const QString& outputDir, const QString& fileTemplate);
    QString getOutputDir() const;
    QString getFileTemplate() const;

private:
    Ui::COutputDialog *ui;

private slots:
    void selectDirDlg();
};

#endif // OUTPUTDIALOG_H
