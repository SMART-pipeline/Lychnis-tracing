#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QVariantMap>
#include <opencv2/core.hpp>

class Util
{
public:
    Util();

    static bool loadJson(const QString &filePath,QVariantMap &params);
    static bool saveJson(const QString &filePath,const QVariantMap& params,bool bCompact=false);
    static bool computeInterSection(cv::Rect& r1,cv::Rect& r2);
};

#endif // UTIL_H
