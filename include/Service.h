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

#include "ConfigObject.h"

#include <string>
#include <map>
#include <stdint.h>

#include "Stream.h"

class Transponder;

class Service : public ConfigObject
{
  public:
    Service( Transponder &transponder, uint16_t pno, uint16_t pid, int config_id );
    Service( Transponder &transponder, std::string configfile );
    virtual ~Service( );

    virtual int GetKey( ) const { return pno; }
    uint16_t GetPID( ) { return pid; }
    void     SetPID( uint16_t pid ) { this->pid = pid; }
    std::string GetName( ) { return name; }
    void        SetName( std::string name ) { this->name = name; }
    std::string GetProvider( ) { return provider; }
    void        SetProvider( std::string provider ) { this->provider = provider; }

    bool UpdateStream( int pid, Stream::Type type );
    std::map<uint16_t, Stream *> GetStreams();

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    bool Tune( );

  private:
    Transponder &transponder;
    uint16_t pno;
    uint16_t pid;
    std::string name;
    std::string provider;

    std::map<uint16_t, Stream *> streams;
};

#endif
