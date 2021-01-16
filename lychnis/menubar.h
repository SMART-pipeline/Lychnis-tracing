#ifndef MENUBAR_H
#define MENUBAR_H

#include <QMenuBar>

class MenuBar : public QMenuBar
{
    Q_OBJECT

    void createFileMenu();
    void createEditMenu();
    void createViewMenu();
    void createHelpMenu();
public:
    explicit MenuBar();

signals:

public slots:
};

#endif // MENUBAR_H
