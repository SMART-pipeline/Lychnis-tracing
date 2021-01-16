#include "volumeviewer.h"
#include "common.h"
#include "../../lab-works/flsm/tstorm-control/processing/imagewriter.h"
#include "worldviewer.h"
#include "util.h"

#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkImageImport.h>
#include <vtkImageData.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkTransform.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkLineSource.h>
#include <vtkCubeSource.h>
#include <vtkOutlineFilter.h>
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>

#include <QKeyEvent>
#include <QDebug>
#include <QFileDialog>

#include <opencv2/core.hpp>

bool g_bVolumnPicking=false,g_bPropPicking=false;//,g_bPathFinding=false;
static double s_colorLine[3]={1,0,0};
//static double s_colorSelectedLine[3]={1,0.6,0};
static double s_colorSelected[3]={1,1,0};
static double s_colorMessage[3]={0,0,1};

inline vtkSmartPointer<vtkActor> getActor(vtkAlgorithmOutput *output,double color[3]){
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();mapper->SetInputConnection(output);
    vtkSmartPointer<vtkProperty> prop=vtkSmartPointer<vtkProperty>::New();prop->SetColor(color);
    vtkSmartPointer<vtkActor> actor=vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);actor->SetProperty(prop);return actor;
}

struct Node{
    double pos[3];int64_t id,groupId;QString msg;
    QList<Node*> parentNodes,subNodes;
    QList<vtkSmartPointer<vtkActor>> actors;vtkSmartPointer<vtkActor> nodeActor;

    Node():groupId(0){}
    ~Node(){release();}

    bool isSpecial(){return !msg.isEmpty();}
    QString toString(double f){return QString("%1 %2 %3").arg(QString::number(pos[0]*f),QString::number(pos[1]*f),QString::number(pos[2]*f));}

    vtkSmartPointer<vtkActor> getLineActor(Node *p){
        vtkSmartPointer<vtkLineSource> line=vtkSmartPointer<vtkLineSource>::New();line->SetPoint1(pos);line->SetPoint2(p->pos);
        return getActor(line->GetOutputPort(),s_colorLine);
    }
    void addParentNode(Node *p,WorldViewerActors *worldActors){//=nullptr
        if(this==p){return;}

        if(!parentNodes.contains(p)){
            parentNodes.append(p);groupId=p->groupId;

            vtkSmartPointer<vtkActor> actor1=getLineActor(p),actor2=getLineActor(p);
            actors.append(actor1);worldActors->nodeActors[actor1.GetPointer()]=actor2;//if(nullptr!=worldActors){}
        }
        if(!p->subNodes.contains(this)){p->subNodes.append(this);}
    }
    void release(){
        foreach(Node *p,subNodes){p->parentNodes.removeOne(this);}
        foreach(Node *p,parentNodes){p->subNodes.removeOne(this);}
    }
};

inline vtkSmartPointer<vtkActor> getNodeActor(Node *p,double color[3]){
    vtkSmartPointer<vtkSphereSource> sphereSource=vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetCenter(p->pos);sphereSource->SetRadius(3);
    return getActor(sphereSource->GetOutputPort(),color);
}

class VolumeInteractorStyle : public vtkInteractorStyleTrackballCamera{
    VolumeInteractorStyle(){}
    void getMousePos(int &x,int &y){x=this->Interactor->GetEventPosition()[0];y=this->Interactor->GetEventPosition()[1];}
public:
    static VolumeInteractorStyle* New();
    vtkTypeMacro(VolumeInteractorStyle, vtkInteractorStyleTrackballCamera);

    VolumeViewer *p_viewer;
protected:
    void OnChar() override{}

    void OnMouseMove() override{
        if(g_bVolumnPicking){/*&&g_bPathFinding
            int x,y;getMousePos(x,y);double pos[3];//p_viewer->pickVolume(x,y);
            if(p_viewer->pickPos(x,y,pos)){p_viewer->findPath(pos);}*/
        }else if(g_bPropPicking){int x,y;getMousePos(x,y);p_viewer->pickNode(x,y);}
        else{vtkInteractorStyleTrackballCamera::OnMouseMove();return;}
    }
    void OnLeftButtonDown() override{
        if(g_bVolumnPicking){int x,y;getMousePos(x,y);p_viewer->pickVolume(x,y);}
        else if(g_bPropPicking){p_viewer->selectPickedNode(false);}
        else{vtkInteractorStyleTrackballCamera::OnLeftButtonDown();return;}
    }
    void OnRightButtonDown() override{
        if(g_bVolumnPicking){}
        else if(g_bPropPicking){p_viewer->selectPickedNode(true);}
        else{vtkInteractorStyleTrackballCamera::OnRightButtonDown();return;}
    }
};
vtkStandardNewMacro(VolumeInteractorStyle);

