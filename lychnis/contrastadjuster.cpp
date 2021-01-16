#include "contrastadjuster.h"
#include "rangeslider.h"

#include <QSpinBox>
#include <QHBoxLayout>
//#include <QDebug>

static const float g_MiddleRangeFactor=0.8;

ContrastAdjuster::ContrastAdjuster(int maxValue, int middleValue, int lowerValue):m_maxValue(maxValue),m_middleValue(middleValue){
    QHBoxLayout *lyt=new QHBoxLayout;setLayout(lyt);lyt->setContentsMargins(0,0,0,0);

    m_slider=new RangeSlider;m_slider->SetRange(0,m_maxValue);m_slider->SetLowerValue(lowerValue);
    connect(m_slider,&RangeSlider::lowerValueChanged,this,&ContrastAdjuster::lowerSliderChanged);
    connect(m_slider,&RangeSlider::upperValueChanged,this,&ContrastAdjuster::upperSliderChanged);

    m_spinbox1=new QSpinBox;m_spinbox1->setMaximumWidth(70);m_spinbox1->setRange(0,m_maxValue);
    m_spinbox2=new QSpinBox;m_spinbox2->setMaximumWidth(70);m_spinbox2->setRange(0,m_maxValue);
    connect(m_spinbox1,SIGNAL(valueChanged(int)),this,SLOT(lowerSpinboxChanged(int)));
    connect(m_spinbox2,SIGNAL(valueChanged(int)),this,SLOT(upperSpinboxChanged(int)));
    m_spinbox1->setValue(lowerValue);m_spinbox2->setValue(m_middleValue);

    lyt->addWidget(m_spinbox1);lyt->addWidget(m_slider,1);lyt->addWidget(m_spinbox2);
}

void ContrastAdjuster::emitValueChanged(){emit valueChanged(m_spinbox1->value(),m_spinbox2->value());}
int ContrastAdjuster::sliderValue2Spinbox(int v){
    double maxX=m_maxValue-1,middleV=g_MiddleRangeFactor*maxX;
    if(v<middleV){v=v/middleV*m_middleValue;}
    else{v=(v-middleV)/double(maxX-middleV)*(m_maxValue-m_middleValue)+m_middleValue;}
    return v;
}
int ContrastAdjuster::spinboxValue2Slider(int v){
    double maxX=m_maxValue-1,middleV=g_MiddleRangeFactor*maxX;
    if(v>m_middleValue){v=double(v-m_middleValue)/(m_maxValue-m_middleValue)*(maxX-middleV)+middleV;}
    else{v=double(v)/m_middleValue*middleV;}
    return v;
}

void ContrastAdjuster::lowerSpinboxChanged(int v){
    int v1=spinboxValue2Slider(v);//qDebug()<<"lowerSpinboxChanged"<<v1;
    m_slider->blockSignals(true);m_slider->SetLowerValue(v1);m_slider->blockSignals(false);
    emitValueChanged();
}
void ContrastAdjuster::upperSpinboxChanged(int v){
    int v1=spinboxValue2Slider(v);//qDebug()<<"upperSpinboxChanged"<<v1;
    m_slider->blockSignals(true);m_slider->SetUpperValue(v1);m_slider->blockSignals(false);
    emitValueChanged();
}
void ContrastAdjuster::lowerSliderChanged(int v){
    int v1=sliderValue2Spinbox(v);//qDebug()<<"lowerSliderChanged"<<v1;
    m_spinbox1->blockSignals(true);m_spinbox1->setValue(v1);m_spinbox1->blockSignals(false);
    emitValueChanged();
}
void ContrastAdjuster::upperSliderChanged(int v){
    int v1=sliderValue2Spinbox(v);//qDebug()<<"upperSliderChanged"<<v1;
    m_spinbox2->blockSignals(true);m_spinbox2->setValue(v1);m_spinbox2->blockSignals(false);
    emitValueChanged();
}
