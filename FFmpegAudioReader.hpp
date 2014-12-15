#ifndef HEADER_GUARD_FFMPEG_AUDIOREADER_H
#define HEADER_GUARD_FFMPEG_AUDIOREADER_H

#include "FFmpegHeaders.hpp"

namespace osgFFmpeg {

class FFmpegParameters;

class FFmpegAudioReader
{
private:
    int16_t *               m_output_buffer;
    unsigned char           m_intermediate_channelsNb;
    ReSampleContext *       m_audio_intermediate_resample_cntx;
    unsigned long	        m_output_buffer_length_prev;
    ReSampleContext *       m_audio_resample_cntx;
    unsigned int		    m_reader_buffer_shift;
    int8_t                  m_decode_buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE];
    //
    AVFormatContext *       m_fmt_ctx_ptr;
    short                   m_audioStreamIndex;
    bool                    m_FirstFrame;
    int                     m_bytesRemaining;
    AVPacket         	    m_packet;

    void                    release_params_getSample(void);
    bool                    GetNextFrame(double & currTime, int16_t * output_buffer, unsigned int & output_buffer_size);
public:
    const int               openFile(const char *filename, FFmpegParameters * parameters);
    int                     seek(int64_t timestamp);
    void                    close(void);

    const int   			getFrameRate(void) const;
    const int				getChannels(void) const;
    const AVSampleFormat	getSampleFormat(void) const;
    const int               getSampleSizeInBytes(void) const;
    const int				getFrameSize(void) const;
    // max value for using for seek
    const int64_t           get_duration(void) const;
    static const int        getSamples(FFmpegAudioReader* media,
                                        unsigned long & msTime,
                                        unsigned short channelsNb,
                                        const AVSampleFormat & output_sampleFormat,
                                        unsigned short & sample_rate,
                                        unsigned long & samplesNb,
                                        unsigned char * bufSamples);
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_AUDIOREADER_H