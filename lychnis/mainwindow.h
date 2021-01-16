#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

    bool m_bEdited;
    QString m_title,m_fileName,m_status;

    void updateTitle();
public:
    MainWindow();
    ~MainWindow();

private slots:
    void setEditedState(bool);
    void setProjectFileName(QString);
protected:
    void closeEvent(QCloseEvent* e);
};

#endif // MAINWINDOW_H
