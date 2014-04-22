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

#include <stdio.h>
#include <syslog.h>

typedef void (*logfnc)( int level, const char *fmt, ... );

class Logger
{
  public:
    static Logger *Instance( );
    virtual ~Logger( );

    virtual void Log( int level, char *log );

  protected:
    static Logger *logger;
    struct loglevel
    {
      const char *name;
      const char *color;
      FILE *io;
    };

    Logger( );
};

class LoggerSyslog : public Logger
{
  public:
    LoggerSyslog( const char *ident );
    virtual ~LoggerSyslog( );

    virtual void Log( int level, char *log );
};

void TVD_Log( int level, const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));

void Log( const char *fmt, ... ) __attribute__ (( format( printf, 1, 2 )));
void LogInfo( const char *fmt, ... ) __attribute__ (( format( printf, 1, 2 )));
void LogWarn( const char *fmt, ... ) __attribute__ (( format( printf, 1, 2 )));
void LogError( const char *fmt, ... ) __attribute__ (( format( printf, 1, 2 )));

#endif

