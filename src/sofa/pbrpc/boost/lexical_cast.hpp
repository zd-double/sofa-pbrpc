// Copyright Kevlin Henney, 2000-2005.
// Copyright Alexander Nasonov, 2006-2010.
// Copyright Antony Polukhin, 2011-2014.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// what:  lexical_cast custom keyword cast
// who:   contributed by Kevlin Henney,
//        enhanced with contributions from Terje Slettebo,
//        with additional fixes and suggestions from Gennaro Prota,
//        Beman Dawes, Dave Abrahams, Daryle Walker, Peter Dimov,
//        Alexander Nasonov, Antony Polukhin, Justin Viiret, Michael Hofmann,
//        Cheng Yang, Matthew Bradbury, David W. Birdsall, Pavel Korzh and other Boosters
// when:  November 2000, March 2003, June 2005, June 2006, March 2011 - 2014

#ifndef SOFA_PBRPC_BOOST_LEXICAL_CAST_INCLUDED
#define SOFA_PBRPC_BOOST_LEXICAL_CAST_INCLUDED

#include <sofa/pbrpc/boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#if defined(BOOST_NO_STRINGSTREAM) || defined(BOOST_NO_STD_WSTRING)
#define BOOST_LCAST_NO_WCHAR_T
#endif

#include <sofa/pbrpc/boost/range/iterator_range_core.hpp>
#include <sofa/pbrpc/boost/lexical_cast/bad_lexical_cast.hpp>
#include <sofa/pbrpc/boost/lexical_cast/try_lexical_convert.hpp>

namespace sofa {
namespace pbrpc {
namespace boost 
{
    template <typename Target, typename Source>
    inline Target lexical_cast(const Source &arg)
    {
        Target result;

        if (!sofa::pbrpc::boost::conversion::detail::try_lexical_convert(arg, result)) {
            sofa::pbrpc::boost::conversion::detail::throw_bad_cast<Source, Target>();
        }

        return result;
    }

    template <typename Target>
    inline Target lexical_cast(const char* chars, std::size_t count)
    {
        return ::sofa::pbrpc::boost::lexical_cast<Target>(
            ::sofa::pbrpc::boost::iterator_range<const char*>(chars, chars + count)
        );
    }

    template <typename Target>
    inline Target lexical_cast(const unsigned char* chars, std::size_t count)
    {
        return ::sofa::pbrpc::boost::lexical_cast<Target>(
            ::sofa::pbrpc::boost::iterator_range<const unsigned char*>(chars, chars + count)
        );
    }

    template <typename Target>
    inline Target lexical_cast(const signed char* chars, std::size_t count)
    {
        return ::sofa::pbrpc::boost::lexical_cast<Target>(
            ::sofa::pbrpc::boost::iterator_range<const signed char*>(chars, chars + count)
        );
    }

#ifndef BOOST_LCAST_NO_WCHAR_T
    template <typename Target>
    inline Target lexical_cast(const wchar_t* chars, std::size_t count)
    {
        return ::sofa::pbrpc::boost::lexical_cast<Target>(
            ::sofa::pbrpc::boost::iterator_range<const wchar_t*>(chars, chars + count)
        );
    }
#endif
#ifndef BOOST_NO_CXX11_CHAR16_T
    template <typename Target>
    inline Target lexical_cast(const char16_t* chars, std::size_t count)
    {
        return ::sofa::pbrpc::boost::lexical_cast<Target>(
            ::sofa::pbrpc::boost::iterator_range<const char16_t*>(chars, chars + count)
        );
    }
#endif
#ifndef BOOST_NO_CXX11_CHAR32_T
    template <typename Target>
    inline Target lexical_cast(const char32_t* chars, std::size_t count)
    {
        return ::sofa::pbrpc::boost::lexical_cast<Target>(
            ::sofa::pbrpc::boost::iterator_range<const char32_t*>(chars, chars + count)
        );
    }
#endif

} // namespace boost
} // namespace pbrpc
} // namespace sofa

#undef BOOST_LCAST_NO_WCHAR_T

#endif // SOFA_PBRPC_BOOST_LEXICAL_CAST_INCLUDED

