/*
 * tvdaemon
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#ifndef _HTTPServer_
#define _HTTPServer_

#include "SocketHandler.h"

#include <map>
#include <string>
#include <list>
#include <vector>
#include <stdint.h>

#define HTTP_VERSION  "HTTP/1.0"
#define RTSP_VERSION  "RTSP/1.0"

struct json_object;

typedef enum
{
  HEAD,
  GET,
  POST,
  PUT,
  DELETE,
  TRACE,
  OPTIONS,
  CONNECT,
  PATCH,
  Unknown
} HTTPMethod;

typedef enum
{
  HTTP_OK                  = 200,
  HTTP_REDIRECT            = 300,
  HTTP_BAD_REQUEST         = 400,
  HTTP_UNAUTHORIZED        = 401,
  HTTP_FORBIDDEN           = 403,
  HTTP_NOT_FOUND           = 404,
  HTTP_INTERNAL_SRV_ERROR  = 500,
  HTTP_NOT_IMPLEMENTED     = 501,
} HTTPStatus;

typedef enum
{
  RTSP_OK                  = 200,
  } RTSPStatus;

struct http_status
{
  HTTPStatus code;
  const char *description;
};

struct mime_type
{
  const char *extension;
  const char *type;
  bool binary;
};

class HTTPDynamicHandler;
class HTTPRequest;

class HTTPServer : public SocketHandler
{
  public:
    HTTPServer( const char *root );
    virtual ~HTTPServer( );

    void AddDynamicHandler( std::string url, HTTPDynamicHandler *handler );

    std::string GetRoot( ) { return _root; }

    static void URLDecode( const std::string &string, std::string &decoded );

    class Response
    {
      public:
        Response( );
        ~Response( );

        void AddStatus( HTTPStatus status );
        void AddStatus( RTSPStatus status );
        void AddTimeStamp( );
        void AddMime( const char *mime );
        void AddHeader( const char *header, const char *fmt, ... ) __attribute__ (( format( printf, 3, 4 )));
        void AddContent( std::string buffer );

        void Finalize( );

        void FreeResponseBuffer( );
        std::string GetBuffer( ) const;
      private:
        std::string _buffer;
        bool header_closed;
    };

  protected:
    // Callbacks
    virtual void Connected   ( int client );
    virtual void Disconnected( int client, bool error );
    virtual void HandleMessage( const int client, const SocketHandler::Message &msg );

    class Message : public SocketHandler::Message
    {
      public:
        Message( int content_length ) : SocketHandler::Message( ), content_length(content_length) { }
        virtual ~Message( ) { };
        virtual int AccumulateData( const char *buffer, int length );
      private:
        int content_length;
    };

    virtual SocketHandler::Message *CreateMessage( int client ) const;

    typedef bool (HTTPServer::*Method)( HTTPRequest &request );

    void NotImplemented( HTTPRequest &request, const char *method );

  private:
    std::string _root;
    std::map<int, HTTPRequest *> _requests;

    std::map<std::string, HTTPDynamicHandler *> dynamic_handlers;

    std::map<std::string, Method> methods;

    bool HandleRequest( HTTPRequest &request );

    // Handle HTTP methods
    bool GET( HTTPRequest &request );
    bool POST( HTTPRequest &request );
    bool OPTIONS( HTTPRequest &request );
    bool DESCRIBE( HTTPRequest &request );
    bool GET_PARAMETER( HTTPRequest &request );
    bool SETUP( HTTPRequest &request );
    bool PLAY( HTTPRequest &request );
    bool TEARDOWN( HTTPRequest &request );

    static int Tokenize( const std::string &string, const char delims[], std::vector<std::string> &tokens, int count = 0 );

};

class HTTPDynamicHandler
{
  public:
    virtual bool HandleDynamicHTTP( const HTTPRequest &request ) = 0;
};

class HTTPRequest
{
  public:
    HTTPRequest( HTTPServer &server, int client ) : server(server), client(client), content_length(-1), keep_alive(false) { }

    bool HasHeader( const char *key ) const;
    bool GetHeader( const char *key, std::string &value ) const;
    bool GetHeader( const char *key, int &value ) const;

    bool HasParam( const char *key ) const;
    bool GetParam( const char *key, std::string &value ) const;
    bool GetParam( const char *key, int &value ) const;

    void Reply( HTTPServer::Response &response ) const;
    void Reply( json_object *obj ) const;
    void Reply( HTTPStatus status ) const;
    void Reply( HTTPStatus status, int ret ) const;
    void NotFound( const char *fmt, ... ) const __attribute__ (( format( printf, 2, 3 )));

    void KeepAlive( bool b ) { keep_alive = b; }
    bool KeepAlive( ) const { return keep_alive; }

    std::string GetDocRoot( ) const { return server.GetRoot( ); }

    void Reset( );

  private:
    HTTPServer &server;
    int client;
    std::string http_method;
    std::string url;
    std::string http_version;
    std::map<std::string, std::string> headers;
    int content_length;
    std::string content;
    std::map<std::string, std::string> parameters;
    bool keep_alive;
  friend class HTTPServer;
};

#endif

