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