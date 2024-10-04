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

    // Base   Stop       Start   Stop      Curr
    //--*-------*----------*------*---------|
    //          <---------->
    //             Paused
    if (bIsStopped)    totalTime = (stopTime - baseTime) - pausedTime;

    // Base         Stop      Start         Curr
    //--*------------*----------*------------|
    //               <---------->
    //                  Paused
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
	if (bIsStopped)
	{
		pausedTime = startTime - stopTime;
		prevTime = GetNow();
		bIsStopped = false;
	}
	Tick();
}

void Timer::Stop()
{
	Tick();
	if (!bIsStopped)
	{
		stopTime = GetNow();
		bIsStopped = true;
	}
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