VolumeViewer::VolumeViewer():m_currentNode(nullptr),m_candidateNode(nullptr),m_maxNodeId(0),m_volumeInfo(nullptr),m_nextVolumeInfo(nullptr),
    m_bNodeVisible(true),m_bOtherNodeVisible(true),m_bAutoTracking(false),m_bAutoLoading(false),
    m_maxNodeGroupId(0),m_volumeBlendMode(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND){

    m_opacity=vtkSmartPointer<vtkPiecewiseFunction>::New();
    m_opacity->AddPoint(0,0);m_opacity->AddPoint(100,0);m_opacity->AddPoint(1000,1);m_opacity->AddPoint(65535,1);
    m_opacityBorder[0]=100;m_opacityBorder[1]=1000;//m_pathFinder.setOpacity(m_opacityBorder);

    m_renderer=vtkSmartPointer<vtkRenderer>::New();
    GetRenderWindow()->AddRenderer(m_renderer);
    m_renderer->SetBackground(0.1,0.1,0.1);

    m_interactor=vtkSmartPointer<VolumeInteractorStyle>::New();
    GetRenderWindow()->GetInteractor()->SetInteractorStyle(m_interactor);m_interactor->p_viewer=this;

    m_picker=vtkSmartPointer<vtkVolumePicker2>::New();
    m_pickerOpacity=vtkVolumePicker::New();m_pickerOpacity->SetTolerance(1e-6);m_pickerOpacity->SetVolumeOpacityIsovalue(0.1);
    m_axes=vtkSmartPointer<vtkAxesActor>::New();m_axes->SetAxisLabels(false);m_axes->SetConeRadius(0);m_axes->SetConeRadius(0);

    m_defaultGroup=new GroupInfo(0);

    Common *c=Common::i();
    connect(c,&Common::toggleShowNodes,[this](){toggleShowNodes(true);});
    connect(c,&Common::toggleShowOtherNodes,[this](){toggleShowNodes(false);});
    connect(c,&Common::toggleFrame,this,&VolumeViewer::toggleShowFrame);

    connect(c,&Common::toggleAutoTracking,[this,c](){
        m_bAutoTracking=!m_bAutoTracking;emit c->showMessage("Camera following is "+QString(m_bAutoTracking?"on":"off"),2000);
    });
    connect(c,&Common::toggleAutoLoading,[this,c](){
        m_bAutoLoading=!m_bAutoLoading;emit c->showMessage("Auto loading is "+QString(m_bAutoLoading?"on":"off"),2000);
    });
    connect(c,&Common::buildNodeGroups,[this](){QList<QList<Node *> > g;buildNodeGroups(g);changeCurrentNode(m_currentNode);});
    connect(c,&Common::exportNodes2ImarisFile,this,&VolumeViewer::exportNodes2File);
    connect(c,&Common::exportNodes2ImarisSpots,this,&VolumeViewer::exportNodes2Spots);
    connect(c,&Common::exportVolume,this,&VolumeViewer::onExportCurrentVolume);
    connect(c,&Common::deleteSelectedNode,this,&VolumeViewer::deleteCurrentNode);
    connect(c,&Common::moveSliceByNode,this,&VolumeViewer::moveSliceByNode);
    //connect(c,&Common::turnOnOffPathFinding,this,&VolumeViewer::turnOnOffPathFinding);
    //connect(&m_pathFinder,&PathFinder::pathFinded,this,&VolumeViewer::onPathFinded,Qt::QueuedConnection);

    connect(c,&Common::toggleColorInversion,[this,c](){
        bool bMIP=(m_volumeBlendMode==vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);bMIP=!bMIP;
        m_volumeBlendMode=(bMIP?vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND:vtkVolumeMapper::MINIMUM_INTENSITY_BLEND);
        m_volumeMapper->SetBlendMode(m_volumeBlendMode);setContrast(m_opacityBorder[0],m_opacityBorder[1]);
    });
    connect(c,&Common::toggleAxis,this,&VolumeViewer::toggleShowAxis);

    connect(c,&Common::selectedNodeInfoChanged,[this,c](int64_t id,QString msg){
        Node *p=m_currentNode;if(nullptr==p||p->id!=id){emit c->showMessage("Unable to update message of node #"+QString::number(id),2000);return;}

        bool bUpdated=false;
        if(p->isSpecial()&&msg.isEmpty()){m_renderer->RemoveActor(p->nodeActor);bUpdated=true;}
        else if(!p->isSpecial()&&!msg.isEmpty()){p->nodeActor=getNodeActor(p,s_colorMessage);m_renderer->AddActor(p->nodeActor);bUpdated=true;}
        GetRenderWindow()->Render();

        p->msg=msg;emit c->showMessage("Message of node #"+QString::number(id)+" has been updated",2000);
    });
    connect(c,&Common::selectedNodeColorChanged,[this,c](int64_t id,double r,double g,double b){
        Node *p=m_currentNode;if(nullptr==p||p->id!=id){emit c->showMessage("Unable to update color of node #"+QString::number(id),2000);return;}
        WorldViewerIds *worldIds=new WorldViewerIds;

        GroupInfo *group=m_nodeGroups.value(p->groupId,m_defaultGroup);group->color[0]=r;group->color[1]=g;group->color[2]=b;
        foreach(Node *p1,m_nodes){
            if(p1->groupId!=p->groupId){continue;}
            foreach(vtkSmartPointer<vtkActor> act,p1->actors){act->GetProperty()->SetColor(group->color);worldIds->nodeIds.append(act.GetPointer());}
        }
        emit c->updateWorldActorColor(worldIds,r,g,b);

        changeCurrentNode(p);
        emit c->showMessage("Color of group #"+QString::number(group->groupId)+" has been updated",2000);
    });
    connect(c,&Common::nodeSelected,[this](int64_t id){
        foreach(Node *p,m_nodes){if(p->id==id){changeCurrentNode(p);break;}}
    });

    connect(c,&Common::centerloaded,this,[this](int maxX,int maxY,int maxZ,int x,int y,int z){
        vtkSmartPointer<vtkCubeSource> cube=vtkSmartPointer<vtkCubeSource>::New();
        cube->SetBounds(0,maxX,0,maxY,0,maxZ);double color[]={0.8,0.8,0.8};
        vtkSmartPointer<vtkOutlineFilter> outlineFilter=vtkSmartPointer<vtkOutlineFilter>::New();
        outlineFilter->SetInputConnection(cube->GetOutputPort());
        m_frameActor=getActor(outlineFilter->GetOutputPort(),color);
        m_renderer->AddActor(m_frameActor);//GetRenderWindow()->Render();
    },Qt::QueuedConnection);

    connect(this,&VolumeViewer::volumeUpdated,this,[this](){
        m_volumeMutex.lock();vtkSmartPointer<vtkVolume> volume=m_nextVolume;m_nextVolume=nullptr;
        VolumeInfo *info=m_nextVolumeInfo;m_nextVolumeInfo=nullptr;
        m_volumeMutex.unlock();if(nullptr==volume){return;}

        m_renderer->RemoveVolume(m_volume);m_renderer->AddVolume(volume);
        delete m_volumeInfo;m_volumeInfo=info;

        bool bInited=(nullptr!=m_volume);m_volume=volume;
        if(!bInited){m_renderer->AddActor(m_axes);}//m_renderer->ResetCamera();
        if(!bInited||m_bAutoTracking){resetCamera();}
        GetRenderWindow()->Render();
    },Qt::QueuedConnection);
}

