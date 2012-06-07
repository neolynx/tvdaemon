/*
 * tvheadend
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#include "HttpServer.h"
#include "Utils.h"
#include "Log.h"

#include <sstream>
#include <istream>
#include <iterator>
#include <vector>
#include <iostream>
#include <fstream>
#include <streambuf>

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

HttpServer::HttpServer( )
{
}

HttpServer::~HttpServer( )
{
}

void HttpServer::Connected( int client )
{
}

void HttpServer::Disconnected( int client, bool error )
{
}

void HttpServer::HandleMessage( const int client, const SocketHandler::Message &msg )
{
  //Log( "HttpServer got message from %d: %s", client, msg.getLine( ).c_str( ));
  const char *buffer = msg.getLine( ).c_str( );
  int length = msg.getLine( ).length( );
  LogWarn( "Got: '%s' from client %d", msg.getLine( ).c_str( ), client );
  if( msg.getLine( ).empty( ))
  {
    HandleHttpRequest( client, _requests[client] );
    DisconnectClient( client );

    _requests.erase( client );
  }
  else
  {
    _requests[client].push_back( msg.getLine( ));
  }
}

bool HttpServer::HandleHttpRequest( const int client, HttpRequest &request )
{
  std::vector<std::string> tokens;
  Utils::StringSplit( request.front( ), std::string(" "), tokens );

  if( tokens[0] == "HEAD" )
  {
  }
  else if( tokens[0] == "GET" )
  {
    HandleMethodGET( client, tokens );
  }
  else if( tokens[0] == "POST" )
  {
  }
  else if( tokens[0] == "PUT" )
  {
  }
  else if( tokens[0] == "DELETE" )
  {
  }
  else if( tokens[0] == "TRACE" )
  {
  }
  else if( tokens[0] == "OPTIONS" )
  {
  }
  else if( tokens[0] == "CONNECT" )
  {
  }
  else if( tokens[0] == "PATCH" )
  {
  }
  return false;
}



bool HttpServer::HandleMethodGET( const int client, const std::vector<std::string> &tokens )
{
  if( tokens.size( ) < 3 )
  {
    return false;
  }
  std::string path = HTDOCS_ROOT;
  path += tokens[1];
  path = Utils::Expand( path.c_str( ));

  std::ifstream file;
  file.open( path.c_str( ), std::ifstream::in );
  if( !file.is_open( ))
  {
    HttpResponse *err_response = new HttpResponse( );
    err_response->AddStatus( HTTP_NOT_FOUND );
    err_response->AddTimeStamp( );
    LogWarn( "Sending: %s", err_response->GetBuffer( ).c_str( ));
    SendToClient( client, err_response->GetBuffer( ).c_str( ), err_response->GetBuffer( ).size( ));
    //SendHttpErrorResponse( client, STATUS_404_NOT_FOUND );
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

  HttpResponse *response = new HttpResponse( );
  response->AddStatus( HTTP_OK );
  response->AddTimeStamp( );
  response->AddMime( extension );
  response->AddContents( file_contents );
  LogWarn( "Sending %s", response->GetBuffer( ).c_str( ));

  return SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
}


HttpServer::HttpResponse::HttpResponse( )
{
}

HttpServer::HttpResponse::~HttpResponse( )
{
}

void HttpServer::HttpResponse::AddStatus( HttpStatus status )
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

void HttpServer::HttpResponse::AddTimeStamp( )
{
  time_t rawtime;
  time( &rawtime );
  struct tm *timeinfo = gmtime( &rawtime );
  char date_buffer[512];
  strftime( date_buffer, sizeof( date_buffer ), "%A, %e %B %Y %H:%M:%S GMT\r\n", timeinfo );
  _buffer.append( date_buffer );
}

void HttpServer::HttpResponse::AddAttribute( std::string &attrib_name, std::string &attrib_value )
{
  AddAttribute( attrib_name.c_str( ), attrib_value.c_str( ));
}

void HttpServer::HttpResponse::AddAttribute( const char *attrib_name, const char *attrib_value )
{
  char tmp[256];
  snprintf( tmp, sizeof( tmp ), "%s: %s\r\n", attrib_name, attrib_value );
  _buffer.append( tmp );
}

void HttpServer::HttpResponse::FreeResponseBuffer( )
{
  _buffer.clear( );
}

void HttpServer::HttpResponse::AddContents( std::string &buffer )
{
  size_t length = buffer.size( );
  char length_str[10];
  snprintf( length_str, sizeof( length_str ), "%d", length );

  AddAttribute( "Content-Length: ", length_str );
  _buffer.append( "\r\n" );
  _buffer += buffer;
}

std::string HttpServer::HttpResponse::GetBuffer( )
{
  return _buffer;
}

void HttpServer::HttpResponse::AddMime( std::string &extension )
{
  uint8_t pos = 0;
  for( uint8_t i = 0; i < ( sizeof( mime_types ) / sizeof( mime_type )); i++ )
  {
    if( extension == mime_types[i].extension )
    {
      pos = i;
      break;
    }
  }

  _buffer.append( "Content-Type: " );
  _buffer += mime_types[pos].type;
  _buffer.append( "\r\n" );
}
