/*
 * tvdaemon
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#ifndef _HttpServer_
#define _HttpServer_

#include "SocketHandler.h"

#include <map>
#include <string>
#include <list>
#include <vector>
#include <stdint.h>

#define HTTP_VERSION  "HTTP 1.0"
#define HTDOCS_ROOT   "~/.tvdaemon"

typedef enum {
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
} HttpMethod;

typedef enum {
  HTTP_OK                  = 200,
  HTTP_REDIRECT            = 300,
  HTTP_BAD_REQUEST         = 400,
  HTTP_UNAUTHORIZED        = 401,
  HTTP_FORBIDDEN           = 403,
  HTTP_NOT_FOUND           = 404,
  HTTP_INTERNAL_SRV_ERROR  = 500,
} HttpStatus;

struct http_status {
  HttpStatus code;
  const char *description;
};

struct mime_type {
  const char *extension;
  const char *type;
  bool binary;
};

typedef std::list<std::string> HttpRequest;

class HttpServer : public SocketHandler
{
  public:
    HttpServer( );
    virtual ~HttpServer( );

  protected:
    // Callbacks
    virtual void Connected   ( int client );
    virtual void Disconnected( int client, bool error );
    virtual void HandleMessage( const int client, const SocketHandler::Message &msg );
  private:
    std::map<int, HttpRequest> _requests;

    bool HandleHttpRequest( const int client, HttpRequest &request );

    // Handle HTTP methods
    bool HandleMethodGET( const int client, const std::vector<std::string> &tokens );
    //bool HandleMethodPOST( const int client, const std::vector<std::string> &tokens );

  private:
  class HttpResponse
  {
    public:
      HttpResponse( );
      ~HttpResponse( );

      void AddStatus( HttpStatus status );
      void AddTimeStamp( );
      void AddMime( std::string &extension );
      void AddAttribute( std::string &attrib_name, std::string &attrib_value );
      void AddAttribute( const char *attrib_name, const char *attrib_value );
      void AddContents( std::string &buffer );

      void FreeResponseBuffer( );
      std::string GetBuffer( );
    private:
      std::string _buffer;
  };
};

#endif

