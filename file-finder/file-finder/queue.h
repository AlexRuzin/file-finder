#pragma once

// I used this header from a previous project I wrote

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <mutex>
#include <deque>
#include <condition_variable>

template <typename T>
class SafeQueue {
private:
    std::deque<T>                   dequeue;
    std::mutex                      queueSync;
    std::condition_variable         condVariable;

public:
    //
    // Thread safe queue
    //  All functions are manually synchronized using a std::mutex
    //
    SafeQueue(void)
    {

    }
    ~SafeQueue(void)
    {

    }

    //
    // Destroy all objects in the queue
    //  Note: If calling pointer destructors is required, use clearPtr()
    //
    void clear(void)
    {
        std::unique_lock<std::mutex> mlock(queueSync);
        dequeue.clear();
    }

    //
    // Clear all objects in the queue and manually invoke the destructor of each element
    //
#if 0
    void clearPtr(void)
    {
        std::unique_lock<std::mutex> mlock(queueSync);

        if (dequeue.size() == 0) {
            return;
        }

        for (std::deque<T>::const_iterator i = dequeue.begin(); i != dequeue.end(); i++) {
            delete* i;
        }

        dequeue.clear();
    }
#endif

    //
    // Blocking access
    //
    T& frontaccess_blocking(void)
    {
        std::unique_lock<std::mutex> mlock(queueSync);
        while (dequeue.empty()) {
            condVariable.wait(mlock);
        }

        return dequeue.front();
    }

    //
    // Non-blocking access
    //
    T frontaccess_nonblock(void)
    {
        std::unique_lock<std::mutex> mlock(queueSync);

        if (dequeue.size() > 0) {
            return dequeue.front();
        }

        return nullptr;
    }


    //
    // Nonblocking pop and return a value
    //  Note: use only if the template value is a pointer
    //
    T frontpop_nonblock(void)
    {
        std::unique_lock<std::mutex> mlock(queueSync);

        if (dequeue.size() > 0) {
            T out = dequeue.front();
            dequeue.pop_front();
            return out;
        }

        return nullptr;
    }

    //
    // Pop the front of the queue, do not invoke destructor on pointer
    //
    void frontpop(void)
    {
        std::unique_lock<std::mutex> mlock(queueSync);
        while (dequeue.empty()) {
            condVariable.wait(mlock);
        }

        dequeue.pop_front();
    }

    //
    // Push object into the queue
    // 
    void pushback(const T& item)
    {
        std::unique_lock<std::mutex> mlock(queueSync);
        dequeue.push_back(item);
        mlock.unlock();
        condVariable.notify_one();
    }

    void pushback(T&& item)
    {
        std::unique_lock<std::mutex> mlock(queueSync);
        dequeue.push_back(std::move(item));
        mlock.unlock();
        condVariable.notify_one();
    }

    //
    // Thread-safe get size
    //
    size_t size(void)
    {
        std::unique_lock<std::mutex> mlock(queueSync);
        size_t size = dequeue.size();
        mlock.unlock();
        return size;
    }
};

#endif //_QUEUE_H_