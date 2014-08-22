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

#ifndef BOOST_LEXICAL_CAST_TRY_LEXICAL_CONVERT_HPP
#define BOOST_LEXICAL_CAST_TRY_LEXICAL_CONVERT_HPP

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#if defined(BOOST_NO_STRINGSTREAM) || defined(BOOST_NO_STD_WSTRING)
#define BOOST_LCAST_NO_WCHAR_T
#endif



#include <climits>
#include <cstddef>
#include <string>
#include <cstring>
#include <cstdio>
#include <boost/limits.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/ice.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/static_assert.hpp>
#include <boost/detail/lcast_precision.hpp>
#include <boost/detail/workaround.hpp>


#ifndef BOOST_NO_STD_LOCALE
#   include <locale>
#else
#   ifndef BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
        // Getting error at this point means, that your STL library is old/lame/misconfigured.
        // If nothing can be done with STL library, define BOOST_LEXICAL_CAST_ASSUME_C_LOCALE,
        // but beware: lexical_cast will understand only 'C' locale delimeters and thousands
        // separators.
#       error "Unable to use <locale> header. Define BOOST_LEXICAL_CAST_ASSUME_C_LOCALE to force "
#       error "boost::lexical_cast to use only 'C' locale during conversions."
#   endif
#endif

#ifdef BOOST_NO_STRINGSTREAM
#include <strstream>
#else
#include <sstream>
#endif

#include <boost/lexical_cast/detail/widest_char.hpp>
#include <boost/lexical_cast/detail/is_character.hpp>
#include <boost/lexical_cast/detail/lcast_char_constants.hpp>
#include <boost/lexical_cast/detail/lcast_unsigned_converters.hpp>
#include <boost/lexical_cast/detail/inf_nan.hpp>


#include <boost/lexical_cast/detail/converter_numeric.hpp>

#include <cmath>
#include <istream>

#ifndef BOOST_NO_CXX11_HDR_ARRAY
#include <array>
#endif

#include <boost/array.hpp>
#include <boost/type_traits/make_unsigned.hpp>
#include <boost/type_traits/is_signed.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <boost/type_traits/has_left_shift.hpp>
#include <boost/type_traits/has_right_shift.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/integer.hpp>
#include <boost/detail/basic_pointerbuf.hpp>
#include <boost/noncopyable.hpp>
#ifndef BOOST_NO_CWCHAR
#   include <cwchar>
#endif

namespace boost {

    namespace detail // normalize_single_byte_char<Char>
    {
        // Converts signed/unsigned char to char
        template < class Char >
        struct normalize_single_byte_char 
        {
            typedef Char type;
        };

        template <>
        struct normalize_single_byte_char< signed char >
        {
            typedef char type;
        };

        template <>
        struct normalize_single_byte_char< unsigned char >
        {
            typedef char type;
        };
    }

    namespace detail // deduce_character_type_later<T>
    {
        // Helper type, meaning that stram character for T must be deduced 
        // at Stage 2 (See deduce_source_char<T> and deduce_target_char<T>)
        template < class T > struct deduce_character_type_later {};
    }

    namespace detail // stream_char_common<T>
    {
        // Selectors to choose stream character type (common for Source and Target)
        // Returns one of char, wchar_t, char16_t, char32_t or deduce_character_type_later<T> types
        // Executed on Stage 1 (See deduce_source_char<T> and deduce_target_char<T>)
        template < typename Type >
        struct stream_char_common: public boost::mpl::if_c<
            boost::detail::is_character< Type >::value,
            Type,
            boost::detail::deduce_character_type_later< Type >
        > {};

        template < typename Char >
        struct stream_char_common< Char* >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< Char* >
        > {};

        template < typename Char >
        struct stream_char_common< const Char* >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< const Char* >
        > {};

        template < typename Char >
        struct stream_char_common< boost::iterator_range< Char* > >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< boost::iterator_range< Char* > >
        > {};
    
        template < typename Char >
        struct stream_char_common< boost::iterator_range< const Char* > >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< boost::iterator_range< const Char* > >
        > {};

        template < class Char, class Traits, class Alloc >
        struct stream_char_common< std::basic_string< Char, Traits, Alloc > >
        {
            typedef Char type;
        };

        template < class Char, class Traits, class Alloc >
        struct stream_char_common< boost::container::basic_string< Char, Traits, Alloc > >
        {
            typedef Char type;
        };

        template < typename Char, std::size_t N >
        struct stream_char_common< boost::array< Char, N > >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< boost::array< Char, N > >
        > {};

        template < typename Char, std::size_t N >
        struct stream_char_common< boost::array< const Char, N > >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< boost::array< const Char, N > >
        > {};

#ifndef BOOST_NO_CXX11_HDR_ARRAY
        template < typename Char, std::size_t N >
        struct stream_char_common< std::array<Char, N > >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< std::array< Char, N > >
        > {};

        template < typename Char, std::size_t N >
        struct stream_char_common< std::array< const Char, N > >: public boost::mpl::if_c<
            boost::detail::is_character< Char >::value,
            Char,
            boost::detail::deduce_character_type_later< std::array< const Char, N > >
        > {};
#endif

#ifdef BOOST_HAS_INT128
        template <> struct stream_char_common< boost::int128_type >: public boost::mpl::identity< char > {};
        template <> struct stream_char_common< boost::uint128_type >: public boost::mpl::identity< char > {};
#endif

#if !defined(BOOST_LCAST_NO_WCHAR_T) && defined(BOOST_NO_INTRINSIC_WCHAR_T)
        template <>
        struct stream_char_common< wchar_t >
        {
            typedef char type;
        };
#endif
    }

    namespace detail // deduce_source_char_impl<T>
    {
        // If type T is `deduce_character_type_later` type, then tries to deduce
        // character type using boost::has_left_shift<T> metafunction.
        // Otherwise supplied type T is a character type, that must be normalized
        // using normalize_single_byte_char<Char>.
        // Executed at Stage 2  (See deduce_source_char<T> and deduce_target_char<T>)
        template < class Char > 
        struct deduce_source_char_impl
        { 
            typedef BOOST_DEDUCED_TYPENAME boost::detail::normalize_single_byte_char< Char >::type type; 
        };
        
        template < class T > 
        struct deduce_source_char_impl< deduce_character_type_later< T > > 
        {
            typedef boost::has_left_shift< std::basic_ostream< char >, T > result_t;

#if defined(BOOST_LCAST_NO_WCHAR_T)
            BOOST_STATIC_ASSERT_MSG((result_t::value), 
                "Source type is not std::ostream`able and std::wostream`s are not supported by your STL implementation");
            typedef char type;
#else
            typedef BOOST_DEDUCED_TYPENAME boost::mpl::if_c<
                result_t::value, char, wchar_t
            >::type type;

            BOOST_STATIC_ASSERT_MSG((result_t::value || boost::has_left_shift< std::basic_ostream< type >, T >::value), 
                "Source type is neither std::ostream`able nor std::wostream`able");
#endif
        };
    }

    namespace detail  // deduce_target_char_impl<T>
    {
        // If type T is `deduce_character_type_later` type, then tries to deduce
        // character type using boost::has_right_shift<T> metafunction.
        // Otherwise supplied type T is a character type, that must be normalized
        // using normalize_single_byte_char<Char>.
        // Executed at Stage 2  (See deduce_source_char<T> and deduce_target_char<T>)
        template < class Char > 
        struct deduce_target_char_impl 
        { 
            typedef BOOST_DEDUCED_TYPENAME normalize_single_byte_char< Char >::type type; 
        };
        
