#include "widgets.h"

#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDir>
#include <QProcess>
#include <QHBoxLayout>
#include <QFile>
#include <QPushButton>
#include <QFileDialog>
#include <QWheelEvent>

PathLineEdit::PathLineEdit(){setFocusPolicy(Qt::NoFocus);}

void PathLineEdit::setText(const QString &text){QLineEdit::setText(text);setToolTip(text);}

void PathLineEdit::contextMenuEvent(QContextMenuEvent *event){
    QMenu *menu=new QMenu(this);QString filePath=text();bool bValid=!filePath.isEmpty();

    menu->addAction("Show in Explorer",[filePath](){
        QString cmdStr="explorer /select, "+QDir::toNativeSeparators(filePath)+"";QProcess::startDetached(cmdStr);
    })->setEnabled(bValid);

    menu->exec(event->globalPos());delete menu;
}

PathEdit::PathEdit(const QString &fileType):m_fileType(fileType){
    m_edit=new PathLineEdit;QPushButton *browseButton=new QPushButton("Browse");
    QHBoxLayout *lyt=new QHBoxLayout;lyt->setContentsMargins(0,0,0,0);setLayout(lyt);lyt->setSpacing(3);
    lyt->addWidget(m_edit,1);lyt->addWidget(browseButton);

    connect(browseButton,&QPushButton::clicked,[this](){
        QString path;
        if(m_fileType.isEmpty()){
            path=QFileDialog::getExistingDirectory(nullptr,"Select a directory",m_edit->text(),QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
        }else{path=QFileDialog::getOpenFileName(nullptr,"Select a file","",m_fileType);}

        if(!path.isEmpty()){m_edit->setText(path);m_path=path;emit valueChanged(path);}
    });
}

QString PathEdit::getPath(){return m_path;}
void PathEdit::setPath(const QString &path){m_edit->setText(path);m_path=path;}

QByteArray WidgetUtil::getStyleSheet(){
    QFile file("://assets/style.css");QByteArray content;
    if(file.open(QIODevice::ReadOnly)){content=file.readAll();}
    return content;
}

SpinBox::SpinBox(){setFocusPolicy(Qt::StrongFocus);}
void SpinBox::wheelEvent(QWheelEvent *event){if(!hasFocus()){event->ignore();}else{QDoubleSpinBox::wheelEvent(event);}}

RangeEdit::RangeEdit(){
    m_rangeEditMin=new QSpinBox;m_rangeEditMax=new QSpinBox;m_rangeEditMin->setRange(0,100000);m_rangeEditMax->setRange(0,100000);
    QHBoxLayout *lyt2=new QHBoxLayout;lyt2->setContentsMargins(0,0,0,0);lyt2->setSpacing(1);
    lyt2->addWidget(m_rangeEditMin);lyt2->addWidget(m_rangeEditMax);setLayout(lyt2);
}
void RangeEdit::setValue(int minValue,int maxValue){m_rangeEditMin->setValue(minValue);m_rangeEditMax->setValue(maxValue);}
bool RangeEdit::getValue(int &minValue,int &maxValue){minValue=m_rangeEditMin->value();maxValue=m_rangeEditMax->value();return maxValue>=minValue;}
QString RangeEdit::toString(){return QString::number(m_rangeEditMin->value())+" "+QString::number(m_rangeEditMax->value());}
void RangeEdit::fromString(const QString &str){QStringList vs=str.split(" ");if(vs.length()==2){m_rangeEditMin->setValue(vs[0].toInt());m_rangeEditMax->setValue(vs[1].toInt());}}
