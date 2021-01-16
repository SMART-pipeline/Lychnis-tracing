#include "viewerpanel.h"
#include "contrastadjuster.h"
#include "util.h"
#include "volumeviewer.h"
#include "stackslider.h"
#include "common.h"
#include "worldviewer.h"

#include <hdf5.h>

#include <QProcess>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QDebug>

struct ProjectInfo{
    QString filePath;double voxelSize;QVariantList nodes,groups;bool bAutoSaved;
};

struct LevelInfo{
    hsize_t dims[3];QList<hid_t> dataIds;int resId;double spacing[3];hid_t dataId;
};
static QMap<int,LevelInfo*> g_levelInfos;

struct opdata {unsigned recurs;struct opdata *prev;int index;};

inline herr_t op_func(hid_t loc_id,const char *name,const H5L_info_t *,void *operator_data){
    struct opdata *od=(struct opdata *)operator_data;
    H5O_info1_t infobuf;H5Oget_info1(loc_id,&infobuf);if(infobuf.type!=H5O_TYPE_GROUP){return 0;}

    hid_t vLevelId=H5Gopen2(loc_id,name,H5P_DEFAULT);
    od->index=QString(name).split(" ").last().toInt();

    if(od->recurs==2){
        hid_t vDataId=H5Dopen2(vLevelId,"Data",H5P_DEFAULT);int resId=od->prev->prev->index;

        LevelInfo *info=g_levelInfos.value(resId,nullptr);
        if(nullptr==info){
            info=new LevelInfo;info->dataId=vDataId;info->resId=resId;g_levelInfos[resId]=info;//info->spacing=pow(2,resId);

            hid_t attr,atype,atype_mem;herr_t ret;
            H5T_class_t type_class;int dimsCount=0;
            foreach(QString name,QList<QString>()<<"ImageSizeX"<<"ImageSizeY"<<"ImageSizeZ"){
                attr=H5Aopen(vLevelId,name.toStdString().c_str(),H5P_DEFAULT);atype=H5Aget_type(attr);
                type_class=H5Tget_class(atype);if(type_class!=H5T_STRING){continue;}
                atype_mem=H5Tget_native_type(atype,H5T_DIR_ASCEND);
                char stringOut[80]={'\0'};ret=H5Aread(attr,atype_mem,stringOut);ret=H5Tclose(atype_mem);

                int v=QString(stringOut).toInt();//qDebug()<<atype_mem<<stringOut<<name<<v;
                info->dims[2-dimsCount]=v;dimsCount++;
            }

            if(dimsCount!=3){
                hid_t dspace=H5Dget_space(vDataId);
                hsize_t maxDims[3];H5Sget_simple_extent_dims(dspace,info->dims,maxDims);
            }

//            LevelInfo *rawLevel=g_levelInfos[0];//qDebug()<<resId<<rawLevel->dims[0]<<rawLevel->dims[1]<<rawLevel->dims[2];
//            for(int i=0;i<3;i++){info->spacing[i]=rawLevel->dims[i]/double(info->dims[i]);}
        }
        info->dataIds.append(vDataId);
    }else{
        struct opdata nextod;nextod.recurs=od->recurs + 1;nextod.prev=od;
        H5Literate(vLevelId,H5_INDEX_NAME,H5_ITER_NATIVE,NULL,op_func,(void *)&nextod);
    }

    return 0;
}


