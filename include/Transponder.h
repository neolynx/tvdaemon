/*
 *  tvdaemon
 *
 *  DVB Transponder class
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

#ifndef _Transponder_
#define _Transponder_

#include "ConfigObject.h"

#include <map>

#include "dvb-frontend.h"

class Source;
class Service;
struct PAT;

class Transponder : public ConfigObject
{
  public:
    Transponder( Source &src, const fe_delivery_system_t delsys, int config_id );
    Transponder( Source &src, std::string configfile );
    virtual ~Transponder( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    static Transponder *Create( Source &source, const fe_delivery_system_t delsys, int config_id );
    static Transponder *Create( Source &source, const struct dvb_desc &info, int config_id );
    static Transponder *Create( Source &source, std::string configfile );

    static bool IsSame( const Transponder &transponder, const struct dvb_entry &info );
    virtual bool IsSame( const Transponder &transponder ) = 0;

    virtual void AddProperty( const struct dtv_property &prop );

    virtual bool GetParams( struct dvb_v5_fe_parms *params ) const;

    virtual std::string toString( ) = 0;

    fe_delivery_system_t GetDelSys( ) const { return delsys; }
    uint32_t GetFrequency( ) const { return frequency; }

    bool UpdateProgram( uint16_t pno, uint16_t pid );
    bool UpdateProgram( uint16_t pno, std::string name, std::string provider );
    bool UpdateStream( uint16_t pid, int id, int type );

    Service *CreateService( std::string  name );
    uint16_t GetTransportStreamID( ) { return TransportStreamID; }
    uint16_t GetVersionNumber( ) {  return VersionNumber; }

    Service *GetService( uint16_t id );
    Source &GetSource( ) const { return source; }

    enum State
    {
      New,
      Scanning,
      Scanned,
      Tuning,
      Tuned,
      Idle
    };

    bool Tune( uint16_t pno );

  protected:
    bool enabled;
    State state;
    Source &source;

    fe_delivery_system_t delsys;
    uint32_t frequency;

    std::map<uint16_t, Service *> services;

    uint16_t TransportStreamID;
    uint16_t VersionNumber;
};

#endif
