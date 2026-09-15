#include "RSock.h"

#include <errno.h>
#include <cstring>
#include <iostream>
#include <system_error>

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace vr::NetworkRttTest
{

namespace
{
    ifreq makeIfreq(const std::string& interface)
    {
        ifreq request{};
        std::strncpy(request.ifr_name, interface.c_str(), IFNAMSIZ - 1);
        request.ifr_name[IFNAMSIZ - 1] = '\0';
        return request;
    }
}  // namespace

// NOLINTBEGIN (*-mt-unsafe)
RSock::RSock(const std::string& interface)
{
    // create socket
    socket_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (socket_ == -1)
    {
        throw std::system_error(errno, std::system_category(), "Error creating socket");
    }

    try
    {
        // get interface index
        struct ifreq ifidx = makeIfreq(interface);
        if (ioctl(socket_, SIOCGIFINDEX, &ifidx) < 0)
        {
            throw std::system_error(errno, std::system_category(), "Couldn't get interface index");
        }

        // enable promiscuous mode
        // Unlike setting IFF_PROMISC via SIOCSIFFLAGS, this membership is reference counted by the kernel and
        // automatically dropped when the socket is closed
        packet_mreq promiscMembership{};
        promiscMembership.mr_ifindex = ifidx.ifr_ifindex;
        promiscMembership.mr_type = PACKET_MR_PROMISC;
        if (setsockopt(socket_, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &promiscMembership, sizeof(promiscMembership)) < 0)
        {
            throw std::system_error(errno, std::system_category(), "Failed to enable promiscuous mode");
        }

        // bind socket
        struct sockaddr_ll bindAddr{};
        bindAddr.sll_family = AF_PACKET;
        bindAddr.sll_ifindex = ifidx.ifr_ifindex;
        bindAddr.sll_protocol = htons(ETH_P_ALL);

        if (bind(socket_, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_ll)) != 0)
        {
            throw std::system_error(errno, std::system_category(), "Binding socket to interface failed");
        }
    } catch (...)
    {
        close(socket_);
        socket_ = -1;
        throw;
    }
}

RSock::RSock(RSock&& other) noexcept : socket_(other.socket_)
{
    other.socket_ = -1;
}

RSock& RSock::operator=(RSock&& other) noexcept
{
    if (this != &other)
    {
        if (socket_ != -1)
        {
            close(socket_);
        }
        socket_ = other.socket_;
        other.socket_ = -1;
    }
    return *this;
}

RSock::~RSock()
{
    /* close socket */
    if (socket_ != -1)
    {
        if (close(socket_) == -1)
        {
            std::cerr << "Closing socket failed with error: " << std::strerror(errno) << "\n";
        }
    }
}

size_t RSock::receive(std::span<std::byte> buffer)
{
    ssize_t bytesRead = recvfrom(socket_, buffer.data(), buffer.size(), 0, NULL, NULL);

    if (bytesRead < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        else
        {
            throw std::system_error(errno, std::system_category(), "Error receiving");
        }
    }

    return bytesRead;
}

size_t RSock::send(std::span<const std::byte> buffer)
{
    ssize_t bytesWritten = write(socket_, buffer.data(), buffer.size());

    if (bytesWritten < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        else
        {
            throw std::system_error(errno, std::system_category(), "Sending failed");
        }
    }

    return static_cast<size_t>(bytesWritten);
}

void RSock::setReceiveTimeout(std::chrono::microseconds timeout)
{
    setBlocking();

    timeval timeval{.tv_sec = timeout.count() / (1000 * 1000), .tv_usec = timeout.count() % (1000 * 1000)};
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&timeval, sizeof(struct timeval)))
    {
        throw std::system_error(errno, std::system_category(), "Setting socket timeout failed");
    }
}

void RSock::setBlocking()
{
    int flags = fcntl(socket_, F_GETFL);
    if (flags == -1)
    {
        throw std::system_error(errno, std::system_category(), "Getting socket flags failed");
    }
    if (fcntl(socket_, F_SETFL, flags & (~O_NONBLOCK)) < 0)
    {
        throw std::system_error(errno, std::system_category(), "Setting socket to blocking mode failed");
    }
}
void RSock::setNonBlocking()
{
    int flags = fcntl(socket_, F_GETFL);
    if (flags == -1)
    {
        throw std::system_error(errno, std::system_category(), "Getting socket flags failed");
    }
    if (fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        throw std::system_error(errno, std::system_category(), "Setting socket to non-blocking mode failed");
    }
}

}  // namespace vr::NetworkRttTest
   // NOLINTEND (*-mt-unsafe)
