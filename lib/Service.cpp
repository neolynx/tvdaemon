/*
 *  transpondereadend
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

#include "Service.h"

#include <algorithm> // find

#include "Transponder.h"
#include "Stream.h"
#include "Log.h"

Service::Service( Transponder &transponder, uint16_t service_id, uint16_t pid, int config_id ) :
  ConfigObject( transponder, "service", config_id ),
  transponder(transponder),
  service_id(service_id),
  pid(pid),
  encrypted(false)
{
}

Service::Service( Transponder &transponder, std::string configfile ) :
  ConfigObject( transponder, configfile ),
  transponder(transponder)
{
}

Service::~Service( )
{
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
  {
    delete it->second;
  }
}

bool Service::SaveConfig( )
{
  Lookup( "ServiceID",    Setting::TypeInt )    = service_id;
  Lookup( "PID",          Setting::TypeInt )    = pid;
  Lookup( "Type",         Setting::TypeInt )    = (int) type;
  Lookup( "Name",         Setting::TypeString ) = name;
  Lookup( "Provider",     Setting::TypeString ) = provider;
  Lookup( "Encrypted",    Setting::TypeInt )    = (int) encrypted;

  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
  {
    it->second->SaveConfig( );
  }

  return WriteConfig( );
}

bool Service::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;

  service_id = (int) Lookup( "ServiceID", Setting::TypeInt );
  pid        = (int) Lookup( "PID", Setting::TypeInt );
  type       = (Type) (int) Lookup( "Type", Setting::TypeInt );
  const char *t = Lookup( "Name", Setting::TypeString );
  if( t ) name = t;
  t =             Lookup( "Provider", Setting::TypeString );
  if( t ) provider = t;
  encrypted    = (int) Lookup( "Encrypted", Setting::TypeInt );

  if( !CreateFromConfig<Stream, uint16_t, Service>( *this, "stream", streams ))
    return false;

  return true;
}

bool Service::UpdateStream( int id, Stream::Type type )
{
  std::map<uint16_t, Stream *>::iterator it = streams.find( id );
  Stream *s;
  if( it == streams.end( ))
  {
    s = new Stream( *this, id, type, streams.size( ));
    streams[id] = s;
    return true;
  }

  //LogWarn( "Already known Stream %d", id );
  s = it->second;
  return s->Update( type );
}

std::map<uint16_t, Stream *> &Service::GetStreams() // FIXME: const
{
  return streams;
}

bool Service::Tune( )
{
  return transponder.Tune( service_id );
}

