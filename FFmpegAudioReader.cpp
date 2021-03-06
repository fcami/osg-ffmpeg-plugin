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


#include "FFmpegAudioReader.hpp"
#include "FFmpegParameters.hpp"
#include <string>
#include <osg/Timer>

#include <stdexcept>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>

namespace osgFFmpeg {

AVRational osg_get_time_base_q(void);


const int
FFmpegAudioReader::openFile(const char *filename, FFmpegParameters * parameters)
{
    int                     err, i;
    AVInputFormat *         iformat     = NULL;
    AVDictionary *          format_opts = NULL;
    AVFormatContext *       fmt_ctx     = NULL;
    //
    //
    //
    m_audioStreamIndex                  = -1;
    m_output_buffer                     = NULL;
    m_reader_buffer_shift               = 0;
    m_output_buffer_length_prev         = 0;
    m_FirstFrame                        = true;
    m_input_currTime                    = 0.0;
#ifdef USE_SWRESAMPLE
    m_audio_swr_cntx                    = NULL;
#else
    m_audio_resample_cntx               = NULL;
    m_audio_intermediate_resample_cntx  = NULL;
#endif
    m_fmt_ctx_ptr                       = NULL;
    //
    //
    //
    if (std::string(filename).compare(0, 5, "/dev/")==0)
    {
#ifdef ANDROID

        av_log(NULL, AV_LOG_ERROR, "Device not supported on Android");

        return -1;
#else
        avdevice_register_all();

        if (parameters)
        {
            av_dict_set(parameters->getOptions(), "video_size", "640x480", 0);
            av_dict_set(parameters->getOptions(), "framerate", "30:1", 0);
        }

        std::string format = "video4linux2";
        iformat = av_find_input_format(format.c_str());

        if (iformat)
        {
            OSG_INFO<<"Found input format: "<<format<<std::endl;
        }
        else
        {
            OSG_INFO<<"Failed to find input format: "<<format<<std::endl;
        }

#endif
    }
    else
    {
        // todo: should be tested for case when \parameters has values
        iformat = parameters ? parameters->getFormat() : 0;
        AVIOContext* context = parameters ? parameters->getContext() : 0;
        if (context != NULL)
        {
            fmt_ctx = avformat_alloc_context();
            fmt_ctx->pb = context;
        }
    }
    //
    // Parse options
    //
    size_t                  threadNb = 0; // By default - autodetect thread number
    AVDictionaryEntry *     dictEntry;
    AVDictionary *          dict = *parameters->getOptions();
    dictEntry = NULL;
    while (dictEntry = av_dict_get(dict, "threads", dictEntry, 0))
    {
        threadNb = atoi(dictEntry->value);
    }

    if ((err = avformat_open_input(&fmt_ctx, filename, iformat, &format_opts)) < 0)
    {
        OSG_NOTICE << "Cannot open file " << filename << " for audio" << std::endl;

        return err;
    }
    //
    // Retrieve stream info
    // Only buffer up to one and a half seconds
    //
// see: https://ffmpeg.org/pipermail/ffmpeg-cvslog/2014-June/078216.html
// "New field int64_t max_analyze_duration2 instead of deprecated int max_analyze_duration."
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(55, 43, 100)
    fmt_ctx->max_analyze_duration2 = AV_TIME_BASE * 1.5f;
#else
    fmt_ctx->max_analyze_duration = AV_TIME_BASE * 1.5f;
#endif
    //
    // fill the streams in the format context
    //
// see: https://gitorious.org/ffmpeg/ffmpeg/commit/afe2726089a9f45d89e81217cd69505c14b94445
// "add avformat_find_stream_info()"
//??? not works: #if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 2, 0)
//
// Answer: http://sourceforge.net/p/cmus/mailman/message/28014386/
// "It seems ffmpeg development is completely mad, although their APIchanges file says
// avcodec_open2() is there from version 53.6.0 (and this is true for the git checkout),
// they somehow managed to not include it in their official 0.8.2 release, which has
// version 53.7.0 (!)."
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 5, 0)
    if ((err = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
        return err;
#else
    if ((err = av_find_stream_info(fmt_ctx)) < 0)
        return err;
#endif
    av_dump_format(fmt_ctx, 0, filename, 0);
    //
    // To find the first audio stream.
    //
    for (i = 0; i < (int)fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_audioStreamIndex = i;
            break;
        }
    }
    if (m_audioStreamIndex < 0)
    {
        av_log(NULL, AV_LOG_WARNING, "Opened file has not audio-streams");
        return -1;
    }
    AVCodecContext *pCodecCtx = fmt_ctx->streams[m_audioStreamIndex]->codec;
    // Check stream sanity
    if (pCodecCtx->codec_id == AV_CODEC_ID_NONE)
    {
        av_log(NULL, AV_LOG_ERROR, "Invalid audio codec");
        return -1;
    }

    AVCodec* codec = avcodec_find_decoder(pCodecCtx->codec_id);
    /**

    See: http://permalink.gmane.org/gmane.comp.video.libav.api/228
    From: http://comments.gmane.org/gmane.comp.video.libav.api/226

    Set AVCodecContext.thread_count to > 1 and libavcodec
    will use as many threads as specified. Thread types are controlled by
    AVCodecContext.thread_type, set this to FF_THREAD_FRAME for
    frame-threading (higher-latency, but scales better at more
    cores/cpus), or FF_THREAD_SLICE for slice-threading (lower-latency,
    but doesn't scale as well). Use frame for watching movies and slice
    for video-conferencing, basically. If you don't care which one it
    uses, set it to both (they're flags), and it'll autodetect which one
    is available and use the best one.
    **/
    pCodecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    pCodecCtx->thread_count = 1;
#ifdef USE_AV_LOCK_MANAGER
    pCodecCtx->thread_type = FF_THREAD_FRAME;
    pCodecCtx->thread_count = threadNb;
#endif // USE_AV_LOCK_MANAGER
// see: https://gitorious.org/libav/libav/commit/0b950fe240936fa48fd41204bcfd04f35bbf39c3
// "introduce avcodec_open2() as a replacement for avcodec_open()."
//??? not works: #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 5, 0)
//
// Answer: http://sourceforge.net/p/cmus/mailman/message/28014386/
// "It seems ffmpeg development is completely mad, although their APIchanges file says
// avcodec_open2() is there from version 53.6.0 (and this is true for the git checkout),
// they somehow managed to not include it in their official 0.8.2 release, which has
// version 53.7.0 (!)."
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 8, 0)
    if (avcodec_open2 (pCodecCtx, codec, NULL) < 0)
#else
    if (avcodec_open (pCodecCtx, codec) < 0)
#endif
    {
        av_log(NULL, AV_LOG_ERROR, "Could not open the required codec for audio");
        return -1;
    }

