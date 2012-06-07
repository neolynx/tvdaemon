/*
 *  tvheadend
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

#include "descriptors/pat.h"
#include "descriptors/pmt.h"

std::string Utils::Expand( std::string path )
{
  wordexp_t p;
  wordexp( path.c_str( ), &p, 0 );
  if( p.we_wordc > 0 )
  {
    return p.we_wordv[0];
  }
  return "";
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

//uint32_t Utils::decihex( uint32_t value )
//{
  //uint32_t ret = 0, pow = 1;

  //for( int i = 0; i < 8; i++ )
  //{
    //if(( value & 0x0f ) > 9 )
      //printf( "houston, we have a problem\n" );
    //ret += ( value & 0x0f ) * pow;
    //value >>= 4;
    //pow *= 10;
  //}
  //return ret;
//}

int Utils::StringSplit( const std::string& input, const std::string& delimiter, std::vector<std::string>& results, bool includeEmpties )
{
  int iPos   = 0;
  int newPos = -1;
  int sizeS2 = (int)delimiter.size( );
  int isize  = (int)input.size( );

  if(( isize == 0 ) || ( sizeS2 == 0 ))
  {
    return 0;
  }

  std::vector<int> positions;

  newPos = input.find( delimiter, 0 );

  if( newPos < 0 )
  {
    return 0;
  }

  int numFound = 0;

  while( newPos >= iPos )
  {
    numFound++;
    positions.push_back( newPos );
    iPos = newPos;
    newPos = input.find( delimiter, iPos + sizeS2 );
  }

  if( numFound == 0 )
  {
    return 0;
  }

  for( int i=0; i <= (int)positions.size( ); ++i )
  {
    std::string s("");
    if( i == 0 )
    {
      s = input.substr( i, positions[i] );
    }
    int offset = positions[i-1] + sizeS2;
    if( offset < isize )
    {
      if( i == positions.size( ))
      {
        s = input.substr( offset );
      }
      else if( i > 0 )
      {
        s = input.substr( positions[i-1] + sizeS2, positions[i] - positions[i-1] - sizeS2 );
      }
    }
    if( includeEmpties || ( s.size( ) > 0 ))
    {
      results.push_back( s );
    }
  }
  return numFound;
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
  char hex[128];
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
    char spaces[128];
    spaces[0] = '\0';
    for( int i = strlen(hex); i < 49; i++ )
      strncat( spaces, " ", sizeof( spaces ));
    ascii[j] = '\0';
    Log( "%s %s %s", hex, spaces, ascii );
  }
}

