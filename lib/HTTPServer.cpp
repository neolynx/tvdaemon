/*
 * tvdaemon
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#include "HTTPServer.h"
#include "Utils.h"
#include "Log.h"

#include <sstream>
#include <istream>
#include <iterator>
#include <vector>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <string.h>

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
  { "jpg", "image/jpeg", true },
  { "jpeg", "image/jpeg", true },
  { "html", "text/html", false },
};

HTTPServer::HTTPServer( const char *root ) : SocketHandler( ), _root(root)
{
}

HTTPServer::~HTTPServer( )
{
}

void HTTPServer::Connected( int client )
{
}

void HTTPServer::Disconnected( int client, bool error )
{
}

void HTTPServer::HandleMessage( const int client, const SocketHandler::Message &msg )
{
  const char *buffer = msg.getLine( ).c_str( );
  int length = msg.getLine( ).length( );
  //LogWarn( "Got: '%s' from client %d", msg.getLine( ).c_str( ), client );
  if( msg.getLine( ).empty( ))
  {
    HandleHTTPRequest( client, _requests[client] );
    DisconnectClient( client );

    _requests.erase( client );
  }
  else
  {
    _requests[client].push_back( msg.getLine( ));
  }
}

bool HTTPServer::HandleHTTPRequest( const int client, HTTPRequest &request )
{
  std::vector<const char *> tokens;
  Tokenize((char *) request.front( ).c_str( ), " ", tokens );

  if( tokens.empty( ))
    return false;

  if( strcmp( tokens[0], "GET" ) == 0 )
  {
    HandleMethodGET( client, tokens );
  }
  else
  {
    HTTPResponse *err_response = new HTTPResponse( );
    err_response->AddStatus( HTTP_NOT_IMPLEMENTED );
    err_response->AddTimeStamp( );
    err_response->AddMime( "html" );
    err_response->AddContents( "<html><body><h1>501 Method not implemented</h1></body></html>" );
    SendToClient( client, err_response->GetBuffer( ).c_str( ), err_response->GetBuffer( ).size( ));
  }
  return false;
}

#define DYNAMIC_URL "/tvd?"

bool HTTPServer::HandleMethodGET( const int client, const std::vector<const char *> &tokens )
{
  if( tokens.size( ) < 3 )
  {
    return false;
  }

  if( strncmp( tokens[1], DYNAMIC_URL, sizeof( DYNAMIC_URL ) - 1 ) == 0 )
  {
    return HandleDynamicGET( client, tokens );
  }
  std::string path = _root;
  path += tokens[1];
  path = Utils::Expand( path.c_str( ));

  if( Utils::IsDir( path ))
  {
    Utils::EnsureSlash( path );
    path += "index.html";
  }
  Log( "HTTPServer: opening: %s", path.c_str( ));

  std::ifstream file;
  file.open( path.c_str( ), std::ifstream::in );
  if( !file.is_open( ))
  {
    HTTPResponse *err_response = new HTTPResponse( );
    err_response->AddStatus( HTTP_NOT_FOUND );
    err_response->AddTimeStamp( );
    err_response->AddMime( "html" );
    err_response->AddContents( "<html><body><h1>404 Not found</h1></body></html>" );
    //LogWarn( "Sending: %s", err_response->GetBuffer( ).c_str( ));
    SendToClient( client, err_response->GetBuffer( ).c_str( ), err_response->GetBuffer( ).size( ));
    return false;
  }

  std::string file_contents;

  file.seekg( 0, std::ios::end );
  file_contents.reserve( file.tellg( ));
  file.seekg( 0, std::ios::beg );

  file_contents.assign((std::istreambuf_iterator<char>( file )), std::istreambuf_iterator<char>( ));
  file.close( );

  std::string basename = Utils::BaseName( path.c_str( ));
  std::string extension = Utils::GetExtension( basename );

  HTTPResponse *response = new HTTPResponse( );
  response->AddStatus( HTTP_OK );
  response->AddTimeStamp( );
  response->AddMime( extension.c_str( ));
  response->AddContents( file_contents );
  //LogWarn( "Sending %s", response->GetBuffer( ).c_str( ));

  return SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
}


HTTPServer::HTTPResponse::HTTPResponse( )
{
}

HTTPServer::HTTPResponse::~HTTPResponse( )
{
}

void HTTPServer::HTTPResponse::AddStatus( HTTPStatus status )
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

void HTTPServer::HTTPResponse::AddTimeStamp( )
{
  time_t rawtime;
  time( &rawtime );
  struct tm *timeinfo = gmtime( &rawtime );
  char date_buffer[512];
  strftime( date_buffer, sizeof( date_buffer ), "%A, %e %B %Y %H:%M:%S GMT\r\n", timeinfo );
  _buffer.append( date_buffer );
}

void HTTPServer::HTTPResponse::AddAttribute( const char *attrib_name, const char *attrib_value )
{
  char tmp[256];
  snprintf( tmp, sizeof( tmp ), "%s: %s\r\n", attrib_name, attrib_value );
  _buffer.append( tmp );
}

void HTTPServer::HTTPResponse::FreeResponseBuffer( )
{
  _buffer.clear( );
}

void HTTPServer::HTTPResponse::AddContents( std::string buffer )
{
  size_t length = buffer.size( );
  char length_str[10];
  snprintf( length_str, sizeof( length_str ), "%d", length );

  AddAttribute( "Content-Length", length_str );
  _buffer.append( "\r\n" );
  _buffer += buffer;
}

std::string HTTPServer::HTTPResponse::GetBuffer( )
{
  return _buffer;
}

void HTTPServer::HTTPResponse::AddMime( const char *mime )
{
  int i = -1;
  for( i = 0; i < ( sizeof( mime_types ) / sizeof( mime_type )); i++ )
    if( strcmp( mime, mime_types[i].extension ) == 0 )
      break;
  if( i < 0 )
    return;

  _buffer.append( "Content-Type: " );
  _buffer += mime_types[i].type;
  _buffer.append( "\r\n" );
}

int HTTPServer::Tokenize( char *string, const char delims[], std::vector<const char *> &tokens )
{
  int len = strlen( string );
  int dlen = strlen( delims );
  int last = -1;
  for( int i = 0; i < len; i++ )
  {
    // eat delimiters
    while( i < len )
    {
      bool found = false;
      for( int j = 0; j < dlen; j++ )
        if( string[i] == delims[j] )
        {
          found = true;
          break;
        }
      if( !found )
        break;
      i++;
    }
    if( i >= len )
      break;

    tokens.push_back( string + i );

    // eat non-delimiters
    while( i < len )
    {
      bool found = false;
      for( int j = 0; j < dlen; j++ )
        if( string[i] == delims[j] )
        {
          found = true;
          break;
        }
      if( found )
        break;
      i++;
    }
    if( i >= len )
      break;

    string[i] = '\0';
  }
}

bool HTTPServer::HandleDynamicGET( const int client, const std::vector<const char *> &tokens )
{
  Log( "dynamic get: %s", tokens[1] );
  std::vector<const char *> params;
  std::map<const char *, const char *> parameters;
  Tokenize((char *) tokens[1], "?&", params );
  for( int i = 1; i < params.size( ); i++ )
  {
    std::vector<const char *> p;
    Tokenize((char *) params[i], "=", p );
    if( p.size( ) != 2 )
    {
      LogWarn( "Ignoring strange parameter: '%s'", params[i] );
      continue;
    }
    parameters[p[0]] = p[1];

    Log( "Param: %s => %s", p[0], p[1] );
  }

  HTTPResponse *err_response = new HTTPResponse( );
  err_response->AddStatus( HTTP_OK );
  err_response->AddTimeStamp( );
  err_response->AddMime( "html" );
  err_response->AddContents( "<html><body>comming soon</body></html>" );
  SendToClient( client, err_response->GetBuffer( ).c_str( ), err_response->GetBuffer( ).size( ));
}

