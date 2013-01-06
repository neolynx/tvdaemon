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

    class Response
    {
      public:
        Response( );
        ~Response( );

        void AddStatus( HTTPStatus status );
        void AddTimeStamp( );
        void AddMime( const char *mime );
        void AddAttribute( const char *attrib_name, const char *attrib_value );
        void AddContents( std::string buffer );

        void FreeResponseBuffer( );
        std::string GetBuffer( ) const;
      private:
        std::string _buffer;
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

  private:
    std::string _root;
    std::map<int, HTTPRequest *> _requests;

    std::map<std::string, HTTPDynamicHandler *> dynamic_handlers;

    bool HandleRequest( HTTPRequest &request );

    // Handle HTTP methods
    bool HandleMethodGET( HTTPRequest &request );
    bool HandleMethodPOST( HTTPRequest &request );

    static int Tokenize( const char *string, const char delims[], std::vector<std::string> &tokens, int count = 0 );

};

class HTTPDynamicHandler
{
  public:
    virtual bool HandleDynamicHTTP( const HTTPRequest &request ) = 0;
};

class HTTPRequest
{
  public:
    HTTPRequest( HTTPServer &server, int client ) : server(server), client(client), content_length(-1) { }

    bool HasParam( std::string key ) const;
    bool GetParam( std::string key, std::string &value ) const;
    bool GetParam( std::string key, int &value ) const;

    void Reply( const HTTPServer::Response &response ) const;
    void Reply( json_object *obj ) const;
    void Reply( HTTPStatus status ) const;
    void Reply( HTTPStatus status, int ret ) const;
    void NotFound( const char *fmt, ... ) const __attribute__ (( format( printf, 2, 3 )));

    std::string GetDocRoot( ) const { return server.GetRoot( ); }

  private:
    HTTPServer &server;
    int client;
    std::list<std::string> header;
    int content_length;
    std::string content;
    std::map<std::string, std::string> parameters;
  friend class HTTPServer;
};

#endif

