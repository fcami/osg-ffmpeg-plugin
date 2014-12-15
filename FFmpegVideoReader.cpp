#include "FFmpegVideoReader.hpp"
#include "FFmpegParameters.hpp"
#include <string>

namespace osgFFmpeg {

#ifdef USE_SWSCALE    
    #define SWS_OSG_CONVERSION_TYPE     SWS_FAST_BILINEAR   // for case, when frames save his resolution, it is enough to use the fastest case of scaling.
#endif

const int
FFmpegVideoReader::openFile(const char *filename,
                            FFmpegParameters * parameters,
                            const bool useRGB_notBGR,
                            const long scaledWidth,
                            float & aspectRatio,
                            float & frame_rate,
                            bool & par_alphaChannel)
{
	int 				    err, i;
    AVInputFormat *         iformat     = NULL;
    AVDictionary *          format_opts = NULL;
	AVFormatContext *       fmt_ctx     = NULL;
    //
    //
    //
	m_videoStreamIndex                  = -1;
	m_FirstFrame                        = true;
	img_convert_ctx                     = NULL;
	m_pSeekFrame                        = NULL;
    m_pSrcFrame                         = NULL;
	m_is_video_duration_determined      = 0;
	m_video_duration                    = 0;
	m_pixelFormat                       = (useRGB_notBGR == true) ? PIX_FMT_RGB24 : PIX_FMT_BGR24;

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
		fprintf(stderr, "Cannot open file %s for video\n", filename);
		return err;
	}
    //
    // Retrieve stream info
    // Only buffer up to one and a half seconds
    //
    fmt_ctx->max_analyze_duration = AV_TIME_BASE * 1.5f;
	/* fill the streams in the format context */
	if ((err = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
	{
		fprintf(stderr, "%s\n", err);
		return err;
	}

	av_dump_format(fmt_ctx, 0, filename, 0);
    //
	// To find the first video stream.
    //
	for (i = 0; i < fmt_ctx->nb_streams; i++)
	{
		if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_videoStreamIndex = i;
			break;
		}
	}
	if (m_videoStreamIndex < 0)
	{
        fprintf(stderr, "Opened file has not video-streams\n");
	    return -1;
	}
	AVCodecContext *pCodecCtx = fmt_ctx->streams[m_videoStreamIndex]->codec;
    // Check stream sanity
    if (pCodecCtx->codec_id == AV_CODEC_ID_NONE)
    {
        fprintf(stderr, "Invalid video codec\n");
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
    //
	if (avcodec_open2(pCodecCtx, codec, NULL) < 0)
	{
        fprintf(stderr, "Could not open the required codec\n");
	    return -1;
	}

	m_fmt_ctx_ptr = fmt_ctx;
	m_pSeekFrame = avcodec_alloc_frame();
    m_pSrcFrame = avcodec_alloc_frame();
	//
    if (scaledWidth > 0)
    {
        //
        // Only if new size is less than actual size
        //
        if (scaledWidth < pCodecCtx->width)
        {
            const double    aspect_dwdh = (double)(pCodecCtx->width) / pCodecCtx->height;
            m_new_width = scaledWidth;
            m_new_height = m_new_width / aspect_dwdh;
        }
        else
        {
            //
            // Actual video size
            //
            m_new_width = pCodecCtx->width;
            m_new_height = pCodecCtx->height;
        }
    }
    else
    {
        //
        // Actual video size
        //
        m_new_width = pCodecCtx->width;
        m_new_height = pCodecCtx->height;
    }
    //
    // Initialize duration to avoid change seek/grab-position of file in future.
    // But this may take a some time during Open a file.
    //
    get_duration();
    //
    aspectRatio = findAspectRatio();
    frame_rate = get_fps();
    par_alphaChannel = alphaChannel();

    return 0;
}

void
FFmpegVideoReader::close(void)
{
    if(m_packet.data != NULL)
    {
        av_free_packet(&m_packet);
    }
	if (m_pSeekFrame)
	{
		av_freep(& m_pSeekFrame);
		m_pSeekFrame = NULL;
	}
    if (m_pSrcFrame)
    {
        av_freep(& m_pSrcFrame);
        m_pSrcFrame = NULL;
    }
#ifdef USE_SWSCALE    
	if (img_convert_ctx)
	{
        sws_freeContext(img_convert_ctx);
        img_convert_ctx = NULL;
	}
#endif
	if (m_fmt_ctx_ptr)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 17, 0)
        avformat_close_input(&m_fmt_ctx_ptr);
#else
        av_close_input_file(m_fmt_ctx_ptr);
#endif
	    m_fmt_ctx_ptr = NULL;
	}
}

