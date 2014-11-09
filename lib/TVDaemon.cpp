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
#include "Port.h"
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
  LogInfo( "Config directory: %s", d.c_str( ));

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
  avahi_client(new Avahi_Client())
{
}

TVDaemon::~TVDaemon( )
{
  delete avahi_client;

  LogInfo( "TVDaemon terminating" );
  if( httpd )
  {
    LogInfo( "Stopping HTTPServer" );
    httpd->Stop( );
    delete httpd;
  }

  LogInfo( "Closing Tuners" );
  LockAdapters( );
  for( std::map<int, Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    it->second->Shutdown( );
  UnlockAdapters( );

  LogInfo( "Stopping Recorder" );
  Recorder::Instance( )->Stop( );

  LogInfo( "Stopping StreamingHandler" );
  StreamingHandler::Instance( )->Shutdown( );

  LogInfo( "Stopping CAMClientHandler" );
  CAMClientHandler::Instance( )->Shutdown( );

  LogInfo( "Saving Config" );
  SaveConfig( );

  LogInfo( "Cleanup" );

  up = false;
  JoinThread( );

  LockAdapters( );
  for( std::map<int, Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    delete it->second;
  UnlockAdapters( );

  for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    delete it->second;
  sources.clear( );

  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    delete it->second;
  channels.clear( );

  instance = NULL;
}

bool TVDaemon::Start( const char* httpRoot )
{
  LogInfo( "Loading Config" );
  if( !LoadConfig( ))
  {
    LogError( "Error loading config directory '%s'", GetConfigDir( ).c_str( ));
    return false;
  }

  LogInfo( "Creating CAM clients" );
  CAMClientHandler::Instance( )->LoadConfig( );

  LogInfo( "Setting up udev" );
  // Setup udev
  udev = udev_new( );
  udev_mon = udev_monitor_new_from_netlink( udev, "udev" );
  udev_monitor_filter_add_match_subsystem_devtype( udev_mon, "dvb", NULL );

  FindAdapters( );
  ProcessAdapters( );

  LogInfo( "Creating Recorder" );
  Recorder::Instance( )->LoadConfig( );

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
  LogInfo( "HTTP Server listening at http://0.0.0.0:%d", HTTPDPORT );

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
    it->second->SaveConfig( );

  for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    it->second->SaveConfig( );

  LockAdapters( );
  for( std::map<int, Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    it->second->SaveConfig( );
  UnlockAdapters( );

  Recorder::Instance( )->SaveConfig( );

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

  LogInfo( "Found config version: %s", version.c_str( ));

  LogInfo( "Loading Channels" );
  if( !CreateFromConfig<Channel, int, TVDaemon>( *this, "channel", channels ))
    return false;
  LogInfo( "Loading Sources" );
  if( !CreateFromConfig<Source, int, TVDaemon>( *this, "source", sources ))
    return false;

  LogInfo( "Loading Adapters" );
  int ret;
  LockAdapters( );
  ret = CreateFromConfig<Adapter, int, TVDaemon>( *this, "adapter", adapters );
  UnlockAdapters( );

  return ret;
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
      AddFrontendToList( dev, udev_device_get_devpath( dev ));
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
          AddFrontendToList( NULL, path.c_str(), adapter_id, frontend_id );
        }
      }
      closedir( dir2 );
    }
  }
  closedir( dir );
}

