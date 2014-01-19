/*
 *  tvdaemon
 *
 *  CAMClient class
 *
 *  Copyright (C) 2014 André Roth
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

#include "CAMClient.h"

#include "Log.h"
#include "TVDaemon.h"

extern "C" {
#include "../tsdecrypt/camd.h"
#include "../tsdecrypt/util.h"
#include "../tsdecrypt/tables.h"
#include "../tsdecrypt/process.h"
}

static void log_tsdecrypt( const char *msg )
{
  int len = strlen(msg);
  if( msg[len - 1] == '\n' )
    ((char *) msg )[len - 1] = '\0';
  Log("tsdecrypt: %s", msg);
}

static void notify_card( void *ctx, uint16_t ca_id )
{
  CAMClient *client = (CAMClient *) ctx;
  client->NotifyCard( ca_id );
}

CAMClient::CAMClient( )
{
  ts_set_log_func( log_tsdecrypt );

  ts = (struct ts *) malloc( sizeof( struct ts ));
  // FIXME: check ts
  //
  data_init( ts );
}

CAMClient::CAMClient( std::string &hostname, std::string &service, std::string &username, std::string &password, std::string &key ) :
  hostname(hostname),
  service(service),
  username(username),
  password(password),
  key(key)
{
  CAMClient( );
}

CAMClient::~CAMClient( )
{
  //if( ts->camd.hostname );
    //free( ts->camd.hostname );
  data_free( ts );
  free( ts );
}

bool CAMClient::SaveConfig( ConfigBase &config )
{
  config.WriteConfig( "hostname", hostname );
  config.WriteConfig( "service",  service );
  config.WriteConfig( "username", username );
  config.WriteConfig( "password", password );
  config.WriteConfig( "key",      key );
  return true;
}

bool CAMClient::LoadConfig( ConfigBase &config )
{
  config.ReadConfig( "hostname", hostname );
  config.ReadConfig( "service",  service );
  config.ReadConfig( "username", username );
  config.ReadConfig( "password", password );
  config.ReadConfig( "key",      key );
  return true;
}


bool CAMClient::Connect( )
{
  ts->ecm_cw_log = 0;
  //ts->forced_caid = 0x1702;
  //ts->req_CA_sys = ts_get_CA_sys( ts->forced_caid );
  ts->camd.hostname = (char *) hostname.c_str( );
  ts->camd.service  = (char *) service.c_str( );
  ts->camd.user     = (char *) username.c_str( );
  ts->camd.pass     = (char *) password.c_str( );

  if( key.length( ) < sizeof(ts->camd.newcamd.hex_des_key) - 1 )
  {
    LogError( "key length invalid: %d/%zd", key.length( ), sizeof(ts->camd.newcamd.hex_des_key) - 1 );
    return false;
  }
  strncpy(ts->camd.newcamd.hex_des_key, key.c_str( ), sizeof(ts->camd.newcamd.hex_des_key) - 1);
  ts->camd.newcamd.hex_des_key[sizeof(ts->camd.newcamd.hex_des_key) - 1] = 0;
  if( decode_hex_string( ts->camd.newcamd.hex_des_key, ts->camd.newcamd.bin_des_key, DESKEY_LENGTH ) < 0 )
  {
    LogError( "invalid key" );
    return false;
  }

  camd_proto_newcamd(&ts->camd.ops);
  ts->camd.ctx = this;
  ts->camd.notify_card = notify_card;
  camd_start(ts);

  return true;
}

bool CAMClient::HandleECM( uint8_t *data, ssize_t len )
{
  uint16_t pid = ts_packet_get_pid(data);
  ts->ecm_pid = pid;
  process_ecm(ts, pid, data);
}

bool CAMClient::Decrypt( uint8_t *data, ssize_t len )
{
  decode_packet(ts, data);
  return true;
}

void CAMClient::NotifyCard( uint16_t caid )
{
  Log( "CAMClient::NotifyCard 0x%04x", caid );
  caids.push_back( caid );
}

bool CAMClient::HasCAID( uint16_t caid )
{
  // FIXME: lock
  return std::find( caids.begin( ), caids.end( ), caid ) != caids.end( );
}