void VolumeViewer::setVolume(void *buffer, size_t dims[], double spacing[], double origin[]){
    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();
    importer->SetDataScalarTypeToUnsignedShort();importer->SetWholeExtent(0,dims[2]-1,0,dims[1]-1,0,dims[0]-1);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(buffer);
    importer->Update();

    vtkSmartPointer<vtkImageData> imageData=vtkSmartPointer<vtkImageData>::New();imageData->DeepCopy(importer->GetOutput());
    imageData->SetSpacing(spacing[2],spacing[1],spacing[0]);

    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapperGpu=vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapperGpu->SetInputData(imageData);volumeMapperGpu->SetSampleDistance(1.0);//volumeMapperGpu->SetAutoAdjustSampleDistances(1);
    volumeMapperGpu->SetBlendMode(m_volumeBlendMode);//vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);
    volumeMapperGpu->SetUseJittering(true);m_volumeMapper=volumeMapperGpu;

    vtkSmartPointer<vtkVolumeProperty> volumeProperty=vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetInterpolationTypeToLinear();volumeProperty->ShadeOff();

    volumeProperty->SetScalarOpacity(m_opacity);

    vtkSmartPointer<vtkColorTransferFunction> color=vtkSmartPointer<vtkColorTransferFunction>::New();
    color->AddRGBPoint(0,1,1,1);color->AddRGBPoint(65535,1,1,1);volumeProperty->SetColor(color);

    vtkSmartPointer<vtkTransform> transform=vtkSmartPointer<vtkTransform>::New();transform->Translate(origin);
    double lengths[3]={dims[2]*spacing[2]/2,dims[1]*spacing[1]/2,dims[0]*spacing[0]/2};
    double center[3]={origin[0]+lengths[0],origin[1]+lengths[1],origin[2]+lengths[2]};
    m_axes->SetOrigin(center);m_axes->SetUserTransform(transform);m_axes->SetTotalLength(lengths);

    vtkSmartPointer<vtkVolume> volume=vtkSmartPointer<vtkVolume>::New();volume->SetPosition(origin);volume->SetOrigin(center);
    volume->SetMapper(volumeMapperGpu);volume->SetProperty(volumeProperty);volume->PickableOn();

    //if(g_bPathFinding){m_pathFinder.stop();}

    m_volumeMutex.lock();
    m_nextVolume=volume;
    VolumeInfo *info=new VolumeInfo;info->buffer=buffer;for(int i=0;i<3;i++){info->dims[i]=dims[i];info->spacing[i]=spacing[i];info->origin[i]=origin[i];}
    delete m_nextVolumeInfo;m_nextVolumeInfo=info;
    m_volumeMutex.unlock();emit volumeUpdated();

    emit Common::i()->volumeInfoChanged(origin[0],origin[1],origin[2],dims[2]*spacing[2],dims[1]*spacing[1],dims[0]*spacing[0]);
}

void VolumeViewer::appendNode(Node *node){
    if(nullptr==m_currentNode){
        m_maxNodeGroupId++;node->groupId=m_maxNodeGroupId;
        GroupInfo *g=new GroupInfo(node->groupId);m_nodeGroups[node->groupId]=g;//if(!m_nodeGroups.contains(node->groupId)){}
        return;
    }

    WorldViewerActors *worldActors=new WorldViewerActors;
    node->addParentNode(m_currentNode,worldActors);m_renderer->AddActor(node->actors[node->parentNodes.indexOf(m_currentNode)]);
    emit Common::i()->addWorldActors(worldActors);
}

bool VolumeViewer::pickPos(int x, int y, double pos[]){
    bool bMIP=(m_volumeBlendMode==vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);
    if(0==(bMIP?m_picker:m_pickerOpacity)->Pick(x,y,0,m_renderer)){return false;}
    (bMIP?m_picker:m_pickerOpacity)->GetPickPosition(pos);return true;
}
void VolumeViewer::pickVolume(int x, int y){
    //bool bMIP=(m_volumeBlendMode==vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);
    //if(0==(bMIP?m_picker:m_pickerOpacity)->Pick(x,y,0,m_renderer)){return;}
    Node *node=new Node;//(bMIP?m_picker:m_pickerOpacity)->GetPickPosition(node->pos);
    if(!pickPos(x,y,node->pos)){delete node;return;}

    node->id=m_maxNodeId;m_maxNodeId++;
    appendNode(node);m_nodes[node->id]=node;//m_nodes.append(node);
    changeCurrentNode(node);emit Common::i()->autoSaveProject();
}

inline double pointArrowDistance(const double p[3],const cv::Point3f &origin,const cv::Point3f &arrow){
    cv::Point3f p1(p[0],p[1],p[2]),dp=p1-origin;double v1=dp.dot(arrow),v2=dp.dot(dp);return v2-v1*v1;
}
void VolumeViewer::pickNode(int x, int y){
    m_renderer->SetDisplayPoint(x,y,0);m_renderer->DisplayToWorld();
    double *picked=m_renderer->GetWorldPoint();
    double cameraPos[3];m_renderer->GetActiveCamera()->GetPosition(cameraPos);

    cv::Point3f pickedPos(picked[0],picked[1],picked[2]),origin(cameraPos[0],cameraPos[1],cameraPos[2]),arrow=pickedPos-origin;
    arrow/=sqrt(arrow.dot(arrow));

    double minDist=DBL_MAX;Node *targetNode=nullptr;
    int64_t groupId=(nullptr==m_currentNode?-1:m_currentNode->groupId);
    foreach(Node *p,m_nodes){
        if(!m_bOtherNodeVisible&&p->groupId!=groupId){continue;}
        double d=pointArrowDistance(p->pos,origin,arrow);if(d<minDist){minDist=d;targetNode=p;}
    }
    if(nullptr!=targetNode){
        if(nullptr!=m_candidateNode){m_renderer->RemoveActor(m_candidateActor);}
        m_candidateNode=targetNode;m_candidateActor=getNodeActor(targetNode,s_colorSelected);
        m_renderer->AddActor(m_candidateActor);GetRenderWindow()->Render();
    }
}
void VolumeViewer::selectPickedNode(bool bConnected){
    if(nullptr==m_candidateNode||!m_nodes.contains(m_candidateNode->id)){return;}//m_nodes.contains(m_candidateNode)
    m_renderer->RemoveActor(m_candidateActor);if(bConnected){appendNode(m_candidateNode);}
    changeCurrentNode(m_candidateNode);m_candidateNode=nullptr;
}

