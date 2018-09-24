#include <limits.h>
#include <float.h>
#include <QSharedPointer>
#include <QMenu>
#include <QMessageBox>
#include <QBuffer>
#include <QDesktopWidget>
#include "ui_graphform.h"
#include "graphform.h"
#include <QDebug>

static QList<int> validArea;
const static double zoomIncrements = 0.2; // in percents of actual data range

void initGraphFormData()
{
    validArea.clear();
    validArea << CWP::Inputs << CWP::Outputs << CWP::Merkers << CWP::DB << CWP::IDB;
}

CGraphForm::CGraphForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CGraphForm)
{
    ui->setupUi(this);

    moveSplitterOnce = true;
    runningCursor = NULL;
    leftCursor = NULL;
    rightCursor = NULL;
    watchpoints.clear();

    ui->plot->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->plot,SIGNAL(mouseMove(QMouseEvent*)),this,SLOT(plotMouseMove(QMouseEvent*)));
    connect(ui->btnLoadCSV,SIGNAL(clicked()),this,SLOT(loadCSV()));
    connect(ui->btnExport,SIGNAL(clicked()),this,SLOT(exportGraph()));
    connect(ui->horizontalScrollBar,SIGNAL(valueChanged(int)),this,SLOT(scrollBarMoved(int)));
    connect(ui->plot,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(plotContextMenu(QPoint)));

    ui->splitter->setCollapsible(0,false);
    ui->splitter->setCollapsible(1,true);

    clearData();
}

CGraphForm::~CGraphForm()
{
    delete ui;
}

void CGraphForm::setupGraphs(const CWPList &wp)
{
    bool lazyModification = ((wp.count()>watchpoints.count()) &&  // new WP added, check old WPs
                             (watchpoints == wp.mid(0,watchpoints.count()))); // old WPs still here, new WPs added to the end
    clearDataEx(lazyModification);

    QCPRange xRange(qQNaN(),qQNaN());
    int idx = 0;
    for (int i=0;i<wp.count();i++) {
        // skip timers and counters, date/time types
        if (!validArea.contains(wp.at(i).varea) || !gSet->plcIsPlottableType(wp.at(i))) continue;

        if (lazyModification && i<watchpoints.count()) {
            // Copy range from initialized graph
            if (!QCPRange::validRange(xRange))
                xRange = ui->plot->graph(idx)->keyAxis()->range();
            // skip old WPs initialization
            idx++;
            continue;
        }

        // default value ranges
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
                idx++;
                continue; // Special type, no visualization
        }

        QCPLayoutGrid* grid = ui->plot->plotLayout();

        // create axis grid, placed in new layout element
        QCPAxisRect* rect = new QCPAxisRect(ui->plot,false);
        grid->addElement(idx,0,rect);

        // grid geometry, affects scrolling and sizeHint's
        grid->setRowSpacing(0);
        rect->setMinimumMargins(QMargins(0,0,0,0));
        rect->setMinimumSize(100,gSet->plotVerticalSize);
        rect->setMaximumSize(getScreenWidth(),gSet->plotVerticalSize);

        // enable drag and zoom for xAxis
        QCPAxis* xAxis = rect->addAxis(QCPAxis::atTop);
        QCPAxis* yAxis = rect->addAxis(QCPAxis::atLeft);
        rect->setRangeDragAxes(xAxis,NULL);
        rect->setRangeZoomAxes(xAxis,NULL);

        // WP name as yAxis title
        QFont font = yAxis->labelFont();
        font.setPointSize(6);
        yAxis->setLabelFont(font);
        QFontMetrics fm(font);
        QString yLabel = fm.elidedText(wp.at(i).label,Qt::ElideRight,gSet->plotVerticalSize);
        yAxis->setLabel(QString("%1\n%2").arg(yLabel,gSet->plcGetAddrName(wp.at(i))));

        // value ticks for analogue WPs at yAxis
        if (wp.at(i).vtype==CWP::S7BOOL)
            yAxis->setTickLabels(false);
        else {
            yAxis->setTickLabels(true);
            yAxis->setTickLabelSide(QCPAxis::lsInside);
            yAxis->setTickLabelFont(font);
        }

        // date/time ticks for xAxis
        if (idx==0) {
            QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
            dateTicker->setDateTimeFormat("h:mm:ss.zzz\nd.MM.yyyy");
            xAxis->setTicker(dateTicker);
            xAxis->setTickLabelFont(font);
        } else
            xAxis->setTickLabels(false);

        xAxis->grid()->setVisible(true);
        yAxis->grid()->setVisible(true);

        // graph parameters
        QCPGraph* graph = ui->plot->addGraph(xAxis,yAxis);
        graph->setAntialiased(gSet->plotAntialiasing);
        graph->setAdaptiveSampling(true);
        graph->setLineStyle(QCPGraph::lsStepLeft);
        if (gSet->plotShowScatter)
            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus));

        // visible graph xRange, 1 min for complete init, copy from old WPs for lazy init
        if (lazyModification && QCPRange::validRange(xRange))
            xAxis->setRange(xRange);
        else {
            double time = static_cast<double>(QDateTime::currentDateTime().toMSecsSinceEpoch())/1000.0;
            xAxis->setRange(time,time+60);
        }

        // default yRange for data type
        double yRange = abs(yMax-yMin);
        yAxis->setRange(yMin-(yRange*0.1),yMax+(yRange*0.1));

        // handle xAxis drag
        connect(xAxis,SIGNAL(rangeChanged(QCPRange)),this,SLOT(plotRangeChanged(QCPRange)));

        idx++;
    }
    ui->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    watchpoints = wp;
}

