#pragma once

#include <chrono>

class Timer  
{
public:
   Timer();
	double elapsedTime() const;

protected:
   std::chrono::time_point<std::chrono::system_clock> _startTime;
};

