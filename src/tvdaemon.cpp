/*
 *  tvdaemon
 *
 *  TVDaemon main
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

#include <signal.h>

#include "TVDaemon.h"
#include "Daemon.h"
#include "Log.h"

bool up = true;

void termination_handler( int signum )
{
  if( up )
  {
    Log( "Signal received, terminating ..." );
    up = false;
  }
}

int main( int argc, char *argv[] )
{
  struct sigaction action;
  action.sa_handler = termination_handler;
  sigemptyset( &action.sa_mask );
  action.sa_flags = 0;
  sigaction( SIGINT, &action, NULL );
  sigaction( SIGTERM, &action, NULL );

  Logger *logger = new LoggerSyslog( "tvdaemon" );
  if( !Daemon::Instance( )->daemonize( "tvdaemon", "/var/run/tvdaemon.pid" ))
  {
    delete logger;
    return -1;
  }

  Log( "TVDaemon starting ..." );
  TVDaemon *tvd = TVDaemon::Instance( );
  if( !tvd->Create( "~/.tvdaemon" ))
  {
    LogError( "Unable to create tvdaemon\n" );
    return -1;
  }
  if( !tvd->Start( ))
  {
    LogError( "Unable to start tvdaemon\n" );
    return -1;
  }

  while( up )
  {
    sleep( 1 );
  }
  delete tvd;
  Log( "TVDaemon terminated" );
  delete logger;
  return 0;
}

