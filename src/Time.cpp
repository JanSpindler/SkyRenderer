#include <engine/util/Time.hpp>

namespace en
{
    std::chrono::time_point<std::chrono::high_resolution_clock> Time::m_Last = std::chrono::high_resolution_clock::now();
    double Time::m_DeltaTime = 0.0;
    
    void Time::Update()
    {
        std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
        std::chrono::nanoseconds delta = now - m_Last;
        m_Last = now;
        m_DeltaTime = (double)delta.count() / 1000000000.0;
    }

    double Time::GetDeltaTime()
    {
        return m_DeltaTime;
    }
}