    m_fmt_ctx_ptr = fmt_ctx;
    //
    // Detect - is it source audio format planar?
    //
    m_isSrcAudioPlanar = false;
    {
#ifdef OSG_ABLE_PLANAR_AUDIO
        AVCodecContext *pCodecCtx   = m_fmt_ctx_ptr->streams[m_audioStreamIndex]->codec;
        if (av_sample_fmt_is_planar(pCodecCtx->sample_fmt) != 0)
            m_isSrcAudioPlanar = true;
#endif // OSG_ABLE_PLANAR_AUDIO
    }
    //
    // Define output sample format
    //
    m_outSampleFormat = pCodecCtx->sample_fmt;
    if (m_isSrcAudioPlanar)
    {
#ifdef OSG_ABLE_PLANAR_AUDIO
        // Set \m_outSampleFormat according to type of converting data in \dePlaneAudio()
        switch (pCodecCtx->sample_fmt)
        {
            case AV_SAMPLE_FMT_U8P:
                {
                    m_outSampleFormat = AV_SAMPLE_FMT_U8;
                    break;
                }
            case AV_SAMPLE_FMT_S16P:
                {
                    m_outSampleFormat = AV_SAMPLE_FMT_S16;
                    break;
                }
            case AV_SAMPLE_FMT_S32P:
                {
                    m_outSampleFormat = AV_SAMPLE_FMT_S32;
                    break;
                }
            case AV_SAMPLE_FMT_FLTP:
                {
                    m_outSampleFormat = AV_SAMPLE_FMT_FLT;
                    break;
                }
            case AV_SAMPLE_FMT_DBLP:
                {
                    m_outSampleFormat = AV_SAMPLE_FMT_DBL;
                    break;
                }
            default:
                {
                    av_log(NULL, AV_LOG_ERROR, "Unsupported of plane audio sample format");
                    return -1;
                }
        };
#endif // OSG_ABLE_PLANAR_AUDIO
    }

