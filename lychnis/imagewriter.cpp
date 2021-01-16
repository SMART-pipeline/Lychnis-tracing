#include "imagewriter.h"
//#include "lzwcodec.h"

#include <opencv2/opencv.hpp>
//#include <QDebug>
//#include <QElapsedTimer>
#include <zlib.h>
#include <thread>

#define MULTI_THREAD 0

namespace flsmio{

ImageWriter::ImageWriter(const QString &filePath, size_t imageNumber):m_dataStrip(512),m_compression(8){
    m_filePath=filePath;m_bDummpy=(imageNumber==1);if(m_bDummpy){return;}

    m_file=fopen(filePath.toStdString().c_str(),"wb");if(nullptr==m_file){return;}
    static const char header[]={0x49,0x49,0x2b,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    fwrite(header,16,1,m_file);m_totalOffset=16;
}

ImageWriter::~ImageWriter(){dumpImages();}

bool ImageWriter::isOpen(){return m_bDummpy||nullptr!=m_file;}

bool ImageWriter::addImage(const cv::Mat &image){
    if(m_bDummpy){return cv::imwrite(m_filePath.toStdString(),image);}
    if(image.empty()||!isOpen()){return false;}

    Image *ptr=new Image;ptr->size=image.size();ptr->compression=m_compression;
    int w=image.cols,h=std::min(m_dataStrip,image.rows);ptr->dataStrip=h;

    const size_t bufferSize=w*h*2;//QElapsedTimer timer;timer.start();
#if MULTI_THREAD
    static const int maxNumThreads=std::thread::hardware_concurrency()/2;
    int numStrips=ceil(image.rows/float(h)),numThreads=std::min(numStrips,maxNumThreads),
            numGroups=ceil(numStrips/float(numThreads));
    uchar *dst=(uchar*)malloc(bufferSize*numStrips);//qDebug()<<numStrips<<numThreads<<numGroups;
    int *dstLengths=new int[numStrips];size_t *dstDatas=new size_t[numStrips];

    QList<std::thread*> threads;
    for(int i=0;i<numThreads;i++){
        threads.append(new std::thread([i,numGroups,numStrips,bufferSize,h,dst,&image,dstLengths,dstDatas](){
            for(int j=0;j<numGroups;j++){
                int index=i*numGroups+j;if(index>=numStrips){break;}
                int row=index*h,h1=std::min(h,image.rows-row);
                size_t dstLen=bufferSize,srcLen=image.cols*h1*2,offset=index*bufferSize;
                uchar *pSrc=image.data+offset,*pDst=dst+offset;
                if(!LZWCodeC::compress(pSrc,srcLen,pDst,dstLen)){pDst=pSrc;dstLen=srcLen;}
                dstLengths[index]=dstLen;dstDatas[index]=(size_t)pDst;
            }
        }));
    }
    foreach(std::thread *t,threads){t->join();delete t;}
    for(int i=0;i<numStrips;i++){
        size_t dstLen=dstLengths[i];fwrite((void*)dstDatas[i],dstLen,1,m_file);
        ptr->stripOffsets.append(m_totalOffset);ptr->stripLengths.append(dstLen);m_totalOffset+=dstLen;
    }
    delete []dstLengths;delete []dstDatas;
#else
    uchar *pData=image.data,*dst=(uchar*)malloc(bufferSize);
    for(int i=0;i<image.rows;i+=h,pData+=bufferSize){
        int h1=std::min(h,image.rows-i);//size_t dstLen=bufferSize,srcLen=w*h1*2;
        unsigned long dstLen=bufferSize,srcLen=w*h1*2;
        if(8==m_compression){compress2(dst,&dstLen,pData,srcLen,4);}
        fwrite(dst,dstLen,1,m_file);

        //uchar *pDst=dst;if(!LZWCodeC::compress(pData,srcLen,dst,dstLen)){pDst=pData;dstLen=srcLen;}fwrite(pDst,dstLen,1,m_file);

        ptr->stripOffsets.append(m_totalOffset);ptr->stripLengths.append(dstLen);
        m_totalOffset+=dstLen;//qDebug()<<i<<dstLen;
    }
#endif
    //qDebug()<<timer.elapsed()<<"ms for writer";
    m_images.append(ptr);free(dst);return true;
}

void ImageWriter::dumpImages(){
    if(!isOpen()||m_bDummpy){return;}
    if(m_images.empty()){fclose(m_file);return;}

    static const char footer[]={
        0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//8 directory entries in total

        0x00,0x01,//1st,image width
        0x10,0x00,//unsigned short
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//just one number in this directory entry
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//value of image width

        0x01,0x01,//2nd,image height
        0x10,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//value of image height

        0x02,0x01,//3rd,color depth
        0x03,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//16

        0x03,0x01,//4th,compressed
        0x03,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//1->no,8->deflate

        0x06,0x01,//5th,invert color
        0x03,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//no

        0x16,0x01,//6th,strip
        0x10,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//image height too

        0x17,0x01,//7th,total byte number
        0x10,0x00,//unsigned int
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//2*width*height

        0x11,0x01,//8th,image data's relative position to the beginning of the file
        0x10,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//16,length of the header

        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//offset to next directory or zero
    };
    static const char description[]={
        0x0E,0x01,//9th,ImageDescription
        0x02,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    };
    const static QString metadata="<?xml version='1.0' encoding='UTF-8' standalone='no'?>"
                                  "<OME xmlns='http://www.openmicroscopy.org/Schemas/OME/2016-06' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' "
                                  "UUID='urn:uuid:4207e728-45ff-4f39-a4d8-556c1f90aa4c' "
                                  "xsi:schemaLocation='http://www.openmicroscopy.org/Schemas/OME/2016-06 http://www.openmicroscopy.org/Schemas/OME/2016-06/ome.xsd'>"
                                  "<Image ID='Image:0'>"
                                  "<Pixels DimensionOrder='XYCZT' ID='Pixels:0' SizeX='%1' SizeY='%2' SizeC='1' SizeZ='%3' SizeT='1' Type='uint16'>"
                                  "<TiffData/></Pixels></Image></OME>";

    const Image *lastPtr=m_images.last();

    int stripNumber=lastPtr->stripLengths.length();bool bStrip=stripNumber>1;

    size_t footerSize=sizeof(footer),bufferSize=footerSize+(bStrip?stripNumber*16:0);
    char *buffer=(char*)malloc(bufferSize);memcpy(buffer,footer,footerSize);

    *(size_t*)(buffer+20)=lastPtr->size.width;*(size_t*)(buffer+40)=lastPtr->size.height;
    *(size_t*)(buffer+80)=lastPtr->compression;*(size_t*)(buffer+120)=lastPtr->dataStrip;
    *(size_t*)(buffer+132)=stripNumber;*(size_t*)(buffer+152)=stripNumber;
    size_t *pNext=(size_t*)(buffer+168);//*pLength=(size_t*)(buffer+140),*pOffset=(size_t*)(buffer+160),

    size_t firstIfdPos=m_totalOffset;

    //add ImageDescription for the first image
    QString metadata1=metadata.arg(QString::number(lastPtr->size.width),QString::number(lastPtr->size.height),QString::number(m_images.length()));
    size_t descLength=metadata1.length()+1,firstBufferSize=bufferSize+20+descLength;//qDebug()<<footerSize<<descLength<<metadata1;

    char *firstBuffer=(char*)malloc(firstBufferSize);//sizeof(description)
    memcpy(firstBuffer,buffer,footerSize-8);memcpy(firstBuffer+footerSize-8,description,4);
    memcpy(firstBuffer+bufferSize+20,metadata1.toStdString().c_str(),descLength);

    firstBuffer[0]=0x09;*(size_t*)(firstBuffer+172)=descLength;
    size_t *pFirstDescription=(size_t*)(firstBuffer+180);
    bool bFirst=true;

    foreach(Image *ptr,m_images){
        char *buffer1=buffer;
        size_t footerSize1=footerSize,*pNext1=pNext,bufferSize1=bufferSize;
        if(bFirst){
            footerSize1=footerSize+20;pNext1=pFirstDescription+1;
            buffer1=firstBuffer;bufferSize1=firstBufferSize;
            *pFirstDescription=m_totalOffset+bufferSize1-descLength;bFirst=false;
        }

        size_t *pOffset=(size_t*)(buffer1+160),*pLength=(size_t*)(buffer1+140);

        if(bStrip){
            size_t *pStrip=(size_t*)(buffer1+footerSize1);//qDebug()<<ptr->stripOffsets;
            foreach(size_t v,ptr->stripOffsets){*pStrip=v;pStrip++;}
            foreach(size_t v,ptr->stripLengths){*pStrip=v;pStrip++;}
            *pOffset=m_totalOffset+footerSize1;*pLength=m_totalOffset+footerSize1+stripNumber*8;
        }else{*pLength=ptr->stripLengths[0];*pOffset=ptr->stripOffsets[0];}

        m_totalOffset+=bufferSize1;*pNext1=(lastPtr==ptr?0:m_totalOffset);
        fwrite(buffer1,bufferSize1,1,m_file);
    }

    _fseeki64(m_file,8,SEEK_SET);fwrite(&firstIfdPos,8,1,m_file);
    free(buffer);free(firstBuffer);qDeleteAll(m_images);fclose(m_file);
}

}
