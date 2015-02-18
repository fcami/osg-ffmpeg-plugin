/* Improved ffmpeg plugin for OpenSceneGraph -
 * Copyright (C) 2014-2015 Digitalis Education Solutions, Inc. (http://DigitalisEducation.com)
 * File author: Oktay Radzhabov (oradzhabov at jazzros dot com)
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/


#ifndef HEADER_GUARD_FFMPEG_VIDEOREADER_H
#define HEADER_GUARD_FFMPEG_VIDEOREADER_H

#include "FFmpegHeaders.hpp"
#include "FFmpegIExternalDecoder.hpp"

namespace osgFFmpeg {

class FFmpegParameters;

class FFmpegVideoReader
{
private:
    bool                m_FirstFrame;
    int                 m_bytesRemaining;
#ifdef USE_SWSCALE
    struct SwsContext * img_convert_ctx;
#endif
    AVRational          m_framerate;
    AVPacket            m_packet;
    AVFrame *           m_pSeekFrame;
    AVFrame *           m_pSrcFrame;
    AVPixelFormat       m_pixelFormat;
    mutable bool        m_is_video_duration_determined;
    mutable int64_t     m_video_duration;
    const bool          m_use_actual_duration;
    double              m_lastFoundInSeekTimeStamp_sec;
    bool                m_seekFoundLastTimeStamp;
    FFmpegIExternalDecoder * m_pExtDecoder;

    unsigned int        m_new_width;
    unsigned int        m_new_height;

    // - [minReqTimeMS] - if greater than 0, it is minimal time which will be searched to return frame.
    //  If negative, then next frame will be returned. Another words, if [minReqTimeMS]>=0, then [timeStampInSec]
    //  will be eq or greater than [minReqTimeMS]
    // - [decodeTillMinReqTime] - if false, then during searching to [minReqTimeMS], packets will not be decoded.
    //  It is fast but frame will be with artifacts. If true - then no artifacts, but slowly.
    //  Has not depending, if [minReqTimeMS] < 0.
    bool                GetNextFrame(AVCodecContext *pCodecCtx, AVFrame *pFrame, unsigned long & currPacketPos, double & currTime, const size_t & drop_frame_nb = 0, const bool decodeTillMinReqTime = true, const double & minReqTimeMS = -1.0);
    const int           ConvertToRGB(AVFrame * pSrcFrame, uint8_t * prealloc_buffer, unsigned char * ptrRGBmap);
public:
    AVFormatContext *   m_fmt_ctx_ptr;
    short               m_videoStreamIndex;

                        FFmpegVideoReader();
    //
    // Search index of video-stream. If no one video-stream had not been found, return error.
    const int           openFile(const char *filename, FFmpegParameters * parameters, float & aspectRatio, float & frame_rate, bool & alphaChannel);
    void                close(void);
    int                 seek(int64_t timestamp, unsigned char * ptrRGBmap);
    // buffer-size should be width*height*3 bytes;
    // - [minReqTimeMS] - if greater than 0, it is minimal time which will be searched to return frame.
    //  If negative, then next frame will be returned. Another words, if [minReqTimeMS]>=0, then [timeStampInSec]
    //  will be eq or greater than [minReqTimeMS]
    int                 grabNextFrame(uint8_t * buffer, double & timeStampInSec, const size_t & drop_frame_nb, const bool decodeTillMinReqTime = true, const double & minReqTimeMS = -1.0);
    //
    //
    //
    // type(int) derived from libavcodec.
    const int           get_width(void) const;
    const int           get_height(void) const;
    const float         get_fps(void) const;
    const AVPixelFormat getPixFmt(void) const;
    // max value for using in seek
    const int64_t       get_duration(void) const;
    float               findAspectRatio() const;
    const bool          alphaChannel() const;

};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_VIDEOREADER_H
