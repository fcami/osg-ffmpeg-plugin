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


#ifndef HEADER_GUARD_FFMPEG_WRAPPER_H
#define HEADER_GUARD_FFMPEG_WRAPPER_H

#include <map>
#include "FFmpegHeaders.hpp"

namespace osgFFmpeg {

class FFmpegParameters;
class FFmpegVideoReader;
class FFmpegAudioReader;

#define FFMPEGAUDIOREADER FFmpegAudioReader

class FFmpegWrapper
{
private:

    static  long                                        g_maxIndexVideoFiles;
    static  std::map<const long, FFmpegVideoReader*>    g_openedVideoFiles;
    static  long                                        g_maxIndexAudioFiles;
    static  std::map<const long, FFMPEGAUDIOREADER*>    g_openedAudioFiles;

    static const short  checkIndexVideoValid(const long indexFile);
    static const short  checkIndexAudioValid(const long indexFile);


public:
    /// =======================================================================================================================================
    /// Access to read video
    /// =======================================================================================================================================
    //
    // Open Video-file(only for video-stream)
    //
    // return values
    // -1: error;
    // (0..N): Index of opened video-file;
    //
    // Notes:
    // - No one exception throws from function;
    // - If [useRGB_notBGR] == true, all Video-data will be represented as RGB24, otherwise as BGR24
    // - If opened media-file has not video-stream, return error;
    static const long  openVideo(const char * pFileName, FFmpegParameters * parameters, const bool useRGB_notBGR, float & aspectRatio, float & frame_rate, bool & alphaChannel);

    // Close Video-file(opened by [openVideo])
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // [indexFile] should be get from [openVideo];
    // - No one exception throws from function;
    static const short closeVideo(const long indexFile);

    // Return short-values [width],[height] of the video-picture;
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // - No one exception throws from function;
    static const short getImageSize(const long indexFile, unsigned short *);

    // Get unsigned long in milliseconds [start position][end position] of the video;
    // General duration is: [end position] - [start position];
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // - For eaxmple: 3 min 1 sec 567 ms = (3 * 60 + 1.567) * 1000
    // - No one exception throws from function;
    static const short getVideoTimeLimits(const long indexFile, unsigned long *);

    // Get image(24-bit) from the video-file by selected time-position;
    // Random-access cost huge time-resources(test shows: 300-500 ms for search/construct required frame)
    //
    // For access to color/byte-components after using this function:
    // RGB-case:
    // RED (x,y):   bufRGB24[y*3*width + 3*x + 0];
    // GREEN (x,y): bufRGB24[y*3*width + 3*x + 1];
    // BLUE (x,y):  bufRGB24[y*3*width + 3*x + 2];
    // BGR-case:
    // BLUE (x,y):  bufRGB24[y*3*width + 3*x + 0];
    // GREEN (x,y): bufRGB24[y*3*width + 3*x + 1];
    // RED (x,y):   bufRGB24[y*3*width + 3*x + 2];
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // - No one exception throws from function;
    // - Size of [bufRGB24] should be at less = (3 * width * height) in bytes;
    // - If [msTime] is out of [getVideoTimeLimits] it will return error without any processing;
    static const short getImage(const long indexFile, unsigned long msTime, unsigned char * bufRGB24);

