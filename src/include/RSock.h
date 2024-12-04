#pragma once

#include <chrono>
#include <span>
#include <string>

namespace vr::NetworkRttTest
{

class RSock
{
   public:
    explicit RSock(const std::string& interface);
    ~RSock();

    RSock(const RSock&) = delete;
    RSock& operator=(const RSock&) = delete;
    RSock(RSock&& other) noexcept;
    RSock& operator=(RSock&& other) noexcept;

    void setBlocking();
    void setNonBlocking();
    void setReceiveTimeout(std::chrono::microseconds timeout);

    size_t receive(std::span<std::byte> buffer);
    size_t send(std::span<const std::byte> buffer);

   private:
    int socket_ = -1;
};

}  // namespace vr::NetworkRttTest