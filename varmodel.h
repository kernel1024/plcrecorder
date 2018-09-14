#ifndef VARMODEL_H
#define VARMODEL_H

#include <QObject>
#include <QItemDelegate>
#include <QWidget>
#include <QAbstractItemModel>
#include "plc.h"

class CVarModel;
class MainWindow;

class CVarDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit CVarDelegate(QObject *parent = nullptr);
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const;

    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const;

    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const;
    void setVarModel(CVarModel* aVmodel);

    void setEditEnabled(bool state);
    bool isEditEnabled();
private:
    CVarModel* vmodel;
    bool editEnabled;

};

class CVarModel : public QAbstractItemModel
{
    Q_OBJECT
    friend class CVarDelegate;
public:
    MainWindow* mainWnd;
    explicit CVarModel(QObject *parent, CPLC* aPlc);
    ~CVarModel();

    Qt::ItemFlags flags(const QModelIndex & index) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount( const QModelIndex & parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    void removeMultipleRows(QList<int> rows);

    CWP getCWP(int idx) const;
    int getCWPCount() const;
    CWPList getCWPList() const;

    void saveWPList(QDataStream& out);
    void loadWPList(QDataStream &in);

    void loadActualsFromPLC(const CWPList& wpl);

    void setEditEnabled(bool state);
    bool isEditEnabled();

private:
    CWPList wp;
    CPLC *plc;
    QPixmap led0, led1;
    bool editEnabled;

signals:
    void syncPLCtoModel(const CWPList& aWatchpoints);

public slots:
    void syncPLC();

};

#endif // VARMODEL_H
