#ifndef HEADER_GUARD_FFMPEG_HEADERS_H
#define HEADER_GUARD_FFMPEG_HEADERS_H

#include <osg/Notify>

extern "C"
{
#define FF_API_OLD_SAMPLE_FMT 0
#include <errno.h>    // for error codes defined in avformat.h
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifndef ANDROID
#include <libavdevice/avdevice.h>
#endif

#include <libavutil/mathematics.h>

#ifdef USE_SWSCALE    
    #include <libswscale/swscale.h>
#endif

// todo: this lib should be added to CMake
#if LIBAVCODEC_VERSION_MAJOR >= 56
#include <libswresample/swresample.h>
#endif

#if LIBAVUTIL_VERSION_INT <  AV_VERSION_INT(50,38,0)
#define AV_SAMPLE_FMT_NONE SAMPLE_FMT_NONE
#define AV_SAMPLE_FMT_U8   SAMPLE_FMT_U8
#define AV_SAMPLE_FMT_S16  SAMPLE_FMT_S16
#define AV_SAMPLE_FMT_S32  SAMPLE_FMT_S32
#define AV_SAMPLE_FMT_FLT  SAMPLE_FMT_FLT
#define AV_SAMPLE_FMT_DBL  SAMPLE_FMT_DBL
#define AV_SAMPLE_FMT_NB   SAMPLE_FMT_NB
#endif


// Changes for FFMpeg version greater than 0.6
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 64, 0)
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#endif

#ifdef AVERROR
#define AVERROR_IO AVERROR(EIO)
#define AVERROR_NUMEXPECTED AVERROR(EDOM)
#define AVERROR_NOMEM AVERROR(ENOMEM)
#define AVERROR_NOFMT AVERROR(EILSEQ)
#define AVERROR_NOTSUPP AVERROR(ENOSYS)
#define AVERROR_NOENT AVERROR(ENOENT)
#endif

#if LIBAVCODEC_VERSION_MAJOR < 56
   #define AV_CODEC_ID_NONE CODEC_ID_NONE
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 55
    #define OSG_ALLOC_FRAME     av_frame_alloc
    #define OSG_FREE_FRAME      av_frame_free
#else
    #define OSG_ALLOC_FRAME     avcodec_alloc_frame
    #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 59, 100)
        #define OSG_FREE_FRAME      avcodec_free_frame
    #else
        #define OSG_FREE_FRAME      av_freep
    #endif
#endif

}

class FormatContextPtr
{
    public:
    
        typedef AVFormatContext T;
    
        explicit FormatContextPtr() : _ptr(0) {}
        explicit FormatContextPtr(T* ptr) : _ptr(ptr) {}
        
        ~FormatContextPtr()
        {
            cleanup();
        }
        
        T* get() { return _ptr; }

        operator bool() const { return _ptr != 0; }

        T * operator-> () const // never throws
        {
            return _ptr;
        }

        void reset(T* ptr) 
        {
            if (ptr==_ptr) return;
            cleanup();
            _ptr = ptr;
        }

        void cleanup()
        {
            if (_ptr) 
            {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 17, 0)
                OSG_NOTICE<<"Calling avformat_close_input("<<&_ptr<<")"<<std::endl;
                avformat_close_input(&_ptr);
#else
                OSG_NOTICE<<"Calling av_close_input_file("<<_ptr<<")"<<std::endl;
                av_close_input_file(_ptr);
#endif
            }
            _ptr = 0;
        }
        
        

    protected:
    
        T* _ptr;
};


#endif // HEADER_GUARD_FFMPEG_HEADERS_H