        template < class T > 
        struct deduce_target_char_impl< deduce_character_type_later<T> > 
        { 
            typedef boost::has_right_shift<std::basic_istream<char>, T > result_t;

#if defined(BOOST_LCAST_NO_WCHAR_T)
            BOOST_STATIC_ASSERT_MSG((result_t::value), 
                "Target type is not std::istream`able and std::wistream`s are not supported by your STL implementation");
            typedef char type;
#else
            typedef BOOST_DEDUCED_TYPENAME boost::mpl::if_c<
                result_t::value, char, wchar_t
            >::type type;
            
            BOOST_STATIC_ASSERT_MSG((result_t::value || boost::has_right_shift<std::basic_istream<wchar_t>, T >::value), 
                "Target type is neither std::istream`able nor std::wistream`able");
#endif
        };
    } 

    namespace detail  // deduce_target_char<T> and deduce_source_char<T>
    {
        // We deduce stream character types in two stages.
        //
        // Stage 1 is common for Target and Source. At Stage 1 we get 
        // non normalized character type (may contain unsigned/signed char)
        // or deduce_character_type_later<T> where T is the original type.
        // Stage 1 is executed by stream_char_common<T>
        //
        // At Stage 2 we normalize character types or try to deduce character 
        // type using metafunctions. 
        // Stage 2 is executed by deduce_target_char_impl<T> and 
        // deduce_source_char_impl<T>
        //
        // deduce_target_char<T> and deduce_source_char<T> functions combine 
        // both stages

        template < class T >
        struct deduce_target_char
        {
            typedef BOOST_DEDUCED_TYPENAME stream_char_common< T >::type stage1_type;
            typedef BOOST_DEDUCED_TYPENAME deduce_target_char_impl< stage1_type >::type stage2_type;

            typedef stage2_type type;
        };

        template < class T >
        struct deduce_source_char
        {
            typedef BOOST_DEDUCED_TYPENAME stream_char_common< T >::type stage1_type;
            typedef BOOST_DEDUCED_TYPENAME deduce_source_char_impl< stage1_type >::type stage2_type;

            typedef stage2_type type;
        };
    }

    namespace detail // extract_char_traits template
    {
        // We are attempting to get char_traits<> from T
        // template parameter. Otherwise we'll be using std::char_traits<Char>
        template < class Char, class T >
        struct extract_char_traits
                : boost::false_type
        {
            typedef std::char_traits< Char > trait_t;
        };

        template < class Char, class Traits, class Alloc >
        struct extract_char_traits< Char, std::basic_string< Char, Traits, Alloc > >
            : boost::true_type
        {
            typedef Traits trait_t;
        };

        template < class Char, class Traits, class Alloc>
        struct extract_char_traits< Char, boost::container::basic_string< Char, Traits, Alloc > >
            : boost::true_type
        {
            typedef Traits trait_t;
        };
    }

    namespace detail // array_to_pointer_decay<T>
    {
        template<class T>
        struct array_to_pointer_decay
        {
            typedef T type;
        };

        template<class T, std::size_t N>
        struct array_to_pointer_decay<T[N]>
        {
            typedef const T * type;
        };
    }

    namespace detail // is_this_float_conversion_optimized<Float, Char>
    {
        // this metafunction evaluates to true, if we have optimized comnversion 
        // from Float type to Char array. 
        // Must be in sync with lexical_stream_limited_src<Char, ...>::shl_real_type(...)
        template <typename Float, typename Char>
        struct is_this_float_conversion_optimized 
        {
            typedef boost::type_traits::ice_and<
                boost::is_float<Float>::value,
#if !defined(BOOST_LCAST_NO_WCHAR_T) && !defined(BOOST_NO_SWPRINTF) && !defined(__MINGW32__)
                boost::type_traits::ice_or<
                    boost::type_traits::ice_eq<sizeof(Char), sizeof(char) >::value,
                    boost::is_same<Char, wchar_t>::value
                >::value
#else
                boost::type_traits::ice_eq<sizeof(Char), sizeof(char) >::value
#endif
            > result_type;

            BOOST_STATIC_CONSTANT(bool, value = (result_type::value) );
        };
    }
    
    namespace detail // lcast_src_length
    {
        // Return max. length of string representation of Source;
        template< class Source,         // Source type of lexical_cast.
                  class Enable = void   // helper type
                >
        struct lcast_src_length
        {
            BOOST_STATIC_CONSTANT(std::size_t, value = 1);
        };

        // Helper for integral types.
        // Notes on length calculation:
        // Max length for 32bit int with grouping "\1" and thousands_sep ',':
        // "-2,1,4,7,4,8,3,6,4,7"
        //  ^                    - is_signed
        //   ^                   - 1 digit not counted by digits10
        //    ^^^^^^^^^^^^^^^^^^ - digits10 * 2
        //
        // Constant is_specialized is used instead of constant 1
        // to prevent buffer overflow in a rare case when
        // <boost/limits.hpp> doesn't add missing specialization for
        // numeric_limits<T> for some integral type T.
        // When is_specialized is false, the whole expression is 0.
        template <class Source>
        struct lcast_src_length<
                    Source, BOOST_DEDUCED_TYPENAME boost::enable_if<boost::is_integral<Source> >::type
                >
        {
#ifndef BOOST_NO_LIMITS_COMPILE_TIME_CONSTANTS
            BOOST_STATIC_CONSTANT(std::size_t, value =
                  std::numeric_limits<Source>::is_signed +
                  std::numeric_limits<Source>::is_specialized + /* == 1 */
                  std::numeric_limits<Source>::digits10 * 2
              );
#else
            BOOST_STATIC_CONSTANT(std::size_t, value = 156);
            BOOST_STATIC_ASSERT(sizeof(Source) * CHAR_BIT <= 256);
#endif
        };

#ifndef BOOST_LCAST_NO_COMPILE_TIME_PRECISION
        // Helper for floating point types.
        // -1.23456789e-123456
        // ^                   sign
        //  ^                  leading digit
        //   ^                 decimal point 
        //    ^^^^^^^^         lcast_precision<Source>::value
        //            ^        "e"
        //             ^       exponent sign
        //              ^^^^^^ exponent (assumed 6 or less digits)
        // sign + leading digit + decimal point + "e" + exponent sign == 5
        template<class Source>
        struct lcast_src_length<
                Source, BOOST_DEDUCED_TYPENAME boost::enable_if<boost::is_float<Source> >::type
            >
        {
            BOOST_STATIC_ASSERT(
                    std::numeric_limits<Source>::max_exponent10 <=  999999L &&
                    std::numeric_limits<Source>::min_exponent10 >= -999999L
                );

            BOOST_STATIC_CONSTANT(std::size_t, value =
                    5 + lcast_precision<Source>::value + 6
                );
        };
#endif // #ifndef BOOST_LCAST_NO_COMPILE_TIME_PRECISION
    }

    namespace detail // lexical_cast_stream_traits<Source, Target>
    {
        template <class Source, class Target>
        struct lexical_cast_stream_traits {
            typedef BOOST_DEDUCED_TYPENAME boost::detail::array_to_pointer_decay<Source>::type src;
            typedef BOOST_DEDUCED_TYPENAME boost::remove_cv<src>::type            no_cv_src;
                
            typedef boost::detail::deduce_source_char<no_cv_src>                           deduce_src_char_metafunc;
            typedef BOOST_DEDUCED_TYPENAME deduce_src_char_metafunc::type           src_char_t;
            typedef BOOST_DEDUCED_TYPENAME boost::detail::deduce_target_char<Target>::type target_char_t;
                
