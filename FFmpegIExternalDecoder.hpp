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


#ifndef HEADER_GUARD_FFMPEG_IEXTERNALDECODER_H
#define HEADER_GUARD_FFMPEG_IEXTERNALDECODER_H

#include "FFmpegHeaders.hpp"

namespace osgFFmpeg {

class FFmpegIExternalDecoder
{

public:

    virtual                     ~FFmpegIExternalDecoder() {}

    virtual AVCodec*            openCodec(AVCodecContext* pCodec, AVStream *) = 0;

    virtual const int           Convert(AVFrame * pSrcFrame, AVFrame * pDstFrame) = 0;

    virtual const AVPixelFormat GetOutPixFmt(void) const = 0;

}; // FFmpegIExternalDecoder

} // namespace osgFFmpeg

#endif // HEADER_GUARD_FFMPEG_IEXTERNALDECODER_H