ViewerPanel::ViewerPanel():m_viewer(new VolumeViewer),m_bRunning(true),m_fileId(-1),m_bUpdated(false),m_centerStep(nullptr),
    m_imsPath2Load(nullptr),m_nextResolution(-1),m_nextBlockSize(nullptr),m_nextCenter(nullptr),
    m_currentLevel(nullptr),m_currentRes(-1),m_initResId(-1),m_currentChannel(0),m_nextChannel(-1),m_nextOrigin(nullptr),
    m_projectInfo2Save(nullptr),m_projectInfo2AutoSave(nullptr),m_bMainSpace(true){

    QVBoxLayout *lyt=new QVBoxLayout;lyt->setContentsMargins(0,0,0,0);
    lyt->setSpacing(1);setLayout(lyt);Common *c=Common::i();

    ContrastAdjuster *adjuster=new ContrastAdjuster(65535,1000,100);
    connect(adjuster,&ContrastAdjuster::valueChanged,m_viewer,&VolumeViewer::setContrast);
    lyt->addWidget(adjuster);

    StackSlider *resSlider=new StackSlider;resSlider->showPlayButton(false);
    connect(this,&ViewerPanel::resolutionNumberChanged,this,[resSlider,this](int number,int defaultRes){
        resSlider->setRange(1,number);resSlider->setValueSimple(defaultRes);m_nextResolution=defaultRes-1;
    },Qt::QueuedConnection);
    connect(resSlider,&StackSlider::valueChanged,[this](int v){m_nextResolution=v-1;});

    connect(c,&Common::channelChanged,[this](int index){m_nextChannel=index;});

    lyt->addWidget(m_viewer,1);lyt->addWidget(resSlider);
    connect(c,&Common::loadIMSFile,this,&ViewerPanel::onOpenIMSFile);
    connect(c,&Common::importNodes,this,&ViewerPanel::onImportNodes);
    connect(c,&Common::loadProject,this,&ViewerPanel::onLoadProject);
    connect(c,&Common::saveProject,this,&ViewerPanel::onSaveProject);
    connect(c,&Common::exportNodes2LychnisFile,this,&ViewerPanel::onExportProject);
    connect(c,&Common::autoSaveProject,this,&ViewerPanel::onAutoSaveProject);
    //connect(c,&Common::switchWorkspace,this,&ViewerPanel::onSwitchWorkspace);

    connect(m_viewer,&VolumeViewer::moveCenter,[this](int axis,int step){delete m_centerStep.exchange(new cv::Point2i(axis,step));});
    connect(m_viewer,&VolumeViewer::centerUpdated,[this,c](int x,int y,int z){
        delete m_nextCenter.exchange(new cv::Point3i(x,y,z));emit c->centerChangedViewer(x,y,z);
    });

    for(int i=0;i<3;i++){m_blockSize[i]=256;}
    connect(c,&Common::blockSizeChangedControl,[this](int x,int y,int z){delete m_nextBlockSize.exchange(new cv::Point3i(x,y,z));});
    connect(c,&Common::centerChangedControl,[this](int x,int y,int z){delete m_nextCenter.exchange(new cv::Point3i(x,y,z));});
    connect(c,&Common::originChangedControl,[this,c](int x,int y,int z){delete m_nextOrigin.exchange(new cv::Point3i(x,y,z));c->p_originZ=z;});

    m_thread=new std::thread(&ViewerPanel::mainLoop,this);
}

ViewerPanel::~ViewerPanel(){
    m_bRunning=false;m_thread->join();delete m_thread;
    if(m_fileId>=0){H5Fclose(m_fileId);}
}

void ViewerPanel::mainLoop(){
    QElapsedTimer timer;

    while(m_bRunning){
        timer.start();

        if(m_imagePath.isEmpty()){openIMSFile();}
        if(!m_imagePath.isEmpty()){
            updateBlockSize();updateCenter();updateResolution();updateOrigin();
            updateChannel();updateBlock();saveProject();
        }

        int timeLeft=100-timer.elapsed();if(timeLeft>0){std::this_thread::sleep_for(std::chrono::milliseconds(timeLeft));}
    }
}

bool ViewerPanel::updateBlockIMS(LevelInfo *p,size_t start[],size_t block[],void *buffer){
    hid_t dspace1=H5Screate_simple(3,p->dims,NULL);
    hsize_t stride[3]={1,1,1},count[3]={1,1,1};

    hid_t memspace=H5Screate_simple(3,block,NULL);
    H5Sselect_hyperslab(dspace1,H5S_SELECT_SET,start,stride,count,block);

    return H5Dread(p->dataIds[m_currentChannel],H5T_NATIVE_USHORT,memspace,dspace1,H5P_DEFAULT,buffer)>=0;
}

void ViewerPanel::updateBlock(){
    if(m_bUpdated){m_bUpdated=false;}else{return;}
    LevelInfo *p=m_currentLevel;if(nullptr==p){return;}
    Common *c=Common::i();emit c->showMessage("Loading ...");

    hsize_t start[3],block[3];
    for(int i=0;i<3;i++){
        double v=m_center[i]/p->spacing[i];int w1=p->dims[i];size_t w2=m_blockSize[i];
        hsize_t v1=std::min(w1-1,std::max(0,int(v-w2/2.0))),v2=std::min(w2,w1-v1);
        start[i]=v1;block[i]=v2;
    }
    void *buffer=malloc(block[0]*block[1]*block[2]*2);

    bool bOK=updateBlockIMS(p,start,block,buffer);

    if(bOK){
        double origin[3]={start[2]*p->spacing[2],start[1]*p->spacing[1],start[0]*p->spacing[0]};
        m_viewer->setVolume(buffer,block,p->spacing,origin);emit c->showMessage("Done",500);
    }else{emit c->showMessage("Failed",3000);free(buffer);}
}