float
FFmpegVideoReader::findAspectRatio() const
{
    float ratio = 0.0f;

    AVCodecContext *pCodecCtx = m_fmt_ctx_ptr->streams[m_videoStreamIndex]->codec;

    if (pCodecCtx->sample_aspect_ratio.num != 0)
        ratio = float(av_q2d(pCodecCtx->sample_aspect_ratio));

    if (ratio <= 0.0f)
        ratio = 1.0f;

    return ratio;
}

const bool
FFmpegVideoReader::alphaChannel() const
{
    AVCodecContext *pCodecCtx = m_fmt_ctx_ptr->streams[m_videoStreamIndex]->codec;

    // Find out whether we support Alpha channel
    return (pCodecCtx->pix_fmt == PIX_FMT_YUVA420P);
}

const float
FFmpegVideoReader::get_fps(void) const
{
    AVStream *      st      = m_fmt_ctx_ptr->streams[m_videoStreamIndex];

    // Find out the framerate
#if LIBAVCODEC_VERSION_MAJOR >= 56
    float           fps = av_q2d(st->avg_frame_rate);
#else
    float           fps = av_q2d(st->r_frame_rate);
#endif

    return fps;
}

const int
FFmpegVideoReader::get_width(void) const
{
    return m_new_width;
}

const int
FFmpegVideoReader::get_height(void) const
{
    return m_new_height;
}


const int64_t
FFmpegVideoReader::get_duration(void) const
{
	if (m_is_video_duration_determined == false)
	{
        const float         lastFrameTime_ms = 1.0 / get_fps() * 1000;
		m_video_duration = m_fmt_ctx_ptr->duration / 1000;
		//
		// Subtract last frame duration.
		//
        m_video_duration -= lastFrameTime_ms;
        //
		// Try seek
		//
		const int           w           = m_new_width;
		const int           h           = m_new_height;
        const int           bufSize     = avpicture_get_size(m_pixelFormat, w, h);
		unsigned char *     pBuf        = (unsigned char*)av_malloc (bufSize);
		FFmpegVideoReader * this_ptr    = const_cast<FFmpegVideoReader *>(this);
		int                 seek_rezult = this_ptr->seek(m_video_duration, pBuf);
		if (seek_rezult < 0)
		{
		    //
		    // Try use shadow-functionality of fundtion FFmpegVideoReader::seek(), when
		    // trying to seek last-position, iterator pass last frame(with last available timestamp)
		    //
			if (m_seekFoundLastTimeStamp == true)
			{
				m_video_duration = m_lastFoundInSeekTimeStamp_sec * 1000;
				seek_rezult = 0;
			}
		}
		if (seek_rezult < 0)
		{
			// If we cannot determine time by represented duration-value,
			// try to determine it by search of last-packet time
			//
			// Seek at start of video
			//
			seek_rezult = this_ptr->seek(0, pBuf);
			//
			// Start we should find guaranty.
			//
			if (seek_rezult >= 0)
			{
				AVPacket				packet;
				//
				packet.data = NULL;
				while (true)
				{
					// Free old packet
					if(packet.data != NULL)
						av_free_packet(& packet);

					bool existRezult;
					// Read new packet
					do
					{
						existRezult = true;
						const int readPacketRez = av_read_frame(m_fmt_ctx_ptr, & packet);
						if(readPacketRez < 0)
						{
							existRezult = false;
							break;
						}
					} while (packet.stream_index != m_videoStreamIndex);
					if (existRezult)
					{
						m_video_duration = (packet.dts * av_q2d(m_fmt_ctx_ptr->streams[m_videoStreamIndex]->time_base)) * 1000;
					}
					else
					{
						break;
					}
				}
				m_video_duration -= lastFrameTime_ms;
			}
		}
        //
        // Seek at start of video
        //
        seek_rezult = this_ptr->seek(0, pBuf);
		av_free (pBuf);
		m_is_video_duration_determined = true;
	}

    return m_video_duration;
}