    // Get file size
/*
    {
        //std::ifstream::pos_type filesize(const char* filename)
        {
            int64_t size = bigFileSize(filename);
        }
    }
*/
    //
    return 0;
}

const bool
FFmpegAudioReader::isAudioPlanar () const
{
    return m_isSrcAudioPlanar;
}

// Audio should be planar
const int
FFmpegAudioReader::dePlaneAudio (const int & nb_samples, AVCodecContext * pCodecCtx, uint8_t **src_data)
{
    if (isAudioPlanar() == false)
        return -1;

#ifdef OSG_ABLE_PLANAR_AUDIO
    const int   plane_size  = calc_samples_get_buffer_size (nb_samples, pCodecCtx) / pCodecCtx->channels;
    //
    // Convert data according to format which set to \m_outSampleFormat
    //
    size_t      write_p = 0;
    size_t      nb;
    int         ch;
    switch (pCodecCtx->sample_fmt)
    {
    case AV_SAMPLE_FMT_U8P: // to AV_SAMPLE_FMT_U8
        {
            uint8_t *       out = (uint8_t *)m_decode_buffer;
            const size_t    nbmax = plane_size/sizeof(uint8_t);
            for (nb = 0; nb < nbmax; ++nb)
            {
                for (ch = 0; ch < pCodecCtx->channels; ++ch)
                {
                    out[write_p] = ((uint8_t *) src_data[ch])[nb];
                    ++write_p;
                }
            }
            break;
        }
    case AV_SAMPLE_FMT_S16P: // to AV_SAMPLE_FMT_S16
        {
            int16_t *       out = (int16_t *)m_decode_buffer;
            const size_t    nbmax = plane_size/sizeof(int16_t);
            for (nb = 0; nb < nbmax; ++nb)
            {
                for (ch = 0; ch < pCodecCtx->channels; ++ch)
                {
                    out[write_p] = ((int16_t *) src_data[ch])[nb];
                    ++write_p;
                }
            }
            break;
        }
    case AV_SAMPLE_FMT_S32P: // to AV_SAMPLE_FMT_S32
        {
            int32_t *       out = (int32_t *)m_decode_buffer;
            const size_t    nbmax = plane_size/sizeof(int32_t);
            for (nb = 0; nb < nbmax; ++nb)
            {
                for (ch = 0; ch < pCodecCtx->channels; ++ch)
                {
                    out[write_p] = ((int32_t *) src_data[ch])[nb];
                    ++write_p;
                }
            }
            break;
        }
    case AV_SAMPLE_FMT_FLTP: // to AV_SAMPLE_FMT_FLT
        {
            float *         out = (float *)m_decode_buffer;
            const size_t    nbmax = plane_size/sizeof(float);
            for (nb = 0; nb < nbmax; ++nb)
            {
                for (ch = 0; ch < pCodecCtx->channels; ++ch)
                {
                    out[write_p] = ((float *) src_data[ch])[nb];
                    ++write_p;
                }
            }
            break;
        }
    case AV_SAMPLE_FMT_DBLP: // to AV_SAMPLE_FMT_DBL
        {
            double *        out = (double *)m_decode_buffer;
            const size_t    nbmax = plane_size/sizeof(double);
            for (nb = 0; nb < nbmax; ++nb)
            {
                for (ch = 0; ch < pCodecCtx->channels; ++ch)
                {
                    out[write_p] = ((double *) src_data[ch])[nb];
                    ++write_p;
                }
            }
            break;
        }
    };
    return 0;
#endif // OSG_ABLE_PLANAR_AUDIO
    return -1;
}

