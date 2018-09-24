#include "specwidgets.h"
#include <QAbstractButton>
#include <QHeaderView>
#include <QMouseEvent>

CTableView::CTableView(QWidget *parent) :
    QTableView(parent)
{
}

void CTableView::selectAll()
{
    if (qobject_cast<QAbstractButton *>(sender())!=NULL) {
        QPoint p = mapFromGlobal(QCursor::pos());
        p.setX(p.x()-verticalHeader()->width());
        p.setY(p.y()-horizontalHeader()->height());
        if (!p.isNull())
            emit ctxMenuRequested(p);
    } else
        QTableView::selectAll();
}

void CTableView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!indexAt(event->pos()).isValid())
        emit ctxMenuRequested(event->pos());
    else
        QTableView::mouseDoubleClickEvent(event);
}


void CSleep::sleep(unsigned long secs)
{
    QThread::sleep(secs);
}

void CSleep::msleep(unsigned long msecs)
{
    QThread::msleep(msecs);
}

void CSleep::usleep(unsigned long usecs)
{
    QThread::usleep(usecs);
}
