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

#ifndef _RPCObject_
#define _RPCObject_

#include <string>
#include <map>
#include <json-c/json.h>

class HTTPRequest;

class JSONObject
{
  public:
    virtual void json( json_object *j ) const = 0;

    virtual bool compare( const JSONObject &other, const int &p ) const = 0;
};

void json_object_time_add( json_object *j, std::string name, time_t tt );
void json_object_object_add( json_object *j, int key, json_object *k );

class RPCHandler
{
  public:
    virtual bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action ) = 0;
};

class RPCObject : public JSONObject, public RPCHandler
{
};

class JSONObjectComparator : public std::binary_function<JSONObject, JSONObject, bool>
{
  public:
    JSONObjectComparator( const int &p ) : p(p) { }

    inline bool operator()( const JSONObject *a, const JSONObject *b )
    {
      return a->compare( *b, p );
    }

  private:
    const int &p;
};

#endif
