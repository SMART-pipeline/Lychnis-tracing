#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QList>
#include <QModelIndex>

QT_BEGIN_NAMESPACE
class QGridLayout;
class SpinBox;
class QLineEdit;
class QComboBox;

class QStandardItemModel;
class QStandardItem;
class QTableView;
QT_END_NAMESPACE

class ControlPanel : public QWidget
{
    Q_OBJECT

    QList<SpinBox*> m_centerBoxes,m_originBoxes;
    SpinBox *m_voxelSizeBox;
    QLineEdit *m_nodePosEdit;QString m_rawNodePos;
    QComboBox *m_currentChannelBox,*m_blockSizeBox;

    QList<QPoint> m_blockSizes;
    QGridLayout *m_channelLyt;

    QTableView *m_pPathTableView;QStandardItemModel *m_pPathModel;
    QList<int64_t> m_specialIds;

    void addSpecialNode(int64_t id,const QString &msg);
    void updateNodePos();
public:
    explicit ControlPanel();

signals:

private slots:
    void centerValueChanged();
    void originValueChanged();
    void blockSizeComboChanged(int);

    void initSpecialNodes(QString ids,QString msgs);
    void specialNodeChanged(int64_t id,QString msg);
    void selectSpecialNode(QModelIndex index);

    void importParameters();
    void onExportCurrentVolume();
};

#endif // CONTROLPANEL_H
