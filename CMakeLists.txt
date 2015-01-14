INCLUDE_DIRECTORIES( ${FFMPEG_INCLUDE_DIRS} )
LINK_DIRECTORIES(${FFMPEG_LIBRARY_DIRS})
SET(TARGET_EXTERNAL_LIBRARIES ${FFMPEG_LIBRARIES} )


IF(FFMPEG_LIBSWSCALE_FOUND)

    INCLUDE_DIRECTORIES( ${FFMPEG_LIBSWSCALE_INCLUDE_DIRS} ${FFMPEG_LIBSWSCALE_INCLUDE_DIRS}/libswscale )

    ADD_DEFINITIONS(-DUSE_SWSCALE)

    SET(TARGET_EXTERNAL_LIBRARIES ${FFMPEG_LIBRARIES} ${FFMPEG_LIBSWSCALE_LIBRARIES})

ENDIF()

IF(FFMPEG_LIBSWRESAMPLE_FOUND)

    INCLUDE_DIRECTORIES( ${FFMPEG_LIBSWRESAMPLE_INCLUDE_DIRS} ${FFMPEG_LIBSWRESAMPLE_INCLUDE_DIRS}/libswresample )

    ADD_DEFINITIONS(-DUSE_SWRESAMPLE)

    SET(TARGET_EXTERNAL_LIBRARIES ${FFMPEG_LIBRARIES} ${FFMPEG_LIBSWRESAMPLE_LIBRARIES})

ENDIF()

ADD_DEFINITIONS(-D__STDC_CONSTANT_MACROS)

# MESSAGE("FFMPEG_LIBAVFORMAT_INCLUDE_DIRS = "  ${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS} )
# MESSAGE("FFMPEG_LIBAVDEVICE_INCLUDE_DIRS = "  ${FFMPEG_LIBAVDEVICE_INCLUDE_DIRS} )
# MESSAGE("FFMPEG_LIBAVCODEC_INCLUDE_DIRS = "  ${FFMPEG_LIBAVCODEC_INCLUDE_DIRS} )
# MESSAGE("FFMPEG_LIBAVUTIL_INCLUDE_DIRS = "  ${FFMPEG_LIBAVUTIL_INCLUDE_DIRS} )
# MESSAGE("FFMPEG_LIBRARIES = "  ${FFMPEG_LIBRARIES} )

SET(TARGET_SRC
    AudioBuffer.cpp
    FFmpegAudioReader.cpp
    FFmpegAudioStream.cpp
    FFmpegFileHolder.cpp
    FFmpegLibAvStreamImpl.cpp
    FFmpegParameters.cpp
    FFmpegPlayer.cpp
    FFmpegRenderThread.cpp
    FFmpegStreamer.cpp
    FFmpegTimer.cpp
    FFmpegVideoReader.cpp
    FFmpegWrapper.cpp
    ReaderWriterFFmpeg2.cpp
    VideoVectorBuffer.cpp
    System_routine.cpp
)

SET(TARGET_H
    AudioBuffer.hpp
    FFmpegAudioReader.hpp
    FFmpegAudioStream.hpp
    FFmpegFileHolder.hpp
    FFmpegHeaders.hpp
    FFmpegILibAvStreamImpl.hpp
    FFmpegLibAvStreamImpl.hpp
    FFmpegParameters.hpp
    FFmpegPlayer.hpp
    FFmpegRenderThread.hpp
    FFmpegStreamer.hpp
    FFmpegTimer.hpp
    FFmpegVideoReader.hpp
    FFmpegWrapper.hpp
    MessageQueue.hpp
    VideoVectorBuffer.hpp
)

IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
ENDIF()

#### end var setup  ###
SETUP_PLUGIN(ffmpeg2 ffmpeg2)

IF(MSVC)
    SET_TARGET_PROPERTIES(${TARGET_DEFAULT_PREFIX}ffmpeg2 PROPERTIES LINK_FLAGS "/OPT:NOREF")
ENDIF(MSVC)