            typedef BOOST_DEDUCED_TYPENAME boost::detail::widest_char<
                target_char_t, src_char_t
            >::type char_type;

#if !defined(BOOST_NO_CXX11_CHAR16_T) && defined(BOOST_NO_CXX11_UNICODE_LITERALS)
            BOOST_STATIC_ASSERT_MSG(( !boost::is_same<char16_t, src_char_t>::value
                                        && !boost::is_same<char16_t, target_char_t>::value),
                "Your compiler does not have full support for char16_t" );
#endif
#if !defined(BOOST_NO_CXX11_CHAR32_T) && defined(BOOST_NO_CXX11_UNICODE_LITERALS)
            BOOST_STATIC_ASSERT_MSG(( !boost::is_same<char32_t, src_char_t>::value
                                        && !boost::is_same<char32_t, target_char_t>::value),
                "Your compiler does not have full support for char32_t" );
#endif

            typedef BOOST_DEDUCED_TYPENAME boost::mpl::if_c<
                boost::detail::extract_char_traits<char_type, Target>::value,
                BOOST_DEDUCED_TYPENAME boost::detail::extract_char_traits<char_type, Target>,
                BOOST_DEDUCED_TYPENAME boost::detail::extract_char_traits<char_type, no_cv_src>
            >::type::trait_t traits;

            typedef boost::type_traits::ice_and<
                boost::is_same<char, src_char_t>::value,                                  // source is not a wide character based type
                boost::type_traits::ice_ne<sizeof(char), sizeof(target_char_t) >::value,  // target type is based on wide character
                boost::type_traits::ice_not<
                    boost::detail::is_character<no_cv_src>::value                     // single character widening is optimized
                >::value                                                                  // and does not requires stringbuffer
            >   is_string_widening_required_t;

            typedef boost::type_traits::ice_not< boost::type_traits::ice_or<
                boost::is_integral<no_cv_src>::value,
                boost::detail::is_this_float_conversion_optimized<no_cv_src, char_type >::value,
                boost::detail::is_character<
                    BOOST_DEDUCED_TYPENAME deduce_src_char_metafunc::stage1_type          // if we did not get character type at stage1
                >::value                                                                  // then we have no optimization for that type
            >::value >   is_source_input_not_optimized_t;

            // If we have an optimized conversion for
            // Source, we do not need to construct stringbuf.
            BOOST_STATIC_CONSTANT(bool, requires_stringbuf = 
                (boost::type_traits::ice_or<
                    is_string_widening_required_t::value, is_source_input_not_optimized_t::value
                >::value)
            );
            
            typedef boost::detail::lcast_src_length<no_cv_src> len_t;
        };
    }
 
    namespace detail // lcast_ret_float
    {

// Silence buggy MS warnings like C4244: '+=' : conversion from 'int' to 'unsigned short', possible loss of data 
#if defined(_MSC_VER) && (_MSC_VER == 1400) 
#  pragma warning(push) 
#  pragma warning(disable:4244) 
#endif 
        template <class T>
        struct mantissa_holder_type
        {
            /* Can not be used with this type */
        };

        template <>
        struct mantissa_holder_type<float>
        {
            typedef unsigned int type;
            typedef double       wide_result_t;
        };

        template <>
        struct mantissa_holder_type<double>
        {
#ifndef BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
            typedef long double  wide_result_t;
#if defined(BOOST_HAS_LONG_LONG)
            typedef boost::ulong_long_type type;
#elif defined(BOOST_HAS_MS_INT64)
            typedef unsigned __int64 type;
#endif
#endif
        };

        template<class Traits, class T, class CharT>
        inline bool lcast_ret_float(T& value, const CharT* begin, const CharT* const end)
        {
            value = static_cast<T>(0);
            if (begin == end) return false;
            if (parse_inf_nan(begin, end, value)) return true;

            CharT const czero = lcast_char_constants<CharT>::zero;
            CharT const minus = lcast_char_constants<CharT>::minus;
            CharT const plus = lcast_char_constants<CharT>::plus;
            CharT const capital_e = lcast_char_constants<CharT>::capital_e;
            CharT const lowercase_e = lcast_char_constants<CharT>::lowercase_e;
            
            /* Getting the plus/minus sign */
            bool const has_minus = Traits::eq(*begin, minus);
            if (has_minus || Traits::eq(*begin, plus)) {
                ++ begin;
                if (begin == end) return false;
            }

#ifndef BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
            std::locale loc;
            typedef std::numpunct<CharT> numpunct;
            numpunct const& np = BOOST_USE_FACET(numpunct, loc);
            std::string const grouping(
                    (loc == std::locale::classic())
                    ? std::string()
                    : np.grouping()
            );
            std::string::size_type const grouping_size = grouping.size();
            CharT const thousands_sep = static_cast<CharT>(grouping_size ? np.thousands_sep() : 0);
            CharT const decimal_point = np.decimal_point();
            bool found_grouping = false;
            std::string::size_type last_grouping_pos = grouping_size - 1;
#else
            CharT const decimal_point = lcast_char_constants<CharT>::c_decimal_separator;
#endif

            bool found_decimal = false;
            bool found_number_before_exp = false;
            typedef int pow_of_10_t;
            pow_of_10_t pow_of_10 = 0;

            typedef BOOST_DEDUCED_TYPENAME mantissa_holder_type<T>::type mantissa_type;
            mantissa_type mantissa=0;
            bool is_mantissa_full = false;
            char length_since_last_delim = 0;

            while (begin != end) {
                if (found_decimal) {
                    /* We allow no thousand_separators after decimal point */

                    const mantissa_type tmp_sub_value = static_cast<mantissa_type>(*begin - czero);
                    if (Traits::eq(*begin, lowercase_e) || Traits::eq(*begin, capital_e)) break;
                    if ( *begin < czero || *begin >= czero + 10 ) return false;
                    if (    is_mantissa_full
                            || ((std::numeric_limits<mantissa_type>::max)() - tmp_sub_value) / 10u  < mantissa
                            ) {
                        is_mantissa_full = true;
                        ++ begin;
                        continue;
                    }

                    -- pow_of_10;
                    mantissa = static_cast<mantissa_type>(mantissa * 10 + tmp_sub_value);

                    found_number_before_exp = true;
                } else {

                    if (*begin >= czero && *begin < czero + 10) {

                        /* Checking for mantissa overflow. If overflow will
                         * occur, them we only increase multiplyer
                         */
                        const mantissa_type tmp_sub_value = static_cast<mantissa_type>(*begin - czero);
                        if(     is_mantissa_full
                                || ((std::numeric_limits<mantissa_type>::max)() - tmp_sub_value) / 10u  < mantissa
                            )
                        {
                            is_mantissa_full = true;
                            ++ pow_of_10;
                        } else {
                            mantissa = static_cast<mantissa_type>(mantissa * 10 + tmp_sub_value);
                        }

                        found_number_before_exp = true;
                        ++ length_since_last_delim;
                    } else if (Traits::eq(*begin, decimal_point) || Traits::eq(*begin, lowercase_e) || Traits::eq(*begin, capital_e)) {
#ifndef BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
                        /* If ( we need to check grouping
                         *      and (   grouping missmatches
                         *              or grouping position is incorrect
                         *              or we are using the grouping position 0 twice
                         *           )
                         *    ) then return error
                         */
                        if( grouping_size && found_grouping
                            && (
                                   length_since_last_delim != grouping[0]
                                   || last_grouping_pos>1
                                   || (last_grouping_pos==0 && grouping_size>1)
                                )
                           ) return false;
#endif

                        if (Traits::eq(*begin, decimal_point)) {
                            ++ begin;
                            found_decimal = true;
                            if (!found_number_before_exp && begin==end) return false;
                            continue;
                        } else {
                            if (!found_number_before_exp) return false;
                            break;
                        }
                    }
#ifndef BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
                    else if (grouping_size && Traits::eq(*begin, thousands_sep)){
                        if(found_grouping)
                        {
                            /* It is not he first time, when we find thousands separator,
                             * so we need to chek, is the distance between two groupings
                             * equal to grouping[last_grouping_pos] */

                            if (length_since_last_delim != grouping[last_grouping_pos] )
                            {
                                if (!last_grouping_pos) return false;
                                else
                                {
                                    -- last_grouping_pos;
                                    if (length_since_last_delim != grouping[last_grouping_pos]) return false;
                                }
                            } else
                                /* We are calling the grouping[0] twice, when grouping size is more than 1 */
                                if (grouping_size>1u && last_grouping_pos+1<grouping_size) return false;

                        } else {
                            /* Delimiter at the begining ',000' */
                            if (!length_since_last_delim) return false;

                            found_grouping = true;
                            if (length_since_last_delim > grouping[last_grouping_pos] ) return false;
                        }

                        length_since_last_delim = 0;
                        ++ begin;

                        /* Delimiter at the end '100,' */
                        if (begin == end) return false;
                        continue;
                    }
#endif
                    else return false;
                }

                ++begin;
            }

            // Exponent found
            if (begin != end && (Traits::eq(*begin, lowercase_e) || Traits::eq(*begin, capital_e))) {
                ++ begin;
                if (begin == end) return false;

                bool const exp_has_minus = Traits::eq(*begin, minus);
                if (exp_has_minus || Traits::eq(*begin, plus)) {
                    ++ begin;
                    if (begin == end) return false;
                }

                pow_of_10_t exp_pow_of_10 = 0;
                while (begin != end) {
                    pow_of_10_t const sub_value = *begin - czero;

                    if ( *begin < czero || *begin >= czero + 10
                         || ((std::numeric_limits<pow_of_10_t>::max)() - sub_value) / 10 < exp_pow_of_10)
                        return false;

                    exp_pow_of_10 *= 10;
                    exp_pow_of_10 += sub_value;
                    ++ begin;
                };

                if (exp_has_minus) {
                    if ((std::numeric_limits<pow_of_10_t>::min)() + exp_pow_of_10 > pow_of_10)
                        return false;   // failed overflow check
                    pow_of_10 -= exp_pow_of_10;
                } else {
                    if ((std::numeric_limits<pow_of_10_t>::max)() - exp_pow_of_10 < pow_of_10)
                        return false;   // failed overflow check
                    pow_of_10 += exp_pow_of_10;
                }
            }

            /* We need a more accurate algorithm... We can not use current algorithm
             * with long doubles (and with doubles if sizeof(double)==sizeof(long double)).
             */
            typedef BOOST_DEDUCED_TYPENAME mantissa_holder_type<T>::wide_result_t wide_result_t;
            const wide_result_t result = std::pow(static_cast<wide_result_t>(10.0), pow_of_10) * mantissa;
            value = static_cast<T>( has_minus ? (boost::math::changesign)(result) : result);

            return !((boost::math::isinf)(value) || (boost::math::isnan)(value));
        }
// Unsilence buggy MS warnings like C4244: '+=' : conversion from 'int' to 'unsigned short', possible loss of data 
#if defined(_MSC_VER) && (_MSC_VER == 1400) 
#  pragma warning(pop) 
#endif 
    }

