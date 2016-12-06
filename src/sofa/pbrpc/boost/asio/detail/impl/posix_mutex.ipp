//
// detail/impl/posix_mutex.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOFA_PBRPC_BOOST_ASIO_DETAIL_IMPL_POSIX_MUTEX_IPP
#define SOFA_PBRPC_BOOST_ASIO_DETAIL_IMPL_POSIX_MUTEX_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <sofa/pbrpc/boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_HAS_PTHREADS)

#include <sofa/pbrpc/boost/asio/detail/posix_mutex.hpp>
#include <sofa/pbrpc/boost/asio/detail/throw_error.hpp>
#include <sofa/pbrpc/boost/asio/error.hpp>

#include <sofa/pbrpc/boost/asio/detail/push_options.hpp>

namespace sofa {
namespace pbrpc {
namespace boost {
namespace asio {
namespace detail {

posix_mutex::posix_mutex()
{
  int error = ::pthread_mutex_init(&mutex_, 0);
  sofa::pbrpc::boost::system::error_code ec(error,
      sofa::pbrpc::boost::asio::error::get_system_category());
  sofa::pbrpc::boost::asio::detail::throw_error(ec, "mutex");
}

} // namespace detail
} // namespace asio
} // namespace boost
} // namespace pbrpc
} // namespace sofa

#include <sofa/pbrpc/boost/asio/detail/pop_options.hpp>

#endif // defined(BOOST_ASIO_HAS_PTHREADS)

#endif // SOFA_PBRPC_BOOST_ASIO_DETAIL_IMPL_POSIX_MUTEX_IPP
