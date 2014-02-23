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
#include <RPCObject.h>
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
#include "CAMClientHandler.h"

#include "SocketHandler.h"
#include "Log.h"
#include "StreamingHandler.h"
#include "Avahi_Client.h"

TVDaemon *TVDaemon::instance = NULL;

bool TVDaemon::Create( const std::string &configdir )
{
  // Setup config directory
  std::string d = Utils::Expand( configdir );
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
  epg_update_interval(0),
  udev(NULL),
  udev_mon(NULL),
  udev_fd(0),
  httpd(NULL),
  up(true),
  recorder(NULL),
  avahi_client(new Avahi_Client())
{
}

TVDaemon::~TVDaemon( )
{
  delete avahi_client;

  Log( "TVDaemon terminating" );
  if( httpd )
  {
    Log( "Stopping HTTPServer" );
    httpd->Stop( );
    delete httpd;
  }
  Log( "Closing Tuners" );
  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    (*it)->Shutdown( );

  Log( "Stopping Recorder" );
  recorder->Stop( );

  Log( "Stopping CAMClientHandler" );
  delete CAMClientHandler::Instance( );

  Log( "Stopping StreamingHandlers" );
  for( std::map<Channel *, StreamingHandler *>::iterator it = streaming_handlers.begin( ); it != streaming_handlers.end( ); it++ )
    delete it->second;

  Log( "Saving Config" );
  SaveConfig( );

  Log( "Cleanup" );
  delete recorder;

  if( up ) up = false;
  JoinThread( );

  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    delete *it;

  for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    delete it->second;

  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    delete it->second;

  instance = NULL;
}

bool TVDaemon::Start( const char* httpRoot )
{
  Log( "Loading Config" );
  if( !LoadConfig( ))
  {
    LogError( "Error loading config directory '%s'", GetConfigDir( ).c_str( ));
    return false;
  }

  Log( "Creating CAM clients" );
  CAMClientHandler *camclienthandler = CAMClientHandler::Instance( );
  camclienthandler->LoadConfig( );

  Log( "Setting up udev" );
  // Setup udev
  udev = udev_new( );
  udev_mon = udev_monitor_new_from_netlink( udev, "udev" );
  udev_monitor_filter_add_match_subsystem_devtype( udev_mon, "dvb", NULL );

  FindAdapters( );

  Log( "Creating Recorder" );
  recorder = new Recorder( *this );
  recorder->LoadConfig( );

  MonitorAdapters( );

  httpd = new HTTPServer( httpRoot );
  httpd->AddDynamicHandler( "tvd", this );
  httpd->SetRTSPHandler( this );
  httpd->SetLogFunc( TVD_Log );

  if( !httpd->CreateServerTCP( HTTPDPORT ))
  {
    LogError( "unable to create web server" );
    return false;
  }

  httpd->Start( );
  Log( "HTTP Server listening on port %d", HTTPDPORT );

  if ( !avahi_client->Start( ))
  {
    LogError( "unable to start avahi client" );
  }
  Log( "Avahi client started" );

  return true;
}

bool TVDaemon::SaveConfig( )
{
  SCOPELOCK( );
  std::string version = PACKAGE_VERSION;
  WriteConfig( "Version", version );
  WriteConfig( "EPGUpdateInterval", epg_update_interval );
  WriteConfigFile( );

  for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
  {
    it->second->SaveConfig( );
  }

  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    it->second->SaveConfig( );

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

  std::string version = "";
  ReadConfig( "Version", version );

  ReadConfig( "EPGUpdateInterval", epg_update_interval );
  if( epg_update_interval == 0 )
    epg_update_interval = 12 * 60 * 60; // 12h

  Log( "Found config version: %s", version.c_str( ));

  Log( "Loading Channels" );
  if( !CreateFromConfig<Channel, int, TVDaemon>( *this, "channel", channels ))
    return false;
  Log( "Loading Sources" );
  if( !CreateFromConfig<Source, int, TVDaemon>( *this, "source", sources ))
    return false;
  Log( "Loading Adapters" );
  if( !CreateFromConfig<Adapter, TVDaemon>( *this, "adapter", adapters ))
    return false;

  return true;
}

void TVDaemon::FindAdapters( )
{
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
      AddFrontend( dev, udev_device_get_devpath( dev ));
    }
  }

  DIR *dir;
  struct dirent dp;
  struct dirent *result = NULL;
  dir = opendir( "/dev/dvb" );
  if( !dir )
    return;
  for( readdir_r( dir, &dp, &result ); result != NULL; readdir_r( dir, &dp, &result ))
  {
    if( dp.d_name[0] == '.' )
      continue;
    int adapter_id;
    if( sscanf( dp.d_name, "adapter%d", &adapter_id ) == 1 )
    {
      DIR *dir2;
      struct dirent dp2;
      struct dirent *result2 = NULL;
      char adapter[255];
      snprintf( adapter, sizeof( adapter ), "/dev/dvb/adapter%d", adapter_id );
      dir2 = opendir( adapter );
      if( !dir2 )
      {
        LogError( "cannot open directory %s", adapter );
        continue;
      }
      for( readdir_r( dir2, &dp2, &result2 ); result2 != NULL; readdir_r( dir2, &dp2, &result2 ))
      {
        if( dp2.d_name[0] == '.' )
          continue;
        int frontend_id;
        if( sscanf( dp2.d_name, "frontend%d", &frontend_id ) == 1 )
        {
          std::string path = std::string( adapter ) + "/" + dp2.d_name;
          AddFrontend( NULL, path.c_str(), adapter_id, frontend_id );
        }
      }
      closedir( dir2 );
    }
  }
  closedir( dir );
}

