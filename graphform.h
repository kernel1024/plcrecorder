#ifndef GRAPHFORM_H
#define GRAPHFORM_H

#include <QWidget>
#include "qcustomplot-source/qcustomplot.h"
#include "global.h"
#include "plc.h"

namespace Ui {
class CGraphForm;
}

class CGraphForm : public QWidget
{
    Q_OBJECT

public:
    explicit CGraphForm(QWidget *parent = nullptr);
    ~CGraphForm();

    void setupGraphs(const CWPList &wp);
    void addData(const CWPList &wp, const QDateTime &time);

private:
    Ui::CGraphForm *ui;
    QCPItemLine *runningCursor;
    int getScreenWidth();

protected:
    virtual void closeEvent(QCloseEvent * event);

signals:
    void logMessage(const QString& msg);
    void stopGraph();

public slots:
    void clearData();

private slots:
    void plotRangeChanged (const QCPRange &newRange);
    void plotMouseMove (QMouseEvent *event);

};

#endif // GRAPHFORM_H
