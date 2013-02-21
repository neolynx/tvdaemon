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
#include "Recorder.h"

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
  Log( "Config directory: %s", d.c_str( ));
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
  Thread( ),
  httpd(NULL),
  up(true),
  recorder(NULL)
{
}

TVDaemon::~TVDaemon( )
{
  Log( "TVDaemon terminating" );
  if( httpd )
  {
    Log( "Stopping HTTPServer" );
    httpd->Stop( );
    delete httpd;
  }
  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    (*it)->Shutdown( );
  recorder->Stop( );

  SaveConfig( );

  delete recorder;

  if( up ) up = false;
  JoinThread( );

  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    delete *it;

  for( std::vector<Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    delete *it;

  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    delete *it;

  instance = NULL;
}

bool TVDaemon::Start( )
{
  recorder = new Recorder( *this );

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
  //UpdateEPG( );
  return true;
}

bool TVDaemon::SaveConfig( )
{
  ScopeLock _l;
  float version = atof( PACKAGE_VERSION );
  WriteConfig( "Version", version );
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

  if( recorder )
    recorder->SaveConfig( );

  return true;
}

bool TVDaemon::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;

  float version = NAN;
  ReadConfig( "Version", version );

  Log( "Found config version: %f", version );

  if( !CreateFromConfig<Channel, TVDaemon>( *this, "channel", channels ))
    return false;
  if( !CreateFromConfig<Source, TVDaemon>( *this, "source", sources ))
    return false;
  if( !CreateFromConfig<Adapter, TVDaemon>( *this, "adapter", adapters ))
    return false;

  recorder->LoadConfig( );

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

  StartThread( );
}

void TVDaemon::Run( )
{
  fd_set fds;
  struct timeval tv;
  int ret;
  struct udev_device *dev;

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
        LogError( "udev: event without device" );
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

Channel *TVDaemon::CreateChannel( Service *service )
{
  ScopeLock _l;
  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    if( (*it)->HasService( service ))
    {
      LogError( "Channel '%s' already exists", service->GetName( ).c_str( ));
      return NULL;
    }
  }
  Channel *c = new Channel( *this, service, channels.size( ));
  channels.push_back( c );
  return c;
}

std::vector<std::string> TVDaemon::GetChannelList( )
{
  ScopeLock _l;
  std::vector<std::string> result;
  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    result.push_back( (*it)->GetName( ));
  }
  return result;
}