void VolumeViewer::changeCurrentNode(Node *p){
    if(nullptr!=m_currentNode){m_renderer->RemoveActor(m_currentNodeActor);}
    m_currentNode=p;Common *c=Common::i();

    if(nullptr!=p){
        m_currentNodeActor=getNodeActor(p,s_colorSelected);
        m_renderer->AddActor(m_currentNodeActor);
        GroupInfo *group=m_nodeGroups.value(p->groupId,m_defaultGroup);
        //if(nullptr==group){emit c->showMessage("No group #"+QString::number(p->groupId),2000);qWarning()<<m_nodeGroups.keys()<<p->groupId;return;}
        for(int i=0;i<3;i++){s_colorLine[i]=group->color[i];}

        emit c->selectedNodeChanged(p->id,p->msg,p->toString(1),group->color[0],group->color[1],group->color[2],true);

        if(m_bAutoLoading){emit centerUpdated(p->pos[0],p->pos[1],p->pos[2]);}
    }else{emit c->selectedNodeChanged(0,"","",-1,-1,-1,false);}

    WorldViewerIds *worldIds1=new WorldViewerIds,*worldIds2=new WorldViewerIds;
    int64_t groupId=(nullptr==m_currentNode?-1:m_currentNode->groupId);
    foreach(Node *p,m_nodes){
        bool bSameGroup=(groupId>=0&&p->groupId==groupId),bVisible=m_bNodeVisible&&(m_bOtherNodeVisible||bSameGroup);
        if(nullptr!=p->nodeActor){p->nodeActor->SetVisibility(bVisible);}

        //GroupInfo *group=m_nodeGroups.value(groupId,m_defaultGroup);
        foreach(vtkSmartPointer<vtkActor> act,p->actors){
            act->SetVisibility(bVisible);
            if(bVisible){
                act->GetProperty()->SetLineWidth(bSameGroup?2:1);(bSameGroup?worldIds2:worldIds1)->nodeIds.append(act.GetPointer());
                //act->GetProperty()->SetColor(bSameGroup?s_colorSelectedLine:group->color);
            }//s_colorLine
        }
    }
    emit c->updateWorldActorWidth(worldIds1,1);emit c->updateWorldActorWidth(worldIds2,2);

    GetRenderWindow()->Render();
}

void VolumeViewer::setContrast(int v1, int v2){
    m_opacity->RemoveAllPoints();v2=std::max(20,v2);v1=std::max(10,std::min(v2-2,v1));
    m_opacityBorder[0]=v1;m_opacityBorder[1]=v2;//m_pathFinder.setOpacity(m_opacityBorder);
    //m_opacity->AddPoint(0,0);m_opacity->AddPoint(v1,0);m_opacity->AddPoint(v2,1);m_opacity->AddPoint(65535,1);
    if(m_volumeBlendMode==vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND){
        m_opacity->AddPoint(0,0);m_opacity->AddPoint(v1,0);m_opacity->AddPoint(v2,1);m_opacity->AddPoint(65535,1);
    }else{
        //m_opacity->AddPoint(0,0);m_opacity->AddPoint(v1-2,0);m_opacity->AddPoint(v1,1);m_opacity->AddPoint(v2,0);m_opacity->AddPoint(65535,0);
        m_opacity->AddPoint(0,1);m_opacity->AddPoint(v1,1);m_opacity->AddPoint(v2,0);m_opacity->AddPoint(65535,0);
    }
    GetRenderWindow()->Render();
}

void VolumeViewer::resetCamera(){if(nullptr!=m_volume){m_renderer->ResetCamera(m_volume->GetBounds());}}

void VolumeViewer::deleteCurrentNode(){
    Common *c=Common::i();if(nullptr==m_currentNode){return;}

    m_nodes.remove(m_currentNode->id);Node *nextSelected=nullptr;//m_nodes.removeOne(m_currentNode);
    if(!m_currentNode->parentNodes.empty()){nextSelected=m_currentNode->parentNodes.first();}
    else if(!m_currentNode->subNodes.empty()){nextSelected=m_currentNode->subNodes.first();}

    WorldViewerIds *worldIds=new WorldViewerIds;
    foreach(vtkSmartPointer<vtkActor> v,m_currentNode->actors){m_renderer->RemoveActor(v);worldIds->nodeIds.append(v.GetPointer());}
    foreach(Node *p,m_currentNode->subNodes){
        int index=p->parentNodes.indexOf(m_currentNode);
        if(index>=0){vtkSmartPointer<vtkActor> v=p->actors[index];m_renderer->RemoveActor(v);p->actors.removeAt(index);worldIds->nodeIds.append(v.GetPointer());}
    }
    if(m_currentNode->isSpecial()){
        m_renderer->RemoveActor(m_currentNode->nodeActor);
        emit c->deleteSpecialNode(m_currentNode->id);
    }
    delete m_currentNode;changeCurrentNode(nextSelected);emit c->autoSaveProject();
    emit c->deleteWorldActor(worldIds);
}

void VolumeViewer::keyPressEvent(QKeyEvent *e){
    if(e->isAutoRepeat()||e->modifiers()!=Qt::NoModifier){return;}
//    Common *c=Common::i();

    switch(e->key()){
    case Qt::Key_R:{resetCamera();GetRenderWindow()->Render();}break;
    case Qt::Key_W:{emit moveCenter(1,1);}break;
    case Qt::Key_S:{emit moveCenter(1,-1);}break;
    case Qt::Key_D:{emit moveCenter(2,1);}break;
    case Qt::Key_A:{emit moveCenter(2,-1);}break;
    case Qt::Key_E:{emit moveCenter(0,1);}break;
    case Qt::Key_Q:{emit moveCenter(0,-1);}break;

    case Qt::Key_Z:{g_bVolumnPicking=true;}break;
    case Qt::Key_X:{g_bPropPicking=true;}break;
    case Qt::Key_V:{toggleShowNodes(true);}break;
    case Qt::Key_F:{findNextNode();}break;
    case Qt::Key_B:{moveSliceByNode();}break;

    case Qt::Key_Delete:{deleteCurrentNode();}break;
    case Qt::Key_Escape:{
        if(nullptr!=m_currentNode){changeCurrentNode(nullptr);}
    }break;
    case Qt::Key_Space:{
        if(nullptr!=m_currentNode){Node *p=m_currentNode;emit centerUpdated(p->pos[0],p->pos[1],p->pos[2]);}
    }break;
    }
}
void VolumeViewer::keyReleaseEvent(QKeyEvent *e){
    if(e->isAutoRepeat()||e->modifiers()!=Qt::NoModifier){return;}

    switch(e->key()){
    case Qt::Key_Z:{g_bVolumnPicking=false;}break;//if(g_bPathFinding){m_pathFinder.stop();}
    case Qt::Key_X:{
        g_bPropPicking=false;
        if(nullptr!=m_candidateNode){
            m_candidateNode=nullptr;m_renderer->RemoveActor(m_candidateActor);
            m_candidateActor=nullptr;GetRenderWindow()->Render();
        }
    }break;
    }
}

