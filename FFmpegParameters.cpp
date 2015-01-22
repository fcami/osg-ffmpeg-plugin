/* Improved ffmpeg plugin for OpenSceneGraph - 
 * Copyright (C) 1998-2010 Robert Osfield
 * Modifications copyright (C) 2014-2015 Digitalis Education Solutions, Inc. (http://DigitalisEducation.com)
 * Modification author: Oktay Radzhabov (oradzhabov at jazzros dot com)
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



#include "FFmpegParameters.hpp"

#include <string>
#include <iostream>
#include <sstream>

#if LIBAVCODEC_VERSION_MAJOR >= 53
extern "C"
{
    #include <libavutil/parseutils.h>
}
#define av_parse_video_frame_size av_parse_video_size
#define av_parse_video_frame_rate av_parse_video_rate
#endif

extern "C"
{
    #include <libavutil/pixdesc.h>
}

inline PixelFormat osg_av_get_pix_fmt(const char *name) { return av_get_pix_fmt(name); }


namespace osgFFmpeg {


const std::string AvStrError(int errnum)
{
    char buf[128];
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

FFmpegParameters::FFmpegParameters() :
    m_format(0),
    m_context(0),
    m_options(0)
{
    // Initialize the dictionary
    av_dict_set(&m_options, "foo", "bar", 0);
}

FFmpegParameters::~FFmpegParameters()
{
    av_dict_free(&m_options);
}


void FFmpegParameters::parse(const std::string& name, const std::string& value)
{
    if (value.empty())
    {
        return;
    }
    if (name == "format")
    {
#ifndef ANDROID
        avdevice_register_all();
#endif
        m_format = av_find_input_format(value.c_str());
        if (!m_format)
            OSG_NOTICE<<"Failed to apply input video format: "<<value.c_str()<<std::endl;
    }
    else if (name == "frame_rate")
        av_dict_set(&m_options, "framerate", value.c_str(), 0);
    else
        av_dict_set(&m_options, name.c_str(), value.c_str(), 0);
}



} // namespace osgFFmpeg
