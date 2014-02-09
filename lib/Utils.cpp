/*
 *  tvdaemon
 *
 *  Utils class
 *
 *  Copyright (C) 2012 André Roth
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Utils.h"

#include <wordexp.h>
#include <sys/stat.h>
#include <libgen.h>  // dirname
#include <string.h>  // strdup
#include <stdlib.h>  // free
#include <stdio.h>   // snprintf
#include <algorithm> // transform
#include <locale>
#include <sys/statvfs.h>

#include "Log.h"

std::string Utils::Expand( const std::string &path )
{
  wordexp_t p;
  std::string ex = path;
  size_t pos = 0;
  while( true )
  {
    pos = ex.find( ' ', pos );
    if( pos == std::string::npos )
      break;
    ex.insert( pos, "\\" );
    pos += 2; // skip backslash and space
  }
  if( wordexp( ex.c_str( ), &p, 0 ) == 0 )
  {
    if( p.we_wordc > 0 )
      ex = p.we_wordv[0];
    wordfree( &p );
  }
  return ex;
}

std::string Utils::DirName( std::string path )
{
  char *tmp = strdup( path.c_str( ));
  char *dir = dirname( tmp );
  if( !dir )
  {
    free( tmp );
    return "";
  }
  std::string r( dir );
  free( tmp );
  return r;
}

std::string Utils::BaseName( std::string path )
{
  char *tmp = strdup( path.c_str( ));
  char *dir = basename( tmp );
  if( !dir )
  {
    free( tmp );
    return "";
  }
  std::string r( dir );
  free( tmp );
  return r;
}

bool Utils::IsDir( std::string path )
{
  struct stat sb;
  if( stat( path.c_str( ), &sb ) == -1 )
    return false;
  return S_ISDIR( sb.st_mode );
}

bool Utils::IsFile( std::string path )
{
  struct stat sb;
  if( stat( path.c_str( ), &sb ) == -1 )
    return false;
  return S_ISREG( sb.st_mode );
}

bool Utils::MkDir( std::string path )
{
  return mkdir( path.c_str( ), 0755 ) == 0;
}

void Utils::EnsureSlash( std::string &dir )
{
  if( dir[dir.length( ) - 1] != '/' )
    dir += "/";
}

bool Utils::DiskFree( std::string &dir, size_t &df )
{
  struct statvfs st;
  if( statvfs( dir.c_str( ), &st ) < 0 )
  {
    return false;
  }
  df = st.f_bsize * st.f_bfree;
  return true;
}

std::string Utils::GetExtension( std::string &filename )
{
  return filename.substr( filename.find_last_of( "." ) + 1 );
}

void Utils::dump( const unsigned char *data, int length )
{
  if( !data )
    return;
  Log( "Dump %d bytes:", length );
  char ascii[17];
  char hex[50];
  int j = 0;
  hex[0] = '\0';
  for( int i = 0; i < length; i++ )
  {
    char t[4];
    snprintf( t, sizeof( t ), "%02x ", (unsigned int) data[i] );
    strncat( hex, t, 3 );
    if( data[i] > 31 && data[i] < 128 )
      ascii[j] = data[i];
    else
      ascii[j] = '.';
    j++;
    if( j == 8 )
      strncat( hex, " ", sizeof( hex ));
    if( j == 16 )
    {
      ascii[j] = '\0';
      Log( "%s  %s", hex, ascii );
      j = 0;
      hex[0] = '\0';
    }
  }
  if( j > 0 && j < 16 )
  {
    char spaces[47];
    spaces[0] = '\0';
    for( int i = strlen(hex); i < 49; i++ )
      strncat( spaces, " ", sizeof( spaces ));
    ascii[j] = '\0';
    Log( "%s %s %s", hex, spaces, ascii );
  }
}

void Utils::ToLower( const std::string &string, std::string &lower )
{
  lower = string;
  std::transform( lower.begin( ), lower.end( ), lower.begin( ), std::bind2nd( std::ptr_fun( &std::tolower<char> ), std::locale( "" )));
}


Name::Name( ) : std::string( )
{
}

Name::Name( const char *str ) : std::string( str )
{
  lower = str;
  Utils::ToLower( lower, lower );
}

Name::Name( const std::string &str ) : std::string( str )
{
  lower = str;
  Utils::ToLower( lower, lower );
}

Name::Name( std::string &str ) : std::string( str )
{
  lower = str;
  Utils::ToLower( lower, lower );
}

Name::Name( Name &other ) : std::string( other )
{
  lower = *this;
  Utils::ToLower( lower, lower );
}

Name::~Name( )
{
}

size_t Name::find( const char *s, size_t pos, size_t n ) const
{
  return lower.find( s, pos, n );
}

bool operator<( const Name &a, const Name &b )
{
  return a.lower < b.lower;
}

Name &Name::operator=( const Name &other )
{
  if( this == &other )
    return *this;
  *((std::string *) this) = other;
  lower = other;
  Utils::ToLower( lower, lower );
  return *this;
}

Name &Name::operator=( const std::string &other )
{
  *((std::string *) this) = other;
  lower = other;
  Utils::ToLower( lower, lower );
  return *this;
}

Name &Name::operator=( const char *str )
{
  *((std::string *) this) = str;
  lower = str;
  Utils::ToLower( lower, lower );
  return *this;
}

int Utils::Tokenize( const std::string &string, const char delims[], std::vector<std::string> &tokens, int count )
{
  int len = string.length( );
  int dlen = strlen( delims );
  int last = -1;
  tokens.clear( );
  for( int i = 0; i < len; )
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
    if( i > len )
      break;

    int j = i;

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

    tokens.push_back( std::string( string.c_str( ) + j,  i - j ));

    if( i == len )
      break;

    if( count )
      if( --count == 0 )
        break;
  }
}

