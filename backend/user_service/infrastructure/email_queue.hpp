#pragma once
#include "common/config.hpp"
#include "domain/email_sender.hpp"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <future>
#include <mutex>
#include <thread>

namespace user_service {

struct EmailTask {
  std::atomic_bool ready;
  std::string to_email;
  std::string subject;
  std::string body;
  std::chrono::steady_clock::time_point created_at;
  std::promise<bool> prom;
  
  EmailTask(const std::string& email, const std::string& subj, const std::string& content)
    : to_email(email), subject(subj), body(content), ready(false),
      created_at(std::chrono::steady_clock::now()) {}

  ~EmailTask(){
    if (prom.get_future().wait_for(std::chrono::seconds(0)) == std::future_status::deferred)
      prom.set_value(false);
  }
};

template <size_t Capacity>
// 场景：单个消费者，消费任务；多个生产者，生产任务
class SMTPEmailQueue final: EmailSender{
private:
  config::SMTPConfig cfg_;
  size_t capacity_;
  std::mutex mtx;
  std::condition_variable condition_;
  std::atomic_bool stop_{false};
  std::jthread thread_;
  std::array<EmailTask, Capacity> data_;
  // (pos=Capacity) new task (pos=tail) >> tail >> head >> worker (pos=0)
  // head和tail只会增加，tail位置还没有元素
  alignas(64) size_t head_{0};
  alignas(64) std::atomic<size_t> tail_{0};
  
public:
  SMTPEmailQueue() : cfg_(config::Config::getInstance().getSMTP()), capacity_(Capacity){

  }
  
  ~SMTPEmailQueue() = default;

  bool available() override {
    return (tail_.load(std::memory_order_acquire) + 1) % capacity_ != head_;
  }

  bool empty() override {
    return head_ == tail_.load(std::memory_order_acquire);
  }

  std::future<bool> addTask(const std::string& to_email, const std::string& subject, const std::string& body) {
    if (!available()) {
      std::promise<bool> p;
      p.set_value(false);
      return p.get_future();
    }
    EmailTask task(to_email, subject, body);
    size_t pos = tail_.load(std::memory_order_relaxed);
    while (!tail_.compare_exchange_weak(pos, (pos + 1)%capacity_ , std::memory_order_relaxed)){
      pos = tail_.load(std::memory_order_relaxed);
    }
    data_[pos] = std::move(task);
    data_[pos].ready.store(true, std::memory_order_release);
    condition_.notify_one();
    return data_[pos].prom.get_future();
  }

  void loop(){
    while (true) {
      std::unique_lock<std::mutex> lock{mtx};
      condition_.wait(lock, [this]() -> bool {
        return stop_.load(std::memory_order_acquire) || 
          ((!empty()) && data_[head_].ready.load(std::memory_order_acquire));
      });// 等待条件变量被唤醒，若任务为空且线程池未被关闭则继续等待

      if (stop_.load(std::memory_order_acquire) && empty()) {
        break;
      }

      sendEmail(data_[head_++]);
    }
  }

  bool sendEmail(EmailTask& task){
    
    task.prom.set_value(true);
  }

};
}