void ViewerPanel::updateBlockSize(){
    cv::Point3i *ptr=m_nextBlockSize.exchange(nullptr);if(nullptr==ptr){return;}
    m_blockSize[0]=ptr->z;m_blockSize[1]=ptr->y;m_blockSize[2]=ptr->x;delete ptr;m_bUpdated=true;
}
void ViewerPanel::updateCenter(){
    cv::Point3i *ptr=m_nextCenter.exchange(nullptr);
    if(nullptr!=ptr){
        m_center[0]=ptr->z;m_center[1]=ptr->y;m_center[2]=ptr->x;
        delete ptr;m_bUpdated=true;return;
    }

    cv::Point2i *ptr1=m_centerStep.exchange(nullptr);
    if(nullptr!=ptr1){
        int i=ptr1->x;double v=m_center[i]+m_currentLevel->spacing[i]*ptr1->y*50;
        m_center[i]=std::max(0.0,std::min(double(g_levelInfos[0]->dims[i]-1),v));

        emit Common::i()->centerChangedViewer(m_center[2],m_center[1],m_center[0]);
        delete ptr1;m_bUpdated=true;return;
    }
}
void ViewerPanel::updateOrigin(){
    cv::Point3i *ptr=m_nextOrigin.exchange(nullptr);if(nullptr==ptr){return;}
    m_origin=*ptr;delete ptr;
}

void ViewerPanel::updateResolution(){
    int resId=m_nextResolution.exchange(-1);if(resId<0){return;}
    LevelInfo *p=g_levelInfos.value(resId,nullptr);if(p==nullptr){return;}
    m_currentRes=p->resId;m_currentLevel=p;m_bUpdated=true;emit Common::i()->spacingChanged(p->spacing[2],p->spacing[1],p->spacing[0]);
}
void ViewerPanel::updateChannel(){
    int channel=m_nextChannel.exchange(-1);if(channel<0){return;}
    m_currentChannel=channel;m_bUpdated=true;initWorldViewer();
}

void ViewerPanel::initWorldViewer(){
    WorldViewerParams *params=new WorldViewerParams;

    LevelInfo *p=g_levelInfos.value(m_initResId,nullptr);
    const size_t w2=256;if(nullptr==p){return;}
    for(int i=0;i<3;i++){
        double v=params->center[i]/p->spacing[i];int w1=p->dims[i];
        hsize_t v1=std::min(w1-1,std::max(0,int(v-w2/2.0))),v2=std::min(w2,w1-v1);
        params->start[i]=v1;params->block[i]=v2;
    }
    params->buffer=malloc(params->block[0]*params->block[1]*params->block[2]*2);
    updateBlockIMS(p,params->start,params->block,params->buffer);

    LevelInfo *rawLevel=g_levelInfos[0];//params->spacing=p->spacing;
    for(int i=0;i<3;i++){params->center[i]=rawLevel->dims[i]/2;params->spacing[i]=p->spacing[i];params->origin[i]=params->start[2-i]*p->spacing[2-i];}

    emit Common::i()->initWorldViewer(params);
}

