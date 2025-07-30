#include "Thread.hpp"

#include <functional>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
DWORD WINAPI winThreadFuncWrapper(LPVOID lpParam)
{
    std::function<void()>* func = static_cast<std::function<void()>*>(lpParam);
    (*func)();
    delete func; // Clean up the allocated function object
    return 0;
}
#else
static pthread_t NULL_THREAD = pthread_t();

static pthread_t thread_get_id(const pthread_t* t)
{
    return *t;
}

static bool thread_isnull(const pthread_t* t)
{
    return thread_get_id(t) == nullptr;
}

void* posixThreadFuncWrapper(void* arg)
{
    auto* func = static_cast<std::function<void()>*>(arg);
    (*func)();
    delete func; // Clean up the allocated function object
    return nullptr;
}

Thread::~Thread()
{
    if (!thread_isnull(&m_threadId))
        std::terminate();
}

Thread::Thread(Thread&& thread) noexcept
    : m_threadId(thread.m_threadId)
{
    thread.m_threadId = NULL_THREAD;
}

Thread& Thread::operator=(Thread&& other) noexcept
{
    if (!thread_isnull(&m_threadId))
        std::terminate();
    m_threadId = other.m_threadId;
    other.m_threadId = NULL_THREAD;
    return *this;
}

bool Thread::joinable() const
{
    return !thread_isnull(&m_threadId);
}

void Thread::join()
{
    int ec = EINVAL;
    if (joinable())
    {
        ec = pthread_join(m_threadId, nullptr);
        if (ec == 0)
            m_threadId = NULL_THREAD;
    }

    if (ec)
        throw std::system_error(ec, std::generic_category(), "thread::join failed");
}
#endif