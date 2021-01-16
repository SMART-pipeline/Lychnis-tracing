#ifndef STACKSLIDER_H
#define STACKSLIDER_H

#include <QWidget>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QSlider;
class QToolButton;
class QLineEdit;
QT_END_NAMESPACE

class StackSlider : public QWidget
{
    Q_OBJECT

    enum States{FREE,INC,DESC,STOP,PRE_INC,PRE_DESC};

    int m_fps;States m_state;QTimer m_timer;
    QSlider *m_slider;

    QToolButton *m_btnPlay,*m_btnPause;
    QLineEdit *m_indexEdit;

    void setValue(int v);
public:
    explicit StackSlider();

    void showPlayButton(bool);
    void setRange(int minV,int maxV);
    QString text();int value();int maxValue();

    bool setValueSimple(int v);bool setValueSilent(int v);
signals:
    void valueChanged(int);
private slots:
    void addSlider();
    void minusSlider();
    void stopSlider();

    void sliderTimeout();
    void startSliderTimer();

    void updateIndexEdit(int);
    void indexEditUpdated();
};

#endif // STACKSLIDER_H