void ViewerPanel::openIMSFile(){
    QString *pathPtr=m_imsPath2Load.exchange(nullptr);if(nullptr==pathPtr){return;}
    QString filePath=*pathPtr;delete pathPtr;Common *c=Common::i();qDeleteAll(g_levelInfos);g_levelInfos.clear();

    int64_t fileId=H5Fopen(filePath.toStdString().c_str(),H5F_ACC_RDONLY,H5P_DEFAULT);
    hid_t datasetId=H5Gopen2(fileId,"DataSet",H5P_DEFAULT);

    herr_t status;H5O_info1_t infobuf;opdata od;od.recurs=0;od.prev=NULL;
    status=H5Oget_info1(m_fileId,&infobuf);status=H5Literate(datasetId,H5_INDEX_NAME,H5_ITER_NATIVE,NULL,op_func,(void *) &od);
    LevelInfo *rawLevel=g_levelInfos.value(0,nullptr);
    if(!rawLevel){H5Fclose(fileId);emit c->showMessage("Empty IMS file",3000);return;}

    foreach(LevelInfo *info,g_levelInfos){for(int i=0;i<3;i++){info->spacing[i]=rawLevel->dims[i]/double(info->dims[i]);}}

    m_fileId=fileId;m_imagePath=filePath;bool bProjectValid=!m_projectPath.isEmpty();
    if(!bProjectValid){for(int i=0;i<3;i++){m_center[i]=rawLevel->dims[i]/2;}}
    emit c->centerloaded(rawLevel->dims[2],rawLevel->dims[1],rawLevel->dims[0],m_center[2],m_center[1],m_center[0]);

    int resId=m_currentRes.load();
    foreach(LevelInfo *info,g_levelInfos){
        m_initResId=info->resId;bool bOK=true;
        for(int i=0;i<3;i++){if(info->dims[i]>m_blockSize[i]){bOK=false;break;}}
        if(bOK){break;}
    }
    if(resId<0){resId=m_initResId;}

    initWorldViewer();

    if(!bProjectValid){emit c->setProjectFileName(QFileInfo(filePath).fileName());}
    emit c->channelLoaded(rawLevel->dataIds.length(),m_currentChannel);
    int resNumber=g_levelInfos.keys().length();emit resolutionNumberChanged(resNumber,resId+1);
}
void ViewerPanel::onOpenIMSFile(){
    QString path=QFileDialog::getOpenFileName(nullptr,"Open IMS file","","IMS file(*.ims;*.h5)");
    if(!path.isEmpty()){delete m_imsPath2Load.exchange(new QString(path));}
}

void ViewerPanel::onLoadProject(){
    if(!m_projectPath.isEmpty()||!m_imagePath.isEmpty()){return;}

    QString path=QFileDialog::getOpenFileName(nullptr,"Open project file","","(*.json)");if(path.isEmpty()){return;}
    QVariantMap params;Common *c=Common::i();
    if(!Util::loadJson(path,params)){emit c->showMessage("Unable to load project file "+path,3000);return;}

    QFileInfo fileInfo(path);QString imagePath=params["image_path"].toString();if(imagePath.isEmpty()){return;}
    if(!QFileInfo(imagePath).isAbsolute()){imagePath=fileInfo.dir().absoluteFilePath(imagePath);}
    m_projectPath=path;emit c->setProjectFileName(fileInfo.fileName());

    QStringList list=params["center"].toString().split(" ");
    if(list.length()==3){for(int i=0;i<3;i++){m_center[2-i]=list[i].toDouble();}}
    list=params["origin"].toString().split(" ");
    if(list.length()==3){
        int vs[3];for(int i=0;i<3;i++){vs[i]=list[i].toInt();}
        m_origin=cv::Point3i(vs[0],vs[1],vs[2]);emit c->originChangedViewer(vs[0],vs[1],vs[2]);c->p_originZ=m_origin.z;
    }

    bool bOK;int resId=params["current_resolution"].toInt(&bOK);if(bOK){m_currentRes=resId;}
    int channelId=params["signal_channel"].toInt(&bOK);if(bOK){m_currentChannel=channelId;}

    double voxelSize=params["voxel_size"].toDouble(&bOK);
    if(bOK&&voxelSize>0){c->p_voxelSize=voxelSize;emit c->voxelSizeLoaded(voxelSize);}

    double offset[3]={0};m_viewer->importNodes(params["nodes"].toList(),params["groups"].toList(),c->p_voxelSize,offset);

    delete m_imsPath2Load.exchange(new QString(imagePath));
}
void ViewerPanel::onImportNodes(){
    if(m_imagePath.isEmpty()){return;}

    QString path=QFileDialog::getOpenFileName(nullptr,"Import nodes from project file","","(*.json;*.csv)");
    if(path.isEmpty()){return;}

    Common *c=Common::i();
    if(path.endsWith(".json")){
        QVariantMap params;
        if(!Util::loadJson(path,params)){emit c->showMessage("Unable to load project file "+path,3000);return;}

        bool bOK;double voxelSize=params["voxel_size"].toDouble(&bOK);
        if(!bOK||voxelSize<=0){voxelSize=c->p_voxelSize;return;}

        QStringList list=params["origin"].toString().split(" ");cv::Point3i origin,dp;bOK=false;
        if(list.length()==3){
            bool bOK1,bOK2,bOK3;origin=cv::Point3i(list[0].toInt(&bOK1),list[1].toInt(&bOK2),list[2].toInt(&bOK3));
            bOK=(bOK1&&bOK2&&bOK3);
        }

        if(bOK){dp=origin-m_origin;}
        double offset[3]={dp.x,dp.y,dp.z};

        m_viewer->importNodes(params["nodes"].toList(),params["groups"].toList(),voxelSize,offset);
    }else if(path.endsWith(".csv")){
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)){emit c->showMessage("Unable to open spots file "+path,2000);return;}

        QTextStream in(&file);QList<cv::Point3d> spots;
        while(!in.atEnd()){
            QStringList line=in.readLine().split(",");if(line.length()<8){continue;}
            if(line[4]!="Spot"||line[5]!="Position"){continue;}
            cv::Point3d p(line[0].toDouble(),line[1].toDouble(),line[2].toDouble());spots.append(p);
        }
        if(!spots.empty()){m_viewer->importSpots(spots);}
    }
}

