#include "statusbar.h"

#include <QHBoxLayout>
#include <QDebug>

StatusBar::StatusBar(bool bLimitLength):m_bLimitLength(bLimitLength){
    QHBoxLayout *lyt=new QHBoxLayout;lyt->setContentsMargins(0,0,0,0);setLayout(lyt);
    lyt->addStretch(1);lyt->addWidget(&m_label);

    m_label.setObjectName("Message");m_label.hide();
    connect(&m_timer,SIGNAL(timeout()),this,SLOT(timeout()));

    if(!bLimitLength){m_label.setWordWrap(true);}
}

StatusBar::~StatusBar(){m_timer.stop();}

void StatusBar::timeout(){m_timer.stop();m_label.hide();}

void StatusBar::showMessage(QString msg, int time){
    if(m_bLimitLength){
        QFontMetrics metrics(font());
        msg=metrics.elidedText(msg,Qt::ElideRight,width());
    }
    m_label.setText(msg);

    m_label.show();m_timer.stop();
    if(time>0){m_timer.setInterval(time);m_timer.start();}
}
