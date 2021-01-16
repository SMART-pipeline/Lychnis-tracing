#include "worldviewer.h"
#include "contrastadjuster.h"
#include "common.h"

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

#include <QVBoxLayout>

//duplicated in volumeviewer
inline vtkSmartPointer<vtkActor> getActor(vtkAlgorithmOutput *output,double color[3]){
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();mapper->SetInputConnection(output);
    vtkSmartPointer<vtkProperty> prop=vtkSmartPointer<vtkProperty>::New();prop->SetColor(color);
    vtkSmartPointer<vtkActor> actor=vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);actor->SetProperty(prop);return actor;
}

WorldViewer::WorldViewer(){
    QVBoxLayout *lyt=new QVBoxLayout;lyt->setContentsMargins(0,0,0,0);//setMinimumHeight(300);
    lyt->setSpacing(1);setLayout(lyt);Common *c=Common::i();

    m_viewer=new QVTKOpenGLWidget;lyt->addWidget(m_viewer,1);

    m_renderer=vtkSmartPointer<vtkRenderer>::New();m_renderer->SetBackground(0.25,0.25,0.25);
    m_viewer->GetRenderWindow()->AddRenderer(m_renderer);
    m_viewer->GetRenderWindow()->GetInteractor()->SetInteractorStyle(vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New());

    m_opacity=vtkSmartPointer<vtkPiecewiseFunction>::New();
    m_opacity->AddPoint(0,0);m_opacity->AddPoint(100,0);m_opacity->AddPoint(1000,1);m_opacity->AddPoint(65535,1);

    ContrastAdjuster *adjuster=new ContrastAdjuster(65535,1000,100);
    connect(adjuster,&ContrastAdjuster::valueChanged,[this](int v1, int v2){
        m_opacity->RemoveAllPoints();v2=std::max(20,v2);v1=std::max(10,std::min(v2-2,v1));
        m_opacity->AddPoint(0,0);m_opacity->AddPoint(v1,0);m_opacity->AddPoint(v2,1);m_opacity->AddPoint(65535,1);
        m_viewer->GetRenderWindow()->Render();
    });
    lyt->addWidget(adjuster);

    connect(c,&Common::initWorldViewer,this,&WorldViewer::onInitWorldViewer,Qt::QueuedConnection);
    connect(c,&Common::addWorldActors,this,[this](void *rawActors){
        WorldViewerActors *actors=(WorldViewerActors*)rawActors;
        QMapIterator<void*,vtkSmartPointer<vtkActor>> iter(actors->nodeActors);
        while(iter.hasNext()){iter.next();m_nodeActors[iter.key()]=iter.value();m_renderer->AddActor(iter.value());}
        m_viewer->GetRenderWindow()->Render();delete actors;
    },Qt::QueuedConnection);
    connect(c,&Common::updateWorldActorColor,[this](void *rawIds,double r,double g,double b){
        WorldViewerIds *ids=(WorldViewerIds*)rawIds;
        foreach(void *id,ids->nodeIds){
            vtkSmartPointer<vtkActor> v=m_nodeActors.value(id,nullptr);if(nullptr!=v){v->GetProperty()->SetColor(r,g,b);}
        }
        m_viewer->GetRenderWindow()->Render();delete ids;
    });
    connect(c,&Common::deleteWorldActor,[this](void *rawIds){
        WorldViewerIds *ids=(WorldViewerIds*)rawIds;
        foreach(void *id,ids->nodeIds){
            vtkSmartPointer<vtkActor> v=m_nodeActors.value(id,nullptr);
            if(nullptr!=v){m_nodeActors.remove(id);m_renderer->RemoveActor(v);}
        }
        m_viewer->GetRenderWindow()->Render();delete ids;
    });
    connect(c,&Common::updateWorldActorWidth,[this](void *rawIds,int width){
        WorldViewerIds *ids=(WorldViewerIds*)rawIds;
        foreach(void *id,ids->nodeIds){
            vtkSmartPointer<vtkActor> v=m_nodeActors.value(id,nullptr);
            if(nullptr!=v){v->GetProperty()->SetLineWidth(width);}
        }
        m_viewer->GetRenderWindow()->Render();delete ids;
    });

    connect(c,&Common::volumeInfoChanged,this,[this](double ox,double oy,double oz,int wx,int wy,int wz){
        vtkSmartPointer<vtkCubeSource> cube=vtkSmartPointer<vtkCubeSource>::New();
        cube->SetBounds(ox,ox+wx,oy,oy+wy,oz,oz+wz);double color[]={0.8,0.8,0.8};
        vtkSmartPointer<vtkOutlineFilter> outlineFilter=vtkSmartPointer<vtkOutlineFilter>::New();
        outlineFilter->SetInputConnection(cube->GetOutputPort());

        m_renderer->RemoveActor(m_frameActor);m_frameActor=getActor(outlineFilter->GetOutputPort(),color);
        m_renderer->AddActor(m_frameActor);

        if(nullptr!=m_volume){
            double bounds[6];m_volume->GetBounds(bounds);
            double cx=ox+wx/2,cy=oy+wy/2,cz=oz+wz/2;

            foreach(vtkSmartPointer<vtkActor> v,m_centerFrames){m_renderer->RemoveActor(v);}
            m_centerFrames.clear();

            vtkSmartPointer<vtkLineSource> lineX=vtkSmartPointer<vtkLineSource>::New();
            lineX->SetPoint1(bounds[0],cy,cz);lineX->SetPoint2(bounds[1],cy,cz);
            vtkSmartPointer<vtkLineSource> lineY=vtkSmartPointer<vtkLineSource>::New();
            lineY->SetPoint1(cx,bounds[2],cz);lineY->SetPoint2(cx,bounds[3],cz);
            vtkSmartPointer<vtkLineSource> lineZ=vtkSmartPointer<vtkLineSource>::New();
            lineZ->SetPoint1(cx,cy,bounds[4]);lineZ->SetPoint2(cx,cy,bounds[5]);

            m_centerFrames.append(getActor(lineX->GetOutputPort(),color));
            m_centerFrames.append(getActor(lineY->GetOutputPort(),color));
            m_centerFrames.append(getActor(lineZ->GetOutputPort(),color));
            foreach(vtkSmartPointer<vtkActor> v,m_centerFrames){m_renderer->AddActor(v);}
        }

        m_viewer->GetRenderWindow()->Render();
    },Qt::QueuedConnection);
}

