#include "menubar.h"
#include "common.h"

#include <QMessageBox>

MenuBar::MenuBar(){
    createFileMenu();createEditMenu();createViewMenu();
    createHelpMenu();
}

void MenuBar::createFileMenu(){
    QMenu *menu=addMenu("File");Common *c=Common::i();

    menu->addAction("Load project",c,&Common::loadProject,QKeySequence(Qt::CTRL|Qt::Key_O));
    menu->addAction("Save project",[c](){emit c->saveProject(false);},QKeySequence(Qt::CTRL|Qt::Key_S));
    menu->addAction("Save project As...",[c](){emit c->saveProject(true);});

    menu->addSeparator();
    menu->addAction("Open IMS file",c,&Common::loadIMSFile);
    menu->addAction("Import nodes",c,&Common::importNodes);
    menu->addAction("Import parameters file",c,&Common::importParameters);

    menu->addSeparator();
    QMenu *menuExp=menu->addMenu("Export nodes to file");
    menuExp->addAction("Imaris format",c,&Common::exportNodes2ImarisFile);
    menuExp->addAction("Lychnis format",c,&Common::exportNodes2LychnisFile);
    menuExp->addAction("Imaris spots",c,&Common::exportNodes2ImarisSpots);
    menu->addAction("Export current volume",c,&Common::exportCurrentVolume);
}

void MenuBar::createEditMenu(){
    QMenu *menu=addMenu("Edit");Common *c=Common::i();
    menu->addAction("Build node groups",c,&Common::buildNodeGroups);
    menu->addAction("Delete selected node",c,&Common::deleteSelectedNode,QKeySequence(Qt::CTRL|Qt::Key_Z));
    //menu->addAction("Turn on/off pathfinding",c,&Common::turnOnOffPathFinding,QKeySequence(Qt::CTRL|Qt::Key_A));
    menu->addAction("Move slice by node",c,&Common::moveSliceByNode,QKeySequence(Qt::CTRL|Qt::Key_B));
}

void MenuBar::createViewMenu(){
    QMenu *menu=addMenu("View");Common *c=Common::i();
    menu->addAction("Show/Hide all fibers",c,&Common::toggleShowNodes,QKeySequence(Qt::ALT|Qt::Key_1));
    menu->addAction("Show/Hide unselected fibers",c,&Common::toggleShowOtherNodes,QKeySequence(Qt::ALT|Qt::Key_2));
    menu->addAction("Toggle camera following",c,&Common::toggleAutoTracking);
    menu->addAction("Toggle auto loading",c,&Common::toggleAutoLoading);
    menu->addAction("Show/Hide outer frame",c,&Common::toggleFrame);
    menu->addAction("Show/Hide axis",c,&Common::toggleAxis);
    //menu->addAction("Switch workspace",c,&Common::switchWorkspace,QKeySequence(Qt::CTRL|Qt::Key_Tab));
    menu->addAction("Toggle color inversion",c,&Common::toggleColorInversion);
    //menu->addAction("Show world viewer",c,&Common::showWorldViewer);
}

void MenuBar::createHelpMenu(){
    QMenu *menu=addMenu("Help");
    menu->addAction("About lychnis",[](){
        QString infoStr="Lychnis is a fiber-tracing tool for IMS format";
        infoStr+="\n\nDeveloped and maintained by Lab of Neurophysics and Neurophysiology (at USTC)"
                 "\nAll Rights Reserved.";

        QMessageBox::information(NULL,"About lychnis",infoStr,QMessageBox::NoButton);
    });
}
