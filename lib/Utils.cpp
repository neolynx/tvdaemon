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
#include <libgen.h> // dirname
#include <string.h> // strdup
#include <stdlib.h> // free
#include <stdio.h>  // snprintf

#include "Log.h"

std::string Utils::Expand( std::string path )
{
  wordexp_t p;
  std::string ex;
  if( wordexp( path.c_str( ), &p, 0 ) == 0 )
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
    snprintf( t, sizeof(t), "%02x ", (unsigned int) data[i] );
    strncat( hex, t, sizeof( hex ));
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

