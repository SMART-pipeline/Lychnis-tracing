#include "stackslider.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QSlider>
#include <QSpinBox>
#include <QLineEdit>
//#include <QLabel>
#include <QApplication>
#include <QStyle>

#include <QTimer>
#include <QDebug>

inline QToolButton *createBtn(QStyle::StandardPixmap icon){
    QToolButton *btn=new QToolButton;
    btn->setAutoRaise(true);btn->setIcon(QApplication::style()->standardIcon(icon));//QIcon(path));
    btn->setMaximumSize(25,25);btn->setIconSize(btn->size());

    return btn;
}

StackSlider::StackSlider():m_fps(8),m_state(States::FREE)
{
    QHBoxLayout *lyt=new QHBoxLayout;setLayout(lyt);
    lyt->setMargin(0);lyt->setContentsMargins(0,0,0,0);//lyt->setSpacing(0);

    QToolButton *btnBack=createBtn(QStyle::SP_MediaSeekBackward),
            *btnGo=createBtn(QStyle::SP_MediaSeekForward);

    m_btnPlay=createBtn(QStyle::SP_MediaPlay);
    m_btnPause=createBtn(QStyle::SP_MediaStop);m_btnPause->hide();
    connect(m_btnPlay,SIGNAL(clicked()),this,SLOT(startSliderTimer()));
    connect(m_btnPause,SIGNAL(clicked()),this,SLOT(stopSlider()));

    connect(btnBack,SIGNAL(pressed()),this,SLOT(minusSlider()));
    connect(btnGo,SIGNAL(pressed()),this,SLOT(addSlider()));
    connect(btnBack,SIGNAL(released()),this,SLOT(stopSlider()));
    connect(btnGo,SIGNAL(released()),this,SLOT(stopSlider()));

    m_slider=new QSlider(Qt::Horizontal);m_slider->setMaximumHeight(20);
    connect(m_slider,SIGNAL(valueChanged(int)),this,SIGNAL(valueChanged(int)));

    m_timer.setSingleShot(true);
    connect(&m_timer,SIGNAL(timeout()),this,SLOT(sliderTimeout()));

    lyt->addWidget(m_btnPlay);lyt->addWidget(m_btnPause);
    lyt->addWidget(btnBack);lyt->addWidget(m_slider);lyt->addWidget(btnGo);

    m_indexEdit=new QLineEdit("0");m_indexEdit->setMaximumWidth(80);m_indexEdit->setObjectName("SliderIndex");
    lyt->addWidget(m_indexEdit);m_indexEdit->setValidator(new QIntValidator());
    connect(m_slider,SIGNAL(valueChanged(int)),this,SLOT(updateIndexEdit(int)));
    connect(m_indexEdit,SIGNAL(returnPressed()),this,SLOT(indexEditUpdated()));
}

void StackSlider::showPlayButton(bool bVisible){m_btnPlay->setVisible(bVisible);}

void StackSlider::setValue(int v){
    if(States::FREE==m_state){return;}

    int v1=v-1;

    if(v1<0){v1+=m_slider->maximum();}
    v1=v1%m_slider->maximum()+1;

    // qDebug()<<v<<v1;

    m_slider->setValue(v1);
}
bool StackSlider::setValueSimple(int v){
    if(v<1||v>m_slider->maximum()){return false;}

    m_slider->setValue(v);

    return true;
}
bool StackSlider::setValueSilent(int v){
    disconnect(m_slider,SIGNAL(valueChanged(int)),this,SIGNAL(valueChanged(int)));
    setValueSimple(v);
    connect(m_slider,SIGNAL(valueChanged(int)),this,SIGNAL(valueChanged(int)));

    updateIndexEdit(v);

    return true;
}

int StackSlider::maxValue(){return m_slider->maximum();}

void StackSlider::addSlider(){
    if(m_state!=States::FREE){return;}
    m_state=PRE_INC;

    setValueSimple(m_slider->value()+1);

    m_timer.start(500);
}
void StackSlider::minusSlider(){
    if(m_state!=States::FREE){return;}
    m_state=States::PRE_DESC;

    setValueSimple(m_slider->value()-1);

    m_timer.start(500);
}

void StackSlider::stopSlider(){
    if(!m_btnPause->isHidden()){
        m_btnPause->hide();m_btnPlay->show();
    }

    m_timer.stop();m_state=States::FREE;
}

void StackSlider::sliderTimeout(){
    switch(m_state){
    case States::PRE_INC:{
        m_state=States::INC;
    }
    case States::INC:{
        setValue(m_slider->value()+1);
    }break;
    case States::PRE_DESC:{
        m_state=States::DESC;
    }
    case States::DESC:{
        setValue(m_slider->value()-1);
    }break;
    default:{
        return;
    }
    }

    m_timer.start(1000/m_fps);
}

void StackSlider::startSliderTimer(){
    if(States::FREE!=m_state){return;}
    m_btnPlay->hide();m_btnPause->show();

    m_state=States::INC;sliderTimeout();
}

void StackSlider::setRange(int minV, int maxV){
    disconnect(m_slider,SIGNAL(valueChanged(int)),this,SIGNAL(valueChanged(int)));
    m_slider->setRange(minV,maxV);
    connect(m_slider,SIGNAL(valueChanged(int)),this,SIGNAL(valueChanged(int)));

    int width=m_slider->width()/m_slider->maximum()*1.2;
    if(width<20){width=20;}

    // qDebug()<<width<<slider->width()<<slider->maximum();
    QString styleStr=QString("QSlider::handle:horizontal{width:%1px;}").arg(QString::number(width));
    m_slider->setStyleSheet(styleStr);

    m_indexEdit->setToolTip("from "+QString::number(minV)+" to "+QString::number(maxV));
}

QString StackSlider::text(){
    QString msg=QString("%1/%2").arg(
                QString::number(m_slider->value()),QString::number(m_slider->maximum()));

    return msg;
}
int StackSlider::value(){
    return m_slider->value();
}

void StackSlider::updateIndexEdit(int v){m_indexEdit->setText(QString::number(v));}
void StackSlider::indexEditUpdated(){
    bool bOK;int v=m_indexEdit->text().toInt(&bOK);
    setValueSimple(v);
}
