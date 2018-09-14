#include <limits.h>
#include <float.h>
#include <QSharedPointer>
#include "ui_graphform.h"
#include "graphform.h"
#include <QDebug>

CGraphForm::CGraphForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CGraphForm)
{
    ui->setupUi(this);

    runningCursor = nullptr;

    connect(ui->plot,&QCustomPlot::mouseMove,this,&CGraphForm::plotMouseMove);

}

CGraphForm::~CGraphForm()
{
    delete ui;
}

void CGraphForm::setupGraphs(const CWPList &wp)
{
    clearData();

    if (runningCursor!=nullptr) {
        ui->plot->removeItem(runningCursor);
        runningCursor = nullptr;
    }

    double time = static_cast<double>(QDateTime::currentDateTime().toMSecsSinceEpoch())/1000.0;

    ui->plot->plotLayout()->clear();

    for (int i=0;i<wp.count();i++) {
        QCPAxisRect* rect = new QCPAxisRect(ui->plot,false);
        QCPLayoutGrid* grid = ui->plot->plotLayout();
        grid->addElement(i,0,rect);

        grid->setRowSpacing(0);
        rect->setMinimumMargins(QMargins(0,0,0,0));
        rect->setMinimumSize(100,gSet->plotVerticalSize);
        rect->setMaximumSize(getScreenWidth(),gSet->plotVerticalSize);

        QCPAxis* xAxis = rect->addAxis(QCPAxis::atTop);
        QCPAxis* yAxis = rect->addAxis(QCPAxis::atLeft);
        rect->setRangeDragAxes(xAxis,nullptr);
        rect->setRangeZoomAxes(xAxis,nullptr);

        yAxis->setTickLabels(false);
        if (i==0) {
            QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
            dateTicker->setDateTimeFormat("h:mm:ss\nd.MM.yy");
            xAxis->setTicker(dateTicker);
        } else
            xAxis->setTickLabels(false);

        QCPGraph* graph = ui->plot->addGraph(xAxis,yAxis);
        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross));

        xAxis->setRange(time,time+60);
        double yMin, yMax;
        switch (wp.at(i).vtype) {
            case CWP::S7BOOL:
                yMin = 0.0;
                yMax = 1.0;
                break;
            case CWP::S7BYTE:
                yMin = 0.0;
                yMax = static_cast<double>(UINT8_MAX);
                break;
            case CWP::S7WORD:
                yMin = 0.0;
                yMax = static_cast<double>(UINT16_MAX);
                break;
            case CWP::S7DWORD:
                yMin = 0.0;
                yMax = static_cast<double>(UINT32_MAX);
                break;
            case CWP::S7INT:
                // -32768 to 32767
                yMin = static_cast<double>(INT16_MIN);
                yMax = static_cast<double>(INT16_MAX);
                break;
            case CWP::S7DINT:
                yMin = static_cast<double>(INT32_MIN);
                yMax = static_cast<double>(INT32_MAX);
                break;
            case CWP::S7REAL:
                yMin = static_cast<double>(FLT_MIN);
                yMax = static_cast<double>(FLT_MAX);
                break;
            default:
                yMin = 0.0;
                yMax = 1.0;
                break;
        }
        double yRange = abs(yMax-yMin);
        yAxis->setRange(yMin-(yRange*0.1),yMax+(yRange*0.1));

        connect(xAxis,SIGNAL(rangeChanged(QCPRange)),this,SLOT(plotRangeChanged(QCPRange)));

        // TODO: set colors...
    }
    ui->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

