#pragma once

#include "esphome/components/socket/socket.h"

#include <cerrno>
#include <sys/select.h>

namespace esphome::tcp_link {

enum class ConnectWait : uint8_t { PENDING, FAILED, DONE };

namespace internal {

// Newer ESPHome has socket::poll_connect(). 2026.9.1 does not, so that overload
// drops out and the select() path below is used.
template<typename Sock>
auto poll_new(Sock &sock, int &err, int) -> decltype(poll_connect(sock, err), ConnectWait()) {
  switch (poll_connect(sock, err)) {
    case decltype(poll_connect(sock, err))::CONNECT_POLL_RESULT_PENDING:
      return ConnectWait::PENDING;
    case decltype(poll_connect(sock, err))::CONNECT_POLL_RESULT_ERROR:
      return ConnectWait::FAILED;
    default:
      return ConnectWait::DONE;
  }
}

template<typename Sock>
ConnectWait poll_new(Sock &sock, int &err, long) {
  int fd = sock.get_fd();
  if (fd < 0 || fd >= FD_SETSIZE) {
    err = EBADF;
    return ConnectWait::FAILED;
  }
  fd_set writefds;
  FD_ZERO(&writefds);
  FD_SET(fd, &writefds);
  struct timeval tv = {0, 0};
#if defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  int ret = lwip_select(fd + 1, nullptr, &writefds, nullptr, &tv);
#else
  int ret = ::select(fd + 1, nullptr, &writefds, nullptr, &tv);
#endif
  if (ret < 0) {
    err = errno;
    return ConnectWait::FAILED;
  }
  if (ret == 0) {
    return ConnectWait::PENDING;
  }
  int error = 0;
  socklen_t len = sizeof(error);
  if (sock.getsockopt(SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
    err = errno;
    return ConnectWait::FAILED;
  }
  if (error != 0) {
    err = error;
    return ConnectWait::FAILED;
  }
  return ConnectWait::DONE;
}

}  // namespace internal

inline ConnectWait poll_connect_result(socket::Socket &sock, int &err) { return internal::poll_new(sock, err, 0); }

}  // namespace esphome::tcp_link
