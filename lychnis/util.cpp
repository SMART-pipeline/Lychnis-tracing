#include "util.h"

#include <QFile>
#include <QDebug>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QVariantMap>
#include <opencv2/opencv.hpp>

using namespace cv;

Util::Util(){

}

bool Util::loadJson(const QString &filePath,QVariantMap &params){
    QFile file(filePath);if(!file.exists()||!file.open(QIODevice::ReadOnly)){return false;}
    QByteArray saveData = file.readAll();file.close();
    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
    params=loadDoc.object().toVariantMap();
    return !params.empty();
}
bool Util::saveJson(const QString &filePath, const QVariantMap& params, bool bCompact){
    QFile file(filePath);if(!file.open(QIODevice::WriteOnly)){qWarning()<<"faild to open json file"<<filePath;return false;}

    QJsonDocument saveDoc(QJsonObject::fromVariantMap(params));
    file.write(saveDoc.toJson(bCompact?QJsonDocument::Compact:QJsonDocument::Indented));
    file.close();return true;
}

bool Util::computeInterSection(Rect& r1,Rect& r2){
    int left=max(r1.x,r2.x);int right=min(r1.x+r1.width,r2.x+r2.width);
    int top=max(r1.y,r2.y);int bottom=min(r1.y+r1.height,r2.y+r2.height);
    int width=right-left;int height=bottom-top;
    if(width<=0||height<=0){return false;}

    r1=Rect(Point(left-r1.x,top-r1.y),Size(width,height));
    r2=Rect(Point(left-r2.x,top-r2.y),Size(width,height));
    return true;
}
