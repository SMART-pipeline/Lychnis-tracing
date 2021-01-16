#include "controlpanel.h"
#include "widgets.h"
#include "util.h"
#include "common.h"
#include "worldviewer.h"

#include <opencv2/core.hpp>

#include <QLabel>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QStackedWidget>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>

#include <QTableView>
#include <QHeaderView>
#include <QStandardItemModel>

#include <QColorDialog>
#include <QFileDialog>
#include <QDebug>

class ColorEditor:public QWidget{
    QColor m_color;QLineEdit *m_edit;QToolButton *m_btn;int64_t m_nodeId;
    QString colorString(){return QString::number(m_color.red())+", "+QString::number(m_color.green())+", "+QString::number(m_color.blue());}
public:
    ColorEditor():m_nodeId(-1){
        QHBoxLayout *lyt=new QHBoxLayout;lyt->setContentsMargins(0,0,0,0);setLayout(lyt);//lyt->setSpacing(5);
        m_edit=new QLineEdit;m_edit->setFocusPolicy(Qt::NoFocus);setEnabled(false);
        //QPushButton *btn=new QPushButton("Select");
        m_btn=new QToolButton;m_btn->setObjectName("ColorPicker");
        QObject::connect(m_btn,&QToolButton::pressed,[this](){
            QColorDialog d(m_color);if(!d.exec()){return;}
            m_color=d.currentColor();setValue(m_nodeId,m_color);//m_edit->setText(colorString());

            cv::Point3d p(m_color.red(),m_color.green(),m_color.blue());p/=255;
            emit Common::i()->selectedNodeColorChanged(m_nodeId,p.x,p.y,p.z);
        });
        lyt->addWidget(m_edit);lyt->addWidget(m_btn);
    }
    void setValue(int64_t nodeId,const QColor &color){
        m_color=color;m_nodeId=nodeId;bool bValid=color.isValid();
        setEnabled(bValid);m_edit->setText(bValid?colorString():"");
        QString btnColor=!bValid?"transparent":QString("rgba(%1,200)").arg(colorString());
        m_btn->setStyleSheet(QString("QToolButton#ColorPicker{background-color:%1;}").arg(btnColor));
    }
};

inline QWidget *createBox(const QString &title,QGridLayout *lyt1,QWidget *extraBtn=nullptr){
    QVBoxLayout *lyt=new QVBoxLayout;lyt->setContentsMargins(0,0,0,0);
    QWidget *box=new QWidget;box->setLayout(lyt);QLabel *header=new QLabel(title);

    QHBoxLayout *headerLyt=new QHBoxLayout;headerLyt->setSpacing(0);
    headerLyt->addWidget(header,1);header->setObjectName("panel-header");
    if(nullptr!=extraBtn){headerLyt->addWidget(extraBtn);extraBtn->setObjectName("panel-header-extra");}

    lyt->addLayout(headerLyt);box->setObjectName("panel");
    int w=5;lyt1->setContentsMargins(w,w,w,w);lyt->addLayout(lyt1,1);return box;
}

