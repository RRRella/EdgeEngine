#pragma once

#include <chrono>

using TimeStamp = std::chrono::time_point<std::chrono::system_clock>;	// TimeStamp != std::time_t
using Duration  = std::chrono::duration<float>;

class Timer
{
public:
	Timer();

	// returns the time duration between Start() and Now, minus the paused duration.
	float TotalTime() const;

	// returns the last delta time measured between Start() and Stop()
	float DeltaTime() const;
	float GetPausedTime() const;
	float GetStopDuration() const;	// gets (Now - stopTime)

	void Reset();
	void Start();
	void Stop();
	inline float StopGetDeltaTimeAndReset() { Stop(); float dt = DeltaTime(); Reset(); return dt; }

	// A Timer as to be updated by calling tick. 
	// Tick() will return the time duration since the last time Tick() is called.
	// First call will return the duration between Start() and Tick().
	float Tick();

private:
	TimeStamp baseTime;
	TimeStamp prevTime , currTime;
	TimeStamp startTime, stopTime;
	Duration  pausedTime;
	Duration  dt;
	bool      bIsStopped;
};