    namespace detail // basic_unlockedbuf
    {
        // acts as a stream buffer which wraps around a pair of pointers
        // and gives acces to internals
        template <class BufferType, class CharT>
        class basic_unlockedbuf : public basic_pointerbuf<CharT, BufferType> {
        public:
           typedef basic_pointerbuf<CharT, BufferType> base_type;
           typedef BOOST_DEDUCED_TYPENAME base_type::streamsize streamsize;

#ifndef BOOST_NO_USING_TEMPLATE
            using base_type::pptr;
            using base_type::pbase;
            using base_type::setbuf;
#else
            charT* pptr() const { return base_type::pptr(); }
            charT* pbase() const { return base_type::pbase(); }
            BufferType* setbuf(char_type* s, streamsize n) { return base_type::setbuf(s, n); }
#endif
        };
    }

    namespace detail
    {
        struct do_not_construct_out_stream_t{};
        
        template <class CharT, class Traits>
        struct out_stream_helper_trait {
#if defined(BOOST_NO_STRINGSTREAM)
            typedef std::ostrstream                                 out_stream_t;
            typedef void                                            buffer_t;
#elif defined(BOOST_NO_STD_LOCALE)
            typedef std::ostringstream                              out_stream_t;
            typedef basic_unlockedbuf<std::streambuf, char>         buffer_t;
#else
            typedef std::basic_ostringstream<CharT, Traits> 
                out_stream_t;
            typedef basic_unlockedbuf<std::basic_streambuf<CharT, Traits>, CharT>  
                buffer_t;
#endif
        };   
    }

    namespace detail // optimized stream wrappers
    {
        template< class CharT // a result of widest_char transformation
                , class Traits
                , bool RequiresStringbuffer
                , std::size_t CharacterBufferSize
                >
        class lexical_istream_limited_src: boost::noncopyable {
            typedef BOOST_DEDUCED_TYPENAME out_stream_helper_trait<CharT, Traits>::buffer_t
                buffer_t;

            typedef BOOST_DEDUCED_TYPENAME out_stream_helper_trait<CharT, Traits>::out_stream_t
                out_stream_t;
    
            typedef BOOST_DEDUCED_TYPENAME boost::mpl::if_c<
                RequiresStringbuffer,
                out_stream_t,
                do_not_construct_out_stream_t
            >::type deduced_out_stream_t;

            // A string representation of Source is written to `buffer`.
            deduced_out_stream_t out_stream;
            CharT   buffer[CharacterBufferSize];

            // After the `operator <<`  finishes, `[start, finish)` is
            // the range to output by `operator >>` 
            const CharT*  start;
            const CharT*  finish;

        public:
            lexical_istream_limited_src() BOOST_NOEXCEPT
              : start(buffer)
              , finish(buffer + CharacterBufferSize)
            {}
    
            const CharT* cbegin() const BOOST_NOEXCEPT {
                return start;
            }

            const CharT* cend() const BOOST_NOEXCEPT {
                return finish;
            }

        private:
            // Undefined:
            lexical_istream_limited_src(lexical_istream_limited_src const&);
            void operator=(lexical_istream_limited_src const&);

/************************************ HELPER FUNCTIONS FOR OPERATORS << ( ... ) ********************************/
            bool shl_char(CharT ch) BOOST_NOEXCEPT {
                Traits::assign(buffer[0], ch);
                finish = start + 1;
                return true;
            }

#ifndef BOOST_LCAST_NO_WCHAR_T
            template <class T>
            bool shl_char(T ch) {
                BOOST_STATIC_ASSERT_MSG(( sizeof(T) <= sizeof(CharT)) ,
                    "boost::lexical_cast does not support narrowing of char types."
                    "Use boost::locale instead" );
#ifndef BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
                std::locale loc;
                CharT const w = BOOST_USE_FACET(std::ctype<CharT>, loc).widen(ch);
#else
                CharT const w = static_cast<CharT>(ch);
#endif
                Traits::assign(buffer[0], w);
                finish = start + 1;
                return true;
            }
#endif

            bool shl_char_array(CharT const* str) BOOST_NOEXCEPT {
                start = str;
                finish = start + Traits::length(str);
                return true;
            }

            template <class T>
            bool shl_char_array(T const* str) {
                BOOST_STATIC_ASSERT_MSG(( sizeof(T) <= sizeof(CharT)),
                    "boost::lexical_cast does not support narrowing of char types."
                    "Use boost::locale instead" );
                return shl_input_streamable(str);
            }
            
