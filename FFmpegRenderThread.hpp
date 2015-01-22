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


#ifndef HEADER_GUARD_FFMPEG_RENDERTHREAD_H
#define HEADER_GUARD_FFMPEG_RENDERTHREAD_H

#include <osg/ImageStream>
#include <OpenThreads/Thread>
#include "FFmpegFileHolder.hpp"

namespace osgFFmpeg {

class FFmpegILibAvStreamImpl;

class FFmpegRenderThread : protected OpenThreads::Thread
{
    osg::ImageStream            * m_pImgStream;
    FFmpegILibAvStreamImpl      * m_pLibAvStream;
    Size                        m_frameSize;
    volatile bool               m_renderingThreadStop;

    virtual void                run();
public:

    virtual                     ~FFmpegRenderThread();

    const int                   Initialize(FFmpegILibAvStreamImpl *, osg::ImageStream *, const Size &);

    void                        Start();
    void                        Stop();
    virtual void                quit(bool waitForThreadToExit = true);
};

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_RENDERTHREAD_H