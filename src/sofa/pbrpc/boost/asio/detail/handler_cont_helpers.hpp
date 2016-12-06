//
// detail/handler_cont_helpers.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOFA_PBRPC_BOOST_ASIO_DETAIL_HANDLER_CONT_HELPERS_HPP
#define SOFA_PBRPC_BOOST_ASIO_DETAIL_HANDLER_CONT_HELPERS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <sofa/pbrpc/boost/asio/detail/config.hpp>
#include <sofa/pbrpc/boost/asio/detail/addressof.hpp>
#include <sofa/pbrpc/boost/asio/handler_continuation_hook.hpp>

#include <sofa/pbrpc/boost/asio/detail/push_options.hpp>

// Calls to asio_handler_is_continuation must be made from a namespace that
// does not contain overloads of this function. This namespace is defined here
// for that purpose.
namespace boost_asio_handler_cont_helpers {

template <typename Context>
inline bool is_continuation(Context& context)
{
#if !defined(BOOST_ASIO_HAS_HANDLER_HOOKS)
  return false;
#else
  using sofa::pbrpc::boost::asio::asio_handler_is_continuation;
  return asio_handler_is_continuation(
      sofa::pbrpc::boost::asio::detail::addressof(context));
#endif
}

} // namespace boost_asio_handler_cont_helpers

#include <sofa/pbrpc/boost/asio/detail/pop_options.hpp>

#endif // SOFA_PBRPC_BOOST_ASIO_DETAIL_HANDLER_CONT_HELPERS_HPP