void VolumeViewer::buildNodeGroups(QList<QList<Node *> > &nodeGroups){
    QMap<int64_t,int64_t> groupMap;//QList<int64_t> groupIds;

    QMap<int64_t,Node*> nodes=m_nodes;QList<Node*> addedNodes;m_maxNodeGroupId=0;
    while(true){
        bool bNode=false;
        foreach(Node *p,nodes){
            if(addedNodes.contains(p)){continue;}else{bNode=true;}

            QList<Node*> nodes1,nodes2;nodes1.append(p);
            m_maxNodeGroupId++;//m_nodeGroups[m_maxNodeGroupId]=new GroupInfo(m_maxNodeGroupId);
            while(!nodes1.empty()){
                Node *p1=nodes1.back();nodes1.pop_back();if(nodes2.contains(p1)){continue;}
                nodes2.append(p1);addedNodes.append(p1);

                if(!groupMap.contains(m_maxNodeGroupId)){
                    groupMap[m_maxNodeGroupId]=p1->groupId;//groupIds.append(p1->groupId);
                }
                p1->groupId=m_maxNodeGroupId;

                foreach(Node *p2,p1->parentNodes+p1->subNodes){
                    if(nodes1.contains(p2)||nodes2.contains(p2)){continue;}
                    nodes1.append(p2);
                }
            }
            if(!m_bOtherNodeVisible){
                if(nodes2.contains(m_currentNode)){nodeGroups.append(nodes2);break;}
            }else{nodeGroups.append(nodes2);}
        }
        if(!bNode){break;}
    }

    QMap<int64_t,GroupInfo*> nodeGroups2;//QMap<int64_t,GroupInfo*> newGroups;//qDebug()<<m_nodeGroups.keys()<<groupMap;
    QMapIterator<int64_t,int64_t> iterGroupMap(groupMap);
    while(iterGroupMap.hasNext()){
        iterGroupMap.next();int64_t id=iterGroupMap.key(),id1=iterGroupMap.value();
    //foreach(int64_t id,groupIds){int64_t id1=groupMap.value(id,-1);
        GroupInfo *group=m_nodeGroups.value(id1,nullptr);
        if(nullptr!=group){nodeGroups2[id]=group;m_nodeGroups.remove(id1);continue;}
        group=new GroupInfo(id);nodeGroups2[id]=group;//newGroups[id]=group;
    }
    qDeleteAll(m_nodeGroups);m_nodeGroups=nodeGroups2;

    foreach(Node *p,m_nodes){
        GroupInfo *group=m_nodeGroups[p->groupId];
        foreach(vtkSmartPointer<vtkActor> act,p->actors){act->GetProperty()->SetColor(group->color);}
    }
}

void VolumeViewer::exportNodes2File(){
    QString path=QFileDialog::getSaveFileName(nullptr,"Export nodes to file","","Imaris scene file(*.xml)");
    if(path.isEmpty()){return;}

    Common *c=Common::i();QFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){emit c->showMessage("Unable to export nodes to file "+path,2000);return;}

    static QString templateDocMain,templateDocPoints;
    if(templateDocMain.isEmpty()){
        QFile srcFile1("://assets/Imaris/nodes-bpImarisApplication.xml"),srcFile2("://assets/Imaris/nodes-bpMeasurementPoints.xml");
        if(!srcFile1.open(QIODevice::ReadOnly)||!srcFile2.open(QIODevice::ReadOnly)){return;}
        templateDocMain=QString::fromUtf8(srcFile1.readAll());templateDocPoints=QString::fromUtf8(srcFile2.readAll());
    }

    //QList<QList<Node*>> nodeGroups;buildNodeGroups(nodeGroups);
    QMap<int64_t,QList<Node*>> nodeGroups;
    int64_t groupId=(nullptr==m_currentNode?-1:m_currentNode->groupId);
    foreach(Node *p,m_nodes){
        if(!m_bOtherNodeVisible&&p->groupId!=groupId){continue;}
        if(!nodeGroups.contains(p->groupId)){nodeGroups[p->groupId]=QList<Node*>();}
        nodeGroups[p->groupId].append(p);
    }

    const double posFactor=c->p_voxelSize;
    QString fileName=QFileInfo(path).baseName(),pathGroups;
    const QString line="<Point Name=\"%1\" Position=\"%2\" TimeIndex=\"0\"/>";

    //QListIterator<QList<Node*>> iter1(nodeGroups);while(iter1.hasNext()){const QList<Node*> *plist=&(iter1.next());
    QMapIterator<int64_t,QList<Node*>> iter1(nodeGroups);
    while(iter1.hasNext()){
        iter1.next();int64_t groupId=iter1.key();const QList<Node*> *plist=&(iter1.value());

        QString paths;if(plist->empty()){continue;}
        QString name1=QString::number(groupId),name2=fileName+"_"+name1;

        foreach(Node *p,*plist){
            bool bSpecial=p->isSpecial();QString name,pos1=p->toString(posFactor);
            if(p->parentNodes.empty()&&bSpecial){
                paths+=line.arg(p->msg,pos1)+line.arg("",pos1)+"\n";continue;
            }

            foreach(Node *p1,p->parentNodes){
                if(bSpecial){bSpecial=false;name=p->msg;}
                paths+=line.arg(name,pos1)+line.arg("",p1->toString(posFactor))+"\n";
            }
        }

        GroupInfo *group=m_nodeGroups.value(groupId,m_defaultGroup);
        QString colorStr=QString("%1,%2,%3").arg(QString::number(group->color[0]),QString::number(group->color[1]),QString::number(group->color[2]));

        QString pointsDoc=templateDocPoints.arg(name2,name1,colorStr,paths);pathGroups+=pointsDoc+"\n";
    }

    QString doc=templateDocMain.arg(pathGroups);file.write(doc.toUtf8());
    file.close();emit c->showMessage("Done",500);
}
void VolumeViewer::exportNodes2Spots(){
    QString path=QFileDialog::getSaveFileName(nullptr,"Export nodes to spots","","Imaris spots file(*.csv)");
    if(path.isEmpty()){return;}

    Common *c=Common::i();QFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){emit c->showMessage("Unable to export nodes to file "+path,2000);return;}

    QTextStream out(&file);emit c->showMessage("Saving imaris spots...");
    out<<",,,,,,,\n==================== ,,,,,,,\nPosition X,Position Y,Position Z,Unit,Category,Collection,Time,ID\n";

    foreach(Node *p,m_nodes){
        if(!m_bOtherNodeVisible&&nullptr!=m_currentNode&&p->groupId!=m_currentNode->groupId){continue;}
        out<<p->pos[0]<<","<<p->pos[1]<<","<<p->pos[2]<<",Pixel,Spot,Position,1,"<<p->id<<"\n";
    }

    out.flush();file.close();emit c->showMessage("Done",500);
}