const int
FFmpegAudioReader::calc_samples_get_buffer_size(const int & nb_samples, AVCodecContext * pCodecCtx)
{
// see: https://gitorious.org/libav/libav/commit/bbb46f3
// "Add av_samples_get_buffer_size(), av_samples_fill_arrays(), and av_samples_alloc(), to samplefmt.h."
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51, 18, 0)
    int         plane_size;
    return av_samples_get_buffer_size ( & plane_size, pCodecCtx->channels, nb_samples, pCodecCtx->sample_fmt, 1);
#else
    return nb_samples * av_get_bytes_per_sample(pCodecCtx->sample_fmt) * pCodecCtx->channels;
#endif
}

const int
FFmpegAudioReader::decodeAudio (int & buffer_size)
{
    AVCodecContext *pCodecCtx   = m_fmt_ctx_ptr->streams[m_audioStreamIndex]->codec;
#if LIBAVCODEC_VERSION_MAJOR >= 54
    AVFrame *       frame = OSG_ALLOC_FRAME();
    if (!frame)
        return -1;

    int             got_frame   = 0;
    int             result;

    result      = avcodec_decode_audio4 (pCodecCtx, frame, & got_frame, & m_packet);

    if (result >= 0 && got_frame) // if no errors
    {
        const int   data_size   = calc_samples_get_buffer_size(frame->nb_samples, pCodecCtx);
        if (buffer_size < data_size)
        {
            av_log(pCodecCtx, AV_LOG_ERROR, "output buffer size is too small for "
            "the current frame (%d < %d)\n", buffer_size, data_size);
            OSG_FREE_FRAME (& frame);
            return -1;
        }
        //
        // Resample planar/nonplanar audio
        //
        if (isAudioPlanar())
        {
            dePlaneAudio (frame->nb_samples, pCodecCtx, frame->extended_data);
        }
        else
        {
            memcpy (m_decode_buffer, frame->data[0], data_size);
        }
        buffer_size = data_size;
    }
    else
    {
        buffer_size = 0;
        if (result < 0)
            buffer_size = -1;
    }

    OSG_FREE_FRAME (& frame);

    return result;
#elif LIBAVCODEC_VERSION_MAJOR >= 53 || (LIBAVCODEC_VERSION_MAJOR==52 && LIBAVCODEC_VERSION_MINOR>=32)
    int ret_value;
    //
    if (isAudioPlanar())
    {
#ifdef OSG_ABLE_PLANAR_AUDIO
        ret_value = avcodec_decode_audio3(pCodecCtx, (int16_t*)m_decode_panar_buffer, & buffer_size, & m_packet);
        //
        if (ret_value > 0 && buffer_size > 0) // if no error
        {
            uint8_t *       ptrArray[128];
            uint8_t *       ptr = (uint8_t*) & m_decode_panar_buffer[0];
            const size_t    samples_nb = buffer_size / pCodecCtx->channels;
            for (int i = 0; i < pCodecCtx->channels; ++i)
            {
                ptrArray[i] = ptr;
                ptr += samples_nb;
            }
            dePlaneAudio (samples_nb, pCodecCtx, ptrArray);
        }
#else // OSG_ABLE_PLANAR_AUDIO
        av_log(NULL, AV_LOG_ERROR, "Decoded audio is planar but used version of libavutil does not support it");
        return -1;
#endif // OSG_ABLE_PLANAR_AUDIO
    }
    else
    {
        ret_value = avcodec_decode_audio3(pCodecCtx, (int16_t*)m_decode_buffer, & buffer_size, & m_packet);
    }
    if (ret_value < 0)
        buffer_size = -1;
    return ret_value;
#else
    //
    int ret_value;
    // Returns negative value if error,
    // otherwise the number of bytes used or zero if no frame could be decompressed.
    ret_value = avcodec_decode_audio2(pCodecCtx, (int16_t*)m_decode_buffer, & buffer_size, m_packet.data, m_packet.size);
    if (ret_value < 0)
        buffer_size = -1;
    return ret_value;
#endif
}

