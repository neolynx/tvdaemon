/*
 *  tvdaemon
 *
 *  DVB Source class
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

#ifndef _Source_
#define _Source_

#include "ConfigObject.h"
#include "RPCObject.h"
#include "Thread.h"
#include "Transponder.h"

#include <string>
#include <vector>

class Port;
class HTTPServer;
class TVDaemon;
class Recording;

class Source : public ConfigObject, public RPCObject, public ThreadBase
{
  public:
    enum Type
    {
      Type_Any    = 0xff,
      Type_DVBS  = 0,
      Type_DVBC,
      Type_DVBT,
      Type_ATSC,
      Type_Last,
    };

    Source( TVDaemon &tvd, std::string name, Type type, int config_id );
    Source( TVDaemon &tvd, std::string configfile );
    virtual ~Source( );

    std::string GetName( ) { return name; }

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    static std::vector<std::string> GetScanfileList( Type type = Type_Any, std::string country = "" );
    bool ReadScanfile( std::string scanfile );

    Transponder *CreateTransponder( const struct dvb_entry &info );
    Transponder *GetTransponder( int id );
    bool TuneTransponder( int id );
    bool ScanTransponder( int id );
    uint GetTransponderCount() { return transponders.size(); }
    Type GetType( ) const { return type; }

    Transponder *GetTransponderForScanning( );

    int CountServices( ) const;

    const std::vector<Transponder *> &GetTransponders( ) { return transponders; }

    bool AddPort( Port *port );
    bool RemovePort( Port *port );
    bool AddTransponder( Transponder *t );

    // RPC
    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );

    bool Record( Recording &rec );

  private:
    TVDaemon &tvd;
    std::string name;

    static int ScanfileLoadCallback( struct dvbcfg_scanfile *channel, void *private_data );

    Type type;

    std::vector<Transponder *> transponders;
    std::vector<Port *> ports;
};

#endif