void CGraphForm::addData(const CWPList &wp, const QDateTime& time, bool noReplot)
{
    if (wp!=watchpoints) { // comparing by uuid, WP must be same count, same order
        emit logMessage(trUtf8("Variables list changed. Initializing graphs."));
        setupGraphs(wp);
    }

    double key = static_cast<double>(time.toMSecsSinceEpoch())/1000.0;
    int idx = 0;

    for (int i=0;i<wp.count();i++) {
        // skip timers and counters, date/time types
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

        // dynamically correct yAxis range for analogue WPs
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

    // Check visible range, move range if needed
    if (ui->checkAutoScroll->isChecked() && !ui->plot->axisRect(0)->axis(QCPAxis::atTop)->range().contains(key)) {
        double size = ui->plot->axisRect(0)->axis(QCPAxis::atTop)->range().size();
        double start = key-(size*0.1);
        for (int i=0;i<ui->plot->axisRectCount();i++)
            ui->plot->axisRect(i)->axis(QCPAxis::atTop)->setRange(start,start+size);
    }

    // silent update (file loading) enabled
    if (!noReplot) {
        updateScrollBarRange();
        ui->plot->replot();
    }
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
    clearDataEx(false);
}

void CGraphForm::clearDataEx(bool clearOnlyCursors)
{
    if (runningCursor!=NULL) ui->plot->removeItem(runningCursor);
    if (leftCursor!=NULL) ui->plot->removeItem(leftCursor);
    if (rightCursor!=NULL) ui->plot->removeItem(rightCursor);
    runningCursor = NULL;
    leftCursor = NULL;
    rightCursor = NULL;

    ui->listRunning->clear();
    ui->listRunning->setColumnCount(0);
    ui->listRunning->setRowCount(0);
    ui->listLeft->clear();
    ui->listLeft->setColumnCount(0);
    ui->listLeft->setRowCount(0);
    ui->listRight->clear();
    ui->listRight->setColumnCount(0);
    ui->listRight->setRowCount(0);

    if (clearOnlyCursors) {
        ui->plot->replot();
        return;
    }

    ui->plot->clearGraphs();
    ui->plot->plotLayout()->clear();

    ui->plot->replot();

    watchpoints.clear();
    ui->horizontalScrollBar->setRange(0,0);
    ui->horizontalScrollBar->setEnabled(false);
}

QCPRange CGraphForm::getTotalKeyRange()
{
    QCPRange res(qQNaN(),qQNaN());
    for (int i=0;i<ui->plot->graphCount();i++) {
        bool foundRange;
        QCPRange dataRange = ui->plot->graph(i)->getKeyRange(foundRange, QCP::sdBoth);
        if (foundRange)
            res.expand(dataRange);
    }

    return res;
}

void CGraphForm::updateScrollBarRange()
{
    QCPRange xRange = getTotalKeyRange();
    if (xRange.size()>0.0) {
        ui->horizontalScrollBar->setMaximum(static_cast<int>(xRange.size())); // scrollbar range from 0, in seconds
        if (!ui->horizontalScrollBar->isEnabled())
            ui->horizontalScrollBar->setEnabled(true);
    }
}

void CGraphForm::zoomAll()
{
    if (ui->plot->axisRectCount()<1 ||
            ui->plot->graphCount()<1) return;

    QCPRange totalRange = getTotalKeyRange();
    if (QCPRange::validRange(totalRange))
        for (int i=0;i<ui->plot->axisRectCount();i++)
            ui->plot->axisRect(i)->axis(QCPAxis::atTop)->setRange(totalRange);

    ui->plot->replot();
}

void CGraphForm::plotRangeChanged(const QCPRange &newRange)
{
    QCPAxis* xAxis = qobject_cast<QCPAxis *>(sender());
    for (int i=0;i<ui->plot->axisRectCount();i++) {
        QCPAxis* ax = ui->plot->axisRect(i)->axis(QCPAxis::atTop);
        if (ax!=xAxis)
            ax->setRange(newRange);
    }

    QCPRange totalRange = getTotalKeyRange();
    if (xAxis!=NULL && // null axis sender when updating from scrollbar
            QCPRange::validRange(totalRange)) {
        ui->horizontalScrollBar->setValue(static_cast<int>(newRange.center()-totalRange.lower));
        ui->horizontalScrollBar->setPageStep(static_cast<int>(newRange.size()));
    }
}

void CGraphForm::plotMouseMove(QMouseEvent *event)
{
    QPointF pos(event->pos());

    QCPAxisRect *rect = ui->plot->axisRectAt(pos);
    if (rect!=NULL) {

        // create 'cursors' layer if needed
        QCPLayer *cursors = ui->plot->layer("cursor");
        if (cursors==NULL) {
            ui->plot->addLayer("cursor");
            ui->plot->setCurrentLayer("main");
            cursors = ui->plot->layer("cursor");
        }

        double x = rect->axis(QCPAxis::atTop)->pixelToCoord(pos.x());

        // cursor type selection
        QCPItemStraightLine* cursor;
        if (event->modifiers() & Qt::ControlModifier) cursor = leftCursor;
        else if (event->modifiers() & Qt::AltModifier) cursor = rightCursor;
        else cursor = runningCursor;

        if (cursor!=NULL)
            ui->plot->removeItem(cursor);

        // cursor init
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

void CGraphForm::scrollBarMoved(int value)
{
    QCPRange totalRange = getTotalKeyRange();
    if (!QCPRange::validRange(totalRange) ||
            ui->plot->axisRectCount()<1) return;

    double newCenter = totalRange.lower + static_cast<double>(value);
    QCPRange currentRange = ui->plot->axisRect(0)->axis(QCPAxis::atTop)->range();

    // check delta for recursion prevention
    if (qAbs(newCenter-currentRange.center()) > 0.05)
    {
        currentRange += newCenter - currentRange.center();
        plotRangeChanged(currentRange);
        ui->plot->replot();
    }
}

void CGraphForm::plotContextMenu(const QPoint &pos)
{
    QMenu cm(ui->plot);

    QAction* acm;
    acm = cm.addAction(QIcon(":/zoom"),trUtf8("Zoom all"));
    connect(acm,SIGNAL(triggered()),this,SLOT(zoomAll()));
    cm.addSeparator();

    acm = cm.addAction(QIcon(":/trash-empty"),trUtf8("Clear plot"));
    connect(acm,SIGNAL(triggered()),this,SLOT(clearData()));

    QPoint p = pos;
    cm.exec(ui->plot->mapToGlobal(p));
}

void CGraphForm::loadCSV()
{
    QString fname = getOpenFileNameD(this,trUtf8("Load CSV file"),gSet->savedAuxDir,
                                     trUtf8("CSV files (*.csv)"));
    if (fname.isEmpty()) return;
    gSet->savedAuxDir = QFileInfo(fname).absolutePath();

    clearData();

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this,trUtf8("PLC recorder error"),
                              trUtf8("Unable to open file %1.").arg(fname));
        return;
    }
    QTextStream in(&f);
    QString s = in.readLine();
    if (!s.startsWith("\"Time\"; ")) {
        QMessageBox::critical(this,trUtf8("PLC recorder error"),
                              trUtf8("Unrecognized CSV file %1.").arg(fname));
        f.close();
        return;
    }
    int lineNum = 2;
    while (!in.atEnd()) {
        s = in.readLine().trimmed();

        // skip empty lines and additional title lines
        // this is support for merged files
        if (s.isEmpty() ||
                s.startsWith("\"Time\"; ")) continue;

        int idx = s.lastIndexOf("; ");
        if (idx<0) {
            QMessageBox::critical(this,trUtf8("PLC recorder error"),
                                  trUtf8("Unexpected end of file %1 at line %2.").arg(fname).arg(lineNum));
            f.close();
            return;
        }
        s = s.section("; ",-1,-1,QString::SectionSkipEmpty);
        QByteArray ba = QByteArray::fromBase64(s.toLatin1());
        if (ba.isEmpty()) {
            QMessageBox::critical(this,trUtf8("PLC recorder error"),
                                  trUtf8("Corrupted scan data in file %1 at line %2.").arg(fname).arg(lineNum));
            f.close();
            return;
        }
        ba = qUncompress(ba);
        if (ba.isEmpty()) {
            QMessageBox::critical(this,trUtf8("PLC recorder error"),
                                  trUtf8("Corrupted compressed data in file %1 at line %2.").arg(fname).arg(lineNum));
            f.close();
            return;
        }

        QBuffer buf(&ba);
        buf.open(QIODevice::ReadOnly);
        QDataStream in(&buf);

        CWPList wp;
        QDateTime dt;
        in >> dt >> wp;
        buf.close();
        ba.clear();

        if (wp.isEmpty()) {
            QMessageBox::critical(this,trUtf8("PLC recorder error"),
                                  trUtf8("Scan data is empty in file %1 at line %2.").arg(fname).arg(lineNum));
            f.close();
            return;
        }

        addData(wp,dt,true);
        qApp->processEvents();

        lineNum++;
    }
    f.close();
    updateScrollBarRange();
    zoomAll();
    qApp->processEvents();
    QMessageBox::information(this,trUtf8("PLC recorder"),
                             trUtf8("File successfully loaded."));
}

void CGraphForm::exportGraph()
{
    QString fname = getSaveFileNameD(this,tr("Save to file"),gSet->savedAuxDir,
                                     tr("PDF file (*.pdf);;PNG file (*.png);;"
                                        "Jpeg file (*.jpg);;BMP file (*.bmp)"));

    if (fname.isNull() || fname.isEmpty()) return;
    gSet->savedAuxDir = QFileInfo(fname).absolutePath();

    QFileInfo fi(fname);
    if (fi.suffix().toLower()=="pdf")
        ui->plot->savePdf(fname);
    else if (fi.suffix().toLower()=="png")
        ui->plot->savePng(fname);
    else if (fi.suffix().toLower()=="jpg")
        ui->plot->saveJpg(fname);
    else if (fi.suffix().toLower()=="bmp")
        ui->plot->saveBmp(fname);
}

void CGraphForm::createCursorSignal(CGraphForm::CursorType cursor, double timestamp)
{
    QDateTime time = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestamp*1000.0));

    CWPList res = watchpoints;
    int idx = 0;
    for (int i=0;i<res.count();i++) {
        // skip timers and counters, date/time types
        if (!validArea.contains(res.at(i).varea) || !gSet->plcIsPlottableType(res.at(i))) {
            res[i].data = QVariant();
            continue;
        }

        bool dataValid = false;
        double data = 0.0;
        const QCPGraph* graph = ui->plot->graph(idx);

        // find nearest value to cursor
        bool foundRange;
        QCPGraphDataContainer::const_iterator it = graph->data()->findBegin(timestamp);
        QCPRange dataRange = graph->getKeyRange(foundRange, QCP::sdBoth);
        if (foundRange &&
                (it != graph->data()->constEnd()))
        {
            // key must be inside actual graph
            if ((it->key > dataRange.lower) &&
                    (it->key < dataRange.upper)) {
                data = it->value;
                dataValid = true;
            }
        }

        // cast aquired data to our data types
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

    // cursor type selection
    QTableWidget* list = NULL;
    QLabel* timeLabel = NULL;
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

    if (list!=NULL && timeLabel!=NULL) {
        QFontMetrics fm(list->font());
        // cursor properties list initialization
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