bool
FFmpegVideoReader::GetNextFrame(AVCodecContext *pCodecCtx,
                                AVFrame *pFrame,
                                unsigned long & currPacketPos,
                                double & currTime)
{
    int                     bytesDecoded;
    int                     frameFinished;
    bool                    isDecodedData   = false;
    double                  pts             = 0;
    //
    // First time we're called, set m_packet.data to NULL to indicate it
    // doesn't have to be freed
    //
    if (m_FirstFrame)
    {
        m_FirstFrame = false;
        m_packet.data = NULL;
        m_bytesRemaining = 0;
    }

    // Decode packets until we have decoded a complete frame
    while(true)
    {
        // Work on the current packet until we have decoded all of it
        while (m_bytesRemaining > 0)
        {
            // Decode the next chunk of data
			bytesDecoded = avcodec_decode_video2 (pCodecCtx, pFrame, & frameFinished, & m_packet);

            // Was there an error?
            if(bytesDecoded < 0)
            {
                fprintf(stderr, "Error while decoding frame\n");
                return false;
            }
            //
            // We should check twice to search rezult, if first-time had not 100% successfull.
            // Some tests prove it.
            //
			if (frameFinished == 0)
			{
			    bytesDecoded = avcodec_decode_video2 (pCodecCtx, pFrame, &frameFinished, &m_packet);
			}

            m_bytesRemaining -= bytesDecoded;

            // Save global pts to be stored in pFrame in first call
            if (m_packet.dts == AV_NOPTS_VALUE
                && pFrame->opaque && *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE)
            {
                pts = *(uint64_t *)pFrame->opaque;
            }
            else
            {
                if(m_packet.dts != AV_NOPTS_VALUE)
                {
                    pts = m_packet.dts;
                }
                else
                {
                    pts = 0;
                }
            }

            pts *= av_q2d(m_fmt_ctx_ptr->streams[m_videoStreamIndex]->time_base);
        
            isDecodedData = true;

            // Did we finish the current frame? Then we can return
            if (frameFinished)
            {
#ifdef FFMPEG_DEBUG
		        fprintf (stdout, "pts: %f\n", pts);
#endif // FFMPEG_DEBUG
		        currTime = pts;
    	        return true;
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
#ifdef FFMPEG_DEBUG
			int64_t	l_pts = m_packet.pts;
			int64_t	l_dts = m_packet.dts;
			fprintf (stdout, "m_packet.pts = %lu; m_packet.dts = %lu; m_packet.pos = %lu;\n",
				(int)l_pts,
				(int)l_dts,
				(int)m_packet.pos);
#endif // FFMPEG_DEBUG
            currPacketPos = m_packet.pos;
            if(readPacketRez < 0)
                goto loop_exit;
        } while(m_packet.stream_index != m_videoStreamIndex);

        m_bytesRemaining = m_packet.size;
    }

loop_exit:

    // Decode the rest of the last frame
    bytesDecoded = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &m_packet);

    if (bytesDecoded > 0)
    {
        if(m_packet.dts == AV_NOPTS_VALUE &&
            pFrame->opaque && *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE)
        {
            pts = *(uint64_t *)pFrame->opaque;
        }
        else
        {
            if(m_packet.dts != AV_NOPTS_VALUE)
            {
                pts = m_packet.dts;
            }
            else
            {
                pts = 0;
            }
        }
        pts *= av_q2d(m_fmt_ctx_ptr->streams[m_videoStreamIndex]->time_base);
        //
        m_bytesRemaining -= bytesDecoded;
        isDecodedData = true;
    }
    // Free last packet
    if(m_packet.data!=NULL)
        av_free_packet(&m_packet);

    if (isDecodedData == false)
        frameFinished = 0;

    // Did we finish the current frame? Then we can return
    if(frameFinished!=0)
    {
        currTime = pts;
    }

    return frameFinished != 0;
}

