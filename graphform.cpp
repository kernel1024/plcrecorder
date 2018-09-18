#include <limits.h>
#include <float.h>
#include <QSharedPointer>
#include "ui_graphform.h"
#include "graphform.h"
#include <QDebug>

const QList<int> validArea({CWP::Inputs,CWP::Outputs,CWP::Merkers,CWP::DB,CWP::IDB});
const double zoomIncrements = 0.2; // in percents of actual data range

CGraphForm::CGraphForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CGraphForm)
{
    ui->setupUi(this);

    moveSplitterOnce = true;
    runningCursor = nullptr;
    leftCursor = nullptr;
    rightCursor = nullptr;
    watchpoints.clear();

    connect(ui->plot,&QCustomPlot::mouseMove,this,&CGraphForm::plotMouseMove);

    ui->splitter->setCollapsible(0,false);
    ui->splitter->setCollapsible(1,true);
}

CGraphForm::~CGraphForm()
{
    delete ui;
}

void CGraphForm::setupGraphs(const CWPList &wp)
{
    clearData();
    watchpoints = wp;

    if (runningCursor!=nullptr) ui->plot->removeItem(runningCursor);
    if (leftCursor!=nullptr) ui->plot->removeItem(leftCursor);
    if (rightCursor!=nullptr) ui->plot->removeItem(rightCursor);
    runningCursor = nullptr;
    leftCursor = nullptr;
    rightCursor = nullptr;

    double time = static_cast<double>(QDateTime::currentDateTime().toMSecsSinceEpoch())/1000.0;

    ui->plot->plotLayout()->clear();
    int idx = 0;

    for (int i=0;i<wp.count();i++) {
        if (!validArea.contains(wp.at(i).varea) || !gSet->plcIsPlottableType(wp.at(i))) continue;
        double yMin, yMax;
        switch (wp.at(i).vtype) {
            case CWP::S7BOOL:
                yMin = 0.0;
                yMax = 1.0;
                break;
            case CWP::S7BYTE:
            case CWP::S7WORD:
            case CWP::S7DWORD:
            case CWP::S7INT:
            case CWP::S7DINT:
            case CWP::S7REAL:
                yMin = -5.0;
                yMax = 50.0;
                break;
            default:
                continue; // Special type, no visualization
        }

        QCPLayoutGrid* grid = ui->plot->plotLayout();

        QCPAxisRect* rect = new QCPAxisRect(ui->plot,false);
        grid->addElement(idx,0,rect);

        grid->setRowSpacing(0);
        rect->setMinimumMargins(QMargins(0,0,0,0));
        rect->setMinimumSize(100,gSet->plotVerticalSize);
        rect->setMaximumSize(getScreenWidth(),gSet->plotVerticalSize);

        QCPAxis* xAxis = rect->addAxis(QCPAxis::atTop);
        QCPAxis* yAxis = rect->addAxis(QCPAxis::atLeft);
        rect->setRangeDragAxes(xAxis,nullptr);
        rect->setRangeZoomAxes(xAxis,nullptr);

        QFont font = yAxis->labelFont();
        font.setPointSize(6);
        yAxis->setLabelFont(font);
        QFontMetrics fm(font);
        QString yLabel = fm.elidedText(wp.at(i).label,Qt::ElideRight,gSet->plotVerticalSize);
        yAxis->setLabel(QString("%1\n%2").arg(yLabel,gSet->plcGetAddrName(wp.at(i))));

        if (wp.at(i).vtype==CWP::S7BOOL)
            yAxis->setTickLabels(false);
        else {
            yAxis->setTickLabels(true);
            yAxis->setTickLabelSide(QCPAxis::lsInside);
            yAxis->setTickLabelFont(font);
        }
        if (idx==0) {
            QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
            dateTicker->setDateTimeFormat("h:mm:ss.zzz\nd.MM.yyyy");
            xAxis->setTicker(dateTicker);
            xAxis->setTickLabelFont(font);
        } else
            xAxis->setTickLabels(false);

        xAxis->grid()->setVisible(true);
        yAxis->grid()->setVisible(true);

        QCPGraph* graph = ui->plot->addGraph(xAxis,yAxis);
        graph->setLineStyle(QCPGraph::lsStepLeft);
        if (gSet->plotShowScatter)
            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus));

        xAxis->setRange(time,time+60);

        double yRange = abs(yMax-yMin);
        yAxis->setRange(yMin-(yRange*0.1),yMax+(yRange*0.1));

        connect(xAxis,SIGNAL(rangeChanged(QCPRange)),this,SLOT(plotRangeChanged(QCPRange)));

        idx++;
    }
    ui->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

