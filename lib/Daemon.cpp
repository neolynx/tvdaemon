/*
 *  tvdaemon
 *
 *  Deamon
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

#include "Daemon.h"

#include "Log.h"

#include <sys/types.h> // pid_t
#include <unistd.h> // getppid
#include <fcntl.h>      // creat
#include <stdio.h>      // printf, freopen
#include <sys/stat.h>   // umask
#include <pwd.h>        // getpwnam
#include <string.h>     // stdlen
#include <stdlib.h>     // exit

Daemon *Daemon::daemon = NULL;

Daemon *Daemon::Instance( )
{
  if( daemon )
    return daemon;
  return new Daemon( );
}

Daemon::Daemon( )
{
  daemon = this;
}

Daemon::~Daemon( )
{
  daemon = NULL;
}

bool Daemon::daemonize( const char *user, const char *pidfile, bool change_cwd )
{
  pid_t pid, sid;
  if( getppid() == 1 ) // already daemonized
  {
    LogError( "daemon cannot daemonize" );
    return false;
  }
  pid = fork();
  if( pid < 0 )
  {
    LogError( "fork error" );
    return false;
  }
  if( pid > 0 ) // parent
  {
    if( pidfile ) // FIXME: remove pidfile on exit
    {
      int fd = creat( pidfile, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
      if( fd > 0 )
      {
        char buf[16];
        snprintf( buf, sizeof( buf ), "%d\n", pid );
        int i =  write( fd, buf, strlen( buf ));
        close( fd );
      }
      else
      {
        LogError( "unable to create PID file: %s", pidfile );
      }
    }
    exit( 0 ); // exit the parent
  }

  // child
  if( pidfile )
    this->pidfile = pidfile;
  if( user )
  {
    {
      struct passwd *pwd = getpwnam( user );
      if( setgid( pwd->pw_gid ) != 0 )
      {
        LogError( "cannot setgid( %d ) for user %s", pwd->pw_gid, user );
        return false;
      }
      if( setuid( pwd->pw_uid ) != 0 )
      {
        LogError( "cannot setuid( %d ) for user %s", pwd->pw_uid, user );
        return false;
      }
      setenv( "HOME", pwd->pw_dir, 1 );
    }
    umask( 0 );
    sid = setsid( );
    if( sid < 0 )
    {
      LogError( "setsid failed" );
      return false;
    }
    if( change_cwd && ( chdir( "/" )) < 0 )
    {
      LogError( "cannot chdir( \"/\" )" );
      return false;
    }
    FILE *f = freopen( "/dev/null", "r", stdin );
    f       = freopen( "/dev/null", "w", stdout );
    f       = freopen( "/dev/null", "w", stderr );
    return true;
  }
}