            bool shl_char_array_limited(CharT const* str, std::size_t max_size) BOOST_NOEXCEPT {
                start = str;
                finish = std::find(start, start + max_size, Traits::to_char_type(0));
                return true;
            }

            template<typename InputStreamable>
            bool shl_input_streamable(InputStreamable& input) {
#if defined(BOOST_NO_STRINGSTREAM) || defined(BOOST_NO_STD_LOCALE)
                // If you have compilation error at this point, than your STL library
                // does not support such conversions. Try updating it.
                BOOST_STATIC_ASSERT((boost::is_same<char, CharT>::value));
#endif

#ifndef BOOST_NO_EXCEPTIONS
                out_stream.exceptions(std::ios::badbit);
                try {
#endif
                bool const result = !(out_stream << input).fail();
                const buffer_t* const p = static_cast<buffer_t*>(
                    static_cast<std::basic_streambuf<CharT, Traits>*>(out_stream.rdbuf())
                );
                start = p->pbase();
                finish = p->pptr();
                return result;
#ifndef BOOST_NO_EXCEPTIONS
                } catch (const ::std::ios_base::failure& /*f*/) {
                    return false;
                }
#endif
            }

            template <class T>
            inline bool shl_unsigned(const T n) {
                CharT* tmp_finish = buffer + CharacterBufferSize;
                start = lcast_put_unsigned<Traits, T, CharT>(n, tmp_finish).convert();
                finish = tmp_finish;
                return true;
            }

            template <class T>
            inline bool shl_signed(const T n) {
                CharT* tmp_finish = buffer + CharacterBufferSize;
                typedef BOOST_DEDUCED_TYPENAME boost::make_unsigned<T>::type utype;
                CharT* tmp_start = lcast_put_unsigned<Traits, utype, CharT>(lcast_to_unsigned(n), tmp_finish).convert();
                if (n < 0) {
                    --tmp_start;
                    CharT const minus = lcast_char_constants<CharT>::minus;
                    Traits::assign(*tmp_start, minus);
                }
                start = tmp_start;
                finish = tmp_finish;
                return true;
            }

            template <class T, class SomeCharT>
            bool shl_real_type(const T& val, SomeCharT* /*begin*/) {
                lcast_set_precision(out_stream, &val);
                return shl_input_streamable(val);
            }

            bool shl_real_type(float val, char* begin) {
                using namespace std;
                const double val_as_double = val;
                finish = start +
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__SGI_STL_PORT) && !defined(_STLPORT_VERSION)
                    sprintf_s(begin, CharacterBufferSize,
#else
                    sprintf(begin, 
#endif
                    "%.*g", static_cast<int>(boost::detail::lcast_get_precision<float>()), val_as_double);
                return finish > start;
            }

            bool shl_real_type(double val, char* begin) {
                using namespace std;
                finish = start +
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__SGI_STL_PORT) && !defined(_STLPORT_VERSION)
                    sprintf_s(begin, CharacterBufferSize,
#else
                    sprintf(begin, 
#endif
                    "%.*g", static_cast<int>(boost::detail::lcast_get_precision<double>()), val);
                return finish > start;
            }

#ifndef __MINGW32__
            bool shl_real_type(long double val, char* begin) {
                using namespace std;
                finish = start +
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__SGI_STL_PORT) && !defined(_STLPORT_VERSION)
                    sprintf_s(begin, CharacterBufferSize,
#else
                    sprintf(begin, 
#endif
                    "%.*Lg", static_cast<int>(boost::detail::lcast_get_precision<long double>()), val );
                return finish > start;
            }
#endif


#if !defined(BOOST_LCAST_NO_WCHAR_T) && !defined(BOOST_NO_SWPRINTF) && !defined(__MINGW32__)
            bool shl_real_type(float val, wchar_t* begin) {
                using namespace std;
                const double val_as_double = val;
                finish = start + swprintf(begin, CharacterBufferSize,
                                       L"%.*g",
                                       static_cast<int>(boost::detail::lcast_get_precision<float >()),
                                       val_as_double );
                return finish > start;
            }

            bool shl_real_type(double val, wchar_t* begin) {
                using namespace std;
                finish = start + swprintf(begin, CharacterBufferSize,
                                          L"%.*g", static_cast<int>(boost::detail::lcast_get_precision<double >()), val );
                return finish > start;
            }

            bool shl_real_type(long double val, wchar_t* begin) {
                using namespace std;
                finish = start + swprintf(begin, CharacterBufferSize,
                                          L"%.*Lg", static_cast<int>(boost::detail::lcast_get_precision<long double >()), val );
                return finish > start;
            }
#endif
            template <class T>
            bool shl_real(T val) {
                CharT* tmp_finish = buffer + CharacterBufferSize;
                if (put_inf_nan(buffer, tmp_finish, val)) {
                    finish = tmp_finish;
                    return true;
                }

                return shl_real_type(val, static_cast<CharT*>(buffer));
            }

/************************************ OPERATORS << ( ... ) ********************************/
        public:
            template<class Alloc>
            bool operator<<(std::basic_string<CharT,Traits,Alloc> const& str) BOOST_NOEXCEPT {
                start = str.data();
                finish = start + str.length();
                return true;
            }

            template<class Alloc>
            bool operator<<(boost::container::basic_string<CharT,Traits,Alloc> const& str) BOOST_NOEXCEPT {
                start = str.data();
                finish = start + str.length();
                return true;
            }

            bool operator<<(bool value) BOOST_NOEXCEPT {
                CharT const czero = lcast_char_constants<CharT>::zero;
                Traits::assign(buffer[0], Traits::to_char_type(czero + value));
                finish = start + 1;
                return true;
            }

            template <class C>
            BOOST_DEDUCED_TYPENAME boost::disable_if<boost::is_const<C>, bool>::type 
            operator<<(const iterator_range<C*>& rng) BOOST_NOEXCEPT {
                return (*this) << iterator_range<const C*>(rng.begin(), rng.end());
            }
            
            bool operator<<(const iterator_range<const CharT*>& rng) BOOST_NOEXCEPT {
                start = rng.begin();
                finish = rng.end();
                return true; 
            }

            bool operator<<(const iterator_range<const signed char*>& rng) BOOST_NOEXCEPT {
                return (*this) << iterator_range<const char*>(
                    reinterpret_cast<const char*>(rng.begin()),
                    reinterpret_cast<const char*>(rng.end())
                );
            }

            bool operator<<(const iterator_range<const unsigned char*>& rng) BOOST_NOEXCEPT {
                return (*this) << iterator_range<const char*>(
                    reinterpret_cast<const char*>(rng.begin()),
                    reinterpret_cast<const char*>(rng.end())
                );
            }

