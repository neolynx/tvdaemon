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
#include <unistd.h> // isatty

Logger *Logger::logger = NULL;

Logger *Logger::Instance( )
{
  if( logger )
    return logger;
  return new Logger( );
}

Logger::Logger( )
{
  if( logger )
    delete logger;
  logger = this;
}

Logger::~Logger( )
{
  logger = NULL;
}

#define LOG_COLOROFF 8
void Logger::Log( int level, char *log )
{
  const char *color = "", *term = "";
  const struct loglevel loglevels[9] = {
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
  if( level > sizeof( loglevels ) / sizeof( struct loglevel ) - 2 )
    level = LOG_INFO;
  if( isatty( loglevels[level].io->_fileno ))
    color = loglevels[level].color;
  const char *tag = loglevels[level].name;
  if( isatty( loglevels[level].io->_fileno ))
    term = loglevels[LOG_COLOROFF].color;
  fprintf( loglevels[level].io, "%s%s %s%s\n", color, tag, log, term );
}

LoggerSyslog::LoggerSyslog( const char *ident )
{
  openlog( ident, LOG_NDELAY, LOG_DAEMON );
}

LoggerSyslog::~LoggerSyslog( )
{
  closelog( );
}

void LoggerSyslog::Log( int level, char *log )
{
  syslog( level, "%s", log );
}


void TVD_Log( int level, const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  Logger::Instance( )->Log( level, msg );
}

void Log( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  Logger::Instance( )->Log( LOG_INFO, msg );
}

void LogWarn( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  Logger::Instance( )->Log( LOG_WARNING, msg );
}

void LogError( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  Logger::Instance( )->Log( LOG_ERR, msg );
}

