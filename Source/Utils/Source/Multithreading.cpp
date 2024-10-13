#include "Multithreading.h"
#include "Utils.h"
#include "Log.h"

#include <cassert>
#include <algorithm>

#define RUN_THREADPOOL_UNIT_TEST 0

static void RUN_THREAD_POOL_UNIT_TEST()
{
	ThreadPool p;
	p.Initialize(ThreadPool::sHardwareThreadCount, "TEST POOL");

	constexpr long long sz = 40000000;
	auto sumRnd = [&]()
	{
		std::vector<long long> nums(sz, 0);
		for (int i = 0; i < sz; ++i)
		{
			nums[i] = MathUtil::RandI(0, 5000);
		}
		unsigned long long result = 0;
		for (int i = 0; i < sz; ++i)
		{
			if (nums[i] > 3000)
				result += nums[i];
		}
		return result;
	};
	auto sum = [&]()
	{
		std::vector<long long> nums(sz, 0);
		for (int i = 0; i < sz; ++i)
		{
			nums[i] = MathUtil::RandI(0, 5000);
		}
		unsigned long long result = 0;
		for (int i = 0; i < sz; ++i)
		{
			result += nums[i];
		}
		return result;
	};

	constexpr int threadCount = 16;
	std::future<unsigned long long> futures[threadCount] =
	{
		p.AddTask(sumRnd),
		p.AddTask(sumRnd),
		p.AddTask(sumRnd),
		p.AddTask(sumRnd),
		p.AddTask(sumRnd),
		p.AddTask(sumRnd),
		p.AddTask(sumRnd),
		p.AddTask(sumRnd),

		p.AddTask(sum),
		p.AddTask(sum),
		p.AddTask(sum),
		p.AddTask(sum),
		p.AddTask(sum),
		p.AddTask(sum),
		p.AddTask(sum),
		p.AddTask(sum),
	};

	std::vector<unsigned long long> results;
	unsigned long long total = 0;
	std::for_each(std::begin(futures), std::end(futures), [&](decltype(futures[0]) f)
	{
		results.push_back(f.get());
		total += results.back();
	});

	std::string strResult = "total (" + std::to_string(total) + ") = ";
	for (int i = 0; i < threadCount; ++i)
	{
		strResult += "(" + std::to_string(results[i]) + ") " + (i == threadCount - 1 ? "" : " + ");
	}
	Log::Info(strResult);
}




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
	if (FAILED(hr)) {
		// Handle error if needed
	}
}
void ThreadPool::Initialize(size_t numThreads, const std::string& ThreadPoolName, unsigned int MarkerColor)
{
	mMarkerColor = MarkerColor;
	mThreadPoolName = ThreadPoolName;
	mbStopWorkers.store(false);
	for (auto i = 0u; i < numThreads; ++i)
	{
		mWorkers.emplace_back(std::thread(&ThreadPool::Execute, this));
		SetThreadName(mWorkers.back(), StrUtil::ASCIIToUnicode(ThreadPoolName).c_str());
	}

#if RUN_THREADPOOL_UNIT_TEST
	RUN_THREAD_POOL_UNIT_TEST();
#endif
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
	while (mTaskQueue.IsQueueEmpty())
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

std::vector<std::pair<size_t, size_t>> PartitionWorkItemsIntoRanges(size_t NumWorkItems, size_t NumWorkerThreadCount)
{
	// @NumWorkItems is distributed as equally as possible between all @NumWorkerThreadCount threads.
	// Two numbers are determined
	// - NumWorkItemPerThread: number of work items each thread will get equally
	// - NumWorkItemPerThread_Extra : number of +1's to be added to each worker
	const size_t NumWorkItemPerThread = NumWorkItems / NumWorkerThreadCount; // amount of work each worker is to get, 
	const size_t NumWorkItemPerThread_Extra = NumWorkItems % NumWorkerThreadCount;

	std::vector<std::pair<size_t, size_t>> vRanges(NumWorkItemPerThread == 0
		? NumWorkItems  // if NumWorkItems < NumWorkerThreadCount, then only create ranges according to NumWorkItems
		: NumWorkerThreadCount // each worker thread gets a range
	);

	size_t iRange = 0;
	for (auto& range : vRanges)
	{
		range.first = iRange != 0 ? vRanges[iRange - 1].second + 1 : 0;
		range.second = range.first + NumWorkItemPerThread - 1 + (NumWorkItemPerThread_Extra > iRange ? 1 : 0);
		assert(range.first <= range.second); // ensure work context bounds
		++iRange;
	}

	return vRanges;
}

size_t CalculateNumThreadsToUse(const size_t NumWorkItems, const size_t NumWorkerThreads, const size_t NumMinimumWorkItemCountPerThread)
{
#define DIV_AND_ROUND_UP(x, y) ((x+y-1)/(y))
	const size_t NumWorkItemsPerAvailableWorkerThread = DIV_AND_ROUND_UP(NumWorkItems, NumWorkerThreads);
	size_t NumWorkerThreadsToUse = NumWorkerThreads;
	if (NumWorkItemsPerAvailableWorkerThread < NumMinimumWorkItemCountPerThread)
	{
		const float OffRatio = float(NumMinimumWorkItemCountPerThread) / float(NumWorkItemsPerAvailableWorkerThread);
		NumWorkerThreadsToUse = static_cast<size_t>(NumWorkerThreadsToUse / OffRatio); // clamp down
		NumWorkerThreadsToUse = std::max((size_t)1, NumWorkerThreadsToUse);
	}
	return NumWorkerThreadsToUse;
}