std::string TVDaemon::GetAdapterName( struct udev_device *dev )
{
  if( !dev )
    return "non-udev";

  udev_device *parent_dev = udev_device_get_parent( dev );
  if( !parent_dev )
    return "udev failure";

  std::string name;

  // try to get the manufacturer, product and serial from udev
  const char *temp = udev_device_get_sysattr_value( parent_dev, "manufacturer" );
  if( temp )
    name = Utils::Trim( temp );
  temp = udev_device_get_sysattr_value( parent_dev, "product" );
  if( temp )
  {
    if( !name.empty( ))
      name += ", ";
    name += Utils::Trim( temp );
  }
  temp = udev_device_get_sysattr_value( parent_dev, "serial" );
  if( temp )
  {
    if( !name.empty( ))
      name += ", ";
    name += Utils::Trim( temp );
  }

  // did provide udev the right information?
  // if not, fall back to just use the driver
  if( name.empty( ))
  {
    temp = udev_device_get_driver( parent_dev );
    if( temp )
      name = Utils::Trim( temp );
    else
    {
      // last resort get -try to use the devpath
      temp = udev_device_get_devpath( parent_dev );
      if( temp )
        name = Utils::Trim( temp );
    }
  }

  return name;
}

void TVDaemon::AddFrontend( struct udev_device *dev, const char *path, int adapter_id, int frontend_id )
{
  std::string uid  = Utils::DirName( path );
  std::string frontend = Utils::BaseName( path );

  if( adapter_id == -1)
  {
    const char *temp = udev_device_get_property_value( dev, "DVB_ADAPTER_NUM" );
    if( temp )
      adapter_id  = strtol( temp, NULL, 10);
  }
  if( frontend_id == -1)
  {
    const char *temp = udev_device_get_property_value( dev, "DVB_DEVICE_NUM" );
    if( temp )
      frontend_id = strtol( temp, NULL, 10);
  }
  if( adapter_id == -1 || frontend_id == -1)
  {
    LogError( "discovered not correctly initialized adapter, '%s'", path );
    return;
  }

  std::string name = GetAdapterName( dev );

  std::string fe_name;
  if( !Frontend::GetInfo( adapter_id, frontend_id, NULL, &fe_name ))
    return;

  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    Adapter *a = *it;
    if( a->GetName( ) == name )
    {
      if( false == a->IsPresent( ))
      {
        a->SetUID( uid );
        a->SetAdapterId( adapter_id );
      }
      if( adapter_id == a->GetAdapterId( ))
      {
        a->SetFrontend( fe_name, frontend_id );
      }
    }
    // prevent double adding of adapters
    // the uid has to be reset on remove
    if( a->GetUID() == uid || a->GetPath() == uid )
    {
      return;
    }
  }

  // Adapter not found, create new
  Adapter *a = new Adapter( *this, name, adapter_id, adapters.size( ));
  adapters.push_back( a );
  a->SetUID( uid );
  a->SetFrontend( fe_name, frontend_id );
}

void TVDaemon::UdevRemove( const char *path )
{
  std::string uid = Utils::DirName( path );
  for( std::vector<Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    if( (*it)->GetUID( ) == uid )
    {
      (*it)->ResetPresence( );
    }
  }
  return;
}

void TVDaemon::MonitorAdapters( )
{
  udev_monitor_enable_receiving( udev_mon );
  udev_fd = udev_monitor_get_fd( udev_mon );
  if( udev_fd < 0 )
  {
    LogError( "Error opening UDEV monitoring socket" );
  }

  StartThread( );
}

