#ifndef WORLDVIEWER_H
#define WORLDVIEWER_H

#include <vtkAutoInit.h> // if not using CMake to compile, necessary to use this macro
#define vtkRenderingCore_AUTOINIT 3(vtkInteractionStyle, vtkRenderingFreeType, vtkRenderingOpenGL2)
#define vtkRenderingVolume_AUTOINIT 1(vtkRenderingVolumeOpenGL2)
#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)

#include <vtkSmartPointer.h>
#include <vtkObjectFactory.h>
#include <vtkRenderWindowInteractor.h>
#include "vtk/vtkVolumePicker2.h"
//#include <vtkVolumePicker.h>
#include <vtkAxesActor.h>

#include <QVTKOpenGLWidget.h>
#include <vtkVolume.h>
#include <vtkPiecewiseFunction.h>
#include <vtkGPUVolumeRayCastMapper.h>

#include <QMutex>
#include <QList>
#include <QMap>

#include <opencv2/core.hpp>

struct WorldViewerParams{
    double center[3],origin[3],spacing[3];size_t start[3],block[3];void *buffer;
};
struct WorldViewerActors{
    QMap<void*,vtkSmartPointer<vtkActor>> nodeActors;//void*->vtkSmartPointer<vtkActor>.GetPointer()
};
struct WorldViewerIds{
    QList<void*> nodeIds;//vtkSmartPointer<vtkActor>.GetPointer()
};

class WorldViewer : public QWidget
{
    Q_OBJECT

    vtkSmartPointer<vtkPiecewiseFunction> m_opacity;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkVolume> m_volume;
    QMap<void*,vtkSmartPointer<vtkActor>> m_nodeActors;
    vtkSmartPointer<vtkActor> m_frameActor;
    QList<vtkSmartPointer<vtkActor>> m_centerFrames;

    QVTKOpenGLWidget *m_viewer;
public:
    explicit WorldViewer();
    void setFibersVisible(bool bVisible);
signals:

private slots:
    void onInitWorldViewer(void*);
};

#endif // WORLDVIEWER_H
