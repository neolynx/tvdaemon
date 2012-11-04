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

#ifndef _ConfigObject_
#define _ConfigObject_

#include <libconfig.h++>
using namespace libconfig;
#include <string>
#include <vector>
#include <map>
#include <list>

#include "Utils.h"

class ConfigBase
{
  public:
    ConfigBase( ) : settings(NULL) { };
    ConfigBase( Setting &settings ) : settings(&settings) { };
    Setting &ConfigArray ( const char *key = NULL );
    Setting &ConfigGroup ( const char *key = NULL );
    Setting &ConfigList  ( const char *key = NULL );
    void     ReadConfig  ( const char *key, int &i );
    void     ReadConfig  ( const char *key, bool &b );
    void     ReadConfig  ( const char *key, uint8_t &u8 );
    void     ReadConfig  ( const char *key, uint16_t &u16 );
    void     ReadConfig  ( const char *key, uint32_t &u32 );
    void     ReadConfig  ( const char *key, float &f );
    void     ReadConfig  ( const char *key, std::string &s );
    void     WriteConfig ( const char *key, int i );
    void     WriteConfig ( const char *key, bool b );
    void     WriteConfig ( const char *key, uint8_t u8 );
    void     WriteConfig ( const char *key, uint16_t u16 );
    void     WriteConfig ( const char *key, uint32_t u32 );
    void     WriteConfig ( const char *key, float f );
    void     WriteConfig ( const char *key, std::string &s );
    bool     DeleteConfig( const char *key );
  protected:
    Setting *settings;
};

class ConfigObject : public ConfigBase
{
  public:
    ConfigObject( );
    ConfigObject( ConfigObject &parent, std::string configfile );
    ConfigObject( ConfigObject &parent, std::string configname, int config_id );
    virtual ~ConfigObject( );

    virtual bool SaveConfig( ) { return true; }
    virtual bool LoadConfig( ) { return true; }
    virtual int GetKey( ) const { return config_id; }

    template <class Class, class Parent> static bool CreateFromConfig( Parent &parent, std::string configname, std::vector<Class *> &list )
    {
      char buf[128];
      int count;
      for( count = 0; count < 1024; count++ )
      {
        std::string dir = parent.GetConfigDir( );
        snprintf( buf, sizeof( buf ), "%s%d/", configname.c_str( ), count );
        dir += buf;

        std::string file = dir + "config";
        if( !Utils::IsFile( file ))
          break;

        Class *c = new Class( parent, file );
        if( !c->LoadConfig( ))
          return false;
        list.push_back( c );
      }
      return true;
    }

    template <class Class, class Key, class Parent> static bool CreateFromConfig( Parent &parent, std::string configname, std::map<Key, Class *> &map )
    {
      char buf[128];
      int count;
      for( count = 0; count < 1024; count++ )
      {
        std::string dir = parent.GetConfigDir( );
        snprintf( buf, sizeof( buf ), "%s%d/", configname.c_str( ), count );
        dir += buf;

        std::string file = dir + "config";
        if( !Utils::IsFile( file ))
          break;

        Class *c = new Class( parent, file );
        if( !c->LoadConfig( ))
          return false;
        map[c->GetKey( )] = c;
      }
      return true;
    }

    template <class Class, class Parent> static bool CreateFromConfigFactory( Parent &parent, std::string configname, std::vector<Class *> &list )
    {
      char buf[128];
      int count;
      for( count = 0; count < 1024; count++ )
      {
        std::string dir = parent.GetConfigDir( );
        snprintf( buf, sizeof( buf ), "%s%d/", configname.c_str( ), count );
        dir += buf;

        std::string file = dir + "config";
        if( !Utils::IsFile( file ))
          break;

        Class *c = Class::Create( parent, file );
        if( !c )
        {
          printf( "Could not create object from '%s'\n", file.c_str( ));
          return false;
        }
        if( !c->LoadConfig( ))
          return false;
        list.push_back( c );
      }
      return true;
    }

    template <class Class, class Parent> static void SaveReferences( Parent &parent, std::string configname, std::vector<Class *> &list )
    {
      parent.DeleteConfig( configname.c_str( ));
      Setting &n = parent.ConfigList( configname.c_str( ));
      typename std::vector<Class *>::iterator it;
      for( it = list.begin( ); it != list.end( ); it++ )
      {
        Setting &n2 = n.add( Setting::TypeArray );
        std::list<int> p = (*it)->GetParentPath( );
        for( std::list<int>::iterator it2 = p.begin( ); it2 != p.end( ); it2++ )
        {
          Setting &n3 = n2.add( Setting::TypeInt );
          n3 = *it2;
        }
      }
    }

    std::string GetConfigDir( )  { return configdir; }
    std::string GetConfigFile( ) { return configfile; }
    bool SetConfigFile( std::string configfile );
    bool WriteConfigFile( );
    bool ReadConfigFile( );

    std::list<int> GetParentPath( );

  private:
    int config_id;
    std::string configname;;
    std::string configfile;;
    std::string configdir;
    Config config;
    ConfigObject *parent;
};

#endif