void ViewerPanel::saveProject(){
    QList<ProjectInfo*> infos;
    ProjectInfo *info=m_projectInfo2Save.exchange(nullptr);if(nullptr!=info){infos.append(info);info->bAutoSaved=false;}
    info=m_projectInfo2AutoSave.exchange(nullptr);if(nullptr!=info){infos.append(info);info->bAutoSaved=true;}
    if(infos.empty()){return;}

    foreach(ProjectInfo *info,infos){
        QFileInfo fileInfo(info->filePath);Common *c=Common::i();bool bAutoSaved=info->bAutoSaved;
        QVariantMap params;params["image_path"]=fileInfo.dir().relativeFilePath(m_imagePath);
        params["center"]=QString("%1 %2 %3").arg(QString::number(m_center[2]),QString::number(m_center[1]),QString::number(m_center[0]));
        params["current_resolution"]=m_currentRes.load();params["signal_channel"]=m_currentChannel;
        params["voxel_size"]=info->voxelSize;params["nodes"]=info->nodes;params["groups"]=info->groups;
        params["origin"]=QString("%1 %2 %3").arg(QString::number(m_origin.x),QString::number(m_origin.y),QString::number(m_origin.z));

        bool bOK=Util::saveJson(info->filePath,params);
        if(bOK){
            if(!bAutoSaved){
                emit c->showMessage("Project has been saved at "+fileInfo.fileName(),1000);
                emit c->setProjectFileName(fileInfo.fileName());
            }//else{emit c->showMessage("Auto saved at "+fileInfo.fileName(),1000);}
        }else{emit c->showMessage("Unable to save project to "+info->filePath,3000);}
    }
}

void ViewerPanel::onSaveProject(bool bSaveAs){
    if(m_imagePath.isEmpty()){return;}

    if(bSaveAs||m_projectPath.isEmpty()){
        QString path=QFileDialog::getSaveFileName(nullptr,"Save as project","","(*.json)");
        if(!path.isEmpty()){m_projectPath=path;}else{return;}
    }

    ProjectInfo *info=new ProjectInfo;info->voxelSize=Common::i()->p_voxelSize;
    info->filePath=m_projectPath;m_viewer->exportNodes(info->nodes,info->groups);
    delete m_projectInfo2Save.exchange(info);
}
void ViewerPanel::onAutoSaveProject(){
    if(m_projectPath.isEmpty()){return;}
    QString filePath=m_projectPath+".autosave.json";

    ProjectInfo *info=new ProjectInfo;info->voxelSize=Common::i()->p_voxelSize;
    info->filePath=filePath;m_viewer->exportNodes(info->nodes,info->groups);
    delete m_projectInfo2AutoSave.exchange(info);
}
void ViewerPanel::onExportProject(){
    if(m_imagePath.isEmpty()){return;}
    QString path=QFileDialog::getSaveFileName(nullptr,"Export project","","(*.json)");if(path.isEmpty()){return;}
    ProjectInfo *info=new ProjectInfo;info->voxelSize=Common::i()->p_voxelSize;
    info->filePath=path;m_viewer->exportNodes(info->nodes,info->groups,true);delete m_projectInfo2Save.exchange(info);
}

/*void ViewerPanel::onSwitchWorkspace(){
    m_bMainSpace=!m_bMainSpace;Common *c=Common::i();
    emit c->setProjectStatus(QString(m_bMainSpace?"":"Reference workspace"));
    emit c->showMessage("WorkSpace "+QString(m_bMainSpace?"1":"2 (reference)"),1000);
}*/
