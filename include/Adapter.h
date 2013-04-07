/*
 *  tvdaemon
 *
 *  DVB Adapter class
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

#ifndef _Adapter_
#define _Adapter_

#include "ConfigObject.h"
#include "RPCObject.h"

#include <string>
#include <map>

class TVDaemon;
class Frontend;
class HTTPServer;

class Adapter : public ConfigObject, public RPCObject
{
  public:
    Adapter( TVDaemon &tvd, std::string uid, std::string name, int config_id );
    Adapter( TVDaemon &tvd, std::string uid, int config_id );
    Adapter( TVDaemon &tvd, std::string configfile );
    virtual ~Adapter( );
    void Shutdown( );

    void SetFrontend( std::string frontend, int adapter_id, int frontend_id );

    void FindConfig( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    std::string GetName( ) { return name; }
    std::string GetUID( ) { return uid; }
    int GetFrontendCount( ) { return frontends.size( ); }
    Frontend *GetFrontend( int id );
    void SetPresence( bool present );
    bool IsPresent( ) const { return present; }

    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );
    virtual bool compare( const JSONObject &other, const int &p ) const;

  private:
    TVDaemon &tvd;
    std::string uid;
    bool present;

    std::vector<Frontend *> frontends;
    std::vector<std::string> ftempnames;
    std::string name;
    std::string configdir;
};

#endif