ControlPanel::ControlPanel(){
    QVBoxLayout *lyt=new QVBoxLayout;lyt->setContentsMargins(0,0,0,0);
    setLayout(lyt);Common *c=Common::i();QGridLayout *lyt1;int line=0;

    m_currentChannelBox=new QComboBox;
    connect(c,&Common::channelLoaded,this,[this](int num,int current){
        for(int i=0;i<num;i++){m_currentChannelBox->addItem("Channel "+QString::number(i+1));}
        m_currentChannelBox->setCurrentIndex(current);
    },Qt::QueuedConnection);
    connect(m_currentChannelBox,SIGNAL(currentIndexChanged(int)),c,SIGNAL(channelChanged(int)));

    m_channelLyt=new QGridLayout;m_channelLyt->setContentsMargins(0,0,0,0);line=0;
    m_channelLyt->addWidget(new QLabel("Signal channel"),line,0);m_channelLyt->addWidget(m_currentChannelBox,line,1);
    lyt->addWidget(createBox("Channels",m_channelLyt));

    m_blockSizes=QList<QPoint>()<<QPoint(64,64)<<QPoint(128,128)<<QPoint(256,256)<<QPoint(512,128)<<QPoint(512,512)<<QPoint(1024,128)
                                   <<QPoint(1024,256)<<QPoint(2048,64)<<QPoint(4096,16)<<QPoint(32,32)<<QPoint(16,16)<<QPoint(32,256)
                                  <<QPoint(2048,128)<<QPoint(2048,256)<<QPoint(4096,32)<<QPoint(4096,64);
    QComboBox *m_blockSizeBox=new QComboBox;
    foreach(QPoint p,m_blockSizes){m_blockSizeBox->addItem(QString("%1x%1x%2").arg(QString::number(p.x()),QString::number(p.y())));}
    m_blockSizeBox->setCurrentIndex(2);connect(m_blockSizeBox,SIGNAL(currentIndexChanged(int)),this,SLOT(blockSizeComboChanged(int)));

    m_voxelSizeBox=new SpinBox;m_voxelSizeBox->setRange(0.01,1000);m_voxelSizeBox->setValue(1);
    void (QDoubleSpinBox::* spinSignal)(double)=&QDoubleSpinBox::valueChanged;
    connect(m_voxelSizeBox,spinSignal,[c,this](double v){if(m_voxelSizeBox->hasFocus()){c->p_voxelSize=v;updateNodePos();}});
    connect(c,&Common::voxelSizeLoaded,[this](double v){m_voxelSizeBox->setValue(v);updateNodePos();});

    QHBoxLayout *lyt2=new QHBoxLayout,*lyt3=new QHBoxLayout;lyt2->setContentsMargins(0,0,0,0);lyt3->setContentsMargins(0,0,0,0);
    for(int i=0;i<3;i++){
        SpinBox *box=new SpinBox;m_centerBoxes.append(box);box->setRange(0,900000);box->setDecimals(0);lyt2->addWidget(box);
        connect(box,SIGNAL(valueChanged(double)),this,SLOT(centerValueChanged()));

        box=new SpinBox;m_originBoxes.append(box);box->setRange(-9000000,9000000);box->setDecimals(0);lyt3->addWidget(box);
        connect(box,spinSignal,this,&ControlPanel::originValueChanged);
    }
    QWidget *centerBox=new QWidget,*originBox=new QWidget;centerBox->setLayout(lyt2);originBox->setLayout(lyt3);
    connect(c,&Common::centerloaded,this,[this](int maxX,int maxY,int maxZ,int x,int y,int z){
        m_centerBoxes[0]->setRange(0,maxX);m_centerBoxes[1]->setRange(0,maxY);m_centerBoxes[2]->setRange(0,maxZ);
        m_centerBoxes[0]->setValue(x);m_centerBoxes[1]->setValue(y);m_centerBoxes[2]->setValue(z);
    },Qt::QueuedConnection);
    connect(c,&Common::centerChangedViewer,this,[this](int x,int y,int z){
        m_centerBoxes[0]->setValue(x);m_centerBoxes[1]->setValue(y);m_centerBoxes[2]->setValue(z);
    },Qt::QueuedConnection);
    connect(c,&Common::originChangedViewer,this,[this](int x,int y,int z){
        m_originBoxes[0]->setValue(x);m_originBoxes[1]->setValue(y);m_originBoxes[2]->setValue(z);updateNodePos();
    },Qt::QueuedConnection);
    connect(c,&Common::spacingChanged,this,[this](double v){for(int i=0;i<3;i++){m_centerBoxes[i]->setSingleStep(50*v);}},Qt::QueuedConnection);

    lyt1=new QGridLayout;lyt1->setContentsMargins(0,0,0,0);line=0;
    lyt1->addWidget(new QLabel("Block size"),line,0);lyt1->addWidget(m_blockSizeBox,line,1);line++;
    lyt1->addWidget(new QLabel("Voxel size"),line,0);lyt1->addWidget(m_voxelSizeBox,line,1);line++;
    lyt1->addWidget(new QLabel("Origin"),line,0);lyt1->addWidget(originBox,line,1);line++;
    lyt1->addWidget(new QLabel("Center"),line,0);lyt1->addWidget(centerBox,line,1);line++;
    lyt->addWidget(createBox("Settings",lyt1));

    QLineEdit *nodeIdEdit=new QLineEdit,*msgEdit=new QLineEdit;m_nodePosEdit=new QLineEdit;
    msgEdit->setToolTip("Press enter to submit");m_nodePosEdit->setToolTip("Global coordinate (x, y, z)");
    nodeIdEdit->setFocusPolicy(Qt::NoFocus);msgEdit->setEnabled(false);m_nodePosEdit->setFocusPolicy(Qt::NoFocus);
    ColorEditor *colorEdit=new ColorEditor;colorEdit->setEnabled(false);

    connect(c,&Common::selectedNodeChanged,[nodeIdEdit,msgEdit,colorEdit,this](uint64_t id,QString msg,QString pos,double r,double g,double b,bool bValid){
        m_rawNodePos=pos;updateNodePos();

        if(!bValid){nodeIdEdit->setText("");msgEdit->setEnabled(false);msgEdit->setText("");colorEdit->setValue(id,QColor());return;}
        nodeIdEdit->setText(QString::number(id));msgEdit->setText(msg);msgEdit->setEnabled(true);colorEdit->setValue(id,QColor(r*255,g*255,b*255));
    });
    connect(msgEdit,&QLineEdit::returnPressed,[nodeIdEdit,msgEdit,c](){
        bool bOK;uint64_t id=nodeIdEdit->text().toULongLong(&bOK);if(!bOK){return;}

        QString msg=msgEdit->text();QRegExp re("[a-zA-Z0-9\\-]*");
        if(!re.exactMatch(msg)){
            emit c->showMessage("Node message can only contain alphabet,number and -",2000);return;
        }else{msgEdit->clearFocus();}

        emit c->selectedNodeInfoChanged(id,msg);
    });

    lyt1=new QGridLayout;lyt1->setContentsMargins(0,0,0,0);line=0;
    lyt1->addWidget(new QLabel("Node ID"),line,0);lyt1->addWidget(nodeIdEdit,line,1);line++;
    lyt1->addWidget(new QLabel("Message"),line,0);lyt1->addWidget(msgEdit,line,1);line++;
    lyt1->addWidget(new QLabel("Color"),line,0);lyt1->addWidget(colorEdit,line,1);line++;
    lyt1->addWidget(new QLabel("Coordinate"),line,0);lyt1->addWidget(m_nodePosEdit,line,1);line++;
    lyt->addWidget(createBox("Selected node",lyt1));

    m_pPathTableView=new QTableView;
    m_pPathTableView->horizontalHeader()->setStretchLastSection(true);
    m_pPathTableView->horizontalHeader()->setHighlightSections(false);
    m_pPathTableView->horizontalHeader()->setSectionsClickable(false);
    m_pPathTableView->verticalHeader()->setVisible(false);
    m_pPathTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pPathTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pPathTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_pPathModel=new QStandardItemModel;m_pPathTableView->setModel(m_pPathModel);
    m_pPathModel->setHorizontalHeaderLabels(QStringList()<<"id"<<"message");

    connect(c,&Common::initSpecialNodes,this,&ControlPanel::initSpecialNodes);
    connect(c,&Common::selectedNodeInfoChanged,this,&ControlPanel::specialNodeChanged);
    connect(c,&Common::deleteSpecialNode,[this](int64_t id){specialNodeChanged(id,"");});
    connect(m_pPathTableView,&QTableView::clicked,this,&ControlPanel::selectSpecialNode);

    lyt1=new QGridLayout;lyt1->setContentsMargins(0,0,0,0);
    lyt1->addWidget(m_pPathTableView,0,0);lyt->addWidget(createBox("Special nodes",lyt1),1);

    WorldViewer *worldViewer=new WorldViewer;
    lyt1=new QGridLayout;lyt1->setContentsMargins(0,0,0,0);
    QPushButton *mapBtn=new QPushButton("Hide fibers");
    connect(mapBtn,&QPushButton::clicked,[mapBtn,worldViewer](){
        static QStringList mapBtnNames=QStringList()<<"Hide fibers"<<"Show fibers";
        int index=(mapBtnNames.indexOf(mapBtn->text())+1)%2;mapBtn->setText(mapBtnNames[index]);
        worldViewer->setFibersVisible(index==0);
    });
    lyt1->addWidget(worldViewer,0,0);lyt->addWidget(createBox("Map",lyt1,mapBtn),1);

    connect(c,&Common::importParameters,this,&ControlPanel::importParameters);
    connect(c,&Common::exportCurrentVolume,this,&ControlPanel::onExportCurrentVolume);
}

