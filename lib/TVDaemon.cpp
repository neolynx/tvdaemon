/*
 *  tvdaemon
 *
 *  TVDaemon main class
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

#include "TVDaemon.h"

#include <string.h>
#include <stdlib.h> // atof, atoi
#include <dirent.h>
#include <sstream>  // ostringstream
#include <unistd.h> // usleep
#include <json/json.h>
#include <math.h>   // NAN
#include <string>
#include <algorithm> // sort

#include "config.h"
#include "Utils.h"
#include "Source.h"
#include "Channel.h"
#include "Adapter.h"
#include "Frontend.h"

#include "SocketHandler.h"
#include "Log.h"

TVDaemon *TVDaemon::instance = NULL;

bool TVDaemon::Create( std::string confdir )
{
  // Setup config directory
  std::string d = Utils::Expand( confdir );
  if( !Utils::IsDir( d ))
  {
    if( !Utils::MkDir( d ))
    {
      LogError( "Cannot create dir '%s'", d.c_str( ));
      return false;
    }
  }
  Utils::EnsureSlash( d );
  SetConfigFile( d + "config" );
  if( !Utils::IsFile( GetConfigFile( )))
    SaveConfig( );
  Log( "TVDconfig: %s", GetConfigFile( ).c_str( ));
  return true;
}

TVDaemon *TVDaemon::Instance( )
{
  if( !instance )
    instance = new TVDaemon( );
  return instance;
}

TVDaemon::TVDaemon( ) :
  ConfigObject( ),
  httpd(NULL),
  up(false)
{
}

TVDaemon::~TVDaemon( )
{
  SaveConfig( );

  if( httpd )
  {
    httpd->Stop( );
    delete httpd;
  }

  if( up )
  {
    up = false;
    pthread_join( thread_udev, NULL );
  }

  for( std::vector<Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
  {
    delete *it;
  }

  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    delete *it;
  }

  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    delete *it;
  }
  instance = NULL;
}

bool TVDaemon::Start( )
{
  if( !LoadConfig( ))
  {
    LogError( "Error loading config directory '%s'", GetConfigDir( ).c_str( ));
    return false;
  }

  // Setup udev
  udev = udev_new( );
  udev_mon = udev_monitor_new_from_netlink( udev, "udev" );
  udev_monitor_filter_add_match_subsystem_devtype( udev_mon, "dvb", NULL );

  FindAdapters( );

  MonitorAdapters( );

  httpd = new HTTPServer( "www" );
  httpd->AddDynamicHandler( "tvd", this );
  httpd->SetLogFunc( TVD_Log );

  if( !httpd->CreateServerTCP( HTTPDPORT ))
  {
    LogError( "unable to create web server" );
  }
  else
  {
    httpd->Start( );
    Log( "HTTP Server listening on port %d", HTTPDPORT );
  }
  return true;
}

bool TVDaemon::SaveConfig( )
{
  float version = atof( PACKAGE_VERSION );
  WriteConfig( "Version", version );
  WriteConfig( "Dir", dir );
  WriteConfigFile( );

  for( std::vector<Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }

  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }

  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool TVDaemon::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;

  float version = NAN;
  ReadConfig( "Version", version );
  ReadConfig( "Dir", dir );

  Log( "Found config version: %f", version );
  if( dir.empty( ))
    dir = "~";

  if( !CreateFromConfig<Source, TVDaemon>( *this, "source", sources ))
    return false;
  if( !CreateFromConfig<Adapter, TVDaemon>( *this, "adapter", adapters ))
    return false;
  if( !CreateFromConfig<Channel, TVDaemon>( *this, "channel", channels ))
    return false;
  return true;
}

int TVDaemon::FindAdapters( )
{
  int count = 0;

  struct udev_enumerate *enumerate = udev_enumerate_new( udev );
  udev_enumerate_add_match_subsystem( enumerate, "dvb" );
  udev_enumerate_scan_devices( enumerate );
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;
  devices = udev_enumerate_get_list_entry( enumerate );
  udev_list_entry_foreach( dev_list_entry, devices )
  {
    const char *path = udev_list_entry_get_name( dev_list_entry );
    dev = udev_device_new_from_syspath( udev, path );
    const char *type = udev_device_get_property_value( dev, "DVB_DEVICE_TYPE" );

    if( strcmp( type, "frontend" ) == 0 )
    {
      UdevAdd( dev, udev_device_get_devpath( dev ));
      count++;
    }
  }
  return count;
}

Adapter *TVDaemon::UdevAdd( struct udev_device *dev, const char *path )
{
  std::string uid  = Utils::DirName( path );
  std::string frontend = Utils::BaseName( path );

  int adapter_id  = atoi( udev_device_get_property_value( dev, "DVB_ADAPTER_NUM" ));
  int frontend_id = atoi( udev_device_get_property_value( dev, "DVB_DEVICE_NUM" ));

  if( adapter_id == -1 || frontend_id == -1)
  {
    LogError( "Udev discovered not correctly initialized adapter" );
    return NULL;
  }
  Adapter *a = NULL;
  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    if( (*it)->GetUID( ) == uid )
    {
      a = *it;
      break;
    }
  }

  std::string name;
  Frontend::GetInfo( adapter_id, frontend_id, NULL, &name );

  Adapter *c = NULL;
  if( a == NULL ) // maybe adapter moved ?
  {
    // Search by name
    // if only one adapter with the same name is found, take it
    for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
      // FIXME: search only non present adapters, optimization
    {
      if( (*it)->GetName( ) == name )
      {
        if( c == NULL )
          c = *it;
        else // we have multiple adapters with the same name
        {
          c = NULL;
          break;
        }
      }
    }
  }

  if( c )
  {
    Log( "Adapter possibly moved, updating configuration" );
    a = c;
  }

  if( a == NULL ) // Adapter not found, create new
  {
    a = new Adapter( *this, uid, name, adapters.size( ));
    adapters.push_back( a );
  }

  a->SetPresence( true );
  a->SetFrontend( frontend, adapter_id, frontend_id );
  return a;
}

void TVDaemon::UdevRemove( const char *path )
{
  std::string uid = Utils::DirName( path );
  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    if( (*it)->GetUID( ) == uid )
    {
      (*it)->SetPresence( false );
      return;
    }
  }
  LogError( "Unknown adapter removed: %s", path );
  return;
}

void TVDaemon::MonitorAdapters( )
{
  udev_monitor_enable_receiving( udev_mon );
  udev_fd = udev_monitor_get_fd( udev_mon );

  if( pthread_create( &thread_udev, NULL, run_udev, (void *) this ) != 0 )
  {
    LogError( "cannot create udev thread" );
  }
}

void *TVDaemon::run_udev( void *ptr )
{
  TVDaemon *t = (TVDaemon *) ptr;
  t->Thread_udev( );
  pthread_exit( NULL );
  return NULL;
}

void TVDaemon::Thread_udev( )
{
  fd_set fds;
  struct timeval tv;
  int ret;
  struct udev_device *dev;

  up = true;
  while( up )
  {
    FD_ZERO( &fds );
    FD_SET( udev_fd, &fds );
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    ret = select( udev_fd + 1, &fds, NULL, NULL, &tv );

    /* Check if our file descriptor has received data. */
    if( ret > 0 && FD_ISSET( udev_fd, &fds ))
    {
      dev = udev_monitor_receive_device( udev_mon );
      if( dev )
      {
        const char *type = udev_device_get_property_value( dev, "DVB_DEVICE_TYPE" );
        if( strcmp( type, "frontend" ) == 0 )
        {
          const char *path = udev_device_get_devpath( dev );
          const char *action = udev_device_get_action( dev );
          if( strcmp( action, "add" ) == 0 )
          {
            Log( "Frontend added: %s", path );
            Adapter *a = UdevAdd( dev, path );
          }
          else if( strcmp( action, "remove" ) == 0 )
          {
            Log( "Frontend removed: %s", path );
            UdevRemove( path );
          }
          else
          {
            LogError( "Unknown udev action '%s' on %s", action, path );
          }

          udev_device_unref( dev );
        }
      }
      else
      {
        LogError( "No Device from receive_device(). An error occured." );
      }
    }
    usleep( 250 * 1000 );
  }
}


