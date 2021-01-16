#ifndef WIDGETS_H
#define WIDGETS_H

#include <QLineEdit>
#include <QStringList>
#include <QDoubleSpinBox>

class PathLineEdit : public QLineEdit
{
public:
    PathLineEdit();
    void setText(const QString &text);
protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
};
class PathEdit : public QWidget{
    Q_OBJECT

    PathLineEdit *m_edit;QString m_path;QString m_fileType;
public:
    PathEdit(const QString &fileType="");

    QString getPath();void setPath(const QString &path);
signals:
    void valueChanged(QString);
};

struct WidgetUtil{
    static QByteArray getStyleSheet();
};

class SpinBox:public QDoubleSpinBox{
public:
    SpinBox();
protected:
    virtual void wheelEvent(QWheelEvent *event);
};

struct RangeEdit:public QWidget{
    QSpinBox *m_rangeEditMin,*m_rangeEditMax;
public:
    RangeEdit();
    void setValue(int minValue,int maxValue);
    bool getValue(int &minValue,int &maxValue);
    QString toString();
    void fromString(const QString &str);
};

#endif // WIDGETS_H
