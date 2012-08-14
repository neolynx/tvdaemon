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
#include "RPCObject.h"
#include "Service.h"

#include <map>

#include "dvb-frontend.h"

class Source;
struct PAT;

class Transponder : public ConfigObject, public RPCObject
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

    virtual std::string toString( ) const = 0;

    fe_delivery_system_t GetDelSys( ) const { return delsys; }
    uint32_t GetFrequency( ) const { return frequency; }

    bool UpdateProgram( uint16_t service_id, uint16_t pid );
    bool UpdateService( uint16_t service_id, Service::Type type, std::string name, std::string provider, bool scrambled );
    bool UpdateStream( uint16_t pid, int id, int type );

    Service *CreateService( std::string  name );
    //uint16_t GetTransportStreamID( ) { return TransportStreamID; }
    //uint16_t GetVersionNumber( ) {  return VersionNumber; }

    const std::map<uint16_t, Service *> &GetServices( ) const { return services; };
    Service *GetService( uint16_t id );
    Source &GetSource( ) const { return source; }

    void SetSignal( uint8_t signal, uint8_t noise ) { this->signal = signal; this->noise = noise; }
    void SetTSID( uint16_t TSID ) { this->TSID = TSID; }
    uint16_t GetTSID( ) { return TSID; }

    void Disable( ) { enabled = false; }
    bool Enabled( ) { return enabled; }
    bool Disabled( ) { return !enabled; }

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

    // RPC
    void json( json_object *entry ) const;
    bool RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters );

  protected:
    bool enabled;
    State state;
    uint8_t signal;
    uint8_t noise;
    Source &source;

    fe_delivery_system_t delsys;
    uint32_t frequency;

    std::map<uint16_t, Service *> services;

    uint16_t TSID;
    //uint16_t VersionNumber;
};

#endif
