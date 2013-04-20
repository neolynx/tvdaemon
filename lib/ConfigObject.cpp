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

Setting &ConfigBase::ConfigArray( const char *key )
{
  if( !key )
    return settings->add( Setting::TypeArray );
  if( settings->exists( key ))
  {
    return (*settings)[key];
  }
  return settings->add( key, Setting::TypeArray );
}

Setting &ConfigBase::ConfigGroup( const char *key )
{
  if( !key )
    return settings->add( Setting::TypeGroup );
  if( settings->exists( key ))
  {
    return (*settings)[key];
  }
  return settings->add( key, Setting::TypeGroup );
}

Setting &ConfigBase::ConfigList( const char *key )
{
  if( !key )
    return settings->add( Setting::TypeList );
  if( settings->exists( key ))
  {
    return (*settings)[key];
  }
  return settings->add( key, Setting::TypeList );
}


void ConfigBase::ReadConfig( const char *key, int &i )
{
  if( settings->exists( key ))
    settings->lookupValue( key, i );
  else
  {
    settings->add( key, Setting::TypeInt );
    i = 0;
  }
}

void ConfigBase::ReadConfig( const char *key, uint8_t &u8 )
{
  int i = 0;
  if( settings->exists( key ))
    settings->lookupValue( key, i );
  else
    settings->add( key, Setting::TypeInt );
  u8 = i;
}

void ConfigBase::ReadConfig( const char *key, uint16_t &u16 )
{
  int i = 0;
  if( settings->exists( key ))
    settings->lookupValue( key, i );
  else
    settings->add( key, Setting::TypeInt );
  u16 = i;
}

void ConfigBase::ReadConfig( const char *key, uint32_t &u32 )
{
  if( settings->exists( key ))
    settings->lookupValue( key, u32 );
  else
  {
    settings->add( key, Setting::TypeInt );
    u32 = 0;
  }
}

void ConfigBase::ReadConfig( const char *key, time_t &t )
{
  std::string tstr;
  if( settings->exists( key ))
    settings->lookupValue( key, tstr );
  else
  {
    settings->add( key, Setting::TypeString );
    t = 0;
    return;
  }

  struct tm tm;
  strptime( tstr.c_str( ), "%Y-%m-%d %H:%M:%S", &tm );
  t = mktime( &tm );
}

void ConfigBase::ReadConfig( const char *key, bool &b )
{
  int i = 0;
  if( settings->exists( key ))
    settings->lookupValue( key, i );
  else
    settings->add( key, Setting::TypeInt );
  b = i != 0;
}

void ConfigBase::ReadConfig( const char *key, float &f )
{
  if( settings->exists( key ))
    settings->lookupValue( key, f );
  else
    f = settings->add( key, Setting::TypeFloat );
}

void ConfigBase::ReadConfig( const char *key, std::string &s )
{
  if( settings->exists( key ))
    settings->lookupValue( key, s );
  else
  {
    settings->add( key, Setting::TypeString );
    s = "";
  }
}

void ConfigBase::ReadConfig( const char *key, Name &n )
{
  if( settings->exists( key ))
  {
    std::string t;
    settings->lookupValue( key, t );
    n = t;
  }
  else
  {
    settings->add( key, Setting::TypeString );
    n = "";
  }
}

void ConfigBase::WriteConfig( const char *key, int i )
{
  if( settings->exists( key ))
    (*settings)[key] = i;
  else
    settings->add( key, Setting::TypeInt ) = i;
}

void ConfigBase::WriteConfig( const char *key, uint8_t u8 )
{
  if( settings->exists( key ))
    (*settings)[key] = u8;
  else
    settings->add( key, Setting::TypeInt ) = u8;
}

void ConfigBase::WriteConfig( const char *key, uint16_t u16 )
{
  if( settings->exists( key ))
    (*settings)[key] = u16;
  else
    settings->add( key, Setting::TypeInt ) = u16;
}

void ConfigBase::WriteConfig( const char *key, uint32_t u32 )
{
  if( settings->exists( key ))
    (*settings)[key] = (int) u32;
  else
    settings->add( key, Setting::TypeInt ) = (int) u32;
}

void ConfigBase::WriteConfig( const char *key, time_t t )
{
  struct tm tm;
  char tstr[255];
  localtime_r( &t, &tm );
  strftime( tstr, sizeof( tstr ), "%Y-%m-%d %H:%M:%S", &tm );
  if( settings->exists( key ))
  {
    if( (*settings)[key].getType( ) != Setting::TypeString )
    {
      DeleteConfig( key );
      settings->add( key, Setting::TypeString ) = tstr;
    }
    (*settings)[key] = tstr;
  }
  else
    settings->add( key, Setting::TypeString ) = tstr;
}

void ConfigBase::WriteConfig( const char *key, bool b )
{
  if( settings->exists( key ))
    (*settings)[key] = b ? 1 : 0;
  else
    settings->add( key, Setting::TypeInt ) = b ? 1 : 0;
}

void ConfigBase::WriteConfig( const char *key, float f )
{
  if( settings->exists( key ))
    (*settings)[key] = f;
  else
    settings->add( key, Setting::TypeFloat ) = f;
}

void ConfigBase::WriteConfig( const char *key, std::string &s )
{
  if( settings->exists( key ))
    (*settings)[key] = s;
  else
    settings->add( key, Setting::TypeString ) = s;
}

bool ConfigBase::DeleteConfig( const char *key )
{
  if( !settings->exists( key ))
    return false;
  settings->remove( key );
  return true;
}


ConfigObject::ConfigObject( ) : ConfigBase( ), parent(NULL), modified(false)
{
  settings = &config.getRoot( );
}

ConfigObject::ConfigObject( ConfigObject &parent, std::string configname, int config_id ) :
  ConfigBase( ),
  configname(configname),
  config_id(config_id),
  parent(&parent),
  modified(false)
{
  if( config_id < 0 )
    LogWarn( "negative config id not supported: %s%d/config", configname.c_str( ), config_id );
  char buf[128];
  snprintf( buf, sizeof( buf ), "%s%d/config", configname.c_str( ), config_id );
  SetConfigFile( parent.GetConfigDir( ) + buf );
  settings = &config.getRoot( );
}

ConfigObject::ConfigObject( ConfigObject &parent, std::string configfile ) :
  ConfigBase( ),
  parent(&parent),
  modified(false)
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
  settings = &config.getRoot( );
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

bool ConfigObject::WriteConfigFile( )
{
  config.writeFile( configfile.c_str( ));
  return true;
}

bool ConfigObject::ReadConfigFile( )
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
  settings = &config.getRoot( );
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