bool
FFmpegAudioReader::GetNextFrame(double & currTime, int16_t * output_buffer, unsigned int & output_buffer_size)
{
    int             bytesDecoded;
    int             buffer_size         = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    //
    // First time we're called, set m_packet.data to NULL to indicate it
    // doesn't have to be freed
    //
    if(m_FirstFrame)
    {
        m_FirstFrame=false;
        m_packet.data=NULL;
        m_bytesRemaining = 0;
    }
    //
    // Decode packets until we have decoded a complete frame
    //
    while (true)
    {
        // Work on the current packet until we have decoded all of it
        while(m_bytesRemaining > 0)
        {
            buffer_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;

            // Decode the next chunk of data
            bytesDecoded = decodeAudio (buffer_size);

            if (bytesDecoded < 0 || buffer_size == 0)
            {
                /* if error, we skip the frame */
                m_packet.size = 0;
                break;
            }

            m_bytesRemaining -= bytesDecoded;
            // Did we finish the current frame? Then we can return
            if (buffer_size > 0)
            {
                double pts = 0;

                if(m_packet.dts != AV_NOPTS_VALUE)
                    pts = m_packet.dts;

                pts *= av_q2d(m_fmt_ctx_ptr->streams[m_audioStreamIndex]->time_base);
#ifdef FFMPEG_DEBUG
                av_log(NULL, AV_LOG_DEBUG, "pts: %f", pts);
#endif // FFMPEG_DEBUG
                currTime = pts;
                memcpy (output_buffer, m_decode_buffer, buffer_size/*value of this variable in bytes*/);
                output_buffer_size = buffer_size;

                return true;
            }
            else
            {
                output_buffer_size = 0;
                // enf of stream
                return false;
            }
        }

        // Read the next packet, skipping all packets that aren't for this
        // stream
        do
        {
            // Free old packet
            if(m_packet.data != NULL)
                av_free_packet(&m_packet);

            // Read new packet
            const int readPacketRez = av_read_frame(m_fmt_ctx_ptr, &m_packet);

            if(readPacketRez < 0)
            {
                if (readPacketRez == static_cast<int>(AVERROR_EOF) ||
                    m_fmt_ctx_ptr->pb->eof_reached)
                {
                    // File(all streams) finished
                }
                else {
                    OSG_FATAL << "av_read_frame() returned " << AvStrError(readPacketRez) << std::endl;
                    throw std::runtime_error("av_read_frame() failed");
                }

                // End of stream. Done decoding.
                return false;
            }
        } while(m_packet.stream_index != m_audioStreamIndex);

        m_bytesRemaining=m_packet.size;
    }

    // Decode the rest of the last frame
    bytesDecoded = decodeAudio (buffer_size);

    // Free last packet
    if(m_packet.data!=NULL)
        av_free_packet(&m_packet);

    return false;
}

int
FFmpegAudioReader::seek(int64_t timestamp )
{
    AVCodecContext *pCodecCtx = m_fmt_ctx_ptr->streams[m_audioStreamIndex]->codec;
    avcodec_flush_buffers(pCodecCtx);
    const int seek_flags = (timestamp >= (int64_t)(m_input_currTime * 1000)) ? (AVSEEK_FLAG_ANY) : (AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
    //
    m_input_currTime = (double)timestamp / 1000.0;
    //
    // convert to AV_TIME_BASE
    //
    timestamp *= AV_TIME_BASE / 1000;

    //
    // add the stream start time
    if (m_fmt_ctx_ptr->start_time != AV_NOPTS_VALUE)
        timestamp += m_fmt_ctx_ptr->start_time;

    int64_t seek_target = av_rescale_q (timestamp,
                                        osg_get_time_base_q(),
                                        m_fmt_ctx_ptr->streams[m_audioStreamIndex]->time_base);

    m_FirstFrame = true;

    //
    //
    const int seekVal = av_seek_frame(m_fmt_ctx_ptr, m_audioStreamIndex, seek_target, seek_flags);
    if (seekVal < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot seek audio frame");
        return -1;
    }

    return 0;
}

const int
FFmpegAudioReader::getFrameRate(void) const
{
    AVCodecContext *pCodecCtx = m_fmt_ctx_ptr->streams[m_audioStreamIndex]->codec;
    return pCodecCtx->sample_rate;
}

const int
FFmpegAudioReader::getFrameSize(void) const
{
    AVCodecContext *pCodecCtx = m_fmt_ctx_ptr->streams[m_audioStreamIndex]->codec;
    return pCodecCtx->frame_size;
}

const int
FFmpegAudioReader::getChannels(void) const
{
    AVCodecContext *pCodecCtx = m_fmt_ctx_ptr->streams[m_audioStreamIndex]->codec;
    return pCodecCtx->channels;
}

const AVSampleFormat
FFmpegAudioReader::getSampleFormat(void) const
{
    return m_outSampleFormat;
}

const int
FFmpegAudioReader::getSampleSizeInBytes(void) const
{
    return av_get_bytes_per_sample(getSampleFormat());
}

void
FFmpegAudioReader::close(void)
{
    release_params_getSample();

// see: https://gitorious.org/ffmpeg/sastes-ffmpeg/commit/5266045
// "add avformat_close_input()."
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 17, 0)
    avformat_close_input(&m_fmt_ctx_ptr);
#else
    av_close_input_file(m_fmt_ctx_ptr);
#endif

}

