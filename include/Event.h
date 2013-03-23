/*
 *  tvdaemon
 *
 *  Event class
 *
 *  Copyright (C) 2013 André Roth
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

#ifndef _Event_
#define _Event_

#include "RPCObject.h"

#include <time.h>
#include <stdint.h>
#include <string>

struct dvb_table_eit_event;
class ConfigBase;
class Channel;

class Event : public RPCObject
{
  public:
    Event( Channel &channel, const struct dvb_table_eit_event *event = NULL );
    ~Event( );

    bool SaveConfig( ConfigBase &config );
    bool LoadConfig( ConfigBase &config );

    int GetID( )                  { return id; }
    std::string GetName( )        { return name; }
    std::string GetDescription( ) { return description; }
    Channel &GetChannel( )        { return channel; }
    time_t GetStart( )            { return start; }
    time_t GetEnd( )              { return start + duration; }
    uint32_t GetDuration( )       { return duration; }

    // RPC
    virtual void json( json_object *j ) const;
    virtual bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action ) { return false; }
    virtual bool compare( const JSONObject &other, const int &p ) const;

  private:
    Channel &channel;
    int id;
    time_t start;
    uint32_t duration;
    std::string name;
    std::string description;
    std::string language;

  friend bool operator<( const Event &a, const Event &b );
};


#endif
