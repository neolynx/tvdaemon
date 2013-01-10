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
#include <map>
#include <stdint.h>

#include "Stream.h"

class Transponder;
class Channel;

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
    void        SetType( Type type ) { this->type = type; }
    std::string GetName( bool lower = false ) { if( lower ) return name_lower; return name; }
    void        SetName( std::string &name );
    std::string GetProvider( ) { return provider; }
    void        SetProvider( std::string provider ) { this->provider = provider; }
    void        SetScrambled( bool scrambled ) { this->scrambled = scrambled; }

    Transponder &GetTransponder( ) const { return transponder; }

    static bool SortByName( const Service *a, const Service *b );

    bool UpdateStream( int pid, Stream::Type type );
    std::map<uint16_t, Stream *> &GetStreams();

    virtual bool SaveConfig( ConfigBase &config );
    virtual bool LoadConfig( ConfigBase &config );

    bool Tune( );

    // RPC
    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );

    static bool SortTypeName( Service *s1, Service *s2 );

  private:
    Transponder &transponder;
    uint16_t service_id;
    uint16_t pid;
    Type type;
    std::string name;
    std::string name_lower;
    std::string provider;
    bool scrambled;
    Channel *channel;

    std::map<uint16_t, Stream *> streams;
};

#endif
