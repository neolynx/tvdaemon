/*
 * tvdaemon
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#include "HTTPServer.h"
#include "Utils.h"
#include "Log.h"

#include <string.h>
#include <RPCObject.h>
#include <sstream>
#include <istream>
#include <iterator>
#include <vector>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <stdlib.h> // atoi
#include <arpa/inet.h> // inet_ntop

#include "StreamingHandler.h" // FIXME: mm.. needed?
#include "TVDaemon.h" // FIXME: mm.. needed?

static const struct http_status response_status[] = {
  { HTTP_OK, "OK" },
  { HTTP_REDIRECT, "Redirect" },
  { HTTP_BAD_REQUEST, "Bad Request" },
  { HTTP_UNAUTHORIZED, "Unauthorized" },
  { HTTP_FORBIDDEN, "Forbidden" },
  { HTTP_NOT_FOUND, "Not Found" },
  { HTTP_INTERNAL_SRV_ERROR, "Internal Server Error" },
};

static const struct mime_type mime_types[] = {
  { "txt",  "text/plain",       false },
  { "html", "text/html",        false },
  { "js",   "text/javascript",  false },
  { "css",  "text/css",         false },
  { "jpg",  "image/jpeg",       true },
  { "jpeg", "image/jpeg",       true },
  { "gif",  "image/gif",        true },
  { "png",  "image/png",        true },
  { "json", "application/json", false },
  { "ico",  "image/x-icon",     true },
};

HTTPServer::HTTPServer( const char *root ) : SocketHandler( ), _root(root), rtsp_handler(NULL)
{
  methods["GET"] = &HTTPServer::GET;
  methods["POST"] = &HTTPServer::POST;
  methods["OPTIONS"] = &HTTPServer::OPTIONS;
  methods["DESCRIBE"] = &HTTPServer::DESCRIBE;
  methods["GET_PARAMETER"] = &HTTPServer::GET_PARAMETER;
  methods["SETUP"] = &HTTPServer::SETUP;
  methods["PLAY"] = &HTTPServer::PLAY;
  methods["TEARDOWN"] = &HTTPServer::TEARDOWN;
}

HTTPServer::~HTTPServer( )
{
}

void HTTPServer::Connected( int client )
{
}

void HTTPServer::Disconnected( int client, bool error )
{
  if( _requests.find( client ) != _requests.end( ))
  {
    delete _requests[client];
    _requests.erase( client );
  }
}

SocketHandler::Message *HTTPServer::CreateMessage( int client ) const
{
  std::map<int, HTTPRequest *>::const_iterator it = _requests.find( client );
  if( it != _requests.end( ) && it->second->content_length != -1 )
    return new HTTPServer::Message( it->second->content_length );
  return new SocketHandler::Message( );
}

int HTTPServer::Message::AccumulateData( const char *buffer, int length )
{
  int i = content_length - line.length( );
  if( length < i )
    i = length;

  line.append( buffer, i );
  if( content_length == line.length( ))
  {
    Submit( );
  }
  return i;
}

void HTTPServer::HandleMessage( const int client, const SocketHandler::Message &msg )
{
  //const char *buffer = msg.getLine( ).c_str( );
  //int length = msg.getLine( ).length( );
  //LogWarn( "Got: '%s' from client %d", msg.getLine( ).c_str( ), client );
  HTTPRequest *request = NULL;
  if( _requests.find( client ) == _requests.end( ))
  {
    request = new HTTPRequest( *this, client ); // freed on disconnect
    _requests[client] = request;
  }
  else
    request = _requests[client];

  if( msg.getLine( ).empty( ))
  {
    if( request->content_length == -1 and request->HasHeader( "Content-Length" ))
    {
      request->GetHeader( "Content-Length", request->content_length );
      //DisconnectClient( client );
      //return;
    }
    if( request->http_method.empty( ))
    {
      LogError( "HTTPServer: no http method '%s'", msg.getLine( ).c_str( ));
      DisconnectClient( client );
      return;
    }

    if( request->content_length <= 0 || request->content.length( ) == request->content_length ) // FIXME: handle on top
    {
      if( !HandleRequest( *request ) or !request->KeepAlive( ))
        DisconnectClient( client );
      else
        request->Reset( );
      return;
    }
    return;
  }

  if( request->content_length > 0 && request->content.length( ) < request->content_length )
  {
    request->content += msg.getLine( );
    if( request->http_method.empty( ))
    {
      LogError( "HTTPServer: no http method '%s'", msg.getLine( ).c_str( ));
      DisconnectClient( client );
      return;
    }

    if( !HandleRequest( *request ) or !request->KeepAlive( ))
      DisconnectClient( client );
    else
      request->Reset( );
  }
  else
  {
    if( request->http_method.empty( )) // handle HTTP request (GET, ...)
    {
      std::vector<std::string> tokens;
      Utils::Tokenize( msg.getLine( ), " ", tokens, 3 );
      if( tokens.size( ) < 3 )
      {
        LogError( "HTTPServer: invalid http request '%s'", msg.getLine( ).c_str( ));
        DisconnectClient( client );
        return;
      }
      request->http_method = tokens[0];
      request->url = tokens[1];
      request->http_version = tokens[2];
      //Log( "HTTPServer: request: '%s', '%s' (%s)", request->http_method.c_str( ), request->url.c_str( ), request->http_version.c_str( ));
      return;
    }
    std::vector<std::string> tokens; // handle HTTP headers
    Utils::Tokenize( msg.getLine( ), ": ", tokens, 2 );
    if( tokens.size( ) < 2 )
    {
      LogError( "HTTPServer: unknown http message '%s'", msg.getLine( ).c_str( ));
      DisconnectClient( client );
      return;
    }
    request->headers[tokens[0]] = tokens[1];
  }
}

void HTTPServer::AddDynamicHandler( std::string url, HTTPDynamicHandler *handler )
{
  url = "/" + url;
  dynamic_handlers[url] = handler;
}

void HTTPServer::SetRTSPHandler( RTSPHandler *handler )
{
  if( !rtsp_handler )
    rtsp_handler = handler;
}

void HTTPServer::NotImplemented( HTTPRequest &request, const char *method )
{
  Response response;
  response.AddStatus( HTTP_NOT_IMPLEMENTED );
  response.AddTimeStamp( );
  response.AddMime( "html" );
  response.AddContent( "<html><body><h1>501 Method not implemented</h1></body></html>" );
  LogError( "HTTPServer: method not implemented: %s", method );
  request.Reply( response );
}

bool HTTPServer::HandleRequest( HTTPRequest &request )
{
  std::map<std::string, Method>::iterator it = methods.find( request.http_method );
  if( it != methods.end( ))
  {
    Log( "HTTP request %s %s %s", inet_ntoa( request.GetClientIP( )), request.http_method.c_str( ), request.url.c_str( ));
    return (this->*it->second)( request );
  }

  NotImplemented( request, request.http_method.c_str( )); // FIXME: pass method as std::string
  return false;
}

bool HTTPServer::GET( HTTPRequest &request )
{
  std::vector<std::string> params;
  Utils::Tokenize( request.url, "?&", params );
  for( int i = 1; i < params.size( ); i++ )
  {
    std::vector<std::string> p;
    Utils::Tokenize( params[i], "=", p );
    if( p.size( ) == 1 )
      request.parameters[p[0]] = "1";
    else if( p.size( ) == 2 )
      URLDecode( p[1], request.parameters[p[0]] );
    else
    {
      LogWarn( "Ignoring strange parameter: '%s'", params[i].c_str( ));
      continue;
    }
    //Log( "Param: %s => %s", p[0], val );
  }

  for( std::map<std::string, HTTPDynamicHandler *>::iterator it = dynamic_handlers.begin( ); it != dynamic_handlers.end( ); it++ )
    if( params[0] == it->first )
    {
      if( !it->second->HandleDynamicHTTP( request ))
      {
        LogError( "RPC Error %s", request.url.c_str( ));
        return false;
      }
      //Log( "RPC %s", url.c_str( ));
      return true;
    }

  // Handle files

  std::string url = _root;
  Utils::EnsureSlash( url );
  url = Utils::Expand( url.c_str( ));
  std::string t = request.url;
  size_t pos;
  while(( pos = t.find( ".." )) != std::string::npos )
    t.replace( pos, 2, "." );
  while( t[0] == '/' )
    t = t.substr( 1, std::string::npos );
  url += t;

  if( url.empty( ))
  {
    LogError( "HTTPServer: file not found: '%s'", request.url.c_str( ));
    Response err_response;
    err_response.AddStatus( HTTP_NOT_FOUND );
    err_response.AddTimeStamp( );
    err_response.AddMime( "html" );
    err_response.AddContent( "<html><body><h1>404 Not found</h1></body></html>" );
    //LogWarn( "Sending: %s", err_response->GetBuffer( ).c_str( ));
    request.Reply( err_response );
    return false;
  }

  if( Utils::IsDir( url ))
  {
    Utils::EnsureSlash( url );
    url += "index.html";
  }

  std::string filename;

  size_t p = url.find_first_of("#?");
  if( p != std::string::npos )
    filename = url.substr( 0, p );
  else
    filename = url;

  std::ifstream file;
  file.open( filename.c_str( ), std::ifstream::in );
  if( !file.is_open( ))
  {
    LogError( "HTTPServer: file not found: %s", url.c_str( ));
    Response err_response;
    err_response.AddStatus( HTTP_NOT_FOUND );
    err_response.AddTimeStamp( );
    err_response.AddMime( "html" );
    err_response.AddContent( "<html><body><h1>404 Not found</h1></body></html>" );
    //LogWarn( "Sending: %s", err_response->GetBuffer( ).c_str( ));
    request.Reply( err_response );
    return false;
  }

  //Log( "HTTPServer: serving: %s", url.c_str( ));
  std::string file_contents;

  file.seekg( 0, std::ios::end );
  file_contents.reserve( file.tellg( ));
  file.seekg( 0, std::ios::beg );

  file_contents.assign((std::istreambuf_iterator<char>( file )), std::istreambuf_iterator<char>( ));
  file.close( );

  std::string basename = Utils::BaseName( filename.c_str( ));
  std::string extension = Utils::GetExtension( basename );

  Response response;
  response.AddStatus( HTTP_OK );
  response.AddTimeStamp( );
  response.AddMime( extension.c_str( ));
  response.AddContent( file_contents );
  //LogWarn( "Sending %s", response->GetBuffer( ).c_str( ));

  request.Reply( response );
  return true;
}

bool HTTPServer::POST( HTTPRequest &request )
{
  std::vector<std::string> params;
  Utils::Tokenize( request.url, "?&", params );
  for( int i = 1; i < params.size( ); i++ )
  {
    std::vector<std::string> p;
    Utils::Tokenize( params[i], "=", p );
    if( p.size( ) == 1 )
      request.parameters[p[0]] = "1";
    else if( p.size( ) == 2 )
      URLDecode( p[1], request.parameters[p[0]] );
    else
    {
      LogWarn( "Ignoring strange parameter: '%s'", params[i].c_str( ));
      continue;
    }
    //Log( "Param: %s => %s", p[0], val );
  }

  if( request.content.length( ) > 0 )
  {
    std::vector<std::string> vars;
    Utils::Tokenize( request.content, "&", vars );
    for( int i = 0; i < vars.size( ); i++ )
    {
      std::vector<std::string> p;
      Utils::Tokenize( vars[i], "=", p );
      if( p.size( ) == 1 )
        request.parameters[p[0]] = "1";
      else if( p.size( ) == 2 )
      {
        URLDecode( p[1], request.parameters[p[0]] );
      }
      else
      {
        LogWarn( "Ignoring strange variable: '%s'", params[i].c_str( ));
        continue;
      }
    }

  }

  for( std::map<std::string, HTTPDynamicHandler *>::iterator it = dynamic_handlers.begin( ); it != dynamic_handlers.end( ); it++ )
    if( params[0] == it->first )
    {
      if( !it->second->HandleDynamicHTTP( request ))
      {
        LogError( "RPC Error %s", request.url.c_str( ));
        return false;
      }
      return true;
    }

  LogError( "HTTPServer: no dynamic handler found for POST request" );
  return false;
}

bool HTTPServer::OPTIONS( HTTPRequest &request )
{
  int seq;
  if( !request.GetHeader( "CSeq", seq ))
  {
    LogError( "HTTPServer::OPTIONS no Cseq found" );
    return false;
  }

  Response response;
  response.AddStatus( RTSP_OK );
  response.AddHeader( "Supported", "play.basic, con.persistent" );
  response.AddHeader( "Cseq", "%d", seq );
  response.AddHeader( "Server", "TVDaemon" );
  //response.AddHeader( "Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD, GET_PARAMETER" );
  response.AddHeader( "Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER" );
  response.AddHeader( "Cache-Control", "no-cache" );
  request.Reply( response );
  request.KeepAlive( true );
  return true;
}

bool HTTPServer::DESCRIBE( HTTPRequest &request )
{
  int seq;
  if( !request.GetHeader( "CSeq", seq ))
  {
    LogError( "HTTPServer::OPTIONS no Cseq found" );
    return false;
  }

  std::vector<std::string> tokens;
  Utils::Tokenize( request.url, "/", tokens, 3 );
  if( tokens.size( ) != 3 ) // rtsp://server:port/channel
  {
    LogError( "TVDaemon::GetStreamingHandler: invalid url: %s (%d tokens)", request.url.c_str( ), tokens.size( ));
    return false;
  }
  std::string channel_name = tokens[2];

  int rtp_port = StreamingHandler::Instance( )->GetFreeRTPPort( );

  char t[255];
  snprintf( t, sizeof( t ), "%d", rtp_port );

  std::string sdp = "v=0\r\n\
o=- 15492257274586900535 15492257274586900535 IN IP6 neutrino\r\n\
s=";
  sdp += channel_name;
  sdp += "\r\n\
i=N/A\r\n\
c=IN IP6 ::1\r\n\
t=0 0\r\n\
a=tool:tvdaemon\r\n\
a=recvonly\r\n\
a=type:broadcast\r\n\
a=charset:UTF-8\r\n\
a=control:";
  sdp += request.url;
  sdp += "\r\n\
m=video ";
  sdp += t;
  sdp += " RTP/AVP 33\r\n\
b=RR:0\r\n\
a=rtpmap:33 MP2T/90000\r\n\
a=control:";
  sdp += request.url;
  sdp += "\r\n";

  Response response;
  response.AddStatus( RTSP_OK );
  response.AddTimeStamp( );
  response.AddHeader( "Content-Base", "%s", request.url.c_str( ));
  response.AddHeader( "Cseq", "%d", seq );
  response.AddHeader( "Server", "TVDaemon" );
  response.AddHeader( "Cache-Control", "no-cache" );

  response.AddContent( sdp.c_str( ));
  request.KeepAlive( true );
  request.Reply( response );
  return true;
}

bool HTTPServer::GET_PARAMETER( HTTPRequest &request )
{
  return HTTPServer::OPTIONS( request );
}

bool HTTPServer::SETUP( HTTPRequest &request )
{
  Log( "HTTPServer::SETUP" );
  if( !rtsp_handler )
  {
    LogError( "RTSP: no handler defined" );
    return false;
  }

  int seq;
  if( !request.GetHeader( "CSeq", seq ))
  {
    LogError( "HTTPServer::SETUP no Cseq found" );
    return false;
  }

  std::string transport;
  if( !request.GetHeader( "Transport", transport ))
  {
    LogError( "HTTPServer::SETUP no Transport found" );
    return false;
  }

  int rtp_port = -1;
  std::vector<std::string> tokens;
  Utils::Tokenize( transport, ";=", tokens );
  for( std::vector<std::string>::iterator it = tokens.begin( ); it != tokens.end( ); it++ )
    if( *it == "client_port" )
    {
      it++;
      rtp_port = atoi( (*it).c_str( ));
    }

  if( rtp_port == -1 )
  {
    LogError( "RTSP: client port not found in SETUP" );
    return false;
  }

  Utils::Tokenize( request.url, "/", tokens, 3 );
  if( tokens.size( ) != 3 ) // rtsp://server:port/channel
  {
    LogError( "RTSP: invalid setup url (/): %s", request.url.c_str( ));
    return false;
  }
  std::string server = tokens[1];
  std::string channel_name;
  HTTPServer::URLDecode( tokens[2], channel_name );

  Channel *channel = TVDaemon::Instance( )->GetChannel( channel_name );

  Utils::Tokenize( server, ":", tokens, 2 );
  if( tokens.size( ) < 1 ) // server:port
  {
    LogError( "RTSP: invalid setup url (:): %s", request.url.c_str( ));
    return false;
  }
  std::string hostname = tokens[0];

  char str[INET_ADDRSTRLEN];
  in_addr addr = request.GetClientIP( );
  inet_ntop( AF_INET, &addr, str, sizeof( str ));

  int session_id = StreamingHandler::Instance( )->Setup( channel, hostname, str, rtp_port );

  Response response;
  response.AddStatus( RTSP_OK );
  response.AddTimeStamp( );
  response.AddHeader( "Cseq", "%d", seq );
  response.AddHeader( "Server", "TVDaemon" );
  transport += ";source=exciton;server_port=7066-7067;ssrc=02E34D2C";
  response.AddHeader( "Transport", "%s", transport.c_str( ));
  char session[256];
  snprintf( session, sizeof( session ), "%d;timeout=60", session_id );
  response.AddHeader( "Session", "%s", session );
  // FIXME: Expires
  response.AddHeader( "Cache-Control", "no-cache" );

  request.KeepAlive( true );
  request.Reply( response );
  return true;
}

bool HTTPServer::PLAY( HTTPRequest &request )
{
  Log( "HTTPServer::PLAY" );
  if( !rtsp_handler )
  {
    LogError( "RTSP: no handler defined" );
    return false;
  }

  std::vector<std::string> tokens;
  std::string session;
  if( !request.GetHeader( "Session", session ))
  {
    LogError( "RTSP:PLAY no session parameter found" );
    return false;
  }
  int session_id = atoi( session.c_str( ));

  StreamingHandler::Instance( )->Play( session_id );

  Response response;
  response.AddStatus( RTSP_OK );
  request.KeepAlive( true );
  request.Reply( response );

  return true;
}

bool HTTPServer::TEARDOWN( HTTPRequest &request )
{
  Log( "HTTPServer::TEARDOWN" );
  if( !rtsp_handler )
  {
    LogError( "RTSP: no handler defined" );
    return false;
  }

  std::vector<std::string> tokens;
  std::string session;
  if( !request.GetHeader( "Session", session ))
  {
    LogError( "RTSP:TEARDOWN no session parameter found" );
    return false;
  }
  int session_id = atoi( session.c_str( ));

  StreamingHandler::Instance( )->Stop( session_id );

  Response response;
  response.AddStatus( RTSP_OK );
  request.KeepAlive( false );
  request.Reply( response );
  return true;
}

HTTPServer::Response::Response( ) : header_closed(false)
{
}

HTTPServer::Response::~Response( )
{
}

void HTTPServer::Response::Finalize( )
{
  if( !header_closed )
  {
    _buffer.append( "\r\n" );
    header_closed = true;
  }
}

void HTTPServer::Response::AddStatus( HTTPStatus status )
{
  char tmp[128];
  uint8_t pos = 0;

  for( uint8_t i = 0; i < ( sizeof( response_status ) / sizeof( http_status )); i++ )
  {
    if( response_status[i].code == status )
    {
      pos = i;
      break;
    }
  }

  snprintf( tmp, sizeof( tmp ), "%s %d %s\r\n", HTTP_VERSION, status, response_status[pos].description );
  _buffer.append( tmp );
}

void HTTPServer::Response::AddStatus( RTSPStatus status )
{
  char tmp[128];
  uint8_t pos = 0;

  for( uint8_t i = 0; i < ( sizeof( response_status ) / sizeof( http_status )); i++ ) // FIXME: use proper lookup
  {
    if( response_status[i].code == (HTTPStatus) status )
    {
      pos = i;
      break;
    }
  }

  snprintf( tmp, sizeof( tmp ), "%s %d %s\r\n", RTSP_VERSION, status, response_status[pos].description );
  _buffer.append( tmp );
}

void HTTPServer::Response::AddTimeStamp( )
{
  time_t now;
  time( &now );
  struct tm tm_now;
  ::gmtime_r( &now, &tm_now );
  char date_buffer[512];
  strftime( date_buffer, sizeof( date_buffer ), "%A, %e %B %Y %H:%M:%S GMT\r\n", &tm_now );
  _buffer.append( date_buffer );
}

void HTTPServer::Response::AddHeader( const char *header, const char *fmt, ... )
{
  char tmp[256];
  char value[256];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( value, sizeof( value ), fmt, ap );
  snprintf( tmp, sizeof( tmp ), "%s: %s\r\n", header, value );
  va_end( ap );
  _buffer.append( tmp );
}

void HTTPServer::Response::FreeResponseBuffer( )
{
  _buffer.clear( );
}

void HTTPServer::Response::AddContent( std::string buffer )
{
  size_t length = buffer.size( );
  char length_str[10];
  snprintf( length_str, sizeof( length_str ), "%d", (int) length );

  AddHeader( "Content-Length", "%s", length_str );

  Finalize( );
  _buffer += buffer;
}

std::string HTTPServer::Response::GetBuffer( ) const
{
  return _buffer;
}

void HTTPServer::Response::AddMime( const char *mime )
{
  int i = -1;
  for( i = 0; i < sizeof( mime_types ) / sizeof( mime_type ); i++ )
    if( strcmp( mime, mime_types[i].extension ) == 0 )
      break;
  if( i >= sizeof( mime_types ) / sizeof( mime_type ))
  {
    LogError( "HTTPServer: unknown mime '%s'", mime );
    return;
  }

  _buffer.append( "Content-Type: " );
  _buffer += mime_types[i].type;
  _buffer.append( "\r\n" );
}

void HTTPRequest::Reset( )
{
  http_method = "";
  url = "";
  http_version = "";
  headers.clear( );
  parameters.clear( );
  content = "";
}

void HTTPRequest::NotFound( const char *fmt, ... ) const
{
  HTTPServer::Response response;
  response.AddStatus( HTTP_NOT_FOUND );
  response.AddTimeStamp( );
  response.AddMime( "txt" );
  va_list ap;
  va_start( ap, fmt );
  char buf[1024];
  vsnprintf( buf, sizeof( buf ), fmt, ap );
  response.AddContent( buf );
  va_end( ap );
  Reply( response );
}

bool HTTPRequest::GetHeader( const char *key, std::string &value ) const
{
  const std::map<std::string, std::string>::const_iterator p = headers.find( std::string( key ));
  if( p == headers.end( ))
  {
    NotFound( "RPC param '%s' not found", key );
    return false;
  }
  value = p->second;
  return true;
}

bool HTTPRequest::GetHeader( const char *key, int &value ) const
{
  std::string t;
  if( !GetHeader( key, t ))
    return false;
  value = atoi( t.c_str( ));
  return true;
}

bool HTTPRequest::HasHeader( const char *key ) const
{
  const std::map<std::string, std::string>::const_iterator p = headers.find( std::string( key ));
  return p != headers.end( );
}

bool HTTPRequest::GetParam( const char *key, std::string &value ) const
{
  const std::map<std::string, std::string>::const_iterator p = parameters.find( key );
  if( p == parameters.end( ))
  {
    NotFound( "RPC param '%s' not found", key );
    return false;
  }
  value = p->second;
  return true;
}

bool HTTPRequest::GetParam( const char *key, int &value ) const
{
  std::string t;
  if( !GetParam( key, t ))
    return false;
  value = atoi( t.c_str( ));
  return true;
}

bool HTTPRequest::HasParam( const char *key ) const
{
  const std::map<std::string, std::string>::const_iterator p = parameters.find( key );
  return p != parameters.end( );
}

void HTTPRequest::Reply( HTTPServer::Response &response ) const
{
  response.Finalize( );
  server.SendToClient( client, response.GetBuffer( ).c_str( ), response.GetBuffer( ).size( ));
}

void HTTPRequest::Reply( HTTPStatus status ) const
{
  HTTPServer::Response response;
  response.AddStatus( status );
  response.AddTimeStamp( );
  Reply( response );
}

void HTTPRequest::Reply( HTTPStatus status, int ret ) const
{
  HTTPServer::Response response;
  response.AddStatus( status );
  response.AddTimeStamp( );
  response.AddMime( "txt" );
  char buf[32];
  snprintf( buf, sizeof( buf ), "%d", ret );
  response.AddContent( buf );
  Reply( response );
}

void HTTPRequest::Reply( json_object *obj ) const
{
  std::string json = json_object_to_json_string( obj );
  HTTPServer::Response response;
  response.AddStatus( HTTP_OK );
  response.AddTimeStamp( );
  response.AddMime( "json" );
  response.AddContent( json );
  Reply( response );
  json_object_put( obj ); // free
}

inline void nibble( char &x )
{
  switch( x )
  {
    case '0' ... '9':
      x -= '0';
      break;
    case 'a' ... 'f':
      x -= 'a' - 10;
      break;
    case 'A' ... 'F':
      x -= 'A' - 10;
      break;
    default:
      LogError( "HTTPServer: not a hex value: %c", x );
  }
}

inline void urlhex( char x, char y, std::string &append )
{
  nibble( x );
  nibble( y );
  append += ( x << 4 ) + y;
}

void HTTPServer::URLDecode( const std::string &string, std::string &decoded )
{
  int len = string.length( );
  for( int j = 0; j < len; j++ )
  {
    if( string[j] == '%' )
    {
      if( j > len - 3 )
      {
        LogError( "HTTPServer: invalid url encoded data: '%s'", string.c_str( ));
        break;
      }
      urlhex( string[j+1], string[j+2], decoded );
      j += 2;
      continue;
    }
    if( string[j] == '+' )
      decoded += ' ';
    else
      decoded += string[j];
  }
}

HTTPRequest::HTTPRequest( HTTPServer &server, int client ) : server(server), client(client), content_length(-1), keep_alive(false)
{
  memset( &client_addr, 0, sizeof( client_addr ));
  server.GetClientAddress( client, client_addr );
}

in_addr HTTPRequest::GetClientIP( ) const
{
  return client_addr.sin_addr;
}

