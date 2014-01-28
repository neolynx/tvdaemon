/*
 *  tvdaemon
 *
 *  DVB Service class
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

#ifndef _Service_
#define _Service_

#include "RPCObject.h"

#include <string>
#include <set>
#include <utility>
#include <stdint.h>

#include "Stream.h"
#include "Utils.h" // Name

class Transponder;
class Channel;
class Activity;
class CAMClient;

class Service : public RPCObject
{
  public:
    Service( Transponder &transponder, uint16_t service_id, uint16_t pid, int config_id );
    Service( Transponder &transponder );
    virtual ~Service( );

    enum Type
    {
      Type_Unknown,
      Type_TV,
      Type_TVHD,
      Type_Radio,
      Type_Last
    };
    static const char *GetTypeName( Type type );

    virtual int GetKey( ) const { return service_id; }
    uint16_t GetPID( ) { return pid; }
    void     SetPID( uint16_t pid ) { this->pid = pid; }
    void     SetType( Type type ) { this->type = type; }
    const Name &GetName( ) { return name; }
    void        SetName( std::string &name );
    std::string GetProvider( ) { return provider; }
    void        SetProvider( std::string provider ) { this->provider = provider; }
    bool        GetScrambled( ) const { return !caids.empty(); }
    Channel    *GetChannel( ) { return channel; }

    void SetCA( uint16_t ca_id, uint16_t ca_pid );
    bool GetECMPID( uint16_t &ca_pid, CAMClient **client );

    Transponder &GetTransponder( ) const { return transponder; }


    bool UpdateStream( int pid, Stream::Type type );
    std::map<uint16_t, Stream *> &GetStreams();

    bool SaveConfig( ConfigBase &config );
    bool LoadConfig( ConfigBase &config );

    int Open( Frontend &frontend, int pid );

    // RPC
    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );
    virtual bool compare( const JSONObject &other, const int &p ) const;

    bool Tune( Activity &act );

    bool ReadEPG( const struct dvb_table_eit_event *event );

  private:
    Transponder &transponder;
    uint16_t service_id;
    uint16_t pid;
    Type type;
    Name name;
    std::string provider;
    Channel *channel;

    std::map<uint16_t, Stream *> streams;

    std::set<std::pair<uint16_t, uint16_t> > caids;
};

#endif