void CGraphForm::addData(const CWPList &wp, const QDateTime& time)
{
    if (wp.count()!=ui->plot->graphCount()) {
        emit logMessage(trUtf8("Variables list changed. Clearing old graph data."));
        setupGraphs(wp);
    }

    double key = static_cast<double>(time.toMSecsSinceEpoch())/1000.0;

    for (int i=0;i<wp.count();i++) {
        double val = 0.0;
        QVariant dt = wp.at(i).data;
        switch (wp.at(i).vtype) {
            case CWP::S7BOOL:
                if (dt.toBool())
                    val = 1.0;
                else
                    val = 0.0;
                break;
            case CWP::S7BYTE:
                val = static_cast<double>(dt.toUInt());
                break;
            case CWP::S7WORD:
                val = static_cast<double>(dt.toUInt());
                break;
            case CWP::S7DWORD:
                val = static_cast<double>(dt.toUInt());
                break;
            case CWP::S7INT:
                val = static_cast<double>(dt.toInt());
                break;
            case CWP::S7DINT:
                val = static_cast<double>(dt.toInt());
                break;
            case CWP::S7REAL:
                val = dt.toDouble();
                break;
            default:
                continue;
        }
        ui->plot->graph(i)->addData(key,val);
    }

    // Check visible range
    if (ui->checkAutoScroll->isChecked() && !ui->plot->axisRect(0)->axis(QCPAxis::atTop)->range().contains(key)) {
        double size = ui->plot->axisRect(0)->axis(QCPAxis::atTop)->range().size();
        double start = key-(size*0.1);
        for (int i=0;i<ui->plot->axisRectCount();i++)
            ui->plot->axisRect(i)->axis(QCPAxis::atTop)->setRange(start,start+size);
    }
    ui->plot->replot();
}

int CGraphForm::getScreenWidth()
{
    int screen = 0;
    QWidget *w = window();
    QDesktopWidget *desktop = QApplication::desktop();
    if (w) {
        screen = desktop->screenNumber(w);
    } else if (desktop->isVirtualDesktop()) {
        screen = desktop->screenNumber(QCursor::pos());
    } else {
        screen = desktop->screenNumber(this);
    }
    QRect rect(desktop->availableGeometry(screen));
    return rect.width();
}

void CGraphForm::closeEvent(QCloseEvent *event)
{
    emit stopGraph();
    event->ignore();
}

void CGraphForm::clearData()
{
    ui->plot->clearGraphs();

}

void CGraphForm::plotRangeChanged(const QCPRange &newRange)
{
    QCPAxis* xAxis = qobject_cast<QCPAxis *>(sender());
    for (int i=0;i<ui->plot->axisRectCount();i++) {
        QCPAxis* ax = ui->plot->axisRect(i)->axis(QCPAxis::atTop);
        if (ax!=xAxis)
            ax->setRange(newRange);
    }
}

void CGraphForm::plotMouseMove(QMouseEvent *event)
{
/*    QCPAbstractPlottable *plottable = ui->plot->plottableAt(event->localPos());
    if (plottable!=nullptr) {

        bool addLines = false;
        QCPLayer *cursors = ui->plot->layer("cursor");
        QList<QCPItemStraightLine*> lines;

        if (cursors==nullptr) {
            ui->plot->addLayer("cursor");
            ui->plot->setCurrentLayer("main");
            cursors = ui->plot->layer("cursor");
            addLines = true;
        } else {
            foreach (QCPLayerable* itm, cursors->children()) {
                QCPItemStraightLine* item = qobject_cast<QCPItemStraightLine *>(itm);
                if (item!=nullptr)
                    lines.append(item);
            }
        }

        double x, y;
        plottable->pixelsToCoords(event->localPos(),x,y);

        if (runningCursor!=nullptr)
            ui->plot->removeItem(runningCursor);
        runningCursor = new QCPItemLine(ui->plot);
        ui->plot-> addItem(runningCursor);
        vCursor1->setClipToAxisRect(false);
        vCursor1->setClipAxisRect(axis1);
        vCursor1->start->setAxes(axis1->axis(QCPAxis::atBottom),axis1->axis(QCPAxis::atLeft));
        vCursor1->end->setAxes(axis1->axis(QCPAxis::atBottom),axis1->axis(QCPAxis::atLeft));
        vCursor1->start->setCoords( x, QCPRange::minRange);
        vCursor1->end->setCoords( x, QCPRange::maxRange);

        for (int i=0;i<ui->plot->axisRectCount();i++) {
            QCPItemStraightLine *line = nullptr;
            if (addLines) {
                line = new QCPItemStraightLine(ui->plot);
                line->setLayer("cursor");
                line->setPen(QPen(QColor(Qt::red)));
            } else {
                if (i<lines.count())
                    line = lines.at(i);
            }

            if (line!=nullptr) {
                line->point1->setCoords(x,0);
                line->point2->setCoords(x,1);
            }
        }
        cursors->replot();
    }*/
}
