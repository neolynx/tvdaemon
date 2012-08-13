/*
 *  tvdaemon
 *
 *  JsonObject class
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

#ifndef _JsonObject_
#define _JsonObject_

#include <string>
#include <map>

struct json_object;
class HTTPServer;

class RPCObject
{
  public:
    virtual void json( json_object *j ) const = 0;
    virtual bool RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters ) = 0;
};

#endif
