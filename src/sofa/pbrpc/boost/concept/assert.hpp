// Copyright David Abrahams 2006. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef SOFA_PBRPC_BOOST_CONCEPT_ASSERT_DWA2006430_HPP
#define SOFA_PBRPC_BOOST_CONCEPT_ASSERT_DWA2006430_HPP

# include <sofa/pbrpc/boost/config.hpp>
# include <sofa/pbrpc/boost/detail/workaround.hpp>

// The old protocol used a constraints() member function in concept
// checking classes.  If the compiler supports SFINAE, we can detect
// that function and seamlessly support the old concept checking
// classes.  In this release, backward compatibility with the old
// concept checking classes is enabled by default, where available.
// The old protocol is deprecated, though, and backward compatibility
// will no longer be the default in the next release.

# if !defined(BOOST_NO_OLD_CONCEPT_SUPPORT)                                         \
    && !defined(BOOST_NO_SFINAE)                                                    \
                                                                                    \
    && !(BOOST_WORKAROUND(__GNUC__, == 3) && BOOST_WORKAROUND(__GNUC_MINOR__, < 4))

// Note: gcc-2.96 through 3.3.x have some SFINAE, but no ability to
// check for the presence of particularmember functions.

#  define BOOST_OLD_CONCEPT_SUPPORT

# endif

#include <sofa/pbrpc/boost/concept/detail/general.hpp>


  // Usage, in class or function context:
  //
  //     BOOST_CONCEPT_ASSERT((UnaryFunctionConcept<F,bool,int>));
  //
# define BOOST_CONCEPT_ASSERT(ModelInParens) \
    BOOST_CONCEPT_ASSERT_FN(void(*)ModelInParens)

#endif // SOFA_PBRPC_BOOST_CONCEPT_ASSERT_DWA2006430_HPP
