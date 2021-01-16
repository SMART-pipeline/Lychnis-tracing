#include "mainwindow.h"
#include "menubar.h"
#include "common.h"
#include "controlpanel.h"
#include "viewerpanel.h"
#include "statusbar.h"

#include <QFile>
#include <QSplitter>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QApplication>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHBoxLayout>

MainWindow::MainWindow():m_bEdited(false){
    Common *c=Common::i();
    QHBoxLayout *lyt1=new QHBoxLayout;lyt1->setContentsMargins(0,0,0,0);lyt1->setSpacing(20);
    lyt1->addWidget(new MenuBar);StatusBar *statusBar=new StatusBar(true);
    lyt1->addWidget(statusBar,1);connect(c,&Common::showMessage,statusBar,&StatusBar::showMessage,Qt::QueuedConnection);

    QSplitter* splitter=new QSplitter(Qt::Horizontal);
    splitter->addWidget(new ViewerPanel);splitter->addWidget(new ControlPanel);
    splitter->setSizes(QList<int>({40000,10000}));splitter->setHandleWidth(3);

    QVBoxLayout *lyt=new QVBoxLayout;lyt->setContentsMargins(0,0,0,0);lyt->setSpacing(0);
    lyt->addLayout(lyt1);lyt->addWidget(splitter,1);
    QWidget *w=new QWidget;w->setLayout(lyt);setCentralWidget(w);

    m_title=QFileInfo(QApplication::applicationFilePath()).completeBaseName();updateTitle();
    connect(c,&Common::setEditedState,this,&MainWindow::setEditedState);
    connect(c,&Common::setProjectFileName,this,&MainWindow::setProjectFileName);
    connect(c,&Common::setProjectStatus,[this](QString status){m_status=status;updateTitle();});

    QFile file("://assets/style.css");if(file.open(QIODevice::ReadOnly)){setStyleSheet(file.readAll());}
}

MainWindow::~MainWindow()
{

}

void MainWindow::updateTitle(){
    static const QString dotText=QString::fromUtf8(QByteArray::fromHex("e2978f"))+" ";
    QStringList ts;if(!m_fileName.isEmpty()){ts.append(m_fileName);}
    if(!m_status.isEmpty()){ts.append("["+m_status+"]");}
    ts.append(m_title);setWindowTitle((m_bEdited?dotText:"")+ts.join(" - "));
}
void MainWindow::setEditedState(bool v){m_bEdited=v;updateTitle();}
void MainWindow::setProjectFileName(QString v){m_fileName=v;updateTitle();}

void MainWindow::closeEvent(QCloseEvent *e){
    if(m_bEdited){
        if(QMessageBox::Yes!=QMessageBox::warning(nullptr,"Checking before exiting",
                                        "Project was modified but not saved.\nDo you still want to close this program?",
                                                   QMessageBox::Yes|QMessageBox::No,QMessageBox::No)){
            e->ignore();return;
        }
    }
    e->accept();
}
