#ifndef GREG_MONTH_HPP___
#define GREG_MONTH_HPP___
/* Copyright (c) 2000 CrystalClear Software, Inc.
 * Disclaimer & Full Copyright at end of file
 * Author: Jeff Garland
 */

#include "boost/date_time/constrained_value.hpp"
#include <stdexcept>


namespace boost {
namespace gregorian {


  //! Simple enum to allow for nice programming with Jan, Feb, etc
  enum months_of_year {Jan=1,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec,NotAMonth,NumMonths};
  
  //! Exception thrown if a greg_month is constructed with a value out of range
  struct bad_month : public std::out_of_range
  {
    bad_month() : std::out_of_range(std::string("Month number is out of range 1..12")) {}
  };
  //! Build a policy class for the greg_month_rep
  typedef CV::simple_exception_policy<unsigned short, 1, 12, bad_month> greg_month_policies;
  //! A constrained range that implements the gregorian_month rules
  typedef CV::constrained_value<greg_month_policies> greg_month_rep;

  
  //! Wrapper class to represent months in gregorian based calendar
  class greg_month : public greg_month_rep {
  public:
    typedef months_of_year month_enum;
    //! Construct a month from the months_of_year enumeration
    greg_month(months_of_year theMonth) : greg_month_rep(theMonth) {}
    //! Construct from a short value
    greg_month(short theMonth) : greg_month_rep(theMonth) {}
    //! Convert the value back to a short
    operator unsigned short()  const {return value_;}
    //! Returns month as number from 1 to 12
    unsigned short as_number() const {return value_;}
    month_enum as_enum() const {return static_cast<month_enum>(value_);}
    const char* as_short_string() const;
    const char* as_long_string()  const;
  };

} } //namespace gregorian

/* Copyright (c) 2000
 * CrystalClear Software, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  CrystalClear Software makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */


#endif