Source *TVDaemon::CreateSource( std::string name, Source::Type type, std::string scanfile )
{
  for( std::vector<Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
  {
    if( (*it)->GetName( ) == name )
    {
      LogError( "Source with name '%s' already exists", name.c_str( ));
      return NULL;
    }
  }

  // FIXME: lock
  Source *s = new Source( *this, name, type, sources.size( ));

  if( scanfile != "" )
  {
    s->ReadScanfile( scanfile );
  }

  sources.push_back( s );
  return s;
}

std::vector<std::string> TVDaemon::GetAdapterList( )
{
  std::vector<std::string> result;
  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    result.push_back( (*it)->GetName( ));
  }
  return result;
}

Adapter *TVDaemon::GetAdapter( int id )
{
  if( id >= adapters.size( ))
    return NULL;
  return adapters[id];
}

std::vector<std::string> TVDaemon::GetSourceList( )
{
  std::vector<std::string> result;
  for( std::vector<Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
  {
    result.push_back( (*it)->GetName( ));
  }
  return result;
}

Source *TVDaemon::GetSource( int id ) const
{
  if( id >= sources.size( ))
    return NULL;
  return sources[id];
}

Channel *TVDaemon::CreateChannel( std::string name )
{
  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    if( (*it)->GetName( ) == name )
    {
      LogError( "Channel '%s' already exists", name.c_str( ));
      return NULL;
    }
  }
  Channel *c = new Channel( *this, name, channels.size( ));
  channels.push_back( c );
  return c;
}