void ControlPanel::centerValueChanged(){
    SpinBox *box=(SpinBox*)sender();if(!box->hasFocus()){return;}
    int x=m_centerBoxes[0]->value(),y=m_centerBoxes[1]->value(),z=m_centerBoxes[2]->value();
    emit Common::i()->centerChangedControl(x,y,z);
}
void ControlPanel::originValueChanged(){
    SpinBox *box=(SpinBox*)sender();if(!box->hasFocus()){return;}
    int x=m_originBoxes[0]->value(),y=m_originBoxes[1]->value(),z=m_originBoxes[2]->value();
    emit Common::i()->originChangedControl(x,y,z);updateNodePos();
}

void ControlPanel::updateNodePos(){
    if(m_rawNodePos.isEmpty()){m_nodePosEdit->setText("");return;}
    QStringList vs=m_rawNodePos.split(" ");if(vs.length()!=3||m_originBoxes.length()!=3){return;}
    bool bOK;double voxelSize=m_voxelSizeBox->text().toDouble(&bOK);if(!bOK||voxelSize<=0){return;}
    QStringList vs1;
    for(int i=0;i<3;i++){
        int v1=round(vs[i].toDouble(&bOK)*voxelSize+m_originBoxes[i]->value());
        if(bOK){vs1.append(QString::number(v1));}else{return;}
    }
    m_nodePosEdit->setText(vs1.join(", "));
}