void TVDaemon::ProcessAdapters( )
{
  ScopeLock Lock( device_list_mutex );
  bool found;
  std::list<device_t>::iterator dev_it = device_list.begin( );
  while( dev_it != device_list.end( ))
  {
    found = false;
    //
    // first round - try to match name and udev uid
    //
    LockAdapters( );
    for( std::map<int, Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    {
      Adapter *a = it->second;
      // prevent double adding of non-udev adapters
      if( a->HasFrontend( dev_it->adapter_id, dev_it->frontend_id ))
      {
        found = true;
        break;
      }
      // try matching the adapter - precise matching
      if( a->GetName( ) == dev_it->adapter_name &&
          a->GetUID( ) == dev_it->uid )
      {
        a->SetFrontend( dev_it->frontend_name, dev_it->adapter_id, dev_it->frontend_id );
        found = true;
        break;
      }
    }
    UnlockAdapters( );
    if( found )
      dev_it = device_list.erase( dev_it );
    else
      dev_it++;
  }

  dev_it = device_list.begin( );
  while( dev_it != device_list.end( ))
  {
    found = false;
    //
    // second round - try to match only the name
    //
    LockAdapters( );
    for( std::map<int, Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    {
      Adapter *a = it->second;
      // prevent double adding of non-udev adapters
      if( a->HasFrontend( dev_it->adapter_id, dev_it->frontend_id ))
      {
        found = true;
        break;
      }
      // try matching the adapter - precise matching
      if( a->GetName( ) == dev_it->adapter_name &&
          a->GetUID( ) == dev_it->uid )
      {
        a->SetFrontend( dev_it->frontend_name, dev_it->adapter_id, dev_it->frontend_id );
        found = true;
        break;
      }
      // try matching the adapter - loose matching
      if( a->GetName( ) == dev_it->adapter_name &&
          a->IsPresent() == false )
      {
        a->SetUID( dev_it->uid );
        a->SetFrontend( dev_it->frontend_name, dev_it->adapter_id, dev_it->frontend_id );
        found = true;
        break;
      }
    }
    UnlockAdapters( );
    if( found )
    {
      dev_it = device_list.erase( dev_it );
      continue;
    }

    //
    // Adapter not found, create new
    //
    Adapter *a = new Adapter( *this, dev_it->adapter_name, dev_it->uid, adapters.size( ));
    a->SetFrontend( dev_it->frontend_name, dev_it->adapter_id, dev_it->frontend_id );

    LockAdapters( );
    int next_id = GetAvailableKey<Adapter, int>( adapters );
    adapters[next_id] = a;
    UnlockAdapters( );

    dev_it = device_list.erase( dev_it );
  }
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

void TVDaemon::AddFrontendToList( struct udev_device *dev, const char *path, const int adapter_id, const int frontend_id )
{
  std::string uid  = Utils::DirName( path );
  if( uid.empty() )
  {
    LogError( "adapter without a uid, '%s'", path );
    return;
  }

  int new_adapter_id = adapter_id;
  if( adapter_id == -1)
  {
    const char *temp = udev_device_get_property_value( dev, "DVB_ADAPTER_NUM" );
    if( temp )
      new_adapter_id  = strtol( temp, NULL, 10);
  }

  int new_frontend_id = frontend_id;
  if( frontend_id == -1)
  {
    const char *temp = udev_device_get_property_value( dev, "DVB_DEVICE_NUM" );
    if( temp )
      new_frontend_id = strtol( temp, NULL, 10);
  }

  if( new_adapter_id == -1 || new_frontend_id == -1)
  {
    LogError( "discovered not correctly initialized adapter, '%s'", path );
    return;
  }

  std::string adapter_name = GetAdapterName( dev );
  std::string frontend_name;
  if( !Frontend::GetInfo( new_adapter_id, new_frontend_id, NULL, &frontend_name ))
    return;

  AddFrontendToList( adapter_name, frontend_name, uid, new_adapter_id, new_frontend_id );
}

void TVDaemon::AddFrontendToList( const std::string& adapter_name, const std::string& frontend_name, const std::string& uid, const int adapter_id, const int frontend_id)
{
  ScopeLock Lock( device_list_mutex );
  device_list.push_back( device_t(adapter_name, frontend_name, uid, adapter_id, frontend_id ));
}

void TVDaemon::RemoveFrontend( const std::string &uid )
{
  ScopeLock _l( mutex_adapters );
  for( std::map<int, Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    if( it->second->GetUID( ) == uid )
      it->second->ResetPresence( );
}

void TVDaemon::UdevRemove( const char *path )
{
  RemoveFrontend( Utils::DirName( path ));
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
          AddFrontendToList( dev, path );
          ProcessAdapters();
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
  // FIXME lock sources
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

Adapter *TVDaemon::GetAdapter( const int id ) const // FIXME: returns unmutexed adapter
{
  ScopeLock _l( mutex_adapters );
  std::map<int, Adapter *>::const_iterator it = adapters.find( id );
  if( it == adapters.end( ))
  {
    LogError( "Adapter %d not found", id );
    return NULL;
  }
  return it->second;
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
    if( it->second->HasService( service ) or it->second->GetName( ) == service->GetName( ))
    {
      LogError( "Channel '%s' already exists", service->GetName( ).c_str( ));
      return NULL;
    }
  }
  int next_id = GetAvailableKey<Channel, int>( channels );
  Channel *c = new Channel( *this, service, next_id );
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
  std::map<int, Channel *>::iterator it = channels.find( id );
  if( it == channels.end( ))
    return NULL;
  return it->second;
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
    return Recorder::Instance( )->RPC( request, cat, action );

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

    Log( "Creating Source '%s' type %d", name.c_str( ), type );

    Source *s = CreateSource( name, type, scanfile );
    if( !s )
    {
      request.NotFound( "Error creating source" );
      return false;
    }

    int adapter_id, frontend_id, port_id;
    adapter_id = frontend_id = port_id = -1;
    if( request.HasParam( "adapter_id" ))
      request.GetParam( "adapter_id", adapter_id );
    if( request.HasParam( "frontend_id" ))
      request.GetParam( "frontend_id", frontend_id );
    if( request.HasParam( "port_id" ))
      request.GetParam( "port_id", port_id );

    Adapter *a = GetAdapter( adapter_id );
    if( a )
    {
      Frontend *f = a->GetFrontend( frontend_id );
      if( f )
      {
        Port *p = f->GetPort( port_id );
        p->SetSource( s );
        s->AddPort( p );
      }
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
    json_object *a = json_object_new_object();

    LockAdapters( );
    for( std::map<int, Adapter *>::iterator it = adapters.begin( ); it != adapters.end( ); it++ )
    {
      json_object *entry = json_object_new_object( );
      it->second->json( entry );
      json_object_object_add( a, it->first, entry );
    }
    UnlockAdapters( );

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

  if( action == "get_camclients" )
  {
    std::vector<const JSONObject *> result;
    CAMClientHandler *camd = CAMClientHandler::Instance( );
    camd->json( result );
    //for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    //{
      //if( !search.empty( ) && it->second->GetName( ).find( search.c_str( ), 0, search.length( )) != 0 )
        //continue;
      //result.push_back( it->second );
    //}
    ServerSideTable( request, result );
    return true;
  }

  if( action == "remove_channel" )
  {
    std::string t;
    if( !request.GetParam( "id", t ))
      return false;

    int i = atoi( t.c_str( ));
    return RemoveChannel( i );
  }

  if( action == "get_playlist" )
  {
    std::string referer, server;
    if( request.GetHeader( "Referer", referer ))
    {
      std::vector<std::string> tokens;
      Utils::Tokenize( referer, "/", tokens, 3 );
      if( tokens.size( ) < 3 )
      {
        LogError( "GetPlaylist: error parsing url (%s)", referer.c_str( ));
        return false;
      }

      server = tokens[1];
    }
    else
    {
      in_addr ip = request.GetServerIP( );
      uint16_t port = request.GetServerPort( );

      char ipaddr[INET_ADDRSTRLEN];
      inet_ntop( AF_INET, &ip, ipaddr, sizeof( ipaddr ));

      char t[32];
      snprintf( t, sizeof( t ), "%s:%d", ipaddr, port );
      server = t;
    }

    std::string xspf = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n\
  <trackList>\n";
    for( std::map<int, Channel *>::iterator it = channels.begin( ); it != channels.end( ); it++ )
    {
      std::string name;
      HTTPServer::URLEncode( it->second->GetName( ), name );
      xspf += "    <track>\n";
      xspf += "      <title>" + it->second->GetName( );
      xspf += "</title>\n";
      xspf += "      <location>";
      xspf += "rtsp://" + server + "/channel/" + name;
      xspf += "</location>\n";
      xspf += "    </track>\n";
    }
    xspf += "  </trackList>\n</playlist>\n";

    HTTPServer::Response response;
    response.AddStatus( HTTP_OK );
    response.AddTimeStamp( );
    response.AddMime( "xspf" );
    response.AddHeader( "Content-Disposition", "inline; filename=\"tvdaemon.xspf\"" );
    response.AddContent( xspf );

    request.Reply( response );
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
        if( difftime((*it2)->GetEnd( ), time( NULL )) <= 0.0 ) // FIXME: stop
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
    Recorder::Instance( )->GetRecordings( result );
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

  if( action == "delete_adapter" )
  {
    int adapter_id;
    if( !request.GetParam( "id", adapter_id ))
    {
      request.NotFound( "id missing" );
      return false;
    }
    LockAdapters( );
    std::map<int, Adapter *>::iterator it = adapters.find( adapter_id );
    if( it == adapters.end( ))
    {
      request.NotFound( "id not found: %d", adapter_id );
      UnlockAdapters( );
      return false;
    }
    it->second->Delete( );
    delete it->second;
    adapters.erase( it );
    UnlockAdapters( );

    return true;
  }

  if( action == "delete_source" )
  {
    int source_id;
    if( !request.GetParam( "id", source_id ))
    {
      request.NotFound( "id missing" );
      return false;
    }
    LockSources( );
    std::map<int, Source *>::iterator it = sources.find( source_id );
    if( it == sources.end( ))
    {
      request.NotFound( "id not found: %d", source_id );
      UnlockSources( );
      return false;
    }

    it->second->Delete( );
    delete it->second;
    sources.erase( it );
    UnlockSources( );
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

  std::map<int, Channel *>::iterator it = channels.find( i );
  if( it != channels.end( ))
  {
    return it->second->RPC( request, cat, action );
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
  ScopeLock _l( mutex_adapters );
  int adapter_id;
  if( !request.GetParam( "adapter_id", adapter_id ))
    return false;

  std::map<int, Adapter *>::iterator it = adapters.find( adapter_id );
  if( it == adapters.end( ))
  {
    request.NotFound( "RPC unknown adapter: %d", adapter_id );
    return false;
  }
  return it->second->RPC( request, cat, action );
}

bool TVDaemon::Schedule( Event &event )
{
  return Recorder::Instance( )->Schedule( event );
}

void TVDaemon::Record( Channel &channel )
{
  Recorder::Instance( )->Record( channel );
}

void TVDaemon::LockAdapters( ) const
{
  mutex_adapters.Lock( );
}

void TVDaemon::UnlockAdapters( ) const
{
  mutex_adapters.Unlock( );
}

void TVDaemon::LockFrontends( ) const
{
  mutex_frontends.Lock( );
}

void TVDaemon::UnlockFrontends( ) const
{
  mutex_frontends.Unlock( );
}

void TVDaemon::LockChannels( ) const
{
  mutex_channels.Lock( );
}

void TVDaemon::UnlockChannels( ) const
{
  mutex_channels.Unlock( );
}

void TVDaemon::LockSources( ) const
{
  mutex_sources.Lock( );
}

void TVDaemon::UnlockSources( ) const
{
  mutex_sources.Unlock( );
}

Channel *TVDaemon::GetChannel( const std::string &channel_name )
{
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

  UnlockChannels( );
  return channel;
}

bool TVDaemon::RemoveChannel( int id )
{
  std::map<int, Channel *>::iterator it = channels.find( id );
  if( it == channels.end( ))
    return false;

  LockChannels( );
  Channel *c = it->second;
  Log( "Removing channel '%s'", c->GetName( ).c_str( ));
  // FIXME:
  // stop recordings
  // delete recordings
  // deleve events
  //

  for( std::map<int, Source *>::iterator it = sources.begin( ); it != sources.end( ); it++ )
    it->second->RemoveChannel( c );

  channels.erase( it );
  c->RemoveConfigFile( );
  delete c;
  UnlockChannels( );
  return true;
}
