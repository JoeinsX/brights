#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class Threadpool {
public:
   explicit Threadpool(const size_t threads) {
      for (size_t i = 0; i < threads; ++i) {
         workers.emplace_back([this] {
            for (;;) {
               std::function<void()> task;
               {
                  std::unique_lock<std::mutex> lock(this->queueMutex);
                  this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                  if (this->stop && this->tasks.empty()) {
                     return;
                  }
                  task = std::move(this->tasks.front());
                  this->tasks.pop();
               }
               task();
            }
         });
      }
   }

   template<class F>
   void enqueue(F&& f) {
      {
         const std::unique_lock<std::mutex> lock(queueMutex);
         tasks.emplace(std::forward<F>(f));
      }
      condition.notify_one();
   }

   void shutdown() {
      {
         const std::unique_lock<std::mutex> lock(queueMutex);
         stop = true;
      }
      condition.notify_all();
      for (std::thread& worker : workers) {
         worker.join();
      }
      workers.clear();
   }

   ~Threadpool() { shutdown(); }

   Threadpool(const Threadpool&) = delete;
   Threadpool(Threadpool&&) = delete;
   Threadpool& operator =(const Threadpool&) = delete;
   Threadpool& operator =(Threadpool&&) = delete;

private:
   std::vector<std::thread> workers;
   std::queue<std::function<void()>> tasks;
   std::mutex queueMutex;
   std::condition_variable condition;
   bool stop{false};
};
