/*
 *  tvdaemon
 *
 *  DVB Frontend class
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

#ifndef _Frontend_
#define _Frontend_

#include "ConfigObject.h"
#include "RPCObject.h"
#include "Source.h"
#include "Thread.h"

#include <string>
#include <vector>
#include <deque>

#include "dvb-frontend.h"

class Adapter;
class Transponder;
class Port;
class Activity;

#define DMX_BUFSIZE 2 * 1024 * 1024

class Frontend : public ConfigObject, public RPCObject, public Thread
{
  public:
    static Frontend *Create( Adapter &adapter, int adapter_id, int frontend_id, int config_id );
    static Frontend *Create( Adapter &adapter, std::string configfile );
    virtual ~Frontend( );

    Port *AddPort( std::string name, int port_num );

    void SetPresence( bool present ) { this->present = present; }
    bool IsPresent( ) { return present; }

    void SetIDs( int adapter_id, int frontend_id );

    Port *GetPort( int id ); // FIXME: used ?
    Port *GetCurrentPort( );

    struct dvb_v5_fe_parms *GetFE( ) { return fe; }
    static bool GetInfo( int adapter_id, int frontend_id, fe_delivery_system_t *delsys, std::string *name = NULL );
    virtual bool GetLockStatus( uint8_t &signal, uint8_t &noise, int retries );
    int OpenDemux( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    enum State
    {
      State_New,
      State_Ready,
      State_Opened,
      State_Tuning,
      State_Last
    };

    // RPC
    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );

    bool Tune( Port &port, Activity &act );
    void Release( );

    virtual bool HandleNIT( struct dvb_table_nit *nit ) = 0;

  protected:
    Frontend( Adapter &adapter, int adapter_id, int frontend_id, int config_id );
    Frontend( Adapter &adapter, std::string configfile );

    bool present;
    bool enabled;
    int adapter_id;
    int frontend_id;
    Adapter &adapter;

    void SetState( State state ) { this->state = state; }

    virtual bool SetTuneParams( Transponder &transponder );

    struct dvb_v5_fe_parms *fe;

    int config_id;
    int current_port;

    std::vector<Port *> ports;

    Transponder *transponder; // current tuned transponder

    Source::Type type;

    void Log( const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));
    void LogWarn( const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));
    void LogError( const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));

    uint8_t filter[18];
    uint8_t mask[18];

    bool up;

  private:
    bool SetPort( int port_id );

    State state;
    int usecount;

    std::map<uint16_t, uint16_t> pid_map;
    std::deque<uint16_t> pno_list;
    uint16_t curPid;

    Thread *idle_thread;
    virtual void Run( );

    bool Open( );
    bool Tune( Transponder &transponder, int timeout = 1000 );
    void Close( );

    Activity *activity;
};

#endif
