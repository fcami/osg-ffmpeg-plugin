```
#!c++
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
```

This works as a drop-in replacement for the ffmpeg plugin in the OpenSceneGraph source tree (3.2.x line). We find this version has fewer playback issues than the bundled plugin. One caveat being this plugin was designed around the ffmpeg fork as opposed to libav. It seems to work with certain versions of libav but ffmpeg is recommended. 

To build, simply rename the existing ffmpeg plugin in the OSG source (i.e. ffmpeg-back) tree and put this one in its place.