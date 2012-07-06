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
  if( level > sizeof( loglevels ) / sizeof( struct loglevel ) - 2 )
    level = LOG_INFO;
  va_list ap;
  va_start( ap, fmt );
  if( isatty( loglevels[level].io->_fileno ))
    fprintf(  loglevels[level].io, loglevels[level].color );
  fprintf(    loglevels[level].io, "%s ", loglevels[level].name );
  vfprintf(   loglevels[level].io, fmt, ap );
  if( isatty( loglevels[level].io->_fileno ))
    fprintf(  loglevels[level].io, loglevels[LOG_COLOROFF].color );
  fprintf(    loglevels[level].io, "\n" );
  va_end( ap );
}