            bool operator<<(char ch)                    { return shl_char(ch); }
            bool operator<<(unsigned char ch)           { return ((*this) << static_cast<char>(ch)); }
            bool operator<<(signed char ch)             { return ((*this) << static_cast<char>(ch)); }
#if !defined(BOOST_LCAST_NO_WCHAR_T)
            bool operator<<(wchar_t const* str)         { return shl_char_array(str); }
            bool operator<<(wchar_t * str)              { return shl_char_array(str); }
#ifndef BOOST_NO_INTRINSIC_WCHAR_T
            bool operator<<(wchar_t ch)                 { return shl_char(ch); }
#endif
#endif
#if !defined(BOOST_NO_CXX11_CHAR16_T) && !defined(BOOST_NO_CXX11_UNICODE_LITERALS)
            bool operator<<(char16_t ch)                { return shl_char(ch); }
            bool operator<<(char16_t * str)             { return shl_char_array(str); }
            bool operator<<(char16_t const * str)       { return shl_char_array(str); }
#endif
#if !defined(BOOST_NO_CXX11_CHAR32_T) && !defined(BOOST_NO_CXX11_UNICODE_LITERALS)
            bool operator<<(char32_t ch)                { return shl_char(ch); }
            bool operator<<(char32_t * str)             { return shl_char_array(str); }
            bool operator<<(char32_t const * str)       { return shl_char_array(str); }
#endif
            bool operator<<(unsigned char const* ch)    { return ((*this) << reinterpret_cast<char const*>(ch)); }
            bool operator<<(unsigned char * ch)         { return ((*this) << reinterpret_cast<char *>(ch)); }
            bool operator<<(signed char const* ch)      { return ((*this) << reinterpret_cast<char const*>(ch)); }
            bool operator<<(signed char * ch)           { return ((*this) << reinterpret_cast<char *>(ch)); }
            bool operator<<(char const* str)            { return shl_char_array(str); }
            bool operator<<(char* str)                  { return shl_char_array(str); }
            bool operator<<(short n)                    { return shl_signed(n); }
            bool operator<<(int n)                      { return shl_signed(n); }
            bool operator<<(long n)                     { return shl_signed(n); }
            bool operator<<(unsigned short n)           { return shl_unsigned(n); }
            bool operator<<(unsigned int n)             { return shl_unsigned(n); }
            bool operator<<(unsigned long n)            { return shl_unsigned(n); }

#if defined(BOOST_HAS_LONG_LONG)
            bool operator<<(boost::ulong_long_type n)   { return shl_unsigned(n); }
            bool operator<<(boost::long_long_type n)    { return shl_signed(n); }
#elif defined(BOOST_HAS_MS_INT64)
            bool operator<<(unsigned __int64 n)         { return shl_unsigned(n); }
            bool operator<<(         __int64 n)         { return shl_signed(n); }
#endif

#ifdef BOOST_HAS_INT128
            bool operator<<(const boost::uint128_type& n)   { return shl_unsigned(n); }
            bool operator<<(const boost::int128_type& n)    { return shl_signed(n); }
#endif
            bool operator<<(float val)                  { return shl_real(val); }
            bool operator<<(double val)                 { return shl_real(val); }
            bool operator<<(long double val)            {
#ifndef __MINGW32__
                return shl_real(val);
#else
                return shl_real(static_cast<double>(val));
#endif
            }
            
            // Adding constness to characters. Constness does not change layout
            template <class C, std::size_t N>
            BOOST_DEDUCED_TYPENAME boost::disable_if<boost::is_const<C>, bool>::type
            operator<<(boost::array<C, N> const& input) BOOST_NOEXCEPT { 
                BOOST_STATIC_ASSERT_MSG(
                    (sizeof(boost::array<const C, N>) == sizeof(boost::array<C, N>)),
                    "boost::array<C, N> and boost::array<const C, N> must have exactly the same layout."
                );
                return ((*this) << reinterpret_cast<boost::array<const C, N> const& >(input)); 
            }

            template <std::size_t N>
            bool operator<<(boost::array<const CharT, N> const& input) BOOST_NOEXCEPT { 
                return shl_char_array_limited(input.begin(), N); 
            }

            template <std::size_t N>
            bool operator<<(boost::array<const unsigned char, N> const& input) BOOST_NOEXCEPT { 
                return ((*this) << reinterpret_cast<boost::array<const char, N> const& >(input)); 
            }

            template <std::size_t N>
            bool operator<<(boost::array<const signed char, N> const& input) BOOST_NOEXCEPT { 
                return ((*this) << reinterpret_cast<boost::array<const char, N> const& >(input)); 
            }
 
#ifndef BOOST_NO_CXX11_HDR_ARRAY
            // Making a Boost.Array from std::array
            template <class C, std::size_t N>
            bool operator<<(std::array<C, N> const& input) BOOST_NOEXCEPT { 
                BOOST_STATIC_ASSERT_MSG(
                    (sizeof(std::array<C, N>) == sizeof(boost::array<C, N>)),
                    "std::array and boost::array must have exactly the same layout. "
                    "Bug in implementation of std::array or boost::array."
                );
                return ((*this) << reinterpret_cast<boost::array<C, N> const& >(input)); 
            }
#endif
            template <class InStreamable>
            bool operator<<(const InStreamable& input)  { return shl_input_streamable(input); }
        };


        template <class CharT, class Traits>
        class lexical_ostream_limited_src: boost::noncopyable {
            //`[start, finish)` is the range to output by `operator >>` 
            const CharT*        start;
            const CharT* const  finish;

        public:
            lexical_ostream_limited_src(const CharT* begin, const CharT* end) BOOST_NOEXCEPT
              : start(begin)
              , finish(end)
            {}

/************************************ HELPER FUNCTIONS FOR OPERATORS >> ( ... ) ********************************/
        private:
            template <typename Type>
            bool shr_unsigned(Type& output) {
                if (start == finish) return false;
                CharT const minus = lcast_char_constants<CharT>::minus;
                CharT const plus = lcast_char_constants<CharT>::plus;
                bool const has_minus = Traits::eq(minus, *start);

                /* We won`t use `start' any more, so no need in decrementing it after */
                if (has_minus || Traits::eq(plus, *start)) {
                    ++start;
                }

                bool const succeed = lcast_ret_unsigned<Traits, Type, CharT>(output, start, finish).convert();

                if (has_minus) {
                    output = static_cast<Type>(0u - output);
                }

                return succeed;
            }

            template <typename Type>
            bool shr_signed(Type& output) {
                if (start == finish) return false;
                CharT const minus = lcast_char_constants<CharT>::minus;
                CharT const plus = lcast_char_constants<CharT>::plus;
                typedef BOOST_DEDUCED_TYPENAME make_unsigned<Type>::type utype;
                utype out_tmp = 0;
                bool const has_minus = Traits::eq(minus, *start);

                /* We won`t use `start' any more, so no need in decrementing it after */
                if (has_minus || Traits::eq(plus, *start)) {
                    ++start;
                }

                bool succeed = lcast_ret_unsigned<Traits, utype, CharT>(out_tmp, start, finish).convert();
                if (has_minus) {
                    utype const comp_val = (static_cast<utype>(1) << std::numeric_limits<Type>::digits);
                    succeed = succeed && out_tmp<=comp_val;
                    output = static_cast<Type>(0u - out_tmp);
                } else {
                    utype const comp_val = static_cast<utype>((std::numeric_limits<Type>::max)());
                    succeed = succeed && out_tmp<=comp_val;
                    output = static_cast<Type>(out_tmp);
                }
                return succeed;
            }

            template<typename InputStreamable>
            bool shr_using_base_class(InputStreamable& output)
            {
                BOOST_STATIC_ASSERT_MSG(
                    (!boost::is_pointer<InputStreamable>::value),
                    "boost::lexical_cast can not convert to pointers"
                );

#if defined(BOOST_NO_STRINGSTREAM) || defined(BOOST_NO_STD_LOCALE)
                BOOST_STATIC_ASSERT_MSG((boost::is_same<char, CharT>::value),
                    "boost::lexical_cast can not convert, because your STL library does not "
                    "support such conversions. Try updating it."
                );
#endif
                typedef BOOST_DEDUCED_TYPENAME out_stream_helper_trait<CharT, Traits>::buffer_t
                    buffer_t;

#if defined(BOOST_NO_STRINGSTREAM)
                std::istrstream stream(start, finish - start);
#else

                buffer_t buf;
                // Usually `istream` and `basic_istream` do not modify 
                // content of buffer; `buffer_t` assures that this is true
                buf.setbuf(const_cast<CharT*>(start), finish - start);
#if defined(BOOST_NO_STD_LOCALE)
                std::istream stream(&buf);
#else
                std::basic_istream<CharT, Traits> stream(&buf);
#endif // BOOST_NO_STD_LOCALE
#endif // BOOST_NO_STRINGSTREAM

#ifndef BOOST_NO_EXCEPTIONS
                stream.exceptions(std::ios::badbit);
                try {
#endif
                stream.unsetf(std::ios::skipws);
                lcast_set_precision(stream, static_cast<InputStreamable*>(0));

                return (stream >> output) 
                    && (stream.get() == Traits::eof());

#ifndef BOOST_NO_EXCEPTIONS
                } catch (const ::std::ios_base::failure& /*f*/) {
                    return false;
                }
#endif
            }