const int64_t
FFmpegAudioReader::get_duration(void) const
{
    return m_fmt_ctx_ptr->duration * 1000 / AV_TIME_BASE; // milliseconds
}

void
FFmpegAudioReader::release_params_getSample(void)
{
#ifdef USE_SWRESAMPLE
    if (m_audio_swr_cntx)
    {
        swr_free( & m_audio_swr_cntx);
        m_audio_swr_cntx = NULL;
    }
#else
    if (m_audio_resample_cntx)
    {
        audio_resample_close(m_audio_resample_cntx);
        m_audio_resample_cntx = NULL;
    }
    if (m_audio_intermediate_resample_cntx)
    {
        audio_resample_close(m_audio_intermediate_resample_cntx);
        m_audio_intermediate_resample_cntx = NULL;
    }
#endif
    m_reader_buffer_shift = 0;
    if (m_output_buffer)
    {
        av_free (m_output_buffer);
        m_output_buffer = NULL;
    }
    m_output_buffer_length_prev = 0;
}

const int
FFmpegAudioReader::guessLayoutByChannelsNb(const int & chNb)
{
    int     layout = AV_CH_LAYOUT_STEREO;

    switch (chNb)
    {
    case 1:
        layout = AV_CH_LAYOUT_MONO;
        break;
    case 2:
        layout = AV_CH_LAYOUT_STEREO;
        break;
    case 6:
        layout = AV_CH_LAYOUT_5POINT1;
        break;
    };

    return layout;
}

