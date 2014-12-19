#include "FFmpegAudioReader.hpp"
#include "FFmpegParameters.hpp"
#include <string>

namespace osgFFmpeg {

const int
FFmpegAudioReader::openFile(const char *filename, FFmpegParameters * parameters)
{
	int 				    err, i;
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
#if LIBAVCODEC_VERSION_MAJOR >= 56
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
        fprintf (stdout, "Device not supported on Android");
        return -1;
#else
        avdevice_register_all();

        if (parameters)
        {
            av_dict_set(parameters->getOptions(), "video_size", "640x480", 0);
            av_dict_set(parameters->getOptions(), "framerate", "1:30", 0);
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

	if ((err = avformat_open_input(&fmt_ctx, filename, iformat, &format_opts)) < 0)
	{
        OSG_NOTICE << "Cannot open file " << filename << " for audio" << std::endl;

		return err;
	}
    //
    // Retrieve stream info
    // Only buffer up to one and a half seconds
    //
#if LIBAVCODEC_VERSION_MAJOR >= 56
    fmt_ctx->max_analyze_duration2 = AV_TIME_BASE * 1.5f;
#else
    fmt_ctx->max_analyze_duration = AV_TIME_BASE * 1.5f;
#endif
    //
    // fill the streams in the format context
#if LIBAVFORMAT_VERSION_MAJOR >= 54
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
        fprintf(stderr, "Opened file has not audio-streams\n");
	    return -1;
	}
	AVCodecContext *pCodecCtx = fmt_ctx->streams[m_audioStreamIndex]->codec;
    // Check stream sanity
    if (pCodecCtx->codec_id == AV_CODEC_ID_NONE)
    {
        fprintf(stderr, "Invalid audio codec\n");
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
#if LIBAVCODEC_VERSION_MAJOR >= 54
	if (avcodec_open2 (pCodecCtx, codec, NULL) < 0)
#else
	if (avcodec_open (pCodecCtx, codec) < 0)
#endif
	{
        fprintf(stderr, "Could not open the required codec for audio\n");
	    return -1;
	}

	m_fmt_ctx_ptr = fmt_ctx;
    //
    // Detect - is it source audio format planar?
    //
    m_isSrcAudioPlanar = false;
    {
        AVCodecContext *pCodecCtx   = m_fmt_ctx_ptr->streams[m_audioStreamIndex]->codec;

#ifdef OSG_ABLE_PLANAR_AUDIO
        if (av_sample_fmt_is_planar(pCodecCtx->sample_fmt) != 0)
            m_isSrcAudioPlanar = true;
#else // OSG_ABLE_PLANAR_AUDIO
        if (pCodecCtx->sample_fmt > AV_SAMPLE_FMT_DBL &&
            pCodecCtx->sample_fmt != AV_SAMPLE_FMT_NB)
        {
            fprintf (stdout, "It seams that source audio sample format is planar, but used version of libavutil does not support it\n");
            m_isSrcAudioPlanar = true;
        }
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
                    fprintf (stdout, "Unsupported of plane audio sample format\n");
                    break;
                }
        };
#endif // OSG_ABLE_PLANAR_AUDIO
    }
    
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
#ifdef OSG_ABLE_PLANAR_AUDIO
    int         plane_size;
    return av_samples_get_buffer_size ( & plane_size, pCodecCtx->channels, nb_samples, pCodecCtx->sample_fmt, 1);
#else // OSG_ABLE_PLANAR_AUDIO
    return nb_samples * av_get_bytes_per_sample(pCodecCtx->sample_fmt) * pCodecCtx->channels;
#endif // OSG_ABLE_PLANAR_AUDIO
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
    do
    {
        result      = avcodec_decode_audio4 (pCodecCtx, frame, & got_frame, & m_packet);
    } while (result == AVERROR(EAGAIN) || !got_frame);

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
    }

    OSG_FREE_FRAME (& frame);

    return result;
#elif LIBAVCODEC_VERSION_MAJOR >= 53 || (LIBAVCODEC_VERSION_MAJOR==52 && LIBAVCODEC_VERSION_MINOR>=32)
    int loc_buffer_size;
    int ret_value;
    //
    if (isAudioPlanar())
    {
#ifdef OSG_ABLE_PLANAR_AUDIO
        do
        {
            loc_buffer_size = buffer_size; // value should be defined before calling \avcodec_decode_audio3()
            ret_value = avcodec_decode_audio3(pCodecCtx, (int16_t*)m_decode_panar_buffer, & loc_buffer_size, & m_packet);
        } while (!loc_buffer_size);
        //
        if (ret_value > 0) // if no error
        {
            uint8_t *       ptrArray[128];
            uint8_t *       ptr = (uint8_t*) & m_decode_panar_buffer[0];
            const size_t    samples_nb = loc_buffer_size / pCodecCtx->channels;
            for (int i = 0; i < pCodecCtx->channels; ++i)
            {
                ptrArray[i] = ptr;
                ptr += samples_nb;
            }
            dePlaneAudio (samples_nb, pCodecCtx, ptrArray);
        }
#else // OSG_ABLE_PLANAR_AUDIO
        fprintf (stdout, "ERROR: decoded audio is planar but used version of libavutil does not support it\n");
        return -1;
#endif // OSG_ABLE_PLANAR_AUDIO
    }
    else
    {
        do
        {
            loc_buffer_size = buffer_size; // value should be defined before
            ret_value = avcodec_decode_audio3(pCodecCtx, (int16_t*)m_decode_buffer, & loc_buffer_size, & m_packet);
        } while (!loc_buffer_size);
    }
    buffer_size = loc_buffer_size;
    return ret_value;
