/*
 *  tvdaemon
 *
 *  ConfigObject class
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

#include "ConfigObject.h"

#include <string>
#include <stdlib.h>

#include "Utils.h"
#include "Log.h"

ConfigObject::ConfigObject( ) : parent(NULL)
{
}

ConfigObject::ConfigObject( ConfigObject &parent, std::string configname, int config_id ) :
  configname(configname),
  config_id(config_id),
  parent(&parent)
{
  if( config_id < 0 )
    LogWarn( "negative config id not supported: %s%d/config", configname.c_str( ), config_id );
  char buf[128];
  snprintf( buf, sizeof( buf ), "%s%d/config", configname.c_str( ), config_id );
  SetConfigFile( parent.GetConfigDir( ) + buf );
}

ConfigObject::ConfigObject( ConfigObject &parent, std::string configfile ) :
  parent(&parent)
{
  // parse config_id out
  size_t pos = configfile.rfind( '/' );
  if( pos == std::string::npos )
  {
    LogWarn( "no '/' found in configfile '%s'", configfile.c_str( ));
    return;
  }
  const char *t = configfile.c_str( );
  pos--;
  while( pos > 1 && t[pos-1] <= '9' && t[pos-1] >= '0' )
    pos--;
  config_id = atoi( t + pos );
  SetConfigFile( configfile );
}

ConfigObject::~ConfigObject( )
{
}

bool ConfigObject::SetConfigFile( std::string configfile )
{
  configdir = Utils::DirName( configfile );
  Utils::EnsureSlash( configdir );
  if( !Utils::IsDir( configdir ))
  {
    if( !Utils::MkDir( configdir ))
    {
      LogError( "Cannot create configdir '%s'", configdir.c_str( ));
      return false;
    }
  }
  this->configfile = configfile;
  return true;
}

bool ConfigObject::WriteConfig( )
{
  config.writeFile( configfile.c_str( ));
  return true;
}

bool ConfigObject::ReadConfig( )
{
  if( !Utils::IsFile( configfile ))
  {
    LogError( "not a config file '%s'", configfile.c_str( ));
    return false;
  }
  try
  {
    config.readFile( configfile.c_str( ));
  }
  catch( libconfig::ParseException &e )
  {
    LogError( "Error reading config file: %s", configfile.c_str( ));
    return false;
  }
  return true;
}

Setting &ConfigObject::Lookup( const char *key, Setting::Type type )
{
  if( config.exists( key ))
    return config.lookup( key );
  Setting &root = GetSettings( );
  return root.add( key, type );
}

bool ConfigObject::DeleteConfig( const char *key )
{
  if( !config.exists( key ))
    return false;
  Setting &root = GetSettings( );
  root.remove( key );
  return true;
}

std::list<int> ConfigObject::GetParentPath( )
{
  std::list<int> path;
  ConfigObject *c = this;
  while( c && c->parent )
  {
    path.push_front( c->GetKey( ));
    c = c->parent;
  }
  return path;
}

