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

class Utils
{
  public:
    static std::string Expand( std::string path );
    static bool IsDir( std::string path );
    static bool IsFile( std::string path );
    static bool MkDir( std::string path );
    static std::string DirName( std::string path );
    static std::string BaseName( std::string path );
    static void EnsureSlash( std::string &dir );

    //static uint32_t decihex( uint32_t value );
    static int StringSplit( const std::string& input, const std::string& delimiter, std::vector<std::string>& results, bool includeEmpties = true );
    static std::string GetExtension( std::string &filename );

    static void dump( const unsigned char *data, int length );
};

#endif