void ControlPanel::importParameters(){
    QString path=QFileDialog::getOpenFileName(nullptr,"Import VISoR parameters file","","VISoR (*Parameters.json);;All Json files (*.json)");if(path.isEmpty()){return;}
    QVariantMap params;Common *c=Common::i();
    if(!Util::loadJson(path,params)){emit c->showMessage("Unable to load parameters file "+path,3000);return;}

    bool bOK;double voxelSize=params["pixel_size"].toDouble(&bOK);
    if(!bOK||voxelSize<=0){emit c->showMessage("Unable to parse pixel size from parameters file",3000);return;}
    QVariantList rois=params["roi"].toList();bOK=false;cv::Point3i origin;
    if(rois.length()==2){
        rois=rois[0].toList();bool bOK1,bOK2,bOK3;
        if(rois.length()==3){
            origin.x=rois[0].toFloat(&bOK1);origin.y=rois[1].toFloat(&bOK2);origin.z=rois[2].toFloat(&bOK3);
            bOK=bOK1&&bOK2&&bOK3;
        }
    }
    if(!bOK){emit c->showMessage("Unable to parse roi from parameters file",3000);return;}

    m_voxelSizeBox->setValue(voxelSize);c->p_voxelSize=voxelSize;
    m_originBoxes[0]->setValue(origin.x);m_originBoxes[1]->setValue(origin.y);m_originBoxes[2]->setValue(origin.z);
    emit c->originChangedControl(origin.x,origin.y,origin.z);
}

void ControlPanel::blockSizeComboChanged(int index){
    QPoint p=m_blockSizes[index];emit Common::i()->blockSizeChangedControl(p.x(),p.x(),p.y());
}

void ControlPanel::addSpecialNode(int64_t id, const QString &msg){
    m_specialIds.append(id);
    QStandardItem *itemId=new QStandardItem(QString::number(id)),*itemMsg=new QStandardItem(msg);
    m_pPathModel->appendRow(QList<QStandardItem*>()<<itemId<<itemMsg);
}
void ControlPanel::initSpecialNodes(QString ids, QString msgs){
    //m_pPathModel->removeRows(0,m_pPathModel->rowCount());m_specialIds.clear();
    QStringList idList=ids.split("\n"),msgList=msgs.split("\n");
    if(idList.empty()||idList.length()!=msgList.length()){return;}

    for(int i=0;i<idList.length();i++){
        bool bOK;int64_t id=idList[i].toLongLong(&bOK);QString msg=msgList[i];
        if(bOK&&id>=0&&!msg.isEmpty()&&!m_specialIds.contains(id)){addSpecialNode(id,msg);}
    }
    m_pPathTableView->scrollToBottom();
}
void ControlPanel::specialNodeChanged(int64_t id, QString msg){
    int index=m_specialIds.indexOf(id);bool bOK=!msg.isEmpty();
    if(index>=0){
        if(bOK){m_pPathModel->item(index,1)->setText(msg);}
        else{m_specialIds.removeAt(index);m_pPathModel->removeRow(index);}
    }else if(bOK){addSpecialNode(id,msg);}
}

void ControlPanel::selectSpecialNode(QModelIndex index){
    int row=index.row();int64_t id=m_specialIds[row];emit Common::i()->nodeSelected(id);
}

void ControlPanel::onExportCurrentVolume(){
    QStringList vs=QStringList()<<m_currentChannelBox->currentText()<<QString::number(m_centerBoxes[0]->value())
            <<QString::number(m_centerBoxes[1]->value())<<QString::number(m_centerBoxes[2]->value());
    emit Common::i()->exportVolume(vs.join("-").replace(" ",""));
}