void TVDaemon::HandleUdev( )
{
  fd_set fds;
  struct timeval tv;
  int ret;
  struct udev_device *dev;

  FD_ZERO( &fds );
  FD_SET( udev_fd, &fds );
  tv.tv_sec = 2;
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
          AddFrontend( dev, path );
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
}

void TVDaemon::Run( )
{
  while( up )
  {
    HandleUdev( );

    //usleep( 250 * 1000 );
  }
}

Source *TVDaemon::CreateSource( const std::string &name, Source::Type type, std::string scanfile )
{
  for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
  {
    if( it->second->GetName( ) == name )
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

  int next_id = GetAvailableKey<Source, int>( sources );
  sources[next_id] = s;
  return s;
}

std::vector<std::string> TVDaemon::GetAdapterList( ) const
{
  std::vector<std::string> result;
  for( std::vector<Adapter *>::const_iterator it = adapters.begin( ); it != adapters.end( ); it++ )
  {
    result.push_back( (*it)->GetName( ));
  }
  return result;
}

Adapter *TVDaemon::GetAdapter( const int id ) const
{
  if( id >= adapters.size( ))
    return NULL;
  return adapters[id];
}

std::vector<std::string> TVDaemon::GetSourceList( )
{
  std::vector<std::string> result;
  for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
  {
    result.push_back( it->second->GetName( ));
  }
  return result;
}

Source *TVDaemon::GetSource( int id ) const
{
  std::map<int, Source *>::const_iterator it = sources.find( id );
  if( it == sources.end( ))
    return NULL;
  return it->second;
}

Channel *TVDaemon::CreateChannel( Service *service )
{
  SCOPELOCK( );
  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    if( it->second->HasService( service ))
    {
      LogError( "Channel '%s' already exists", service->GetName( ).c_str( ));
      return NULL;
    }
  }
  Channel *c = new Channel( *this, service, channels.size( ));
  int next_id = GetAvailableKey<Channel, int>( channels );
  channels[next_id] = c;
  return c;
}

std::vector<std::string> TVDaemon::GetChannelList( )
{
  SCOPELOCK( );
  std::vector<std::string> result;
  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    result.push_back( it->second->GetName( ));
  return result;
}

Channel *TVDaemon::GetChannel( int id )
{
  SCOPELOCK( );
  if( id < 0 or id >= channels.size( ))
  {
    LogError( "Channel not found: %d", id );
    return NULL;
  }
  return channels[id];
}

