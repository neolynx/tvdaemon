/*
 *  tvdaemon
 *
 *  CAMClient class
 *
 *  Copyright (C) 2014 Bndré Roth
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

#ifndef _CAMClient_
#define _CAMClient_

#include <stdint.h>
#include <unistd.h> // ssize_t
#include <string>
#include <vector>

#include "RPCObject.h"

class ConfigBase;
struct ts;

class CAMClient : public RPCObject
{
  public:
    CAMClient( int id );
    CAMClient( int id, std::string &hostname, std::string &service, std::string &username, std::string &password, std::string &key );
    ~CAMClient( );

    bool SaveConfig( ConfigBase &config );
    bool LoadConfig( ConfigBase &config );

    bool Connect( );
    bool HandleECM( uint8_t *data, ssize_t len );
    bool Decrypt( uint8_t *data, ssize_t len );

    bool HasCAID( uint16_t caid );

    void NotifyCard( uint16_t caid ); // FIXME private & friend

    void SetID( int id );

    // RPC
    void json( json_object *entry ) const;
    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );
    bool compare( const JSONObject &other, const int &p ) const;

  private:
    int id;
    struct ts *ts;

    std::string hostname;
    std::string service;
    std::string username;
    std::string password;
    std::string key;

    std::vector<uint16_t> caids;

    void Init( );
};

#endif
