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

#define DMX_BUFSIZE 2 * 1024 * 1024

class Frontend : public ConfigObject, public RPCObject, public ThreadBase
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

    static bool GetInfo( int adapter_id, int frontend_id, fe_delivery_system_t *delsys, std::string *name = NULL );
    virtual bool SetPort( int port_id );
    virtual bool Tune( Transponder &transponder, int timeout = 1000 );
    virtual void Untune();
    virtual bool GetLockStatus( uint8_t &signal, uint8_t &noise, int retries );
    int OpenDemux( );
    bool Open( );
    void Close( );

    bool Scan( Transponder &transponder );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    enum State
    {
      New,
      Opened,
      Closed,
      Closing,
      Tuning,
      Tuned,
      TuningFailed,
      Scanning,
      Shutdown,
    };

    bool TunePID( Transponder &t, uint16_t pno );

    // RPC
    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );

  protected:
    Frontend( Adapter &adapter, int adapter_id, int frontend_id, int config_id );
    Frontend( Adapter &adapter, std::string configfile );

    bool present;
    bool enabled;
    State state;
    int adapter_id;
    int frontend_id;
    Adapter &adapter;

    struct dvb_v5_fe_parms *fe;

    int config_id;
    int current_port;

    std::vector<Port *> ports;

    Transponder *transponder; // current tuned transponder

    Source::Type type;

    bool HandlePAT( struct section *section );
    virtual bool HandleNIT( struct dvb_table_nit *nit ) = 0;
    bool HandleSDT( struct section *section );
    bool HandlePMT( struct section *section, uint16_t pid );

    void Log( const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));
    void LogWarn( const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));
    void LogError( const char *fmt, ... ) __attribute__ (( format( printf, 2, 3 )));

    uint8_t filter[18];
    uint8_t mask[18];

    bool up;

  private:
    void Init( );

    std::map<uint16_t, uint16_t> pid_map;
    std::deque<uint16_t> pno_list;
    uint16_t curPid;

    Thread *thread_idle;
    void Thread_idle( );
};

#endif
