 
 #pragma once

#include <queue>
#include <mutex>
#include <condition_variable>



// Created by Adam Kecskes
// https://github.com/K-Adam/SafeQueue


#include <utility>

template<class T>
class SafeQueue {

	std::queue<T> q;

	std::mutex mtx;
	std::condition_variable cv;

	std::condition_variable sync_wait;
	bool finish_processing = false;
	int sync_counter = 0;

	void DecreaseSyncCounter() {
		if (--sync_counter == 0) {
			sync_wait.notify_one();
		}
	}

public:

	typedef typename std::queue<T>::size_type size_type;
    SafeQueue(SafeQueue&& other) noexcept {
    // Move the contents of 'other' to the new object

    // Example: Move the elements from 'other' to 'this' queue
    std::lock_guard<std::mutex> lock(other.mutex_);
    q = std::move(other.q);
    }
     SafeQueue(const SafeQueue& other) {
        // Copy the contents of 'other' to the new object

        // Example: Copy the elements from 'other' to 'this' queue
        std::lock_guard<std::mutex> lock(other.mutex_);
        for (const auto& item : other.q) {
            q.emplace(item);
        }
    }
	SafeQueue() {}

	~SafeQueue() {
		Finish();
	}

	void Produce(T&& item) {

		std::lock_guard<std::mutex> lock(mtx);

		q.push(std::move(item));
		cv.notify_one();

	}

	size_type Size() {

		std::lock_guard<std::mutex> lock(mtx);

		return q.size();

	}

	[[nodiscard]]
	bool Consume(T& item) {

		std::lock_guard<std::mutex> lock(mtx);

		if (q.empty()) {
			return false;
		}

		item = std::move(q.front());
		q.pop();

		return true;

	}

	[[nodiscard]]
	bool ConsumeSync(T& item) {

		std::unique_lock<std::mutex> lock(mtx);

		sync_counter++;

		cv.wait(lock, [&] {
			return !q.empty() || finish_processing;
		});

		if (q.empty()) {
			DecreaseSyncCounter();
			return false;
		}

		item = std::move(q.front());
		q.pop();

		DecreaseSyncCounter();
		return true;

	}

	void Finish() {

		std::unique_lock<std::mutex> lock(mtx);

		finish_processing = true;
		cv.notify_all();

		sync_wait.wait(lock, [&]() {
			return sync_counter == 0;
		});

		finish_processing = false;

	}

};


 template <typename T>
    class SharedQueue
    {
    public:
        SharedQueue();
        ~SharedQueue();

        SharedQueue(const SharedQueue& other) {
        // Copy the contents of 'other' to the new object

        // Example: Copy the elements from 'other' to 'this' queue
        std::lock_guard<std::mutex> lock(other.mutex_);
        for (const auto& item : other.queue_) {
            queue_.emplace(item);
        }
        }

        SharedQueue(SharedQueue&& other) noexcept {
        // Move the contents of 'other' to the new object

        // Example: Move the elements from 'other' to 'this' queue
        std::lock_guard<std::mutex> lock(other.mutex_);
        queue_ = std::move(other.queue_);
         }
    

        T& front();
        void pop_front();

        void push_back(const T& item);
        void push_back(T&& item);

        std::size_t size();
        // bool empty();

    private:
        std::deque<T> queue_;
        std::mutex mutex_;
        std::condition_variable cond_;
    }; 

    template <typename T>
    SharedQueue<T>::SharedQueue(){}

    template <typename T>
    SharedQueue<T>::~SharedQueue(){}

    template <typename T>
    T& SharedQueue<T>::front()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
        {
            cond_.wait(mlock);
        }
        return queue_.front();
    }

   

    template <typename T>
    void SharedQueue<T>::pop_front()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
        {
            cond_.wait(mlock);
        }
        queue_.pop_front();
    }     

    template <typename T>
    void SharedQueue<T>::push_back(const T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        queue_.push_back(item);
        mlock.unlock();     // unlock before notificiation to minimize mutex con
        cond_.notify_one(); // notify one waiting thread

    }

    template <typename T>
    void SharedQueue<T>::push_back(T&& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        queue_.push_back(std::move(item));
        mlock.unlock();     // unlock before notificiation to minimize mutex con
        cond_.notify_one(); // notify one waiting thread

    }

    template <typename T>
    std::size_t SharedQueue<T>::size()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        std::size_t size = queue_.size();
        mlock.unlock();
        return size;
    }
   
