#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
#include "varmodel.h"
#include "plc.h"
#include "global.h"
#include "mainwindow.h"

CVarDelegate::CVarDelegate(QObject *parent) :
    QItemDelegate(parent)
{
    vmodel = nullptr;
    editEnabled = true;
}

QWidget *CVarDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (!index.isValid()) return nullptr;
    if (!editEnabled) return nullptr;
    if ((index.row()<0) || (index.row()>=index.model()->rowCount())) return nullptr;
    if (index.column()==0)
        return new QLineEdit(parent);
    else if (index.column()==1)
        return new QLineEdit(parent);
    else if (index.column()==2)
        return new QComboBox(parent);
    else if (index.column()==3)
        return nullptr; // read-only column
    return nullptr;
}

void CVarDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (vmodel==nullptr) return;
    if (!index.isValid()) return;
    if ((index.row()<0) || (index.row()>=index.model()->rowCount())) return;

    CWP wp = vmodel->wp.at(index.row());

    QLineEdit *le = qobject_cast<QLineEdit *>(editor);
    QComboBox *cb = qobject_cast<QComboBox *>(editor);

    if ((index.column()==0) && (le!=nullptr))
        le->setText(wp.label);
    else if ((index.column()==1) && (le!=nullptr))
        le->setText(gSet->plcGetAddrName(wp));
    else if ((index.column()==2) && (cb!=nullptr)) {
        cb->clear();
        QStringList sl = gSet->plcAvailableTypeNames();
        cb->addItems(sl);
        cb->setCurrentIndex(sl.indexOf(gSet->plcGetTypeName(wp)));
    }
}

void CVarDelegate::setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const
{
    if (vmodel==nullptr) return;
    if (!index.isValid()) return;
    if ((index.row()<0) || (index.row()>=index.model()->rowCount())) return;

    QLineEdit *le = qobject_cast<QLineEdit *>(editor);
    QComboBox *cb = qobject_cast<QComboBox *>(editor);

    if ((index.column()==0) && (le!=nullptr)) {
        vmodel->wp[index.row()].label=le->text();
        vmodel->syncPLC();
    } else if ((index.column()==1) && (le!=nullptr)) {
        if (!gSet->plcParseAddr(le->text(),vmodel->wp[index.row()]))
            QMessageBox::warning(vmodel->mainWnd,trUtf8("PLC recorder error"),
                                 trUtf8("Unable to parse address. Possible syntax error."));
        vmodel->syncPLC();
    } else if ((index.column()==2) && (cb!=nullptr)) {
        if (!gSet->plcSetTypeForName(cb->currentText(),vmodel->wp[index.row()]))
            QMessageBox::warning(vmodel->mainWnd,trUtf8("PLC recorder error"),
                                 trUtf8("Unable to parse type. Possible syntax error."));
        vmodel->syncPLC();
    }
}

void CVarDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

void CVarDelegate::setVarModel(CVarModel *aVmodel)
{
    vmodel = aVmodel;
}

void CVarDelegate::setEditEnabled(bool state)
{
    editEnabled = state;
}

bool CVarDelegate::isEditEnabled()
{
    return editEnabled;
}

CVarModel::CVarModel(QObject *parent, CPLC* aPlc)
    : QAbstractItemModel(parent)
{
    wp.clear();
    plc = aPlc;
    led0.load(":/led0");
    led1.load(":/led1");
    editEnabled = true;
}

CVarModel::~CVarModel()
{
    wp.clear();
}

Qt::ItemFlags CVarModel::flags(const QModelIndex &) const
{
    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
}

QVariant CVarModel::data(const QModelIndex &index, int role) const
{
    // 4 columns: label | area(db)offset.bitnum | type | actual value

    if (!index.isValid()) return QVariant();

    int row = index.row();
    int column = index.column();

    if ((row<0) || (row>=wp.count()) || (column<0) || (column>=4)) return QVariant();
    if (role==Qt::DisplayRole) {
        if (column==0) return wp.at(row).label;
        else if (column==1) return gSet->plcGetAddrName(wp.at(row));
        else if (column==2) return gSet->plcGetTypeName(wp.at(row));
        else if (column==3) return gSet->plcFormatActualValue(wp.at(row));
        else return QVariant();
    } else if (role==Qt::DecorationRole) {
        CWP awp = wp.at(row);
        if ((column==3) && (awp.vtype==CWP::S7BOOL) && (awp.data.canConvert<bool>())) {
            if (awp.data.toBool())
                return led1;
            else
                return led0;
        } else
            return QVariant();
    } else
        return QVariant();
}

QVariant CVarModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
                case 0: return trUtf8("Name");
                case 1: return trUtf8("Address");
                case 2: return trUtf8("Type");
                case 3: return trUtf8("Value");
                default: return QVariant();
            }
        } else {
            return trUtf8("%1").arg(section+1);
        }
    }
    return QVariant();
}

int CVarModel::rowCount(const QModelIndex &) const
{
    return wp.count();
}

int CVarModel::columnCount(const QModelIndex &) const
{
    return 4;
}

QModelIndex CVarModel::index(int row, int column, const QModelIndex &) const
{
    if ((row<0) || (row>=wp.count()) || (column<0) || (column>=4)) return QModelIndex();
    return createIndex(row,column);
}

QModelIndex CVarModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

bool CVarModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (!editEnabled) return false;
    if ((row<0) || (row>wp.count())) return false;
    beginInsertRows(parent,row,row+count-1);
    for (int i=0;i<count;i++)
        wp.insert(row,CWP());
    endInsertRows();
    return true;
}

bool CVarModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (!editEnabled) return false;
    if ((row<0) || (row>=wp.count())) return false;
    beginRemoveRows(parent,row,row+count-1);
    for (int i=0;i<count;i++)
        if (row<wp.count())
            wp.removeAt(row);
    endRemoveRows();
    return true;
}

void CVarModel::removeMultipleRows(QList<int> rows)
{
    CWPList wl;
    for (int i=0;i<rows.count();i++)
        if ((rows.at(i)>=0) && (rows.at(i)<wp.count()))
            wl << wp.at(rows.at(i));
    for (int i=0;i<wl.count();i++)
        removeRow(wp.indexOf(wl.at(i)));
    wl.clear();
}

CWP CVarModel::getCWP(int idx) const
{
    return wp.at(idx);
}

int CVarModel::getCWPCount() const
{
    return wp.count();
}

CWPList CVarModel::getCWPList() const
{
    return wp;
}

void CVarModel::saveWPList(QDataStream &out)
{
    out << static_cast<int>(wp.count());
    out << wp;
}

void CVarModel::loadWPList(QDataStream &in)
{
    if (!editEnabled) return;
    removeRows(0,wp.count());
    int cnt;
    in >> cnt;
    beginInsertRows(QModelIndex(),0,cnt-1);
    in >> wp;
    endInsertRows();
    syncPLC();
}

void CVarModel::loadActualsFromPLC(const CWPList &wpl)
{
    if (wp.count()!=wpl.count()) {
        mainWnd->appendLog(trUtf8("Variables list is different. Unable to load VAT."));
        return;
    }
    for (int i=0;i<wpl.count();i++) {
        wp[i].data = wpl.at(i).data;
        wp[i].dataSign = wpl.at(i).dataSign;
    }

    emit dataChanged(index(0,3),index(wp.count()-1,3));
}

void CVarModel::setEditEnabled(bool state)
{
    editEnabled = state;
}

bool CVarModel::isEditEnabled()
{
    return editEnabled;
}

void CVarModel::syncPLC()
{
    emit syncPLCtoModel(wp);
}
