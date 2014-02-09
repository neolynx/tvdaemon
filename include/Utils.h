/*
 *  tvdaemon
 *
 *  DVB Utils class
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

#ifndef _Utils_
#define _Utils_

#include <string>
#include <stdint.h>
#include <vector>

#define KB(x) (size_t)x*1024
#define MB(x) (size_t)x*1024*1024
#define GB(x) (size_t)x*1024*1024*1024

namespace Utils
{
  std::string Expand( const std::string &path );
  bool IsDir( std::string path );
  bool IsFile( std::string path );
  bool MkDir( std::string path );
  std::string DirName( std::string path );
  std::string BaseName( std::string path );
  void EnsureSlash( std::string &dir );
  bool DiskFree( std::string &dir, size_t &df );

  std::string GetExtension( std::string &filename );

  void dump( const unsigned char *data, int length );
  void ToLower( const std::string &string, std::string &lower );

  int Tokenize( const std::string &string, const char delims[], std::vector<std::string> &tokens, int count = 0 );
};

class Name : public std::string
{
  public:
    Name( );
    Name( const char *str );
    Name( const std::string &str );
    Name( std::string &str );
    Name( Name &other );
    virtual ~Name( );

    virtual size_t find( const char *s, size_t pos, size_t n ) const;
    virtual Name &operator=( const std::string &other );
    virtual Name &operator=( const Name &other );
    virtual Name &operator=( const char *str );

    const std::string &GetLower( ) const { return lower; }

  protected:
    std::string lower;

  friend bool operator<( const Name &a, const Name &b );
};

#endif