    // Get next frame(24-bit) from the video-file;
    //
    // For access to color/byte-components after using this function:
    // RGB-case:
    // RED (x,y):   bufRGB24[y*3*width + 3*x + 0];
    // GREEN (x,y): bufRGB24[y*3*width + 3*x + 1];
    // BLUE (x,y):  bufRGB24[y*3*width + 3*x + 2];
    // BGR-case:
    // BLUE (x,y):  bufRGB24[y*3*width + 3*x + 0];
    // GREEN (x,y): bufRGB24[y*3*width + 3*x + 1];
    // RED (x,y):   bufRGB24[y*3*width + 3*x + 2];
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // - No one exception throws from function;
    // - Size of [bufRGB24] should be at less = (3 * width * height) in bytes;
    // - [timeStampInSec] - output value is current time-position of returned frame in seconds;
    // - [minReqTimeMS] - if greater than 0, it is minimal time which will be searched to return frame.
    //  If negative, then next frame will be returned. Another words, if [minReqTimeMS]>=0, then [timeStampInSec]
    //  will be eq or greater than [minReqTimeMS]
    // - [decodeTillMinReqTime] - if false, then during searching to [minReqTimeMS], packets will not be decoded.
    //  It is fast but frame will be with artifacts. If true - then no artifacts, but slowly.
    //  Has not depending, if [minReqTimeMS] < 0.
    static const short getNextImage(const long indexFile, unsigned char * bufRGB24, double & timeStampInSec, const size_t & drop_frame_nb, const bool decodeTillMinReqTime = true, const double minReqTimeMS = -1.0);

    /// =======================================================================================================================================
    /// Access to read audio
    /// =======================================================================================================================================
    //
    // Open Audio-file(only for audio-stream)
    //
    // return values
    // -1: error;
    // (0..N): Index of opened audio-file;
    //
    // Notes:
    // - No one exception throws from function;
    // - If opened media-file has not audio-stream, return error;
    // - Automatic initialization of stream-grabber(getAudioSamples) after successful seeking by ZERO.
    static const long  openAudio(const char * pFileName, FFmpegParameters * parameters);

    // Notes:
    // - No one exception throws from function;
    // - Automatic initialization of stream-grabber(getAudioSamples) after successful seeking.
    static const short seekAudio(const long indexFile, const unsigned long);

    // Get unsigned long in milliseconds [start position][end position] of the audio;
    // General duration is: [end position] - [start position];
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // - No one exception throws from function;
    // - 3 min 1 sec 567 ms = (3 * 60 + 1.567) * 1000
    static const short getAudioTimeLimits(const long indexFile, unsigned long *);

    // Get Info about opened Audio
    // data[0] = number of bytes for sizeof(sample). For example: 16-bit audio return 2;
    // data[1] = number of channels. For Stereo return 2, for Mono return 1;
    // data[2] = sample rate. For emaple: 44100, 22050, 11025...
    // data[3] = (int)AVSampleFormat
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // - No one exception throws from function;
    static const short getAudioInfo(const long indexFile, unsigned long * data);

    // Close Audio-file(opened by [openAudio])
    //
    // return values
    // 0: No errors
    // other: error
    //
    // Notes:
    // [indexFile] should be get from [openAudio];
    // - No one exception throws from function;
    static const short closeAudio(const long indexFile);

    // Streaming-grabbing of required number of audio-samples from the opened audio-file.
    // Each calling this function will start of grabbing from the previous end-point.
    // If you need start of grabbing from the custom point, you should use seekAudio().
    //
    // Fictive calling this function with [channelsNb] equal to ZERO for initialization occured by
    // seekAudio() and openAudio() automatically.
    //
    //
    // 0 < [samplesNb] < 32768.
    //
    // if [max_avail_time_micros] <= 0.0, then no effect. Otherwise: this is max time(in microsec) for grabbing audio.
    // For time-critical cases, it is not necessary grab required number of samples. It could be less.
    //
    // return values
    // (0..N): number of grabbed samples;
    // -1: error;
    //
    // Notes:
    // - No one exception throws from function;
    // - [msTime] - Fictive parameter. Does not use;
    // - size of bufSamples(in bytes) should be at less = [samplesNb]*[channelsNb]*[sizeofSample/*according to \output_sampleFormat*/]
    // #warning TODO (ROS#1#): When grab some files (example: Bunny.ogg) first grab will return 0 but should not.
    static const int getAudioSamples(const long indexFile,
                                        unsigned long msTime,
                                        unsigned short channelsNb,
                                        const AVSampleFormat & output_sampleFormat,
                                        unsigned short sample_rate,
                                        unsigned long samplesNb,
                                        unsigned char * bufSamples,
                                        const double & max_avail_time_micros);
};
} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_WRAPPER_H
