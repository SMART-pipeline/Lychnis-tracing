#ifndef IMAGEWRITER_H
#define IMAGEWRITER_H

#include <QList>
#include <opencv2/core.hpp>

namespace flsmio{

struct Image{
    cv::Size size;int compression,dataStrip;
    size_t dataOffset,dataLength;QList<size_t> stripFileOffsets,stripLengths,stripOffsets;
};

class ImageWriter
{
    bool m_bDummpy;QString m_filePath;
    FILE *m_file;size_t m_totalOffset;
    QList<Image*> m_images;const int m_dataStrip,m_compression;

    void dumpImages();
public:
    ImageWriter(const QString &filePath,size_t imageNumber=0);
    ~ImageWriter();

    bool isOpen();
    virtual bool addImage(const cv::Mat &image);
};

}

#endif // IMAGEWRITER_H
