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

typedef std::list<std::string> HTTPRequest;

class HTTPServer : public SocketHandler
{
  public:
    HTTPServer( const char *root );
    virtual ~HTTPServer( );

  protected:
    // Callbacks
    virtual void Connected   ( int client );
    virtual void Disconnected( int client, bool error );
    virtual void HandleMessage( const int client, const SocketHandler::Message &msg );

  private:
    std::string _root;
    std::map<int, HTTPRequest> _requests;

    bool HandleHTTPRequest( const int client, HTTPRequest &request );

    // Handle HTTP methods
    bool HandleMethodGET( const int client, HTTPRequest &request );
    //bool HandleMethodPOST( const int client, const std::vector<std::string> &tokens );

    bool HandleDynamicGET( const int client, const std::vector<const char *> &tokens );

    static int Tokenize( char *string, const char delims[], std::vector<const char *> &tokens, int count = 0 );

  private:
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
};

#endif