void WorldViewer::onInitWorldViewer(void *rawParams){
    WorldViewerParams *params=(WorldViewerParams*)rawParams;
    bool bInited=(nullptr!=m_volume);if(bInited){m_renderer->RemoveVolume(m_volume);}

    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();
    importer->SetDataScalarTypeToUnsignedShort();importer->SetWholeExtent(0,params->block[2]-1,0,params->block[1]-1,0,params->block[0]-1);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(params->buffer);
    importer->Update();

    vtkSmartPointer<vtkImageData> imageData=vtkSmartPointer<vtkImageData>::New();imageData->DeepCopy(importer->GetOutput());
    imageData->SetSpacing(params->spacing[2],params->spacing[1],params->spacing[0]);

    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapperGpu=vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapperGpu->SetInputData(imageData);volumeMapperGpu->SetSampleDistance(1.0);//volumeMapperGpu->SetAutoAdjustSampleDistances(1);
    volumeMapperGpu->SetBlendMode(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);
    volumeMapperGpu->SetUseJittering(true);//m_volumeMapper=volumeMapperGpu;

    vtkSmartPointer<vtkVolumeProperty> volumeProperty=vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetInterpolationTypeToLinear();volumeProperty->ShadeOff();

    volumeProperty->SetScalarOpacity(m_opacity);

    vtkSmartPointer<vtkColorTransferFunction> color=vtkSmartPointer<vtkColorTransferFunction>::New();
    color->AddRGBPoint(0,1,1,1);color->AddRGBPoint(65535,1,1,1);volumeProperty->SetColor(color);

    m_volume=vtkSmartPointer<vtkVolume>::New();m_volume->SetPosition(params->origin);m_volume->SetOrigin(params->center);
    m_volume->SetMapper(volumeMapperGpu);m_volume->SetProperty(volumeProperty);//volume->PickableOn();

    m_renderer->AddVolume(m_volume);if(!bInited){m_renderer->ResetCamera(m_volume->GetBounds());}
    m_viewer->GetRenderWindow()->Render();free(params->buffer);delete params;
}

void WorldViewer::setFibersVisible(bool bVisible){
    foreach(vtkSmartPointer<vtkActor> v,m_nodeActors){v->SetVisibility(bVisible);}
    m_viewer->GetRenderWindow()->Render();
}
