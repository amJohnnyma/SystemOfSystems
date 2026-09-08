#ifndef THREAD_POOL_H
#define THREAD_POOL_H


#include <thread>
#include <vector>

class ThreadPool
{
    private:
        unsigned int num_threads = 0;
        std::vector<std::thread> workers;

    public:
        ThreadPool()
        {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) {num_threads = 1;} 
            workers.reserve(num_threads);
        }
        ~ThreadPool();


        // Takes in a function and a assigns a worker this job
        // joins the thread to process
        void add_job();



};

#endif
