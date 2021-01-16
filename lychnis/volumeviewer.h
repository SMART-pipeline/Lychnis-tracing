#ifndef VOLUMEVIEWER_H
#define VOLUMEVIEWER_H

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

//#include "pathfinder.h"

QT_BEGIN_NAMESPACE
struct Node;
class VolumeInteractorStyle;
QT_END_NAMESPACE

struct VolumeInfo{
    void *buffer; size_t dims[3]; double spacing[3],origin[3];
    VolumeInfo():buffer(nullptr){}
    ~VolumeInfo(){free(buffer);}
};
struct GroupInfo{
    int64_t groupId;double color[3];
    GroupInfo(int64_t id):groupId(id){color[0]=1;color[1]=0;color[2]=0;}
};

class VolumeViewer : public QVTKOpenGLWidget
{
    Q_OBJECT

    vtkSmartPointer<vtkPiecewiseFunction> m_opacity;
    vtkSmartPointer<vtkVolume> m_volume,m_nextVolume;QMutex m_volumeMutex;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> m_volumeMapper;
    int m_volumeBlendMode,m_opacityBorder[2];
    VolumeInfo *m_volumeInfo,*m_nextVolumeInfo;

    vtkSmartPointer<VolumeInteractorStyle> m_interactor;
    vtkSmartPointer<vtkVolumePicker2> m_picker;vtkVolumePicker *m_pickerOpacity;
    vtkSmartPointer<vtkAxesActor> m_axes;

    int64_t m_maxNodeId,m_maxNodeGroupId;bool m_bNodeVisible,m_bOtherNodeVisible,m_bAutoTracking;
    QMap<int64_t,Node*> m_nodes;Node *m_currentNode,*m_candidateNode;
    vtkSmartPointer<vtkActor> m_currentNodeActor,m_candidateActor,m_frameActor;
    QMap<int64_t,GroupInfo*> m_nodeGroups;GroupInfo *m_defaultGroup;

    //PathFinder m_pathFinder;
    //QList<vtkSmartPointer<vtkActor>> m_pathActors;
    bool m_bAutoLoading;

    void resetCamera();void changeCurrentNode(Node *);
    void buildNodeGroups(QList<QList<Node*>> &);void toggleShowNodes(bool bAll);
    void findNextNode();int getPixel(int pos[3]);
public:
    explicit VolumeViewer();

    void setVolume(void *buffer, size_t dims[3], double spacing[3], double origin[3]);
    void pickVolume(int x,int y);void pickNode(int x,int y);void selectPickedNode(bool bConnected);
    bool pickPos(int x,int y,double pos[3]);//void findPath(double pos[3]);
    void appendNode(Node*);

    void exportNodes2File();void exportNodes2Spots();
    void exportNodes(QVariantList &,QVariantList &groups,bool bVisibleOnly=false);
    void importNodes(const QVariantList &,const QVariantList &groups,double voxelSize,double offset[3]);
    void importSpots(const QList<cv::Point3d> &spots);
signals:
    void volumeUpdated();
    void moveCenter(int axis,int step);
    void centerUpdated(int x,int y,int z);
public slots:
    void setContrast(int,int);
private slots:
    void toggleShowFrame();void toggleShowAxis();
    void onExportCurrentVolume(QString);
    void deleteCurrentNode();
    void moveSliceByNode();
    //void turnOnOffPathFinding();
    //void onPathFinded(Path *path);
protected:
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
};

#endif // VOLUMEVIEWER_H
