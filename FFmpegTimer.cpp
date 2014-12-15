#include "FFmpegTimer.hpp"

namespace osgFFmpeg
{

FFmpegTimer::FFmpegTimer()
{
    Reset();
}

void
FFmpegTimer::Start()
{
    m_stop_time_ms = 0;

    // Make stop-time less than start-time

    m_timer.setStartTick(1);
    m_start_time_ms = m_timer.time_m();
}

void
FFmpegTimer::Stop()
{
    m_offset = ElapsedMilliseconds();

    if (m_stop_time_ms < m_start_time_ms)
        m_stop_time_ms = m_start_time_ms;
}

void
FFmpegTimer::Reset()
{
    m_start_time_ms = 0;
    m_stop_time_ms = 0;
    m_offset = 0;
    m_timer.setStartTick(0);
}

const unsigned long
FFmpegTimer::ElapsedMilliseconds() const
{
    unsigned long spent_time_ms = 0;
    if (m_stop_time_ms >= m_start_time_ms)
    {
        spent_time_ms = m_stop_time_ms - m_start_time_ms;
    }
    else
    {
        spent_time_ms = m_timer.time_m() - m_start_time_ms;
    }
    return m_offset + spent_time_ms;
}

void
FFmpegTimer::ElapsedMilliseconds(const unsigned long & ms)
{
    m_offset = ms;
}



} // namespace osgFFmpeg
