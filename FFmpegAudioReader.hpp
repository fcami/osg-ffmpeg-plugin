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


#ifndef HEADER_GUARD_FFMPEG_AUDIOREADER_H
#define HEADER_GUARD_FFMPEG_AUDIOREADER_H

#include "FFmpegHeaders.hpp"

namespace osgFFmpeg {

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif

class FFmpegParameters;

class FFmpegAudioReader
{
private:
    int16_t *               m_output_buffer;
    unsigned char           m_intermediate_channelsNb;
#ifdef USE_SWRESAMPLE
    SwrContext *            m_audio_swr_cntx;
#else
//
// From the sources of libavcodec conclude, that struct ReSampleContext available till
// major version of libavcodec less than 57. Moreover, ffmpeg suggests to use another library libswresample
// because ReSampleContext is deprecated and will be removed since major version of libavcodec 57.
//
// See: libavcodec\version.h:
//
// #ifndef FF_API_AUDIO_CONVERT
// #define FF_API_AUDIO_CONVERT     (LIBAVCODEC_VERSION_MAJOR < 57)
// #endif
// ...
// #ifndef FF_API_AVCODEC_RESAMPLE
// #define FF_API_AVCODEC_RESAMPLE  FF_API_AUDIO_CONVERT
// #endif
//
// See: libavcodec\avcodec.h:
//
// #if FF_API_AVCODEC_RESAMPLE
// /**
//  * @defgroup lavc_resample Audio resampling
//  * @ingroup libavc
//  * @deprecated use libswresample instead
//  *
//  * @{
//  */
// struct ReSampleContext;
// struct AVResampleContext;
//
// typedef struct ReSampleContext ReSampleContext;
// ...

    ReSampleContext *       m_audio_resample_cntx;
    ReSampleContext *       m_audio_intermediate_resample_cntx;
#endif
    unsigned long           m_output_buffer_length_prev;
    unsigned int            m_reader_buffer_shift;
    int8_t                  m_decode_buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE];
#ifdef OSG_ABLE_PLANAR_AUDIO
    int8_t                  m_decode_panar_buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE];
#endif
    //
    AVFormatContext *       m_fmt_ctx_ptr;
    short                   m_audioStreamIndex;
    bool                    m_FirstFrame;
    int                     m_bytesRemaining;
    AVPacket                m_packet;
    bool                    m_isSrcAudioPlanar;
    AVSampleFormat          m_outSampleFormat;
    double                  m_input_currTime;

    static const int        guessLayoutByChannelsNb(const int & chNb);
    static const int        calc_samples_get_buffer_size(const int & nb_samples, AVCodecContext * pCodecCtx);
    void                    release_params_getSample(void);
    const int               decodeAudio(int & buffer_size);
    const bool              isAudioPlanar () const;
    const int               dePlaneAudio (const int & nb_samples, AVCodecContext * pCodecCtx, uint8_t **src_data);
    bool                    GetNextFrame(double & currTime, int16_t * output_buffer, unsigned int & output_buffer_size);
public:
    const int               openFile(const char *filename, FFmpegParameters * parameters);
    int                     seek(int64_t timestamp);
    void                    close(void);

    const int               getFrameRate(void) const;
    const int               getChannels(void) const;
    const AVSampleFormat    getSampleFormat(void) const;
    const int               getSampleSizeInBytes(void) const;
    const int               getFrameSize(void) const;
    // max value for using for seek
    const int64_t           get_duration(void) const;
    static const int        getSamples(FFmpegAudioReader* media,
                                        unsigned long & msTime,
                                        unsigned short channelsNb,
                                        const AVSampleFormat & output_sampleFormat,
                                        unsigned short & sample_rate,
                                        unsigned long & samplesNb,
                                        unsigned char * bufSamples,
                                        const double & max_avail_time_micros);
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_AUDIOREADER_H
