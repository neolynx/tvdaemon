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
#include "JsonObject.h"
#include "TVDaemon.h"
#include "Transponder.h"

#include <string>
#include <vector>

class Port;

class Source : public ConfigObject, public JsonObject
{
  public:
    Source( TVDaemon &tvd, std::string name, int config_id );
    Source( TVDaemon &tvd, std::string configfile );
    virtual ~Source( );

    std::string GetName( ) { return name; }

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );
    virtual void json( JAULA::Value_Object &entry ) const;

    bool ReadScanfile( std::string scanfile );

    Transponder *CreateTransponder( const struct dvb_entry &info );
    Transponder *GetTransponder( int id );
    bool TuneTransponder( int id );
    bool ScanTransponder( int id );
    uint GetTransponderCount() { return transponders.size(); }
    TVDaemon::SourceType GetType( ) const { return type; }

    bool AddPort( Port *port );
    bool AddTransponder( Transponder *t );

    bool Tune( Transponder &transponder, uint16_t pno );

  private:
    TVDaemon &tvd;
    std::string name;

    static int ScanfileLoadCallback( struct dvbcfg_scanfile *channel, void *private_data );

    TVDaemon::SourceType type;

    std::vector<Transponder *> transponders;
    std::vector<Port *> ports;
};

#endif