const int
FFmpegAudioReader::getSamples(FFmpegAudioReader* input_audio,
                        unsigned long & startTimeMS,
                        unsigned short output_channels,
                        const AVSampleFormat & output_sampleFormat,
                        unsigned short & output_FrameRate,
                        unsigned long & samplesNb,
                        unsigned char * bufSamples,
                        const double & max_avail_time_micros)
{
    // Check initialization/fictive variant of calling this function
    if (output_channels == 0)
    {
        input_audio->release_params_getSample();
        return 0;
    }

    if (samplesNb > 32767)
    {
        //
        // If number of output-samples will be greater than 32767 it can occure situation when asked samples for
        // audio_resample() will be more than greatest-limit. So, here we predict this problem by limitation of
        // output samples.
        //
        return -1;
    }

    osg::Timer              loc_timer;
    const double            start_timer_micros      = loc_timer.time_u();
    //
    const int               input_FrameRate         = input_audio->getFrameRate();
    const int               input_Channels          = input_audio->getChannels();
    //
    unsigned int            output_buffer_size;
#ifdef USE_SWRESAMPLE
    AVCodecContext *        pCodecCtx               = input_audio->m_fmt_ctx_ptr->streams[input_audio->m_audioStreamIndex]->codec;
#endif
    //
    // Calculate required buffer-size [input_buffer_size] for DECODER
    //
    const AVSampleFormat    input_sampleFormat      = input_audio->getSampleFormat();
    const int               input_sampleformat_size = av_get_bytes_per_sample (input_sampleFormat);
    unsigned int            input_buffer_size = samplesNb * input_Channels * input_sampleformat_size;
    input_buffer_size *= (double)input_FrameRate / output_FrameRate;
    //
    // We should ensure that (input_buffer_size / channels / input_sampleformat_size) will have
    // correct info after rescale-operation [*= input_FrameRate / get_audio_sampleFrameRate()]
    // So here we should check:
    //
    if (input_buffer_size % (input_Channels * input_sampleformat_size) > 0)
    {
        const int shift = input_buffer_size % (input_Channels * input_sampleformat_size);
        input_buffer_size -= shift;
        input_buffer_size += input_Channels * input_sampleformat_size;
    }
    //
    // Allocate internal buffer for reading from the input-stream(decoding)
    //
    unsigned int            readed_from_decoder;
    const unsigned long     output_buffer_length = AVCODEC_MAX_AUDIO_FRAME_SIZE *
                                                input_sampleformat_size *
                                                input_Channels + input_buffer_size
                                                + FF_INPUT_BUFFER_PADDING_SIZE;
    //
    readed_from_decoder = input_audio->m_reader_buffer_shift;
    if (input_audio->m_output_buffer == NULL)
    {
        input_audio->m_output_buffer = (int16_t*)av_malloc(output_buffer_length);
        input_audio->m_output_buffer_length_prev = output_buffer_length;
    }
    else
    {
        if (output_buffer_length > input_audio->m_output_buffer_length_prev)
        {
            int16_t         * output_buffer_temp = (int16_t*)av_malloc(output_buffer_length);
            memmove(output_buffer_temp, input_audio->m_output_buffer, input_audio->m_output_buffer_length_prev);
            av_free (input_audio->m_output_buffer);
            input_audio->m_output_buffer = output_buffer_temp;
            input_audio->m_output_buffer_length_prev = output_buffer_length;
        }
    }
    while (true)
    {
        //
        // Read [output_buffer_size] bytes from the INPUT buffer to [m_output_buffer]
        //
        if (input_audio->GetNextFrame(input_audio->m_input_currTime,
                                        (int16_t*)(((unsigned char *)input_audio->m_output_buffer) + input_audio->m_reader_buffer_shift),
                                        output_buffer_size) == false)
        {
            // If this is not first call to GetNextFrame()
            if (readed_from_decoder > 0)
                input_buffer_size = readed_from_decoder;
            else
                break;
        }
        else
        {
            readed_from_decoder += output_buffer_size;
            input_audio->m_reader_buffer_shift += output_buffer_size;
        }
        //
        // If we decoded enough(more than required for encoder) bytes
        // or spent time more than available(if defined, i.e. \max_avail_time_micros > 0.0),
        // or cannot decode more ...
        //
        const double    ellapsed_time_micros = loc_timer.time_u() - start_timer_micros;
        if (input_audio->m_reader_buffer_shift >= input_buffer_size ||
            output_buffer_size == 0 ||
            (max_avail_time_micros > 0.0 && ellapsed_time_micros > max_avail_time_micros) )
        {
            const unsigned int  reader_buffer_shift_0   = input_audio->m_reader_buffer_shift;
            //
            // Cutoff encoded-bytes by limit of decoded-stream
            //
            if (input_buffer_size > readed_from_decoder)
                input_buffer_size = readed_from_decoder;

            int audio_resample_rc;
#ifdef USE_SWRESAMPLE
            const int output_layout                     = guessLayoutByChannelsNb(output_channels);
            // Fix when layout is not set.
            const int input_layout                      = pCodecCtx->channel_layout == 0 ? guessLayoutByChannelsNb(input_Channels) : pCodecCtx->channel_layout;
            //
            // For this version it is enough to use one resampler
            if (input_audio->m_audio_swr_cntx == NULL)
            {
                int er;
                input_audio->m_audio_swr_cntx = swr_alloc();
                if (input_audio->m_audio_swr_cntx == NULL)
                    return -1;
/*
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "in_channel_layout",  input_layout, 0);
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "out_channel_layout", output_layout,  0);
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "in_sample_rate",     input_FrameRate, 0);
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "out_sample_rate",    output_FrameRate, 0);
                er = av_opt_set_sample_fmt(input_audio->m_audio_swr_cntx, "in_sample_fmt",  input_sampleFormat, 0);
                er = av_opt_set_sample_fmt(input_audio->m_audio_swr_cntx, "out_sample_fmt", output_sampleFormat,  0);
*/
                int log_offset = 0;
                swr_alloc_set_opts(input_audio->m_audio_swr_cntx,
                                      output_layout, output_sampleFormat, output_FrameRate,
                                      input_layout, input_sampleFormat, input_FrameRate,
                                      log_offset, NULL);

                er = swr_init(input_audio->m_audio_swr_cntx);
                if (er != 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "Cannot init resampler for audio");
                    return -1;
                }
            }
            const unsigned long   nb_resample_samples = input_buffer_size / input_Channels / input_sampleformat_size;
            audio_resample_rc = swr_convert(input_audio->m_audio_swr_cntx,
                                    (uint8_t**)(&bufSamples),
                                    nb_resample_samples,
                                    (const uint8_t**)(&(input_audio->m_output_buffer)),
                                    nb_resample_samples);
#else
            if (input_audio->m_audio_resample_cntx == NULL)
            {
                if (input_Channels == 6 && (output_channels != 2 && output_channels != 6))
                {
                    input_audio->m_intermediate_channelsNb = 2;
                    input_audio->m_audio_intermediate_resample_cntx = av_audio_resample_init(input_audio->m_intermediate_channelsNb, input_Channels,
                                                                                             output_FrameRate, input_FrameRate,
                                                                                             output_sampleFormat,
                                                                                             input_sampleFormat,
                                                                                             16, 10, 1, 1.0);
                    input_audio->m_audio_resample_cntx = av_audio_resample_init(output_channels, input_audio->m_intermediate_channelsNb,
                                                                                 output_FrameRate, output_FrameRate,
                                                                                 output_sampleFormat,
                                                                                 output_sampleFormat,
                                                                                 16, 10, 1, 1.0);
                }
                else
                {
                    input_audio->m_audio_resample_cntx = av_audio_resample_init(output_channels, input_Channels,
                                                                                 output_FrameRate, input_FrameRate,
                                                                                 output_sampleFormat,
                                                                                 input_sampleFormat,
                                                                                 16, 10, 1, 1.0);
                }
            }
            if (input_audio->m_audio_intermediate_resample_cntx == NULL)
            {
                /* resample audio. 'nb_resample_samples' is the number of input samples */
                const unsigned long   nb_resample_samples = input_buffer_size / input_Channels / input_sampleformat_size;
                audio_resample_rc = audio_resample(input_audio->m_audio_resample_cntx,
                                                        (int16_t*)bufSamples,
                                                        (int16_t*)input_audio->m_output_buffer,
                                                        nb_resample_samples);
            }
            else
            {
                /* resample audio. 'nb_resample_samples' is the number of input samples */
                const unsigned long   nb_resample_intermediate_samples = input_buffer_size / input_Channels / input_sampleformat_size;
                unsigned char   *intermediate_buffer = (unsigned char*)av_malloc(samplesNb * input_audio->m_intermediate_channelsNb * input_sampleformat_size);
                int audio_resample_intermediate_rc = audio_resample(input_audio->m_audio_intermediate_resample_cntx,
                                                                    (int16_t*)intermediate_buffer,
                                                                    (int16_t*)input_audio->m_output_buffer,
                                                                    nb_resample_intermediate_samples);

                audio_resample_rc = audio_resample(input_audio->m_audio_resample_cntx,
                                                        (int16_t*)bufSamples,
                                                        (int16_t*)intermediate_buffer,
                                                        audio_resample_intermediate_rc);
                av_free (intermediate_buffer);
            }
#endif
            input_audio->m_reader_buffer_shift = reader_buffer_shift_0 - input_buffer_size;

            memmove (input_audio->m_output_buffer,
                    ((unsigned char*)input_audio->m_output_buffer) + input_buffer_size,
                    input_audio->m_reader_buffer_shift);

            return audio_resample_rc;
        }
    }
    return 0;
}

} // namespace osgFFmpeg
