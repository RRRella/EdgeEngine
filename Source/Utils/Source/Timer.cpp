#include "Timer.h"

Timer::Timer()
	:
	bIsStopped(true)
{
	Reset();
}

TimeStamp GetNow() { return std::chrono::system_clock::now();}

float Timer::TotalTime() const
{
	Duration totalTime = Duration::zero();

    if (bIsStopped)    totalTime = (stopTime - baseTime) - pausedTime;

	else totalTime = (currTime - baseTime) - pausedTime;

	return totalTime.count();
}

float Timer::DeltaTime() const
{
	return dt.count();
}

void Timer::Reset()
{
	baseTime = prevTime = currTime = startTime = stopTime = GetNow();
	bIsStopped = true;
	dt = Duration::zero();
}

void Timer::Start()
{
	Tick();
	if (bIsStopped)
	{
		startTime = GetNow();
		pausedTime = startTime - stopTime;
		bIsStopped = false;
	}
}

void Timer::Stop()
{
	if (!bIsStopped)
	{
		stopTime = GetNow();
		bIsStopped = true;
	}
	Tick();
}

float Timer::Tick()
{
	if (bIsStopped)
	{
		dt = Duration::zero();
		return dt.count();
	}

	currTime = GetNow();
	dt = currTime - prevTime;

	prevTime = currTime;
	dt = dt.count() < 0.0f ? dt.zero() : dt;	// make sure dt >= 0 (is this necessary?)

	return dt.count();
}

float Timer::GetPausedTime() const
{
	return pausedTime.count();
}
float Timer::GetStopDuration() const
{
	Duration stopDuration = GetNow() - stopTime;
	return stopDuration.count();
}

