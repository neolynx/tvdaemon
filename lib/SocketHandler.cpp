/*
 * tvdaemon
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#include "SocketHandler.h"

#include <netinet/in.h> // sockaddr_in
#include <linux/un.h>   // sockaddr_un, UNIX_PATH_MAX
#include <netdb.h>      // getaddrinfo
#include <sys/time.h>   // timeval
#include <unistd.h>     // sleep, exit
#include <stdlib.h>     // calloc
#include <string.h>     // memset
#include <stdio.h>      // printf, freopen
#include <sys/stat.h>   // umask
#include <pwd.h>        // getpwnam
#include <syslog.h>     // openlog, vsyslog, closelog
#include <stdarg.h>     // va_start, va_end
#include <fcntl.h>      // creat

bool SocketHandler::log2syslog = false;

SocketHandler::SocketHandler() : up(false), connected(false), autoreconnect(false), sd(0), host(NULL), port(0), socket(0)
{
  pthread_mutex_init( &mutex, 0 );
  logfunc = StdLog;
}

SocketHandler::~SocketHandler()
{
  Stop( );
}

bool SocketHandler::CreateClient( SocketType sockettype, const char *host, int port, const char *socket, bool autoreconnect )
{
  this->host = (char *) host;
  this->port = port;
  this->socket = (char *) socket;
  this->autoreconnect = autoreconnect;
  this->sockettype = sockettype;
  this->role = CLIENT;

  if( sd )
    return false;

  if( !CreateSocket( ))
    return false;

  return true;
}

bool SocketHandler::CreateServer( SocketType sockettype, int port, const char *socket )
{
  if( sd )
    return false;

  this->role = SERVER;
  this->sockettype = sockettype;
  this->port = port;
  this->socket = (char *) socket;

  if( !CreateSocket( ))
  {
    SHLogError( "socket creation failed" );
    return false;
  }

  int yes = 1;
  if( setsockopt( sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(  int )) != 0 )
  {
    SHLogError( "setsockopt failed" );
    goto errorexit;
  }

  if( !Bind( ))
  {
    goto errorexit;
  }

  return true;

errorexit:
  close( sd );
  sd = 0;
  return false;
}

bool SocketHandler::Start( )
{
  switch( role )
  {
    case SERVER:
      return StartServer( );
    case CLIENT:
      return StartClient( );
  }
  return false;
}

void SocketHandler::Stop( )
{
  if( up )
  {
    up = false;
    pthread_join( handler, NULL);
  }
  if( sd )
  {
    close( sd );
    sd = 0;
  }
}

bool SocketHandler::StartClient( )
{
  if( !sd )
    return false;

  if( Connect( ))
  {
    connected = true;
    Connected( sd );
  }
  else if( !autoreconnect )
  {
    SHLogError( "Connection refused" );
    goto errorexit;
  }

  up = true;
  if( pthread_create( &handler, NULL, run, (void*) this ) != 0 )
  {
    SHLogError( "thread creation failed" );
    goto errorexit;
  }
  return true;

errorexit:
  close( sd );
  sd = 0;
  return false;
}

bool SocketHandler::StartServer( )
{
  if( !sd )
    return false;

  int backlog = 15;
  if( listen( sd, backlog ) != 0 )
  {
    SHLogError( "listen failed" );
    goto errorexit;
  }

  up = true;
  if( pthread_create( &handler, NULL, run, (void *) this ) != 0 )
  {
    SHLogError( "thread creation failed" );
    goto errorexit;
  }
  return true;

errorexit:
  close( sd );
  sd = 0;
  return false;
}

bool SocketHandler::Bind( )
{
  bool ok = false;
  switch( sockettype )
  {
    case UNIX:
      {
        struct sockaddr_un addr;
        memset( &addr, 0, sizeof( struct sockaddr_un ));
        addr.sun_family = AF_UNIX;
        strncat( addr.sun_path, this->socket, UNIX_PATH_MAX - 1 );
        // FIXME: verify it's a socket
        unlink( this->socket );
        ok = bind( sd, (struct sockaddr*) &addr, sizeof( addr )) == 0;
        if( !ok )
          SHLogError( "binding socket %s failed", this->socket );
        break;
      }
    case TCP:
      {
        struct sockaddr_in addr;
        memset( &addr, 0, sizeof( struct sockaddr_in ));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons( port );
        addr.sin_addr.s_addr = INADDR_ANY;
        ok = bind( sd, (struct sockaddr*) &addr, sizeof( addr )) == 0;
        if( !ok )
          SHLogError( "binding port %d failed", port );
        break;
      }
  }
  return ok;
}

bool SocketHandler::Connect( )
{
  switch( sockettype )
  {
    case TCP:
      {
        struct sockaddr_in addr;
        struct addrinfo hints;
        struct addrinfo *result;
        memset( &addr,  0, sizeof( struct sockaddr_in ));
        memset( &hints, 0, sizeof( struct addrinfo ));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags |= AI_CANONNAME;

        int s = getaddrinfo( host, NULL, &hints, &result );
        if( s != 0 )
        {
          return false;
        }

        while( result )
        {
          if( result->ai_family == AF_INET )
            break;
          result = result->ai_next;
        }
        addr.sin_addr.s_addr = ((struct sockaddr_in *) result->ai_addr )->sin_addr.s_addr;
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons( port );

        freeaddrinfo( result );

        return ( connect( sd, (struct sockaddr*) &addr, sizeof( addr )) == 0 );
      }
    case UNIX:
      {
        struct sockaddr_un addr;
        memset( &addr, 0, sizeof( struct sockaddr_un ));
        addr.sun_family = AF_UNIX;
        strncat( addr.sun_path, this->socket, UNIX_PATH_MAX - 1 );
        return ( connect( sd, (struct sockaddr*) &addr, sizeof( addr )) == 0 );
      }
  }
}

bool SocketHandler::CreateSocket( )
{
  switch( this->sockettype )
  {
    case UNIX:
      sd = ::socket( PF_UNIX, SOCK_STREAM, 0 );
      break;
    case TCP:
      sd = ::socket( PF_INET, SOCK_STREAM, 0 );
      break;
  }
  if( sd < 0 )
  {
    sd = 0;
    SHLogError( "socket creation failed" );
    return false;
  }
  return true;
}

void *SocketHandler::run( void *ptr )
{
  SocketHandler *sh = (SocketHandler *) ptr;
  sh->Run( );
  pthread_exit( 0 );
  return NULL;
}

void SocketHandler::Run( )
{
  fd_set tmp_fds;
  char buf[2048];
  int readpos = 0;
  int writepos = 0;

  FD_ZERO( &fds );
  fdmax = sd;
  FD_SET ( sd, &fds );

  while( up )
  {
    if( role == CLIENT && !connected )
    {
      if( !Connect( ))
      {
        if( autoreconnect )
        {
          sleep( 1 );
          continue;
        }
        else
        {
          SHLogError( "Reconnect refused" );
          up = false;
        }
      }
      else
      {
        connected = true;
        fdmax = sd;
        FD_SET ( sd, &fds );
        Connected( sd );
      }
    }

    tmp_fds = fds;

    struct timeval timeout = { 1, 0 }; // 1 sec
    if( select( fdmax + 1, &tmp_fds, NULL, NULL, &timeout ) == -1 )
    {
      SHLogError( "select error" );
      up = false;
      continue;
    }

    switch( role )
    {
      case SERVER:
        for( int i = 0; i <= fdmax; i++ )
        {
          if( FD_ISSET( i, &tmp_fds ))
          {
            if( i == sd ) // new connection
            {
              struct sockaddr_in clientaddr;
              socklen_t addrlen = sizeof(clientaddr);
              int newfd;
              if(( newfd = accept( sd, (struct sockaddr *) &clientaddr, &addrlen )) == -1 )
              {
                SHLogError( "accept error" );
                continue;
              }

              FD_SET( newfd, &fds );
              if( newfd > fdmax )
                fdmax = newfd;

              Connected( newfd );
            }
            else // handle data
            {
              int len = recv( i, buf + writepos, sizeof( buf ) - writepos, 0 );
              if( len <= 0 )
              {
                if( len != 0 )
                  SHLogError( "Error receiving data..." );
                DisconnectClient( i, len != 0 );
              }
              else
              {
                writepos += len;
                if( writepos == sizeof( buf ))
                  writepos = 0;
                int already_read;
                while( readpos != writepos && up ) // handle all data
                {
                  if( !messages[i] )
                    messages[i] = CreateMessage( i );

                  already_read = messages[i]->AccumulateData( buf + readpos, len );
                  if( already_read > len )
                  {
                    // FIXME: log
                    already_read = len;
                  }

                  if( messages[i]->isSubmitted( ))
                  {
                    HandleMessage( i, *messages[i] );
                    delete messages[i];
                    messages[i] = NULL;
                  }
                  len     -= already_read;
                  readpos += already_read;
                  if( readpos == sizeof( buf ))
                    readpos = 0;
                  //SHLogError( "writepos: %d, readpos: %d\n", writepos, readpos );
                }
              }
            }
          } // if( FD_ISSET( i, &tmp_fds ))
        } // for( int i = 0; i <= fdmax; i++ )
        break;

      case CLIENT:
        if( FD_ISSET( sd, &tmp_fds ))
        {
          int len = recv( sd, buf + writepos, sizeof( buf ) - writepos, 0 );
          if( len <= 0 )
          {
            if( len != 0 )
              SHLogError( "Error receiving data..." );
            Disconnected( sd, len != 0 );
            connected = false;
            close( sd );
            sd = 0;
            if( autoreconnect )
            {
              if( !CreateSocket( ))
                up = false;
              {
                fdmax = sd;
                FD_SET ( sd, &fds );
              }
            }
            else
            {
              up = false;
            }
          }
          else
          {
            writepos += len;
            if( writepos == sizeof( buf ))
              writepos = 0;
            int already_read;
            while( readpos != writepos && up ) // handle all data
            {
              if( !messages[sd] )
                messages[sd] = CreateMessage( sd );

              already_read = messages[sd]->AccumulateData( buf + readpos, len );
              if( already_read > len )
              {
                // FIXME: log
                already_read = len;
              }

              if( messages[sd]->isSubmitted( ))
              {
                HandleMessage( sd, *messages[sd] );
                delete messages[sd];
                messages[sd] = NULL;
              }
              len     -= already_read;
              readpos += already_read;
              if( readpos == sizeof( buf ))
                readpos = 0;
              //Log( "writepos: %d, readpos: %d\n", writepos, readpos );
            }
          }
        }
        break;
    } // switch( type )
  } // while( up )
}

bool SocketHandler::Send( const char *buffer, int len )
{
  if( role != CLIENT || !connected )
    return false;

  int written = 0;
  while( written < len )
  {
    int n = write( sd, buffer, len );
    if( n < 0 )
    {
      SHLogError( "error writing to socket" );
      Disconnected( sd, true );
      close( sd );
      sd = 0;
      connected = false;
      return false;
    }
    written += n;
  }
  return true;
}

bool SocketHandler::SendToClient( int client, const char *buffer, int len )
{
  if( role != SERVER )
    return false;

  int written = 0;
  while( written < len )
  {
    int n = write( client, buffer, len );
    if( n < 0 )
    {
      SHLogError( "error writing to socket" );
      DisconnectClient( client );
      return false;
    }
    written += n;
  }
  return true;
}

bool SocketHandler::Lock( )
{
  pthread_mutex_lock( &mutex );
}

bool SocketHandler::Unlock( )
{
  pthread_mutex_unlock( &mutex );
}

void SocketHandler::Dump( const char *buffer, int length )
{
  for( int i = 0; i < length; i++ )
    printf( "%02x ", buffer[i] );
  printf( "\n" );
}

SocketHandler::Message *SocketHandler::CreateMessage( int /* client */ ) const
{
  return new Message( );
}