            template<class T>
            inline bool shr_xchar(T& output) BOOST_NOEXCEPT {
                BOOST_STATIC_ASSERT_MSG(( sizeof(CharT) == sizeof(T) ),
                    "boost::lexical_cast does not support narrowing of character types."
                    "Use boost::locale instead" );
                bool const ok = (finish - start == 1);
                if (ok) {
                    CharT out;
                    Traits::assign(out, *start);
                    output = static_cast<T>(out);
                }
                return ok;
            }

            template <std::size_t N, class ArrayT>
            bool shr_std_array(ArrayT& output) BOOST_NOEXCEPT {
                using namespace std;
                const std::size_t size = static_cast<std::size_t>(finish - start);
                if (size > N - 1) { // `-1` because we need to store \0 at the end 
                    return false;
                }

                memcpy(&output[0], start, size * sizeof(CharT));
                output[size] = Traits::to_char_type(0);
                return true;
            }

/************************************ OPERATORS >> ( ... ) ********************************/
        public:
            bool operator>>(unsigned short& output)             { return shr_unsigned(output); }
            bool operator>>(unsigned int& output)               { return shr_unsigned(output); }
            bool operator>>(unsigned long int& output)          { return shr_unsigned(output); }
            bool operator>>(short& output)                      { return shr_signed(output); }
            bool operator>>(int& output)                        { return shr_signed(output); }
            bool operator>>(long int& output)                   { return shr_signed(output); }
#if defined(BOOST_HAS_LONG_LONG)
            bool operator>>(boost::ulong_long_type& output)     { return shr_unsigned(output); }
            bool operator>>(boost::long_long_type& output)      { return shr_signed(output); }
#elif defined(BOOST_HAS_MS_INT64)
            bool operator>>(unsigned __int64& output)           { return shr_unsigned(output); }
            bool operator>>(__int64& output)                    { return shr_signed(output); }
#endif

#ifdef BOOST_HAS_INT128
            bool operator>>(boost::uint128_type& output)        { return shr_unsigned(output); }
            bool operator>>(boost::int128_type& output)         { return shr_signed(output); }
#endif

            bool operator>>(char& output)                       { return shr_xchar(output); }
            bool operator>>(unsigned char& output)              { return shr_xchar(output); }
            bool operator>>(signed char& output)                { return shr_xchar(output); }
#if !defined(BOOST_LCAST_NO_WCHAR_T) && !defined(BOOST_NO_INTRINSIC_WCHAR_T)
            bool operator>>(wchar_t& output)                    { return shr_xchar(output); }
#endif
#if !defined(BOOST_NO_CXX11_CHAR16_T) && !defined(BOOST_NO_CXX11_UNICODE_LITERALS)
            bool operator>>(char16_t& output)                   { return shr_xchar(output); }
#endif
#if !defined(BOOST_NO_CXX11_CHAR32_T) && !defined(BOOST_NO_CXX11_UNICODE_LITERALS)
            bool operator>>(char32_t& output)                   { return shr_xchar(output); }
#endif
            template<class Alloc>
            bool operator>>(std::basic_string<CharT,Traits,Alloc>& str) { 
                str.assign(start, finish); return true; 
            }

            template<class Alloc>
            bool operator>>(boost::container::basic_string<CharT,Traits,Alloc>& str) { 
                str.assign(start, finish); return true; 
            }

            template <std::size_t N>
            bool operator>>(boost::array<CharT, N>& output) BOOST_NOEXCEPT { 
                return shr_std_array<N>(output); 
            }

            template <std::size_t N>
            bool operator>>(boost::array<unsigned char, N>& output) BOOST_NOEXCEPT { 
                return ((*this) >> reinterpret_cast<boost::array<char, N>& >(output)); 
            }

            template <std::size_t N>
            bool operator>>(boost::array<signed char, N>& output) BOOST_NOEXCEPT { 
                return ((*this) >> reinterpret_cast<boost::array<char, N>& >(output)); 
            }
 
#ifndef BOOST_NO_CXX11_HDR_ARRAY
            template <class C, std::size_t N>
            bool operator>>(std::array<C, N>& output) BOOST_NOEXCEPT { 
                BOOST_STATIC_ASSERT_MSG(
                    (sizeof(boost::array<C, N>) == sizeof(boost::array<C, N>)),
                    "std::array<C, N> and boost::array<C, N> must have exactly the same layout."
                );
                return ((*this) >> reinterpret_cast<boost::array<C, N>& >(output));
            }
#endif

            bool operator>>(bool& output) BOOST_NOEXCEPT {
                output = false; // Suppress warning about uninitalized variable

                if (start == finish) return false;
                CharT const zero = lcast_char_constants<CharT>::zero;
                CharT const plus = lcast_char_constants<CharT>::plus;
                CharT const minus = lcast_char_constants<CharT>::minus;

                const CharT* const dec_finish = finish - 1;
                output = Traits::eq(*dec_finish, zero + 1);
                if (!output && !Traits::eq(*dec_finish, zero)) {
                    return false; // Does not ends on '0' or '1'
                }

                if (start == dec_finish) return true;

                // We may have sign at the beginning
                if (Traits::eq(plus, *start) || (Traits::eq(minus, *start) && !output)) {
                    ++ start;
                }

                // Skipping zeros
                while (start != dec_finish) {
                    if (!Traits::eq(zero, *start)) {
                        return false; // Not a zero => error
                    }

                    ++ start;
                }

                return true;
            }

            bool operator>>(float& output) { return lcast_ret_float<Traits>(output,start,finish); }

        private:
            // Not optimised converter
            template <class T>
            bool float_types_converter_internal(T& output, int /*tag*/) {
                if (parse_inf_nan(start, finish, output)) return true;
                bool const return_value = shr_using_base_class(output);

                /* Some compilers and libraries successfully
                 * parse 'inf', 'INFINITY', '1.0E', '1.0E-'...
                 * We are trying to provide a unified behaviour,
                 * so we just forbid such conversions (as some
                 * of the most popular compilers/libraries do)
                 * */
                CharT const minus = lcast_char_constants<CharT>::minus;
                CharT const plus = lcast_char_constants<CharT>::plus;
                CharT const capital_e = lcast_char_constants<CharT>::capital_e;
                CharT const lowercase_e = lcast_char_constants<CharT>::lowercase_e;
                if ( return_value &&
                     (
                        Traits::eq(*(finish-1), lowercase_e)                   // 1.0e
                        || Traits::eq(*(finish-1), capital_e)                  // 1.0E
                        || Traits::eq(*(finish-1), minus)                      // 1.0e- or 1.0E-
                        || Traits::eq(*(finish-1), plus)                       // 1.0e+ or 1.0E+
                     )
                ) return false;

                return return_value;
            }

            // Optimised converter
            bool float_types_converter_internal(double& output, char /*tag*/) {
                return lcast_ret_float<Traits>(output, start, finish);
            }
        public:

            bool operator>>(double& output) {
                /*
                 * Some compilers implement long double as double. In that case these types have
                 * same size, same precision, same max and min values... And it means,
                 * that current implementation of lcast_ret_float cannot be used for type
                 * double, because it will give a big precision loss.
                 * */
                boost::mpl::if_c<
#if (defined(BOOST_HAS_LONG_LONG) || defined(BOOST_HAS_MS_INT64)) && !defined(BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS)
                    boost::type_traits::ice_eq< sizeof(double), sizeof(long double) >::value,
#else
                     1,
#endif
                    int,
                    char
                >::type tag = 0;

                return float_types_converter_internal(output, tag);
            }

            bool operator>>(long double& output) {
                int tag = 0;
                return float_types_converter_internal(output, tag);
            }

            // Generic istream-based algorithm.
            // lcast_streambuf_for_target<InputStreamable>::value is true.
            template <typename InputStreamable>
            bool operator>>(InputStreamable& output) { 
                return shr_using_base_class(output); 
            }
        };
    }

    namespace detail
    {
        template<typename T>
        struct is_stdstring
            : boost::false_type
        {};

        template<typename CharT, typename Traits, typename Alloc>
        struct is_stdstring< std::basic_string<CharT, Traits, Alloc> >
            : boost::true_type
        {};

        template<typename CharT, typename Traits, typename Alloc>
        struct is_stdstring< boost::container::basic_string<CharT, Traits, Alloc> >
            : boost::true_type
        {};

        template<typename Target, typename Source>
        struct is_arithmetic_and_not_xchars
        {
            BOOST_STATIC_CONSTANT(bool, value = (
                boost::type_traits::ice_and<
                    boost::type_traits::ice_not<
                        boost::detail::is_character<Target>::value
                    >::value,
                    boost::type_traits::ice_not<
                        boost::detail::is_character<Source>::value
                    >::value,
                    boost::is_arithmetic<Source>::value,
                    boost::is_arithmetic<Target>::value       
                >::value
            ));
        };

        /*
         * is_xchar_to_xchar<Target, Source>::value is true, 
         * Target and Souce are char types of the same size 1 (char, signed char, unsigned char).
         */
        template<typename Target, typename Source>
        struct is_xchar_to_xchar 
        {
            BOOST_STATIC_CONSTANT(bool, value = (
                boost::type_traits::ice_and<
                     boost::type_traits::ice_eq<sizeof(Source), sizeof(Target)>::value,
                     boost::type_traits::ice_eq<sizeof(Source), sizeof(char)>::value,
                     boost::detail::is_character<Target>::value,
                     boost::detail::is_character<Source>::value
                >::value
            ));
        };

        template<typename Target, typename Source>
        struct is_char_array_to_stdstring
            : boost::false_type
        {};

        template<typename CharT, typename Traits, typename Alloc>
        struct is_char_array_to_stdstring< std::basic_string<CharT, Traits, Alloc>, CharT* >
            : boost::true_type
        {};

        template<typename CharT, typename Traits, typename Alloc>
        struct is_char_array_to_stdstring< std::basic_string<CharT, Traits, Alloc>, const CharT* >
            : boost::true_type
        {};

        template<typename CharT, typename Traits, typename Alloc>
        struct is_char_array_to_stdstring< boost::container::basic_string<CharT, Traits, Alloc>, CharT* >
            : boost::true_type
        {};

        template<typename CharT, typename Traits, typename Alloc>
        struct is_char_array_to_stdstring< boost::container::basic_string<CharT, Traits, Alloc>, const CharT* >
            : boost::true_type
        {};

        template<typename Target, typename Source>
        struct lexical_converter_impl
        {
            typedef lexical_cast_stream_traits<Source, Target>  stream_trait;

            typedef detail::lexical_istream_limited_src<
                BOOST_DEDUCED_TYPENAME stream_trait::char_type,
                BOOST_DEDUCED_TYPENAME stream_trait::traits,
                stream_trait::requires_stringbuf,
                stream_trait::len_t::value + 1
            > i_interpreter_type;

            typedef detail::lexical_ostream_limited_src<
                BOOST_DEDUCED_TYPENAME stream_trait::char_type,
                BOOST_DEDUCED_TYPENAME stream_trait::traits
            > o_interpreter_type;

            static inline bool try_convert(const Source& arg, Target& result) {
                i_interpreter_type i_interpreter;

                // Disabling ADL, by directly specifying operators.
                if (!(i_interpreter.operator <<(arg)))
                    return false;

                o_interpreter_type out(i_interpreter.cbegin(), i_interpreter.cend());

                // Disabling ADL, by directly specifying operators.
                if(!(out.operator >>(result)))
                    return false;

                return true;
            }
        };

        template <typename Target, typename Source>
        struct copy_converter_impl
        {
// MSVC fail to forward an array (DevDiv#555157 "SILENT BAD CODEGEN triggered by perfect forwarding",
// fixed in 2013 RTM).
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && (!defined(BOOST_MSVC) || BOOST_MSVC >= 1800)
            template <class T>
            static inline bool try_convert(T&& arg, Target& result) {
                result = static_cast<T&&>(arg); // eqaul to `result = std::forward<T>(arg);`
                return true;
            }
#else
            static inline bool try_convert(const Source& arg, Target& result) {
                result = arg;
                return true;
            }
#endif
        };
    }

    namespace conversion { namespace detail {

        template <typename Target, typename Source>
        inline bool try_lexical_convert(const Source& arg, Target& result)
        {
            typedef BOOST_DEDUCED_TYPENAME boost::detail::array_to_pointer_decay<Source>::type src;

            typedef BOOST_DEDUCED_TYPENAME boost::type_traits::ice_or<
                boost::detail::is_xchar_to_xchar<Target, src >::value,
                boost::detail::is_char_array_to_stdstring<Target, src >::value,
                boost::type_traits::ice_and<
                     boost::is_same<Target, src >::value,
                     boost::detail::is_stdstring<Target >::value
                >::value,
                boost::type_traits::ice_and<
                     boost::is_same<Target, src >::value,
                     boost::detail::is_character<Target >::value
                >::value
            > shall_we_copy_t;

            typedef boost::detail::is_arithmetic_and_not_xchars<Target, src >
                shall_we_copy_with_dynamic_check_t;

            // We do evaluate second `if_` lazily to avoid unnecessary instantiations
            // of `shall_we_copy_with_dynamic_check_t` and improve compilation times.
            typedef BOOST_DEDUCED_TYPENAME boost::mpl::if_c<
                shall_we_copy_t::value,
                boost::mpl::identity<boost::detail::copy_converter_impl<Target, src > >,
                boost::mpl::if_<
                     shall_we_copy_with_dynamic_check_t,
                     boost::detail::dynamic_num_converter_impl<Target, src >,
                     boost::detail::lexical_converter_impl<Target, src >
                >
            >::type caster_type_lazy;

            typedef BOOST_DEDUCED_TYPENAME caster_type_lazy::type caster_type;

            return caster_type::try_convert(arg, result);
        }

        template <typename Target, typename CharacterT>
        inline bool try_lexical_convert(const CharacterT* chars, std::size_t count, Target& result)
        {
            BOOST_STATIC_ASSERT_MSG(
                boost::detail::is_character<CharacterT>::value,
                "This overload of try_lexical_convert is meant to be used only with arrays of characters."
            );
            return ::boost::conversion::detail::try_lexical_convert(
                ::boost::iterator_range<const CharacterT*>(chars, chars + count), result
            );
        }

    }} // namespace conversion::detail

    namespace conversion {
        // ADL barrier
        using ::boost::conversion::detail::try_lexical_convert;
    }

} // namespace boost

#undef BOOST_LCAST_NO_WCHAR_T

#endif // BOOST_LEXICAL_CAST_TRY_LEXICAL_CONVERT_HPP

