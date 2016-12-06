//
// ip/host_name.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOFA_PBRPC_BOOST_ASIO_IP_HOST_NAME_HPP
#define SOFA_PBRPC_BOOST_ASIO_IP_HOST_NAME_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <sofa/pbrpc/boost/asio/detail/config.hpp>
#include <string>
#include <sofa/pbrpc/boost/system/error_code.hpp>

#include <sofa/pbrpc/boost/asio/detail/push_options.hpp>

namespace sofa {
namespace pbrpc {
namespace boost {
namespace asio {
namespace ip {

/// Get the current host name.
BOOST_ASIO_DECL std::string host_name();

/// Get the current host name.
BOOST_ASIO_DECL std::string host_name(sofa::pbrpc::boost::system::error_code& ec);

} // namespace ip
} // namespace asio
} // namespace boost
} // namespace pbrpc
} // namespace sofa

#include <sofa/pbrpc/boost/asio/detail/pop_options.hpp>

#if defined(BOOST_ASIO_HEADER_ONLY)
# include <sofa/pbrpc/boost/asio/ip/impl/host_name.ipp>
#endif // defined(BOOST_ASIO_HEADER_ONLY)

#endif // SOFA_PBRPC_BOOST_ASIO_IP_HOST_NAME_HPP
