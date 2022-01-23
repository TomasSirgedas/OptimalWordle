#include "Timer.h"

Timer::Timer()
{
   _startTime = std::chrono::system_clock::now();
}

double Timer::elapsedTime() const
{
   return std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::system_clock::now() - _startTime ).count() * 1e-9;
}