int
FFmpegVideoReader::grabNextFrame(uint8_t * buffer, double & timeStampInSec)
{
    unsigned long       packetPos;
    int                 rezValue    = -1;
    AVCodecContext *    pCodecCtx   = m_fmt_ctx_ptr->streams[m_videoStreamIndex]->codec;
    AVFrame *           pFrameRGB   = avcodec_alloc_frame();
    if(pFrameRGB == NULL)
    {
        return -1;
    }

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill ((AVPicture *)pFrameRGB, buffer, m_pixelFormat, m_new_width, m_new_height);

    if (GetNextFrame(pCodecCtx, m_pSrcFrame, packetPos, timeStampInSec))
    {
    	// Convert image to necessary format
#ifdef USE_SWSCALE
		if(img_convert_ctx == NULL)
		{
            img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                            pCodecCtx->pix_fmt,
                                            m_new_width, m_new_height,
                                            m_pixelFormat,
                                            SWS_OSG_CONVERSION_TYPE, NULL, NULL, NULL);
            if(img_convert_ctx == NULL)
            {
                av_free(pFrameRGB);

                fprintf(stderr, "Cannot initialize the video conversion context\n");
                return -1;
            }
		}

		sws_scale(img_convert_ctx, m_pSrcFrame->data,
				  m_pSrcFrame->linesize, 0,
				  pCodecCtx->height,
				  pFrameRGB->data, pFrameRGB->linesize);
#else
        img_convert(pFrameRGB->data, m_pixelFormat, m_pSrcFrame->data,
                    pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
#endif

        rezValue = 0;
    }
    //
    av_free(pFrameRGB);

    return rezValue;
}