void VolumeViewer::exportNodes(QVariantList &list, QVariantList &groups, bool bVisibleOnly){
    foreach(Node *p,m_nodes){
        if(bVisibleOnly&&!m_bOtherNodeVisible&&nullptr!=m_currentNode&&p->groupId!=m_currentNode->groupId){continue;}

        QVariantMap v;v["id"]=p->id;v["group_id"]=p->groupId;
        v["position"]=QString("%1 %2 %3").arg(QString::number(p->pos[0]),QString::number(p->pos[1]),QString::number(p->pos[2]));
        QStringList parentNodeIds;foreach(Node *p1,p->parentNodes){parentNodeIds.append(QString::number(p1->id));}
        if(!parentNodeIds.empty()){v["parent_ids"]=parentNodeIds.join(" ");}
        if(!p->msg.isEmpty()){v["message"]=p->msg;}
        list.append(v);
    }
    foreach(GroupInfo *p,m_nodeGroups){
        QVariantMap v;v["color"]=QString("%1 %2 %3").arg(QString::number(p->color[0]),QString::number(p->color[1]),QString::number(p->color[2]));
        v["id"]=p->groupId;groups.append(v);
    }
}
void VolumeViewer::importNodes(const QVariantList &list, const QVariantList &groups, double voxelSize, double offset[]){
    Common *c=Common::i();//if(!m_nodes.empty()){emit c->showMessage("Cannot import nodes to a modified environment",3000);return;}
    double factor=1/c->p_voxelSize;int64_t maxGroupId=m_maxNodeGroupId;

    QMap<Node*,QList<int64_t>> parentIds;QMap<int64_t,int64_t> idMaps;//QMap<int64_t,Node*> nodes;
    foreach(QVariant v,list){
        QVariantMap v1=v.toMap();QStringList list=v1["position"].toString().split(" ");if(list.length()!=3){continue;}
        Node *p=new Node;p->id=v1["id"].toLongLong();p->groupId=m_maxNodeGroupId+v1["group_id"].toLongLong();
        for(int i=0;i<3;i++){p->pos[i]=(list[i].toDouble()*voxelSize+offset[i])/c->p_voxelSize;}

        Node *p1=m_nodes.value(p->id,nullptr);
        if(nullptr!=p1){
            //if(p->pos[0]==p1->pos[0]&&p->pos[1]==p1->pos[1]&&p->pos[2]==p1->pos[2]){delete p;continue;}
            if(abs(p->pos[0]-p1->pos[0])<factor&&abs(p->pos[1]-p1->pos[1])<factor&&abs(p->pos[2]-p1->pos[2])<factor){delete p;continue;}
            else{idMaps[p->id]=m_maxNodeId;p->id=m_maxNodeId;m_maxNodeId++;}
        }
        maxGroupId=std::max(p->groupId,maxGroupId);

        p->msg=v1["message"].toString();m_nodes[p->id]=p;//nodes[p->id]=p;
        QString idStr=v1["parent_ids"].toString();
        if(!idStr.isEmpty()){
            list=idStr.split(" ");QList<int64_t> ids;foreach(QString v2,list){ids.append(v2.toLongLong());}
            parentIds[p]=ids;
        }
    }

    foreach(QVariant v,groups){
        QVariantMap v1=v.toMap();int64_t groupId=m_maxNodeGroupId+v1["id"].toLongLong();
        QStringList list=v1["color"].toString().split(" ");if(list.length()!=3){continue;}
        GroupInfo *p=new GroupInfo(groupId);for(int i=0;i<3;i++){p->color[i]=list[i].toDouble();}
        m_nodeGroups[groupId]=p;
    }
    m_maxNodeGroupId=maxGroupId;

    WorldViewerActors *worldActors=new WorldViewerActors;

    QMap<int64_t,Node*> specialNodes;//QStringList specialIdStr,specialMsgStr;
    double lineColor[3]={s_colorLine[0],s_colorLine[1],s_colorLine[2]};
    QMapIterator<Node*,QList<int64_t>> iter(parentIds);
    while(iter.hasNext()){
        iter.next();Node *p=iter.key();
        foreach(int64_t id,iter.value()){
            Node *p1=m_nodes.value(idMaps.value(id,id),nullptr);GroupInfo *group=m_nodeGroups.value(p1->groupId,m_defaultGroup);
            for(int i=0;i<3;i++){s_colorLine[i]=group->color[i];}
            if(nullptr!=p1){p->addParentNode(p1,worldActors);}
        }
        foreach(vtkSmartPointer<vtkActor> v,p->actors){m_renderer->AddActor(v);}
        if(p->isSpecial()){
            p->nodeActor=getNodeActor(p,s_colorMessage);m_renderer->AddActor(p->nodeActor);
            //specialIdStr.append(QString::number(p->id));specialMsgStr.append(p->msg);
            specialNodes[p->id]=p;
        }
    }
    for(int i=0;i<3;i++){s_colorLine[i]=lineColor[i];}

    QStringList specialIdStr,specialMsgStr;
    foreach(Node *p,specialNodes){specialIdStr.append(QString::number(p->id));specialMsgStr.append(p->msg);}
    emit c->initSpecialNodes(specialIdStr.join("\n"),specialMsgStr.join("\n"));

    if(!m_nodes.empty()){m_maxNodeId=m_nodes.keys().last()+1;}
    GetRenderWindow()->Render();

    emit c->addWorldActors(worldActors);
}
void VolumeViewer::importSpots(const QList<cv::Point3d> &spots){
    Common *c=Common::i();if(!m_nodes.empty()){emit c->showMessage("Cannot import nodes to a modified environment",3000);return;}
    foreach(cv::Point3d pos,spots){
        Node *p=new Node;p->id=m_maxNodeId;m_maxNodeId++;
        p->pos[0]=pos.x;p->pos[1]=pos.y;p->pos[2]=pos.z;m_nodes[p->id]=p;
        p->nodeActor=getNodeActor(p,s_colorMessage);m_renderer->AddActor(p->nodeActor);
    }
    GetRenderWindow()->Render();qDebug()<<m_nodes.keys().length()<<"nodes";
}

