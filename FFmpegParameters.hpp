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



#ifndef HEADER_GUARD_OSGFFMPEG_FFMPEG_PARAMETERS_H
#define HEADER_GUARD_OSGFFMPEG_FFMPEG_PARAMETERS_H

#include "FFmpegHeaders.hpp"

#include <osg/Notify>


namespace osgFFmpeg {

const std::string AvStrError(int errnum);


class FFmpegParameters : public osg::Referenced
{
public:

    FFmpegParameters();
    ~FFmpegParameters();

    bool isFormatAvailable() const { return m_format!=NULL; }
    
    AVInputFormat* getFormat() { return m_format; }
    AVDictionary** getOptions() { return &m_options; }
    void setContext(AVIOContext* context) { m_context = context; }
    AVIOContext* getContext() { return m_context; }
    
    void parse(const std::string& name, const std::string& value);

protected:

    AVInputFormat* m_format;
    AVIOContext* m_context;
    AVDictionary* m_options;
};



} // namespace osgFFmpeg



#endif // HEADER_GUARD_OSGFFMPEG_FFMPEG_PARAMETERS_H
