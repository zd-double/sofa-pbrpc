//
// generic/detail/impl/endpoint.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOFA_PBRPC_BOOST_ASIO_GENERIC_DETAIL_IMPL_ENDPOINT_IPP
#define SOFA_PBRPC_BOOST_ASIO_GENERIC_DETAIL_IMPL_ENDPOINT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <sofa/pbrpc/boost/asio/detail/config.hpp>

#include <cstring>
#include <typeinfo>
#include <sofa/pbrpc/boost/asio/detail/socket_ops.hpp>
#include <sofa/pbrpc/boost/asio/detail/throw_error.hpp>
#include <sofa/pbrpc/boost/asio/detail/throw_exception.hpp>
#include <sofa/pbrpc/boost/asio/error.hpp>
#include <sofa/pbrpc/boost/asio/generic/detail/endpoint.hpp>

#include <sofa/pbrpc/boost/asio/detail/push_options.hpp>

namespace sofa {
namespace pbrpc {
namespace boost {
namespace asio {
namespace generic {
namespace detail {

endpoint::endpoint()
{
  init(0, 0, 0);
}

endpoint::endpoint(const void* sock_addr,
    std::size_t sock_addr_size, int sock_protocol)
{
  init(sock_addr, sock_addr_size, sock_protocol);
}

void endpoint::resize(std::size_t new_size)
{
  if (new_size > sizeof(sofa::pbrpc::boost::asio::detail::sockaddr_storage_type))
  {
    sofa::pbrpc::boost::system::error_code ec(sofa::pbrpc::boost::asio::error::invalid_argument);
    sofa::pbrpc::boost::asio::detail::throw_error(ec);
  }
  else
  {
    size_ = new_size;
    protocol_ = 0;
  }
}

bool operator==(const endpoint& e1, const endpoint& e2)
{
  using namespace std; // For memcmp.
  return e1.size() == e2.size() && memcmp(e1.data(), e2.data(), e1.size()) == 0;
}

bool operator<(const endpoint& e1, const endpoint& e2)
{
  if (e1.protocol() < e2.protocol())
    return true;

  if (e1.protocol() > e2.protocol())
    return false;

  using namespace std; // For memcmp.
  std::size_t compare_size = e1.size() < e2.size() ? e1.size() : e2.size();
  int compare_result = memcmp(e1.data(), e2.data(), compare_size);

  if (compare_result < 0)
    return true;

  if (compare_result > 0)
    return false;

  return e1.size() < e2.size();
}

void endpoint::init(const void* sock_addr,
    std::size_t sock_addr_size, int sock_protocol)
{
  if (sock_addr_size > sizeof(sofa::pbrpc::boost::asio::detail::sockaddr_storage_type))
  {
    sofa::pbrpc::boost::system::error_code ec(sofa::pbrpc::boost::asio::error::invalid_argument);
    sofa::pbrpc::boost::asio::detail::throw_error(ec);
  }

  using namespace std; // For memset and memcpy.
  memset(&data_.generic, 0, sizeof(sofa::pbrpc::boost::asio::detail::sockaddr_storage_type));
  memcpy(&data_.generic, sock_addr, sock_addr_size);

  size_ = sock_addr_size;
  protocol_ = sock_protocol;
}

} // namespace detail
} // namespace generic
} // namespace asio
} // namespace boost
} // namespace pbrpc
} // namespace sofa

#include <sofa/pbrpc/boost/asio/detail/pop_options.hpp>

#endif // SOFA_PBRPC_BOOST_ASIO_GENERIC_DETAIL_IMPL_ENDPOINT_IPP