void VolumeViewer::toggleShowNodes(bool bAll){
    QString msg;
    if(bAll){m_bNodeVisible=!m_bNodeVisible;msg="All fibers are "+QString(m_bNodeVisible?"visible":"hidden");}
    else{m_bOtherNodeVisible=!m_bOtherNodeVisible;msg="All other fibers are "+QString(m_bOtherNodeVisible?"visible":"hidden");}
    emit Common::i()->showMessage(msg,2000);

    changeCurrentNode(m_currentNode);
}
void VolumeViewer::toggleShowFrame(){
    if(nullptr==m_frameActor){return;}
    bool bVisible=!m_frameActor->GetVisibility();m_frameActor->SetVisibility(bVisible);GetRenderWindow()->Render();
    emit Common::i()->showMessage("Outer frame is "+QString(bVisible?"visible":"hidden"),2000);
}
void VolumeViewer::toggleShowAxis(){
    if(nullptr==m_axes){return;}
    bool bVisible=!m_axes->GetVisibility();m_axes->SetVisibility(bVisible);GetRenderWindow()->Render();
    emit Common::i()->showMessage("Outer frame is "+QString(bVisible?"visible":"hidden"),2000);
}

void VolumeViewer::onExportCurrentVolume(QString fileName){
    if(nullptr==m_volumeInfo){return;}
    static QString folderPath;
    QString path=QFileDialog::getSaveFileName(nullptr,"Export current volume",folderPath+"/"+fileName,"Image stack(*.tif)");if(path.isEmpty()){return;}
    VolumeInfo *info=m_volumeInfo;flsmio::ImageWriter *writer=new flsmio::ImageWriter(path);
    int x=info->dims[2],y=info->dims[1],z=info->dims[0],area=x*y;uint16_t *data=(uint16_t*)info->buffer;
    for(int i=0;i<z;i++,data+=area){cv::Mat m(cv::Size(x,y),CV_16UC1,data);writer->addImage(m);}
    delete writer;emit Common::i()->showMessage("Volume has been saved at "+path,2000);
    folderPath=QFileInfo(path).dir().absolutePath();
}

void VolumeViewer::moveSliceByNode(){
    Node *p=m_currentNode;VolumeInfo *info=m_volumeInfo;if(nullptr==info||nullptr==p){return;}

    Common *c=Common::i();double voxelSize=c->p_voxelSize;
    int slice=int((p->pos[2])*voxelSize+c->p_originZ)/300,dstSlice=-1;Node *dstNode=nullptr;
    foreach(Node *p1,p->parentNodes+p->subNodes){
        int slice1=int((p1->pos[2])*voxelSize+c->p_originZ)/300;if(slice==slice1){continue;}
        if(nullptr!=dstNode){emit c->showMessage("Ambiguous node",2000);return;}else{dstNode=p1;dstSlice=slice1;}
    }
    if(nullptr==dstNode){emit c->showMessage("Please select an edge node",2000);return;}

    cv::Point offset((p->pos[0]-dstNode->pos[0])/info->spacing[0],(p->pos[1]-dstNode->pos[1])/info->spacing[1]);
    size_t x=info->dims[2],y=info->dims[1],z=info->dims[0],area=x*y;uint16_t *data=(uint16_t*)info->buffer;
    //qDebug()<<slice<<dstSlice<<offset.x<<offset.y;//offset=cv::Point(100,50);

    uint16_t *dstBuffer=(uint16_t*)malloc(area*z*2),*pBuffer=dstBuffer;//memset(dstBuffer,0,area*z*2);
    for(int i=0;i<z;i++,data+=area,pBuffer+=area){
        int slice1=int((i*info->spacing[2]+info->origin[2])*voxelSize+c->p_originZ)/300;//qDebug()<<i<<slice1;
        if(slice1!=slice){memcpy(pBuffer,data,area*2);continue;}

        cv::Rect r1(0,0,x,y),r2(offset.x,offset.y,x,y);if(!Util::computeInterSection(r1,r2)){memset(pBuffer,0,area*2);continue;}
        cv::Mat src(cv::Size(x,y),CV_16UC1,data),dst(src.size(),src.type(),pBuffer);src(r1).copyTo(dst(r2));
    }

    setVolume(dstBuffer,info->dims,info->spacing,info->origin);
}

