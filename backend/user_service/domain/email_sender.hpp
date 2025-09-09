#pragma once
#include <string>
#include <future>

namespace user_service {
// 异步邮箱发送
class EmailSender {
public:
  virtual ~EmailSender() = default;
  // 是否可以添加新任务
  virtual bool available() = 0;
  virtual bool empty() = 0;
  // 添加任务，在执行任务前返回future<任务结果>
  virtual std::future<bool> addTask(const std::string& to_email, const std::string& subject, const std::string& body) = 0;
};
}