std::vector<std::string> TVDaemon::GetChannelList( )
{
  std::vector<std::string> result;
  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    result.push_back( (*it)->GetName( ));
  }
  return result;
}

Channel *TVDaemon::GetChannel( int id )
{
  if( id >= channels.size( ))
    return NULL;
  return channels[id];
}

bool TVDaemon::HandleDynamicHTTP( const HTTPRequest &request )
{
  std::string cat;
  if( !request.GetParam( "c", cat ))
    return false;
  std::string action;
  if( !request.GetParam( "a", action ))
    return false;

  if( cat == "tvdaemon" )
    return RPC( request, cat, action );

  if( cat == "source" )
    return RPC_Source( request, cat, action );
  if( cat == "transponder" )
    return RPC_Source( request, cat, action );
  if( cat == "service" )
    return RPC_Source( request, cat, action );

  if( cat == "adapter" )
    return RPC_Adapter( request, cat, action );
  if( cat == "frontend" )
    return RPC_Adapter( request, cat, action );
  if( cat == "port" )
    return RPC_Adapter( request, cat, action );

  if( cat == "channel" )
    return RPC( request, cat, action );

  request.NotFound( "RPC unknown category: %s", cat.c_str( ));
  return false;
}

bool TVDaemon::RPC( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  if( action == "get_sources" )
  {
    json_object *h = json_object_new_object();
    json_object_object_add( h, "iTotalRecords", json_object_new_int( sources.size( )));
    json_object *a = json_object_new_array();

    for( std::vector<Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    {
      json_object *entry = json_object_new_object( );
      (*it)->json( entry );
      json_object_array_add( a, entry );
    }

    json_object_object_add( h, "data", a );
    request.Reply( h );
    return true;
  }

  if( action == "list_sourcetypes" )
  {
    json_object *h = json_object_new_object( );
    json_object *a = json_object_new_array( );

    json_object *entry = json_object_new_object( );
    json_object_object_add( entry, "id", json_object_new_int( Source::Type_DVBS ));
    json_object_object_add( entry, "type", json_object_new_string( "DVB-S" ));
    json_object_array_add( a, entry );

    entry = json_object_new_object( );
    json_object_object_add( entry, "id", json_object_new_int( Source::Type_DVBC ));
    json_object_object_add( entry, "type", json_object_new_string( "DVB-C" ));
    json_object_array_add( a, entry );

    entry = json_object_new_object( );
    json_object_object_add( entry, "id", json_object_new_int( Source::Type_DVBT ));
    json_object_object_add( entry, "type", json_object_new_string( "DVB-T" ));
    json_object_array_add( a, entry );

    entry = json_object_new_object( );
    json_object_object_add( entry, "id", json_object_new_int( Source::Type_ATSC ));
    json_object_object_add( entry, "type", json_object_new_string( "ATSC" ));
    json_object_array_add( a, entry );

    json_object_object_add( h, "data", a );
    request.Reply( h );
    return true;
  }

  if( action == "create_source" )
  {
    std::string name;
    if( !request.GetParam( "name", name ))
      return false;
    std::string scanfile;
    if( !request.GetParam( "scanfile", scanfile ))
      return false;
    Source::Type type;
    if( !request.GetParam( "type", (int &) type ))
      return false;

    Log( "Creating Source '%s' type %d initialized from '%s'", name.c_str( ), type, scanfile.c_str( ));

    Source *s = CreateSource( name, type, scanfile );
    if( !s )
    {
      request.NotFound( "Error creating source" );
      return false;
    }

    request.Reply( HTTP_OK, s->GetKey( ));
    return true;
  }

  if( action == "get_scanfiles" )
  {
    Source::Type source_type = Source::Type_Any;
    if( request.HasParam( "type" ))
    {
      std::string t;
      request.GetParam( "type", t );
      int i = atoi( t.c_str( ));
      if( i != Source::Type_Any && i < Source::Type_Last )
        source_type = (Source::Type) i;
    }

    std::vector<std::string> scanfiles = Source::GetScanfileList( source_type );

    json_object *a = json_object_new_array( );
    for( int i = 0; i < scanfiles.size( ); i++ )
      json_object_array_add( a, json_object_new_string( scanfiles[i].c_str( )));
    request.Reply( a );
    return true;
  }

  if( action == "get_devices" )
  {
    json_object *a = json_object_new_array();

    for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    {
      json_object *entry = json_object_new_object( );
      (*it)->json( entry );
      json_object_array_add( a, entry );
    }

    request.Reply( a );
    return true;
  }

  if( action == "list_channels" )
  {
    int count = channels.size( );
    json_object *h = json_object_new_object();
    //std::string echo =  parameters["sEcho"];
    int echo = 1; //atoi( parameters[std::string("sEcho")].c_str( ));
    json_object_object_add( h, "sEcho", json_object_new_int( echo ));
    json_object_object_add( h, "iTotalRecords", json_object_new_int( count ));
    json_object_object_add( h, "iTotalDisplayRecords", json_object_new_int( count ));
    json_object *a = json_object_new_array();

    for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    {
      json_object *entry = json_object_new_array( );
      (*it)->json( entry );
      json_object_array_add( a, entry );
    }

    json_object_object_add( h, "aaData", a );
    request.Reply( h );
    return true;
  }

  request.NotFound( "RPC unknown action: %s", action.c_str( ));
  return false;
  }


bool TVDaemon::RPC_Source( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  std::string t;
  if( !request.GetParam( "source_id", t ))
    return false;

  int i = atoi( t.c_str( ));
  if( i >= 0 && i < sources.size( ))
  {
    return sources[i]->RPC( request, cat, action );
  }

  request.NotFound( "RPC unknown source: %d", i );
  return false;
}

bool TVDaemon::RPC_Adapter( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  std::string t;
  if( !request.GetParam( "adapter_id", t ))
    return false;

  int i = atoi( t.c_str( ));
  if( i >= 0 && i < adapters.size( ))
  {
    return adapters[i]->RPC( request, cat, action );
  }

  request.NotFound( "RPC unknown adapter: %d", i );
  return false;
}