#else
    //
    int loc_buffer_size;
    int ret_value;
    do
    {
        loc_buffer_size = buffer_size;
        // Returns negative value if error,
        // otherwise the number of bytes used or zero if no frame could be decompressed.
        ret_value = avcodec_decode_audio2(pCodecCtx, (int16_t*)m_decode_buffer, & loc_buffer_size, m_packet.data, m_packet.size);
    } while (ret_value == 0);
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

            if (bytesDecoded < 0)
			{
                /* if error, we skip the frame */
                m_packet.size = 0;
                break;
            }

            m_bytesRemaining -= bytesDecoded;
            // Did we finish the current frame? Then we can return
            if (buffer_size)
            {
                double pts = 0;

                if(m_packet.dts != AV_NOPTS_VALUE)
                    pts = m_packet.dts;

                pts *= av_q2d(m_fmt_ctx_ptr->streams[m_audioStreamIndex]->time_base);
#ifdef FFMPEG_DEBUG
				fprintf (stdout, "pts: %f\n", pts);
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
FFmpegAudioReader::seek(int64_t timestamp)
{
	timestamp *= 1000;
    //
    // add the stream start time
    if (m_fmt_ctx_ptr->start_time != AV_NOPTS_VALUE)
        timestamp += m_fmt_ctx_ptr->start_time;

	int64_t 	    seek_target     = timestamp;
	{
		AVRational q;
		q.num = 1;
		q.den = AV_TIME_BASE;

        seek_target= av_rescale_q (timestamp, q, m_fmt_ctx_ptr->streams[m_audioStreamIndex]->time_base);
	}

    m_FirstFrame = true;

    //
    // For some AC3-audio seek by AVSEEK_FLAG_BACKWARD returns WRONG values.
    const int seekVal = av_seek_frame(m_fmt_ctx_ptr, m_audioStreamIndex, seek_target, AVSEEK_FLAG_ANY);
    if (seekVal < 0)
    {
        fprintf (stderr, "Cannot seek audio frame\n");
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

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 17, 0)
    avformat_close_input(&m_fmt_ctx_ptr);
#else
    av_close_input_file(m_fmt_ctx_ptr);
#endif

}

const int64_t
FFmpegAudioReader::get_duration(void) const
{
    return m_fmt_ctx_ptr->duration / 1000;
}

void
FFmpegAudioReader::release_params_getSample(void)
{
#if LIBAVCODEC_VERSION_MAJOR >= 56
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
                        unsigned char * bufSamples)
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

    //
    const int       		input_FrameRate         = input_audio->getFrameRate();
    const int				input_Channels          = input_audio->getChannels();
    //
    unsigned int    		output_buffer_size;
    double  				input_currTime;
#if LIBAVCODEC_VERSION_MAJOR >= 56
    AVCodecContext *        pCodecCtx               = input_audio->m_fmt_ctx_ptr->streams[input_audio->m_audioStreamIndex]->codec;
#endif
	//
	// Calculate required buffer-size [input_buffer_size] for DECODER
	//
    const AVSampleFormat	input_sampleFormat      = input_audio->getSampleFormat();
	const int 				input_sampleformat_size = av_get_bytes_per_sample (input_sampleFormat);
	unsigned int 			input_buffer_size = samplesNb * input_Channels * input_sampleformat_size;
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
	unsigned int			readed_from_decoder;
	const unsigned long		output_buffer_length = AVCODEC_MAX_AUDIO_FRAME_SIZE *
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
			int16_t			* output_buffer_temp = (int16_t*)av_malloc(output_buffer_length);
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
	    if (input_audio->GetNextFrame(input_currTime,
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
		// or cannot decode more ...
		//
		if (input_audio->m_reader_buffer_shift >= input_buffer_size || output_buffer_size == 0)
		{
			const unsigned int 	reader_buffer_shift_0 = input_audio->m_reader_buffer_shift;
			//
			// Cutoff encoded-bytes by limit of decoded-stream
			//
			if (input_buffer_size > readed_from_decoder)
				input_buffer_size = readed_from_decoder;

			int audio_resample_rc;
#if LIBAVCODEC_VERSION_MAJOR >= 56
            const int output_layout = guessLayoutByChannelsNb(output_channels);
            // Fix when layout is not set.
            const int input_layout = pCodecCtx->channel_layout == 0 ? guessLayoutByChannelsNb(input_Channels) : pCodecCtx->channel_layout;
            //
            // For this version it is enough to use one resampler
            if (input_audio->m_audio_swr_cntx == NULL)
            {
                int er;
                input_audio->m_audio_swr_cntx = swr_alloc();
                if (input_audio->m_audio_swr_cntx == NULL)
                    return -1;
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "in_channel_layout",  input_layout, 0);
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "out_channel_layout", output_layout,  0);
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "in_sample_rate",     input_FrameRate, 0);
                er = av_opt_set_int(input_audio->m_audio_swr_cntx, "out_sample_rate",    output_FrameRate, 0);
                er = av_opt_set_sample_fmt(input_audio->m_audio_swr_cntx, "in_sample_fmt",  input_sampleFormat, 0);
                er = av_opt_set_sample_fmt(input_audio->m_audio_swr_cntx, "out_sample_fmt", output_sampleFormat,  0);
                er = swr_init(input_audio->m_audio_swr_cntx);
                if (er != 0)
                {
                    fprintf (stderr, "Cannot init resampler for video\n");
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