void TVDaemon::UpdateEPG( )
{
  SCOPELOCK( ); // FIXME: use mutex on map
  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    it->second->UpdateEPG( );
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

  if( cat == "recorder" )
    return recorder->RPC( request, cat, action );

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

    for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    {
      json_object *entry = json_object_new_object( );
      it->second->json( entry );
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
    std::string search;
    if( request.HasParam( "search" ))
    {
      std::string t;
      request.GetParam( "search", t );
      Utils::ToLower( t, search );
    }

    std::vector<const JSONObject *> result;
    for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    {
      if( !search.empty( ) && it->second->GetName( ).find( search.c_str( ), 0, search.length( )) != 0 )
        continue;
      result.push_back( it->second );
    }
    ServerSideTable( request, result );
    return true;
  }

  if( action == "get_transponders" )
  {
    std::string search;
    if( request.HasParam( "search" ))
    {
      std::string t;
      request.GetParam( "search", t );
      Utils::ToLower( t, search );
    }

    std::vector<const JSONObject *> result;
    for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    {
      const std::map<int, Transponder *> &transponders = it->second->GetTransponders( );
      for( std::map<int, Transponder *>::const_iterator it2 = transponders.begin( ); it2 != transponders.end( ); it2++ )
      {
        std::string t;
        Utils::ToLower( it2->second->toString( ), t );
        if( !search.empty( ) && t.find( search.c_str( ), 0, search.length( )) == std::string::npos )
          continue;
        result.push_back( it2->second );
      }
    }
    ServerSideTable( request, result );
    return true;
  }

  if( action == "get_services" )
  {
    std::string search;
    if( request.HasParam( "search" ))
    {
      std::string t;
      request.GetParam( "search", t );
      Utils::ToLower( t, search );
    }

    std::vector<const JSONObject *> result;
    for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    {
      const std::map<int, Transponder *> &transponders = it->second->GetTransponders( );
      for( std::map<int, Transponder *>::const_iterator it2 = transponders.begin( ); it2 != transponders.end( ); it2++ )
      {
        const std::map<uint16_t, Service *> &services = it2->second->GetServices( );
        for( std::map<uint16_t, Service *>::const_iterator it3 = services.begin( ); it3 != services.end( ); it3++ )
        {
          if( !search.empty( ) && it3->second->GetName( ).find( search.c_str( ), 0, search.length( )) != 0 )
            continue;
          result.push_back( it3->second );
        }
      }
    }
    ServerSideTable( request, result );
    return true;
  }

  if( action == "get_epg" )
  {
    std::string search;
    if( request.HasParam( "search" ))
    {
      std::string t;
      request.GetParam( "search", t );
      Utils::ToLower( t, search );
    }

    std::vector<const JSONObject *> result;
    for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    {
      const std::vector<Event *> &events = it->second->GetEvents( );
      for( std::vector<Event *>::const_iterator it2 = events.begin( ); it2 != events.end( ); it2++ )
      {
        //Log( "time : %d", time( NULL ));
        //Log( "start: %d", (*it2)->GetStart( ));
        if( (*it2)->GetStart( ) + (*it2)->GetDuration( ) < time( NULL )) // FIXME: stop
          continue;
        const Name &name = (*it2)->GetName( );
        const Name &description = (*it2)->GetDescription( );
        const Name &channel = (*it2)->GetChannel( ).GetName( );
        if( !search.empty( ) and name.find( search.c_str( ), 0, search.length( )) == std::string::npos
            and description.find( search.c_str( ), 0, search.length( )) == std::string::npos
            and channel.find( search.c_str( ), 0, search.length( )) == std::string::npos )
          continue;
        result.push_back( *it2 );
      }
    }
    ServerSideTable( request, result );
    return true;
  }

  if( action == "get_recordings" )
  {
    //std::string search;
    //if( request.HasParam( "search" ))
    //{
      //std::string t;
      //request.GetParam( "search", t );
      //Utils::ToLower( t, search );
    //}
    std::vector<const JSONObject *> result;
    recorder->GetRecordings( result );
    ServerSideTable( request, result );
    return true;
  }

  if( action == "scan" )
  {
    for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
      it->second->Scan( );
    request.Reply( HTTP_OK );
    return true;
  }

  if( action == "update_epg" )
  {
    UpdateEPG( );
    request.Reply( HTTP_OK );
    return true;
  }

  request.NotFound( "RPC unknown action: %s", action.c_str( ));
  return false;
}

void TVDaemon::ServerSideTable( const HTTPRequest &request, std::vector<const JSONObject *> &data ) const
{
  int count = data.size( );

  int p = 7;
  std::sort( data.begin( ), data.end( ), JSONObjectComparator( p ));

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
  SCOPELOCK( );
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
    Log( "RPC on source %d", i );
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

void TVDaemon::Record( Channel &channel )
{
  recorder->Record( channel );
}

void TVDaemon::LockFrontends( )
{
  frontends_mutex.Lock( );
}

void TVDaemon::UnlockFrontends( )
{
  frontends_mutex.Unlock( );
}

void TVDaemon::LockChannels( )
{
  channels_mutex.Lock( );
}

void TVDaemon::UnlockChannels( )
{
  channels_mutex.Unlock( );
}

StreamingHandler *TVDaemon::GetStreamingHandler( std::string url )
{
  std::vector<std::string> tokens;
  Utils::Tokenize( url,"/", tokens, 3 );
  if( tokens.size( ) != 3 ) // rtsp://server:port/channel
  {
    LogError( "RTSP: invalid setup url: %s", url.c_str( ));
    return NULL;
  }
  std::string channel_name = tokens[2];

  LockChannels( );
  Channel *channel = NULL;
  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
  {
    if( it->second->GetName( ) == channel_name )
    {
      channel = it->second;
      break;
    }
  }
  if( !channel )
  {
    LogError( "RTSP: unknown channel '%s'", channel_name.c_str( ));
    UnlockChannels( );
    return NULL;
  }

  StreamingHandler *streaming_handler = NULL;
  std::map<Channel *, StreamingHandler *>::iterator it = streaming_handlers.find( channel ); // FIXME: key should be channel and remote_ip, maybe session
  if( it == streaming_handlers.end( ))
  {
    streaming_handler = new StreamingHandler( channel );
    streaming_handlers[channel] = streaming_handler;
  }
  else
    streaming_handler = streaming_handlers[channel];

  UnlockChannels( );
  return streaming_handler;
}
