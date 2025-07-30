#pragma once

#include <functional>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

constexpr size_t MACOS_CUSTOM_THREAD_STACK_SIZE = 8 * 1024 * 1024; // 8 MB, default is 512KB

void* posixThreadFuncWrapper(void* arg);

class Thread
{
protected:
#ifdef _WIN32
    HANDLE m_handle;
    DWORD m_threadId;
#else
    pthread_t m_threadId{};
#endif

public:
    Thread() = default;
    Thread(Thread&& other) noexcept;
    Thread(const Thread&) = delete;
    Thread& operator=(Thread&& other) noexcept;
    ~Thread();

    template<typename Callable>
    explicit Thread(Callable&& func)
    {
        // We need to dynamically allocate the std::function object to pass it
        // across the C-style thread boundary. It will be deleted by the wrapper.
        auto* func_ptr = new std::function<void()>(func);
        std::unique_ptr<std::function<void()>> func_owner(func_ptr);

        pthread_attr_t attr;
        int ret = pthread_attr_init(&attr);
        if (ret != 0)
        {
            throw std::system_error(ret, std::generic_category(), "error initializing pthread attributes");
        }

#ifdef __APPLE__
        // Set stack size
        ret = pthread_attr_setstacksize(&attr, MACOS_CUSTOM_THREAD_STACK_SIZE);
        if (ret != 0)
        {
            pthread_attr_destroy(&attr);
            throw std::system_error(ret, std::generic_category(), "error setting pthread stack size");
        }
#endif

        ret = pthread_create(&m_threadId, &attr, posixThreadFuncWrapper, func_owner.release());
        pthread_attr_destroy(&attr);

        if (ret != 0)
        {
            delete func_ptr; // thread wrapper function won't delete the func
            throw std::system_error(ret, std::generic_category(), "error creating thread");
        }
    }

    [[nodiscard]] bool joinable() const;
    void join();
};
