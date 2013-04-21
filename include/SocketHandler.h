/*
 * tvdaemon
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#ifndef _SocketHandler_
#define _SocketHandler_

#include <sys/socket.h> // fd_set
#include <pthread.h>
#include <string>
#include <map>
#include <stdarg.h>     // va_list, va_start, va_end
#include <syslog.h>

typedef void (*shlog)( int level, const char *fmt, ... );

#define SHLog( fmt, arg... ) do {\
  logfunc( LOG_INFO, fmt, ##arg ); \
  } while (0)
#define SHLogError( fmt, arg... ) do {\
  logfunc( LOG_ERR, fmt, ##arg ); \
  } while (0)

class SocketHandler
{
  private:
    static bool log2syslog;
    bool connected;
    bool autoreconnect;
    int sd;
    int fdmax;
    fd_set fds;

    char *host;
    int port;
    char *socket;

    pthread_t handler;
    pthread_mutex_t mutex;

    enum SocketType
    {
      UNIX,
      TCP
    } sockettype;

    enum Role
    {
      SERVER,
      CLIENT
    } role;

    bool CreateServer( SocketType sockettype, int port, const char *socket );
    bool CreateClient( SocketType sockettype, const char *host, int port, const char *socket, bool autoreconnect );
    bool StartServer( );
    bool StartClient( );

    bool CreateSocket( );
    bool Bind( );
    bool Connect( );

    static void *run( void *ptr );

  public:
    virtual ~SocketHandler( );

    bool CreateServerTCP ( int port )           { return CreateServer( TCP, port, NULL ); }
    bool CreateServerUnix( const char *socket ) { return CreateServer( UNIX, 0, socket ); }
    bool CreateClientTCP ( const char *host, int port, bool autoreconnect = false ) { return CreateClient( TCP, host, port, NULL, autoreconnect ); }
    bool CreateClientUnix( const char *socket,         bool autoreconnect = false ) { return CreateClient( UNIX, NULL, 0, socket, autoreconnect ); }
    bool Start( );
    void Stop ( );

    virtual bool Send( const char *buffer, int len );
    virtual bool SendToClient( int client, const char *buffer, int len );

    bool isUp( ) { return up; }
    bool isConnected( ) { return connected; }

    void SetLogFunc( shlog logfunc ) { this->logfunc = logfunc; };

  protected:
    bool up;

    class Message
    {
      private:
        bool submitted;
      protected:
        std::string line;
      public:
        Message( ) { line = ""; submitted = false; }
        virtual ~Message( ) { };
        virtual int AccumulateData( const char *buffer, int length );
        void Submit( );// { submitted = true; };
        bool isSubmitted( ) { return submitted; }
        const std::string &getLine( ) const { return line; }
    };

    SocketHandler( );
    void Run( );
    bool Lock( );
    bool Unlock( );

    static void Dump( const char *buffer, int length );

    virtual Message *CreateMessage( int client ) const;

    void DisconnectClient( int client, bool error = false );

    // Callbacks
    virtual void Connected( int client ) = 0;
    virtual void Disconnected( int client, bool error ) = 0;
    virtual void HandleMessage( const int client, const Message &msg ) = 0;

  private:
    std::map<int, Message *> messages;

    shlog logfunc;

    static void StdLog      ( int level, const char *fmt, ... );
};

#endif

