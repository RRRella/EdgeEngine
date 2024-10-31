#include "Multithreading.h"
#include "Utils.h"
#include "Log.h"

#include <cassert>
#include <algorithm>

void Semaphore::Wait()
{
	std::unique_lock<std::mutex> lk(mtx);
	cv.wait(lk, [&]() {return currVal > 0; });
	--currVal;
	return;
}

void Semaphore::Signal()
{
	std::unique_lock<std::mutex> lk(mtx);
	currVal = std::min<unsigned short>(currVal+1u, maxVal);
	cv.notify_one();
}

static void SetThreadName(std::thread& th, const wchar_t* threadName) {
	HRESULT hr = SetThreadDescription(th.native_handle(), threadName);
	if (FAILED(hr)) {}
}

void ThreadPool::Initialize(size_t numThreads, const std::string& ThreadPoolName)
{
	mThreadPoolName = ThreadPoolName;
	mbStopWorkers.store(false);
	for (auto i = 0u; i < numThreads; ++i)
	{
		mWorkers.emplace_back(std::thread(&ThreadPool::Execute, this));
		SetThreadName(mWorkers.back(), StrUtil::ASCIIToUnicode(ThreadPoolName).c_str());
	}
}

void ThreadPool::Destroy()
{
	mbStopWorkers.store(true);

	mCondVar.notify_all();

	for (auto& worker : mWorkers)
	{
		worker.join();
	}
}

void ThreadPool::RunRemainingTasksOnThisThread()
{
	Task task; 
	while (!mTaskQueue.IsQueueEmpty())
	{
		mTaskQueue.PopTask(task);
		task(); 
		mTaskQueue.OnTaskComplete(); 
	}
}
void ThreadPool::Execute()
{
	Task task;

	std::unique_lock lock(mMtx, std::defer_lock);

	while (!mbStopWorkers)
	{
		lock.lock();
		mCondVar.wait(lock,[&] { return mbStopWorkers || !mTaskQueue.IsQueueEmpty(); });

		if (mbStopWorkers)
			break;
		
		mTaskQueue.PopTask(task);

		lock.unlock();

		task();
		mTaskQueue.OnTaskComplete();
	}
}

void TaskQueue::PopTask(Task& task)
{
	std::lock_guard<std::mutex> lk(mutex);
	
	task = std::move(queue.front());
	queue.pop();
}