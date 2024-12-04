#ifndef NETWORKDRIVER_H
#define NETWORKDRIVER_H

#include <cstddef>
#include <span>

namespace vr::NetworkRttTest
{

/**
 * @concept NetworkDriver
 * @brief Specifies the interface for network communication operations
 *
 * A type satisfies NetworkDriver if it provides methods to send and receive data
 * over a network connection.
 */
template <typename T>
concept NetworkDriver = requires(T driver, std::span<std::byte> buffer, std::span<const std::byte> constBuffer) {
    /**
     * @brief Receive data from the network
     * @param buffer Span where received data will be stored
     * @return Number of bytes actually received
     */
    { driver.receive(buffer) } -> std::same_as<size_t>;

    /**
     * @brief Send data over the network
     * @param buffer Span of data to send
     * @return Number of bytes actually sent
     */
    { driver.send(constBuffer) } -> std::same_as<size_t>;
};
}  // namespace vr::NetworkRttTest

#endif  // NETWORKDRIVER_H
