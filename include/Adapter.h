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
#include <vector>

class TVDaemon;
class Frontend;
class HTTPServer;

class Adapter : public ConfigObject, public RPCObject
{
  public:
    Adapter( TVDaemon &tvd, const std::string &name, const std::string &uid, const int config_id );
    Adapter( TVDaemon &tvd, const std::string &configfile );
    virtual ~Adapter( );
    void Abort( );
    void Shutdown( );
    void Delete( );

    void SetFrontend( const std::string &name, const int adapter_id, const int frontend_id );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    std::string GetName( ) const { return name; }
    std::string GetUID( ) const { return uid; }
    void SetUID( const std::string &uid ) { this->uid = uid; }
    int GetFrontendCount( ) { return frontends.size( ); }
    Frontend *GetFrontend ( const int id ) const;
    void ResetPresence( );
    bool IsPresent( ) const;

    bool HasFrontend( int adapter_id, int frontend_id );

    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );
    virtual bool compare( const JSONObject &other, const int &p ) const;

  private:
    TVDaemon &tvd;
    std::string uid;

    std::vector<Frontend *> frontends;
    std::string name;
};

#endif