Channel *TVDaemon::GetChannel( int id )
{
  ScopeLock _l;
  if( id < 0 or id >= channels.size( ))
  {
    LogError( "Channel not found: %d", id );
    return NULL;
  }
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
    return RPC_Channel( request, cat, action );

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

  if( action == "get_service_types" )
  {
    json_object *a = json_object_new_array( );
    for( int i = 0; i < Service::Type_Last; i++ )
      json_object_array_add( a, json_object_new_string( Service::GetTypeName((Service::Type) i )));
    request.Reply( a );
    return true;
  }

  if( action == "get_sourcetypes" )
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

  if( action == "get_channels" )
  {
    ScopeLock _l;
    json_object *h = json_object_new_object( );
    json_object *a = json_object_new_array();

    std::string search;
    if( request.HasParam( "search" ))
    {
      std::string t;
      request.GetParam( "search", t );
      Utils::ToLower( t, search );
    }

    std::vector<Channel *> result;

    for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    {
      std::string t;
      Utils::ToLower( (*it)->GetName( ), t );
      if( !search.empty( ) && t.find( search.c_str( ), 0, search.length( )) != 0 )
        continue;
      result.push_back( *it );
    }

    int count = result.size( );

    //std::sort( result.begin( ), result.end( ), Service::SortByName );

    int start = -1;
    if( request.HasParam( "start" ))
      request.GetParam( "start", start );
    if( start < 0 )
      start = 0;
    if( start > count )
      start = count;

    int page_size = -1;
    if( request.HasParam( "page_size" ))
      request.GetParam( "page_size", page_size );
    if( page_size <= 0 )
      page_size = 10;

    int end = start + page_size;
    if( end > count )
      end = count;
    for( int i = start; i < end; i++ )
    {
      json_object *entry = json_object_new_object( );
      result[i]->json( entry );
      json_object_array_add( a, entry );
    }
    json_object_object_add( h, "count", json_object_new_int( count ));
    json_object_object_add( h, "start", json_object_new_int( start ));
    json_object_object_add( h, "end", json_object_new_int( end ));
    json_object_object_add( h, "data", a );

    request.Reply( h );
    return true;
  }

  if( action == "get_services" )
  {
    json_object *h = json_object_new_object( );
    json_object *a = json_object_new_array();

    std::string search;
    if( request.HasParam( "search" ))
    {
      std::string t;
      request.GetParam( "search", t );
      Utils::ToLower( t, search );
    }

    std::vector<Service *> result;

    for( std::vector<Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    {
      const std::vector<Transponder *> &transponders = (*it)->GetTransponders( );
      for( std::vector<Transponder *>::const_iterator it2 = transponders.begin( ); it2 != transponders.end( ); it2++ )
      {
        const std::map<uint16_t, Service *> &services = (*it2)->GetServices( );
        for( std::map<uint16_t, Service *>::const_iterator it3 = services.begin( ); it3 != services.end( ); it3++ )
        {
          if( !search.empty( ) && it3->second->GetName( true ).find( search.c_str( ), 0, search.length( )) != 0 )
            continue;
          result.push_back( it3->second );
        }
      }
    }

    int count = result.size( );

    std::sort( result.begin( ), result.end( ), Service::SortByName );

    int start = -1;
    if( request.HasParam( "start" ))
      request.GetParam( "start", start );
    if( start < 0 )
      start = 0;
    if( start > count )
      start = count;

    int page_size = -1;
    if( request.HasParam( "page_size" ))
      request.GetParam( "page_size", page_size );
    if( page_size <= 0 )
      page_size = 10;

    int end = start + page_size;
    if( end > count )
      end = count;
    for( int i = start; i < end; i++ )
    {
      json_object *entry = json_object_new_object( );
      result[i]->json( entry );
      json_object_object_add( entry, "transponder_id", json_object_new_int( result[i]->GetTransponder( ).GetKey( )));
      json_object_object_add( entry, "source_id", json_object_new_int( result[i]->GetTransponder( ).GetSource( ).GetKey( )));
      json_object_array_add( a, entry );
    }
    json_object_object_add( h, "count", json_object_new_int( count ));
    json_object_object_add( h, "start", json_object_new_int( start ));
    json_object_object_add( h, "end", json_object_new_int( end ));
    json_object_object_add( h, "data", a );
    request.Reply( h );
    return true;
  }

  if( action == "get_epg" )
  {
    json_object *h = json_object_new_object( );
    json_object *a = json_object_new_array();

    std::string search;
    if( request.HasParam( "search" ))
    {
      std::string t;
      request.GetParam( "search", t );
      Utils::ToLower( t, search );
    }

    std::vector<Event *> result;

    for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    {
      const std::vector<Event *> &events = (*it)->GetEvents( );
      for( std::vector<Event *>::const_iterator it2 = events.begin( ); it2 != events.end( ); it2++ )
      {
        //Log( "time : %d", time( NULL ));
        //Log( "start: %d", (*it2)->GetStart( ));
        if( (*it2)->GetStart( ) + (*it2)->GetDuration( ) < time( NULL )) // FIXME: stop
          continue;
        std::string name;
        Utils::ToLower( (*it2)->GetName( ), name );
        std::string description;
        Utils::ToLower( (*it2)->GetDescription( ), description );
        if( !search.empty( ) and name.find( search.c_str( ), 0, search.length( )) == std::string::npos
                             and description.find( search.c_str( ), 0, search.length( )) == std::string::npos )
          continue;
        result.push_back( *it2 );
      }
    }

    int count = result.size( );

    std::sort( result.begin( ), result.end( ), Event::SortByStart );

    int start = -1;
    if( request.HasParam( "start" ))
      request.GetParam( "start", start );
    if( start < 0 )
      start = 0;
    if( start > count )
      start = count;

    int page_size = -1;
    if( request.HasParam( "page_size" ))
      request.GetParam( "page_size", page_size );
    if( page_size <= 0 )
      page_size = 10;

    int end = start + page_size;
    if( end > count )
      end = count;
    for( int i = start; i < end; i++ )
    {
      json_object *entry = json_object_new_object( );
      result[i]->json( entry );
      json_object_array_add( a, entry );
    }
    json_object_object_add( h, "count", json_object_new_int( count ));
    json_object_object_add( h, "start", json_object_new_int( start ));
    json_object_object_add( h, "end", json_object_new_int( end ));
    json_object_object_add( h, "data", a );
    request.Reply( h );
    return true;
  }

  request.NotFound( "RPC unknown action: %s", action.c_str( ));
  return false;
}

void TVDaemon::ServerSideTable( const HTTPRequest &request, const std::vector<JSONObject *> &data ) const
{
  int count = data.size( );

  int start = -1;
  if( request.HasParam( "start" ))
    request.GetParam( "start", start );
  if( start < 0 )
    start = 0;
  if( start > count )
    start = count;

  int page_size = -1;
  if( request.HasParam( "page_size" ))
    request.GetParam( "page_size", page_size );
  if( page_size <= 0 )
    page_size = 10;

  int end = start + page_size;
  if( end > count )
    end = count;

  json_object *h = json_object_new_object( );
  json_object *a = json_object_new_array();

  for( int i = start; i < end; i++ )
  {
    json_object *entry = json_object_new_object( );
    Log( "data[i]->json( entry ); %p", data[i] );
    data[i]->json( entry );
    json_object_array_add( a, entry );
  }
  json_object_object_add( h, "count", json_object_new_int( count ));
  json_object_object_add( h, "start", json_object_new_int( start ));
  json_object_object_add( h, "end", json_object_new_int( end ));
  json_object_object_add( h, "data", a );
  request.Reply( h );
}

bool TVDaemon::RPC_Channel( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  ScopeLock _l;
  std::string t;
  if( !request.GetParam( "channel_id", t ))
    return false;

  int i = atoi( t.c_str( ));
  if( i >= 0 && i < channels.size( ))
  {
    return channels[i]->RPC( request, cat, action );
  }

  request.NotFound( "RPC unknown channel: %d", i );
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

bool TVDaemon::Schedule( Event &event )
{
  return recorder->Schedule( event );
}

void TVDaemon::UpdateEPG( )
{
  ScopeLock _l;
  for( std::vector<Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    (*it)->UpdateEPG( );
  }
}
