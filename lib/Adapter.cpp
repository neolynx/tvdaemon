/*
 *  tvdaemon
 *
 *  DVB Adapter class
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

#include "Adapter.h"

#include "TVDaemon.h"
#include "Frontend.h"
#include "Utils.h"
#include "Log.h"

#include <algorithm> // find

Adapter::Adapter( TVDaemon &tvd, std::string uid, std::string name, int config_id ) :
  ConfigObject( tvd, "adapter", config_id ),
  tvd(tvd),
  uid(uid),
  name(name),
  present(false)
{
  Log( "Creating new Adapter: %s", uid.c_str( ));
}

Adapter::Adapter( TVDaemon &tvd, std::string configfile ) :
  ConfigObject( tvd, configfile ),
  tvd(tvd),
  present(false)
{
}

Adapter::~Adapter( )
{
  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
  {
    delete *it;
  }
}

bool Adapter::SaveConfig( )
{
  Lookup( "Name", Setting::TypeString )    = name;
  Lookup( "UDev-ID", Setting::TypeString ) = uid;

  WriteConfig( );

  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool Adapter::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;
  name = (const char *) Lookup( "Name", Setting::TypeString );
  uid  = (const char *) Lookup( "UDev-ID", Setting::TypeString );

  Log( "Found configured Adapter '%s'", name.c_str( ));

  if( !CreateFromConfigFactory<Frontend, Adapter>( *this, "frontend", frontends ))
    return false;
  return true;
}

void Adapter::SetFrontend( std::string frontend, int adapter_id, int frontend_id )
{
  if( std::find( ftempnames.begin( ), ftempnames.end( ), frontend ) != ftempnames.end( ))
  {
    LogError( "Frontend already set" );
    return;
  }
  int id = ftempnames.size( );
  ftempnames.push_back( frontend );

  Frontend *f = NULL;
  if( id >= frontends.size( ))
  {
    // create frontend
    f = Frontend::Create( *this, adapter_id, frontend_id, frontends.size( ));
    if( f )
        frontends.push_back( f );
  }
  else
  {
    f = frontends[id];
    f->SetIDs( adapter_id, frontend_id );
  }

  if( f )
    f->SetPresence( true );
}

void Adapter::SetPresence( bool present )
{
  if( this->present && present )
    return; // already present

  if( this->present && !present ) // removed
  {
    ftempnames.clear( );
  }

  if( present )
    Log( "Adapter hardware is present '%s'", name.c_str( ));
  else
    Log( "Adapter hardware removed '%s'", name.c_str( ));

  this->present = present;
}

Frontend *Adapter::GetFrontend( int id )
{
  if( id >= frontends.size( ))
    return NULL;
  return frontends[id];
}

