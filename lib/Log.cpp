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

#include "Log.h"

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h> // isatty

static const struct loglevel
{
  const char *name;
  const char *color;
  FILE *io;
} loglevels[9] = {
  {"EMERG   ", "\033[31m", stderr },
  {"ALERT   ", "\033[31m", stderr },
  {"CRITICAL", "\033[31m", stderr },
  {"ERROR   ", "\033[31m", stderr },
  {"WARNING ", "\033[33m", stdout },
  {"NOTICE  ", "\033[36m", stdout },
  {"INFO    ", "\033[36m", stdout },
  {"DEBUG   ", "\033[32m", stdout },
  {"",         "\033[0m",  stdout },
};
#define LOG_COLOROFF 8

void TVD_Log( int level, const char *fmt, ... )
{
  char log[255];
  const char *color = "", *term = "";
  if( level > sizeof( loglevels ) / sizeof( struct loglevel ) - 2 )
    level = LOG_INFO;
  if( isatty( loglevels[level].io->_fileno ))
    color = loglevels[level].color;
  const char *tag = loglevels[level].name;
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( log, sizeof( log ), fmt, ap );
  va_end( ap );
  if( isatty( loglevels[level].io->_fileno ))
    term = loglevels[LOG_COLOROFF].color;
  fprintf( loglevels[level].io, "%s%s %s%s\n", color, tag, log, term );
}

void TVD_Log( int level, char *log )
{
  const char *color = "", *term = "";
  if( level > sizeof( loglevels ) / sizeof( struct loglevel ) - 2 )
    level = LOG_INFO;
  if( isatty( loglevels[level].io->_fileno ))
    color = loglevels[level].color;
  const char *tag = loglevels[level].name;
  if( isatty( loglevels[level].io->_fileno ))
    term = loglevels[LOG_COLOROFF].color;
  fprintf( loglevels[level].io, "%s%s %s%s\n", color, tag, log, term );
}


void Log( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  TVD_Log( LOG_INFO, msg );
}

void LogWarn( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  TVD_Log( LOG_WARNING, msg );
}

void LogError( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  TVD_Log( LOG_ERR, msg );
}

