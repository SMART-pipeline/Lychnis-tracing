#ifndef CONTRASTADJUSTER_H
#define CONTRASTADJUSTER_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class RangeSlider;
class QSpinBox;
QT_END_NAMESPACE

class ContrastAdjuster : public QWidget
{
    Q_OBJECT

    int m_maxValue,m_middleValue;
    QSpinBox *m_spinbox1,*m_spinbox2;RangeSlider *m_slider;

    void emitValueChanged();
    int spinboxValue2Slider(int);int sliderValue2Spinbox(int);
public:
    ContrastAdjuster(int maxValue, int middleValue, int lowerValue);
signals:
    void valueChanged(int,int);
private slots:
    void lowerSpinboxChanged(int);
    void upperSpinboxChanged(int);
    void lowerSliderChanged(int);
    void upperSliderChanged(int);
};

#endif // CONTRASTADJUSTER_H
