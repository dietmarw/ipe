// -*- objc -*-
// Check OS X API levels
/*

    This file is part of the extensible drawing editor Ipe.
    Copyright (C) 1993-2016  Otfried Cheong

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, you have permission to link Ipe with the
    CGAL library and distribute executables, as long as you follow the
    requirements of the Gnu General Public License in regard to all of
    the software in the executable aside from CGAL.

    Ipe is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
    License for more details.

    You should have received a copy of the GNU General Public License
    along with Ipe; if not, you can find it at
    "http://www.gnu.org/copyleft/gpl.html", or write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef IPEOSX_H
#define IPEOSX_H

// --------------------------------------------------------------------

#ifdef __OBJC__
#import <CoreFoundation/CFAvailability.h>
#undef CF_AVAILABLE
#define IPE_AVAILABLE(_mac) IPE_AVAILABLE_##_mac
#define CF_AVAILABLE(_mac, _ios) IPE_AVAILABLE(_mac)
#define IPE_AVAILABLE_10_0 __attribute__((availability(macosx,introduced=10_0)))
#define IPE_AVAILABLE_10_1 __attribute__((availability(macosx,introduced=10_1)))
#define IPE_AVAILABLE_10_2 __attribute__((availability(macosx,introduced=10_2)))
#define IPE_AVAILABLE_10_3 __attribute__((availability(macosx,introduced=10_3)))
#define IPE_AVAILABLE_10_4 __attribute__((availability(macosx,introduced=10_4)))
#define IPE_AVAILABLE_10_5 __attribute__((availability(macosx,introduced=10_5)))
#define IPE_AVAILABLE_10_6 __attribute__((availability(macosx,introduced=10_6)))
#define IPE_AVAILABLE_10_7 __attribute__((availability(macosx,introduced=10_7)))
#define IPE_AVAILABLE_10_8 __attribute__((availability(macosx,introduced=10_8)))
#define IPE_AVAILABLE_10_9 __attribute__((weak_import,deprecated("API in 10.9")))
#define IPE_AVAILABLE_10_10 __attribute__((weak_import,deprecated("API in 10.10")))
#define IPE_AVAILABLE_10_10_3 __attribute__((weak_import,deprecated("API in 10.10.3")))
#define IPE_AVAILABLE_10_11 __attribute__((weak_import,deprecated("API in 10.11")))
#define IPE_AVAILABLE_NA __attribute__((weak_import,deprecated("API not available")))
#endif

// --------------------------------------------------------------------
#endif
