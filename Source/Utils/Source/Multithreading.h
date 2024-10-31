#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <atomic>

// utility function for checking if a std::future<> is ready without blocking
template<typename R> bool is_ready(std::future<R> const& f) 
{ 
	return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; 
}

class Semaphore
{
public:
	Semaphore(int val, int max) : maxVal(max), currVal(val) {}

	void Wait();
	void Signal();

private:
	unsigned short currVal, maxVal; // 65,535 max threads assumed.
	std::mutex mtx;
	std::condition_variable cv;
};

// --------------------------------------------------------------------------------------------------------------------------------------
//
// Thread Pool
//
//---------------------------------------------------------------------------------------------------------------------------------------
using Task = std::function<void()>;
class TaskQueue
{
public:
	template<class T>
	void AddTask(std::shared_ptr<T>& pTask);
	void PopTask(Task& task);

	inline bool IsQueueEmpty()      const { std::unique_lock<std::mutex> lock(mutex); return queue.empty(); }
	inline int  GetNumActiveTasks() const { return activeTasks; }

	// must be called after Task() completes.
	inline void OnTaskComplete() { --activeTasks; }

private:
	std::atomic<int> activeTasks = 0;
	mutable std::mutex mutex;
	std::queue<Task> queue;
};

template<class T>
inline void TaskQueue::AddTask(std::shared_ptr<T>& pTask)
{
	std::unique_lock<std::mutex> lock(mutex);
	queue.emplace([=]() { (*pTask)(); });
	++activeTasks;
}

class ThreadPool
{
public:
	inline static const size_t sHardwareThreadCount = std::thread::hardware_concurrency();

	void Initialize(size_t numWorkers, const std::string& ThreadPoolName);
	void Destroy();

	inline int GetNumActiveTasks() const { return IsExiting() ? 0 : mTaskQueue.GetNumActiveTasks(); };
	inline size_t GetThreadPoolSize() const { return mWorkers.size(); }
	
	inline std::string GetThreadPoolName() const { return mThreadPoolName; }
	inline std::string GetThreadPoolWorkerName() const { return mThreadPoolName + "_Worker"; }
	void RunRemainingTasksOnThisThread();

	inline bool IsExiting() const { return mbStopWorkers.load(); }

	// Adds a task to the thread pool and returns the std::future<> 
	// containing the return type of the added task.
	//
	template<class T>
	decltype(auto) AddTask(T&& task);

private:
	void Execute(); // workers run Execute();

	std::mutex               mMtx;
	std::condition_variable  mCondVar;
	std::atomic<bool>        mbStopWorkers;
	TaskQueue                mTaskQueue;
	std::vector<std::thread> mWorkers;
	std::string              mThreadPoolName;
};

template<class T>
decltype(auto) ThreadPool::AddTask(T&& task)
{
	auto pTask = std::make_shared<std::packaged_task<decltype(task())()>>(std::forward<T>(task));
	mTaskQueue.AddTask(pTask);

	mCondVar.notify_one();
	
	return pTask->get_future();
}

// --------------------------------------------------------------------------------------------------------------------------------------
//
// Buffered Container
//
//---------------------------------------------------------------------------------------------------------------------------------------
//
// Double buffered generic container for multithreaded data communication.
//
// A use case is might be an event handling queue:
// - Producer writes event data into the Front container, might run in high frequency
// - Consumer thread wants to process the events, in batch
// - Consumer thread switches the container into a 'read-only' state with SwapBuffers() 
//   and gets the Back container which the main thread was writing previously.
// - Main thread now writes into the new Front container after the container switching with SwapBuffers()
// 
//
template<class TContainer, class TItem>
class BufferedContainer
{
public:
	TContainer& GetBackContainer() { return mBufferPool[(iBuffer + 1) % 2]; }
	const TContainer& GetBackContainer() const { return mBufferPool[(iBuffer + 1) % 2]; }
	
	void AddItem(TItem&& item)
	{
		std::unique_lock<std::mutex> lk(mMtx);
		mBufferPool[iBuffer].emplace(std::forward<TItem>(item));
	}
	void SwapBuffers() 
	{
		std::unique_lock<std::mutex> lk(mMtx);
		iBuffer ^= 1;
	}
	bool IsEmpty() const
	{
		std::unique_lock<std::mutex> lk(mMtx);
		return mBufferPool[iBuffer].empty();
	}
private:
	mutable std::mutex mMtx;
	std::array<TContainer, 2> mBufferPool;
	int iBuffer = 0; // ping-pong index
};

// --------------------------------------------------------------------------------------------------------------------------------------
//
// ConcurrentQueue
//
//---------------------------------------------------------------------------------------------------------------------------------------
template<class T>
class ConcurrentQueue
{
public:
	ConcurrentQueue(void (*pfnProcess)(T&)) : mpfnProcess(pfnProcess) {}

	void Enqueue(T&& item);
	T Dequeue();

	void ProcessItems();

private:
	mutable std::mutex mMtx; // if compare_exchange any better?
	std::queue<T>      mQueue;
	void (*mpfnProcess)(T&);
};

template<class T>
inline void ConcurrentQueue<T>::Enqueue(T&& item)
{
	std::lock_guard<std::mutex> lk(mMtx);
	mQueue.push(std::forward<T>(item));
}


template<class T>
inline T ConcurrentQueue<T>::Dequeue()
{
	std::lock_guard<std::mutex> lk(mMtx);
	T item = mQueue.front();
	mQueue.pop();
	return item;
}

template<class T>
inline void ConcurrentQueue<T>::ProcessItems()
{
	if (!mpfnProcess)
		return;
	
	std::unique_lock<std::mutex> lk(mMtx);
	do
	{
		mpfnProcess(std::move(mQueue.front()));
		mQueue.pop();
	} while (!mQueue.empty());
}
