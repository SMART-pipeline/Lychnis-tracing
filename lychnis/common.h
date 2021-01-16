#ifndef COMMON_H
#define COMMON_H

#include <QObject>

class Common : public QObject
{
    Q_OBJECT

    explicit Common();
public:
    static Common *i(){static Common c;return &c;}

    double p_voxelSize;int p_originZ;
signals:
    void showMessage(const QString & message, int timeout = 0);
    void setEditedState(bool);void setProjectFileName(QString);void setProjectStatus(QString);

    void centerChangedViewer(int x,int y,int z);void centerloaded(int maxX,int maxY,int maxZ,int x,int y,int z);
    void centerChangedControl(int x,int y,int z);void blockSizeChangedControl(int x,int y,int z);
    void originChangedViewer(int x,int y,int z);void originChangedControl(int x,int y,int z);
    void spacingChanged(double,double,double);void channelLoaded(int,int);void channelChanged(int);
    void volumeInfoChanged(double ox,double oy,double oz,int wx,int wy,int wz);

    void voxelSizeLoaded(double);

    void selectedNodeChanged(int64_t id,QString msg,QString pos,double r,double g,double b,bool bValid);
    void selectedNodeInfoChanged(int64_t id,QString msg);void selectedNodeColorChanged(int64_t,double r,double g,double b);
    void initSpecialNodes(QString ids,QString msgs);void deleteSpecialNode(int64_t id);void nodeSelected(int64_t id);

    void toggleShowNodes();void toggleShowOtherNodes();void toggleAutoTracking();void toggleAutoLoading();
    void toggleFrame();void toggleColorInversion();void toggleAxis();
    void buildNodeGroups();void deleteSelectedNode();void moveSliceByNode();//void turnOnOffPathFinding();
    void initWorldViewer(void*);//void showWorldViewer();
    void addWorldActors(void*);//WorldViewerActors
    void updateWorldActorColor(void*,double r,double g,double b);void deleteWorldActor(void *);void updateWorldActorWidth(void*,int);//WorldViewerIds

    void loadProject();void saveProject(bool);void autoSaveProject();
    void loadIMSFile();void exportNodes2LychnisFile();
    void exportNodes2ImarisFile();void exportNodes2ImarisSpots();
    void exportCurrentVolume();void exportVolume(QString);
    void importNodes();void importParameters();
public slots:
};

#endif // COMMON_H
