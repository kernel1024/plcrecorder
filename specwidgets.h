#ifndef SPECWIDGETS_H
#define SPECWIDGETS_H

#include <QTableView>
#include <QWidget>
#include <QPoint>
#include <QThread>

class CTableView : public QTableView
{
    Q_OBJECT
public:
    explicit CTableView(QWidget *parent = 0);
    
signals:
    void ctxMenuRequested(const QPoint& pos);

private slots:
    void selectAll();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event);
    
};

class CSleep : public QThread
{
public:
    static void sleep(unsigned long secs);
    static void msleep(unsigned long msecs);
    static void usleep(unsigned long usecs);
};
#endif // SPECWIDGETS_H
