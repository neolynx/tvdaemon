/*
 *  tvdaemon
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

void TVD_Log( int level, const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));
void TVD_Log( int level, char *msg );

void Log( const char *fmt, ... ) __attribute__ (( format( printf, 1, 2 )));
void LogWarn( const char *fmt, ... ) __attribute__ (( format( printf, 1, 2 )));
void LogError( const char *fmt, ... ) __attribute__ (( format( printf, 1, 2 )));

#endif

