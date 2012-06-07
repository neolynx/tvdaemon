/*
 *  tvheadend
 *
 *  Logging
 *
 *  Copyright (C) 2012 André Roth
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _Log_
#define _Log_

#include <syslog.h>

void TVH_Log( int level, const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));

#define Log( fmt, arg... ) do {\
  TVH_Log( LOG_INFO, fmt, ##arg ); \
  } while (0)
#define LogWarn( fmt, arg... ) do {\
  TVH_Log( LOG_WARNING, fmt, ##arg ); \
  } while (0)
#define LogError( fmt, arg... ) do {\
  TVH_Log( LOG_ERR, fmt, ##arg ); \
  } while (0)

#endif
