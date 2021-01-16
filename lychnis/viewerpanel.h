#ifndef VIEWERPANEL_H
#define VIEWERPANEL_H

#include <QWidget>
#include <QMap>
#include <atomic>
#include <thread>

#include <opencv2/core.hpp>

QT_BEGIN_NAMESPACE
struct LevelInfo;
struct ProjectInfo;
class VolumeViewer;
QT_END_NAMESPACE

class ViewerPanel : public QWidget
{
    Q_OBJECT

    VolumeViewer *m_viewer;

    int64_t m_fileId;int m_currentChannel;
    std::atomic_int m_nextResolution,m_nextChannel;
    double m_center[3];int m_blockSize[3];cv::Point3i m_origin;

    int m_sliceBlockSize,m_sliceThickness,m_initResId;
    QString m_sliceImagePath;int m_firstSliceIndex,m_sliceNumber;

    LevelInfo *m_currentLevel;std::atomic_int m_currentRes;
    std::atomic<cv::Point3i*> m_nextBlockSize,m_nextCenter,m_nextOrigin;
    std::atomic<cv::Point2i*> m_centerStep;

    bool m_bRunning,m_bUpdated,m_bMainSpace;std::thread *m_thread;
    std::atomic<QString*> m_imsPath2Load;
    std::atomic<ProjectInfo*> m_projectInfo2Save,m_projectInfo2AutoSave;
    QString m_imagePath,m_imageFolder,m_projectPath;

    void updateBlock();
    bool updateBlockIMS(LevelInfo *p, size_t start[], size_t block[], void *buffer);

    void updateResolution();void updateChannel();
    void updateBlockSize();void updateCenter();void updateOrigin();
    void initWorldViewer();

    void mainLoop();
    void openIMSFile();
    void saveProject();
public:
    explicit ViewerPanel();
    ~ViewerPanel();

signals:
    void resolutionNumberChanged(int number,int defaultRes);
private slots:
    void onOpenIMSFile();
    void onLoadProject();void onImportNodes();
    void onSaveProject(bool);void onAutoSaveProject();void onExportProject();
    //void onSwitchWorkspace();
};

#endif // VIEWERPANEL_H
