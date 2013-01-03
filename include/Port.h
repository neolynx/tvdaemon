/*
 *  tvdaemon
 *
 *  DVB Port class
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

#ifndef _Port_
#define _Port_

#include "ConfigObject.h"
#include "RPCObject.h"

#include <stdint.h>

class Frontend;
class Transponder;

class Port : public ConfigObject, public RPCObject
{
  public:
    Port( Frontend &frontend, int config_id, std::string name = "Default Port", int id = 0 );
    Port( Frontend &frontend, std::string configfile );
    virtual ~Port( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    int GetPortNum( ) { return port_num; }
    void SetSource( int id ) { source_id = id; }

    bool Tune( Transponder &transponder );
    bool Scan( Transponder &transponder );

    bool Tune( Transponder &transponder, uint16_t pno );
    void Untune( );

    // RPC
    void json( json_object *entry ) const;
    bool RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters );

  private:
    Frontend &frontend;
    std::string name;
    int port_num;
    int source_id;

};

#endif
