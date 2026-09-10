#ifndef THREAD_POOL_H
#define THREAD_POOL_H


#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <future>
#include <type_traits>

class ThreadPool
{
    private:
        unsigned int num_threads_ = 0;
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable cv_;
        bool stop_ = false;


    public:
        ThreadPool()
        {
            num_threads_ = std::thread::hardware_concurrency();
            if (num_threads_ == 0) {num_threads_ = 1;} 
            workers_.reserve(num_threads_);
            for (size_t i = 0; i < num_threads_; i ++)
            {
                workers_.emplace_back([this]
                        {
                            while(true)
                            {
                            std::function<void()> task;
                            {
                            std::unique_lock<std::mutex> lock(queue_mutex_);
                            cv_.wait(lock, [this]
                                    {
                                    return !tasks_.empty() || stop_;
                                    });

                            if (stop_ && tasks_.empty())
                            {
                            return;
                            }

                            task = std::move(tasks_.front());
                            tasks_.pop();
                            }
                            task();

                        }

                        });
            }
        }
        ~ThreadPool()
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                stop_ = true;
            }

            cv_.notify_all();

            for (auto& t: workers_)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }
        }

        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
            {
                using return_type = typename std::invoke_result<F, Args...>::type;

                auto task = std::make_shared
                    <std::packaged_task<return_type()>>
                    (
                     std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                     );
                std::future<return_type> res = task->get_future();
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    if (stop_)
                    {

                        return std::future<typename  std::invoke_result<F, Args...>::type>();
                        
                    }
                    tasks_.emplace([task]() {(*task)();});

                }
                cv_.notify_one();
                return res;
            }

        unsigned int get_num_threads() const {return num_threads_;}




};

#endif
