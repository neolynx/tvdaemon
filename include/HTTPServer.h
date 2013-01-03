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

struct HTTPRequest
{
  HTTPRequest( ) { content_length = -1; }
  std::list<std::string> header;
  int content_length;
  std::string content;
};

class HTTPDynamicHandler
{
  public:
    virtual bool HandleDynamicHTTP( const int client, const std::map<std::string, std::string> &parameters ) = 0;
};

class HTTPServer : public SocketHandler
{
  public:
    HTTPServer( const char *root );
    virtual ~HTTPServer( );

    void AddDynamicHandler( std::string url, HTTPDynamicHandler *handler );

    std::string GetRoot( ) { return _root; }

    class HTTPResponse
    {
      public:
        HTTPResponse( );
        ~HTTPResponse( );

        void AddStatus( HTTPStatus status );
        void AddTimeStamp( );
        void AddMime( const char *mime );
        void AddAttribute( const char *attrib_name, const char *attrib_value );
        void AddContents( std::string buffer );

        void FreeResponseBuffer( );
        std::string GetBuffer( );
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

    bool HandleHTTPRequest( const int client, HTTPRequest &request );

    // Handle HTTP methods
    bool HandleMethodGET( const int client, HTTPRequest &request );
    bool HandleMethodPOST( const int client, HTTPRequest &request );

    static int Tokenize( const char *string, const char delims[], std::vector<std::string> &tokens, int count = 0 );

};

#endif