int SocketHandler::Message::AccumulateData( const char *buffer, int length )
{
  bool end = false;
  int i;
  //  Log( "accumulating data: " );
  //  Dump( buffer, length );
  for( i = 0; i < length; i++ )
  {
    if( buffer[i] == '\0' ||  buffer[i] == '\n' ) // ||  buffer[i] == '\r' )
    {
      end = true;
      i++;
      break;
    }
    else if( buffer[i] == '\r' )
      continue;
    else
    {
      if( end )
        break;
      line += buffer[i];
    }
  }
  if( end )
  {
    Submit( );
  }
  return i;
}

void SocketHandler::Message::Submit( )
{
  //if( line.length( ) >= 4 )
  //{
    //size_t len = line.length( );
    //if(( line[len-1] == 0x0a ) && ( line[len-2] == 0x0d ) && ( line[len-3] == 0x0a ) && ( line[len-4] == 0x0d ))
    //{
     //submitted = true;
    //}
  //}
  submitted = true;
}

void SocketHandler::DisconnectClient( int client, bool error )
{
  Disconnected( client, error );
  close( client );
  FD_CLR( client, &fds );
}

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

void SocketHandler::StdLog( int level, const char *fmt, ... )
{
  if( level > sizeof( loglevels ) / sizeof( struct loglevel ) - 2 )
    level = LOG_INFO;
  va_list ap;
  va_start( ap, fmt );
  if( isatty( loglevels[level].io->_fileno ))
    fputs( loglevels[level].color, loglevels[level].io );
  fprintf(    loglevels[level].io, "%s ", loglevels[level].name );
  vfprintf(   loglevels[level].io, fmt, ap );
  fprintf(    loglevels[level].io, "\n" );
  if( isatty( loglevels[level].io->_fileno ))
    fputs( loglevels[LOG_COLOROFF].color, loglevels[level].io );
  va_end( ap );
}

