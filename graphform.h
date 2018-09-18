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
    enum CursorType {
        ctRunning = 1,
        ctLeft = 2,
        ctRight = 3
    };
    Q_ENUM(CursorType)

    explicit CGraphForm(QWidget *parent = nullptr);
    ~CGraphForm();

    void setupGraphs(const CWPList &wp);
    void addData(const CWPList &wp, const QDateTime &time);

private:
    Ui::CGraphForm *ui;
    CWPList watchpoints;
    QCPItemStraightLine *runningCursor, *leftCursor, *rightCursor;
    bool moveSplitterOnce;
    int getScreenWidth();
    void createCursorSignal(CGraphForm::CursorType cursor, double timestamp);

protected:
    virtual void closeEvent(QCloseEvent * event);
    virtual void showEvent(QShowEvent * event);

signals:
    void logMessage(const QString& msg);
    void stopGraph();
    void cursorMoved(CGraphForm::CursorType cursor, const QDateTime &time, const CWPList &wp);

public slots:
    void clearData();

private slots:
    void plotRangeChanged (const QCPRange &newRange);
    void plotMouseMove (QMouseEvent *event);

};

#endif // GRAPHFORM_H