int
FFmpegVideoReader::seek(int64_t timestamp, unsigned char * ptrRGBmap)
{
    if (ptrRGBmap == NULL)
    {
        fprintf (stderr, "Parameter is NULL\n");
        return -1;
    }

	timestamp *= 1000;
	int             ret             = -1;
    const double    requiredTime    = timestamp / 1000000.0;
    //
    // add the stream start time
    if (m_fmt_ctx_ptr->start_time != AV_NOPTS_VALUE)
        timestamp += m_fmt_ctx_ptr->start_time;

	int64_t 	    seek_target     = timestamp;
	{
		AVRational a;
		a.num = 1;
		a.den = AV_TIME_BASE;

		seek_target= av_rescale_q (timestamp, a, m_fmt_ctx_ptr->streams[m_videoStreamIndex]->time_base);
	}

    m_FirstFrame = true;

	int             retValueSeekFrame = av_seek_frame (m_fmt_ctx_ptr, m_videoStreamIndex, seek_target, AVSEEK_FLAG_BACKWARD);
    //
	// Try to resolve TROUBLE by seeking with other flags.
	// When I want seek(0), AVSEEK_FLAG_BACKWARD returns ERROR, but AVSEEK_FLAG_ANY returns OK but not
	// actual time(not 0 but 0.0xx). It is better than nothing.
    //
	if (retValueSeekFrame < 0)
		 retValueSeekFrame = av_seek_frame(m_fmt_ctx_ptr, m_videoStreamIndex, seek_target, AVSEEK_FLAG_ANY);
    
    if (retValueSeekFrame >= 0)
	{
		AVCodecContext *    pCodecCtx = m_fmt_ctx_ptr->streams[m_videoStreamIndex]->codec;
        avcodec_flush_buffers(pCodecCtx);
        //
        unsigned long       packetPosLoop;
        double			    timeLoop;
        bool                bSuccess = GetNextFrame(pCodecCtx, m_pSeekFrame, packetPosLoop, timeLoop);
        double			    timeLoopPrev = timeLoop;
        if (bSuccess)
        {
            //
            // Sometime seek found position AFTER required (timeLoopPrev >= requiredTime),
            // and it will stock in some frames on the rezults.
            // It should be avoid by search BEFORE, when seek wrong.
            //
            while (timeLoopPrev > requiredTime && seek_target > 0)
            {
				AVRational q;
				q.num = 1;
				q.den = AV_TIME_BASE;

                // Go-back with 0.1-sec step till seek found WELL-rezults.
                seek_target -= av_rescale_q(1000000 * 0.1, q, m_fmt_ctx_ptr->streams[m_videoStreamIndex]->time_base);
                av_seek_frame(m_fmt_ctx_ptr, m_videoStreamIndex, seek_target, AVSEEK_FLAG_BACKWARD);
                bSuccess = GetNextFrame(pCodecCtx, m_pSeekFrame, packetPosLoop, timeLoop);
                timeLoopPrev = timeLoop;
            }
			m_seekFoundLastTimeStamp = false;
            while (true)
            {
				if ((timeLoopPrev <= requiredTime && timeLoop >= requiredTime)
                    || timeLoopPrev >= requiredTime)
				{
					break;
				}
				timeLoopPrev = timeLoop;
				//
                bSuccess = GetNextFrame(pCodecCtx, m_pSeekFrame, packetPosLoop, timeLoop);
                if (bSuccess == false)
                {
                	//
                	// To accelerate seach of last time-stamp(actual video duration)
                	// we save these values to use it in searching of video-duration
                	//
                	// For other cases, is will return error as before.
                	//
                	m_lastFoundInSeekTimeStamp_sec = timeLoopPrev;
                	m_seekFoundLastTimeStamp = true;
                	//
                	//
                	return -1;
                }
            }
            //
            //
            const int   rgbFrameSize    = avpicture_get_size(m_pixelFormat, m_new_width, m_new_height);
            AVFrame *   pFrameRGB       = avcodec_alloc_frame();
            if(pFrameRGB==NULL)
                return -1;
            // Determine required buffer size and allocate buffer
            uint8_t *   buffer          = (uint8_t*)av_malloc(rgbFrameSize);

            // Assign appropriate parts of buffer to image planes in pFrameRGB
            avpicture_fill((AVPicture *)pFrameRGB, buffer, m_pixelFormat, m_new_width, m_new_height);
            //
            // Convert the image into necessary format
#ifdef USE_SWSCALE
            if(img_convert_ctx == NULL)
            {
                img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                                pCodecCtx->pix_fmt,
                                                m_new_width, m_new_height,
                                                m_pixelFormat,
                                                SWS_OSG_CONVERSION_TYPE, NULL, NULL, NULL);
                if(img_convert_ctx == NULL)
                {
                    av_free(buffer);
                    av_free(pFrameRGB);

                    fprintf(stderr, "Cannot initialize the video conversion context!\n");
                    return -1;
                }
            }
            sws_scale(img_convert_ctx, m_pSeekFrame->data,
                      m_pSeekFrame->linesize, 0,
                      pCodecCtx->height,
                      pFrameRGB->data, pFrameRGB->linesize);
#else
        img_convert(pFrameRGB->data, m_pixelFormat, m_pSeekFrame->data,
                    pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
#endif
            memcpy (ptrRGBmap, pFrameRGB->data[0], rgbFrameSize);

            //
            //
            av_free(buffer);
            av_free(pFrameRGB);

            ret = 0;
        }
    }
    else
    {
    	fprintf (stderr, "Cannot seek video frame\n");
    }

	return ret;
}

} // namespace osgFFmpeg