void CGraphForm::addData(const CWPList &wp, const QDateTime& time)
{
    if (wp.count()!=watchpoints.count()) {
        emit logMessage(trUtf8("Variables list changed. Clearing old graph data."));
        setupGraphs(wp);
    }

    double key = static_cast<double>(time.toMSecsSinceEpoch())/1000.0;
    int idx = 0;

    for (int i=0;i<wp.count();i++) {
        if (!validArea.contains(wp.at(i).varea) || !gSet->plcIsPlottableType(wp.at(i))) continue;
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

        QCPGraph* graph = ui->plot->graph(idx);
        graph->addData(key,val);

        if (wp.at(i).vtype!=CWP::S7BOOL) {
            QCPAxis* yAxis = graph->valueAxis();
            bool foundRange;
            QCPRange dataRange = graph->getValueRange(foundRange, QCP::sdBoth);
            if (foundRange &&
                    (dataRange.lower < yAxis->range().lower ||
                     dataRange.upper > yAxis->range().upper)) {
                double increment = dataRange.size()*zoomIncrements;
                yAxis->setRange(dataRange.lower-increment,
                                dataRange.upper+increment);
            }
        }
        idx++;
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

void CGraphForm::showEvent(QShowEvent *)
{
    if (moveSplitterOnce) {
        int min, max;
        ui->splitter->getRange(0,&min,&max);
        ui->splitter->setSizes(QList<int>() << max << 10);
        moveSplitterOnce = false;
    }
}

void CGraphForm::clearData()
{
    ui->plot->clearGraphs();
    watchpoints.clear();
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
    QCPAxisRect *rect = ui->plot->axisRectAt(event->localPos());
    if (rect!=nullptr) {

        QCPLayer *cursors = ui->plot->layer("cursor");

        if (cursors==nullptr) {
            ui->plot->addLayer("cursor");
            ui->plot->setCurrentLayer("main");
            cursors = ui->plot->layer("cursor");
        }

        double x = rect->axis(QCPAxis::atTop)->pixelToCoord(event->localPos().x());

        QCPItemStraightLine* cursor;
        if (event->modifiers() & Qt::ControlModifier) cursor = leftCursor;
        else if (event->modifiers() & Qt::AltModifier) cursor = rightCursor;
        else cursor = runningCursor;

        if (cursor!=nullptr)
            ui->plot->removeItem(cursor);

        cursor = new QCPItemStraightLine(ui->plot);
        cursor->setLayer(cursors);
        cursor->setClipToAxisRect(false);
        cursor->setClipAxisRect(rect);
        cursor->point1->setAxes(rect->axis(QCPAxis::atTop),rect->axis(QCPAxis::atLeft));
        cursor->point2->setAxes(rect->axis(QCPAxis::atTop),rect->axis(QCPAxis::atLeft));
        cursor->point1->setCoords(x,QCPRange::minRange);
        cursor->point2->setCoords(x,QCPRange::maxRange);

        if (event->modifiers() & Qt::ControlModifier) {
            cursor->setPen(QPen(QBrush(QColor(Qt::green)),2));
            leftCursor = cursor;
            createCursorSignal(ctLeft,x);
        } else if (event->modifiers() & Qt::AltModifier) {
            cursor->setPen(QPen(QBrush(QColor(Qt::red)),2));
            rightCursor = cursor;
            createCursorSignal(ctRight,x);
        } else {
            cursor->setPen(QPen(QBrush(QColor(Qt::black)),2));
            runningCursor = cursor;
            createCursorSignal(ctRunning,x);
        }

        cursors->replot();
    }
}

void CGraphForm::createCursorSignal(CGraphForm::CursorType cursor, double timestamp)
{
    QDateTime time = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestamp*1000.0));

    CWPList res = watchpoints;
    int idx = 0;
    for (int i=0;i<res.count();i++) {
        if (!validArea.contains(res.at(i).varea) || !gSet->plcIsPlottableType(res.at(i))) {
            res[i].data = QVariant();
            continue;
        }

        bool dataValid = false;
        double data = 0.0;
        const QCPGraph* graph = ui->plot->graph(idx);

        bool foundRange;
        QCPGraphDataContainer::const_iterator it = graph->data()->findBegin(timestamp);
        QCPRange dataRange = graph->getKeyRange(foundRange, QCP::sdBoth);
        if (foundRange &&
                (it != graph->data()->constEnd()))
        {
            if ((it->key > dataRange.lower) &&
                    (it->key < dataRange.upper)) {
                data = it->value;
                dataValid = true;
            }
        }

        if (dataValid) {
            switch (watchpoints.at(i).vtype) {
                case CWP::S7BOOL:
                    res[i].data = QVariant(data>0.5);
                    break;
                case CWP::S7BYTE:
                case CWP::S7WORD:
                case CWP::S7DWORD:
                    res[i].data = QVariant(static_cast<uint>(data));
                    break;
                case CWP::S7INT:
                case CWP::S7DINT:
                    res[i].data = QVariant(static_cast<int>(data));
                    break;
                case CWP::S7REAL:
                    res[i].data = QVariant(data);
                    break;
                default:
                    res[i].data = QVariant();
                    break;
            };
        } else
            res[i].data = QVariant();

        idx++;
    }

    QTableWidget* list = nullptr;
    QLabel* timeLabel = nullptr;
    switch (cursor) {
        case ctRunning:
            list = ui->listRunning;
            timeLabel = ui->lblTimeRunning;
            break;
        case ctLeft:
            list = ui->listLeft;
            timeLabel = ui->lblTimeLeft;
            break;
        case ctRight:
            list = ui->listRight;
            timeLabel = ui->lblTimeRight;
            break;
    }
    if (list!=nullptr && timeLabel!=nullptr) {
        QFontMetrics fm(list->font());
        if (list->rowCount()!=res.count()) {
            list->clearContents();
            list->setRowCount(res.count());
            list->setColumnCount(2);
            for (int i=0;i<res.count();i++) {
                list->setItem(i,0,new QTableWidgetItem(QString()));
                list->setItem(i,1,new QTableWidgetItem(QString()));
                list->setRowHeight(i,fm.height()+5);
            }
        }

        for (int i=0;i<res.count();i++) {
            if (list->item(i,0)->text()!=res.at(i).label)
                list->item(i,0)->setText(res.at(i).label);
            list->item(i,1)->setText(gSet->plcFormatActualValue(res.at(i)));
        }

        timeLabel->setText(time.toString("h:mm:ss.zzz d.MM.yy"));
    }

    emit cursorMoved(cursor,time,res);
}