/*void VolumeViewer::turnOnOffPathFinding(){
    if(nullptr==m_volumeInfo){return;}
    bool v=!g_bPathFinding;Common *c=Common::i();
    //if(v&&nullptr==m_currentNode){emit c->showMessage("Please select a node before turn on pathfinding",2000);return;}
    g_bPathFinding=v;if(!v){m_pathFinder.stop();}
    emit c->showMessage("Pathfinding is turned "+QString(v?"on":"off"),2000);
}
void VolumeViewer::findPath(double pos[]){
    if(nullptr==m_currentNode||nullptr==m_volumeInfo){return;}

    PathData *data=new PathData;
    data->origin=cv::Point3i(m_volumeInfo->origin[0],m_volumeInfo->origin[1],m_volumeInfo->origin[2]);
    data->start=cv::Point3i(m_currentNode->pos[0],m_currentNode->pos[1],m_currentNode->pos[2])-data->origin;
    data->end=cv::Point3i(pos[0],pos[1],pos[2])-data->origin;
    data->size=cv::Point3i(m_volumeInfo->dims[2],m_volumeInfo->dims[1],m_volumeInfo->dims[0]);
    data->data=(uint16_t*)m_volumeInfo->buffer;

    m_pathFinder.findPath(data);
}
void VolumeViewer::onPathFinded(Path *path){
    foreach(vtkSmartPointer<vtkActor> v,m_pathActors){m_renderer->RemoveActor(v);}
    m_pathActors.clear();

    if(!path->points.empty()&&nullptr!=m_currentNode){
        cv::Point3i p1(m_currentNode->pos[0],m_currentNode->pos[1],m_currentNode->pos[2]);
        foreach(cv::Point3i p,path->points){
            vtkSmartPointer<vtkLineSource> line=vtkSmartPointer<vtkLineSource>::New();
            line->SetPoint1(p1.x,p1.y,p1.z);line->SetPoint2(p.x,p.y,p.z);
            vtkSmartPointer<vtkActor> act=getActor(line->GetOutputPort(),s_colorLine);
            m_pathActors.append(act);m_renderer->AddActor(act);p1=p;
        }
    }
    qDebug()<<"render"<<path->points.length();
    delete path;GetRenderWindow()->Render();
}*/

int VolumeViewer::getPixel(int pos2[]){
    int v=*((uint16_t*)m_volumeInfo->buffer+int(pos2[2]*m_volumeInfo->dims[2]*m_volumeInfo->dims[1]+pos2[1]*m_volumeInfo->dims[2]+pos2[0]));
    //int v1=m_opacityBorder[0],v2=m_opacityBorder[1];if(v<=v1){v=0;}else if(v>=v2){v=65535;}else{v=(v-v1)*65535/(v2-v1);}
    return v;
}
void VolumeViewer::findNextNode(){
    if(nullptr==m_volumeInfo){return;}
    Common *c=Common::i();if(nullptr==m_currentNode){emit c->showMessage("No node is selected",2000);return;}
    Node *currentNode=m_currentNode;cv::Point3d firstDir1;int firstPixel2;

    for(int times=0;times<100;times++){
        QList<Node*> nodes=currentNode->parentNodes+currentNode->subNodes;
        if(nodes.length()!=1){emit c->showMessage("Selected node is not an endpoint",2000);goto End;}

        Node *p2=currentNode,*p1=nodes.first();int pos1[3],pos2[3],pos3[3];
        for(int i=0;i<3;i++){
            pos1[i]=(p1->pos[i]-m_volumeInfo->origin[i])/m_volumeInfo->spacing[i];
            pos2[i]=(p2->pos[i]-m_volumeInfo->origin[i])/m_volumeInfo->spacing[i];
            //pos3[i]=2*pos2[i]-pos1[i];

            double v2=pos2[i];//,v3=pos3[i],vmax=;
            if(v2<0||v2>=m_volumeInfo->dims[2-i]){emit c->showMessage("Selected node is not contained in current volume",2000);goto End;}
            //if(v3<0||v3>=vmax){emit c->showMessage("Predicted node is not contained in current volume",2000);return;}
        }
        int pixel2=getPixel(pos2);//*((uint16_t*)m_volumeInfo->buffer+int(pos2[2]*m_volumeInfo->dims[2]*m_volumeInfo->dims[1]+pos2[1]*m_volumeInfo->dims[2]+pos2[0]));
        cv::Point3d dir1(pos2[0]-pos1[0],pos2[1]-pos1[1],pos2[2]-pos1[2]);dir1/=sqrt(dir1.dot(dir1));
        if(0==times){firstDir1=dir1;firstPixel2=pixel2;}

        const int area=5;int minDiff=INT_MAX;double pos4[3]={-1},posDir1[3]={dir1.x,dir1.y,dir1.z};
        for(int i=0;i<3;i++){
            pos3[i]=pos2[i]+posDir1[i]*(3+area);double v3=pos3[i];
            if(v3<0||v3>=m_volumeInfo->dims[2-i]){
                if(0==times){emit c->showMessage("Predicted node is not contained in current volume",2000);}
                goto End;
            }
        }

        for(int i=-area;i<=area;i++){
            for(int j=-area;j<=area;j++){
                for(int k=-area;k<=area;k++){
                    //if(i==0&&j==0&&k==0){continue;}

                    int dp[]={i,j,k};bool bOK=true;
                    for(int s=0;s<3;s++){
                        int v4=pos3[s]+dp[s];dp[s]=v4;
                        if(v4<0||v4>=m_volumeInfo->dims[2-s]){bOK=false;break;}
                    }

                    if(!bOK){continue;}
                    cv::Point3d dir2(dp[0]-pos2[0],dp[1]-pos2[1],dp[2]-pos2[2]);
                    double length=sqrt(dir2.dot(dir2));if(length==0){continue;}
                    dir2/=length;if(dir1.dot(dir2)<=0.5||firstDir1.dot(dir2)<=0.5){continue;}

                    int pixel4=getPixel(dp);//*((uint16_t*)m_volumeInfo->buffer+dp[2]*m_volumeInfo->dims[2]*m_volumeInfo->dims[1]+dp[1]*m_volumeInfo->dims[2]+dp[0]);
                    int diff=abs(firstPixel2-pixel4);if(diff>minDiff){continue;}
                    minDiff=diff;for(int s=0;s<3;s++){pos4[s]=dp[s];}
                }
            }
        }
        if(pos4[0]<0||minDiff>firstPixel2*0.5){break;}//qDebug()<<"No candidate node";

        Node *node=new Node;
        for(int i=0;i<3;i++){node->pos[i]=pos4[i]*m_volumeInfo->spacing[i]+m_volumeInfo->origin[i];}
        node->id=m_maxNodeId;m_maxNodeId++;//m_currentNode=node;appendNode(node);
        WorldViewerActors *worldActors=new WorldViewerActors;
        node->addParentNode(currentNode,worldActors);m_renderer->AddActor(node->actors[node->parentNodes.indexOf(currentNode)]);
        m_nodes[node->id]=node;emit c->addWorldActors(worldActors);

        currentNode=node;
    }
End:
    if(currentNode==m_currentNode){return;}
    changeCurrentNode(currentNode);emit c->autoSaveProject();
}
