/*
 *  tvdaemon
 *
 *  DVB Channel class
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

#ifndef _Channel_
#define _Channel_

#include "ConfigObject.h"
#include "RPCObject.h"

#include <string>
#include <vector>

class TVDaemon;
class Service;
class Recording;

class Channel : public ConfigObject, public RPCObject
{
  public:
    Channel( TVDaemon &tvd, Service *service, int config_id );
    Channel( TVDaemon &tvd, std::string configfile );
    virtual ~Channel( );

    std::string GetName( ) { return name; }

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    bool AddService( Service *service );
    bool HasService( Service *service ) const;

    const std::vector<Service *> &GetServices( ) const { return services; };

    // RPC
    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );

    bool Record( Recording &rec );

  private:
    TVDaemon &tvd;
    std::string name;
    int number;

    std::vector<Service *> services;
};

#endif
