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

#ifndef _TVDaemon_
#define _TVDaemon_

#include "ConfigObject.h"
#include "HTTPServer.h"
#include "Thread.h"
#include "Source.h"

#include <map>
#include <vector>
#include <list>
#include <libudev.h>

#define SCANFILE_DIR "/usr/share/dvb/"

#define HTTPDPORT    7777

class Adapter;
class Source;
class Channel;
class Recorder;
class Event;
class Avahi_Client;

class TVDaemon : public ConfigObject, public HTTPDynamicHandler, public RTSPHandler, public Thread
{
  public:
    static TVDaemon *Instance( );
    bool Create( const std::string& configdir );
    virtual ~TVDaemon( );

    bool Start( const char* httpRoot );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    Adapter *GetAdapter( const int id ) const;

    Source *CreateSource( const std::string &name, Source::Type type, std::string scanfile = "" );
    std::vector<std::string> GetSourceList( );
    Source *GetSource( int id ) const;

    std::vector<std::string> GetChannelList( );
    Channel *GetChannel( int id );
    Channel *GetChannel( const std::string &channel_name );
    Channel *CreateChannel( Service *service );

    void LockAdapters( ) const;
    void UnlockAdapters( ) const;
    void LockFrontends( ) const;
    void UnlockFrontends( ) const;
    void LockChannels( ) const;
    void UnlockChannels( ) const;
    void LockSources( ) const;
    void UnlockSources( ) const;

    // RPC
    virtual bool HandleDynamicHTTP( const HTTPRequest &request );
    bool RPC        ( const HTTPRequest &request, const std::string &cat, const std::string &action );
    bool RPC_Channel( const HTTPRequest &request, const std::string &cat, const std::string &action );
    bool RPC_Source ( const HTTPRequest &request, const std::string &cat, const std::string &action );
    bool RPC_Adapter( const HTTPRequest &request, const std::string &cat, const std::string &action );

    void ServerSideTable( const HTTPRequest &request, std::vector<const JSONObject *> &data ) const;

    bool Schedule( Event &event );
    void Record( Channel &channel );
    void UpdateEPG( );
    int GetEPGUpdateInterval( ) const { return epg_update_interval; }

    void ProcessAdapters( );
    void AddFrontendToList( const std::string& adapter_name, const std::string& frontend_name, const std::string& uid, const int adapter_id, const int frontend_id);
    void RemoveFrontend( const std::string& uid );

    bool RemoveChannel( int id );

  private:
    TVDaemon( );
    void FindAdapters( );
    void MonitorAdapters( );

    static TVDaemon *instance;
    int epg_update_interval;

    Mutex mutex_frontends;

    Mutex mutex_adapters;
    std::map<int, Adapter *> adapters;
    Mutex mutex_sources;
    std::map<int, Source *>  sources;
    Mutex mutex_channels;
    std::map<int, Channel *> channels;

    bool up;

    // udev
    std::string GetAdapterName( struct udev_device *dev );
    void AddFrontendToList( struct udev_device *dev, const char *path, const int adapter_id=-1, const int frontend_id=-1 );
    void UdevRemove( const char *path );
    struct udev *udev;
    struct udev_monitor *udev_mon;
    int udev_fd;

    HTTPServer *httpd;

    virtual void Run( );
    void HandleUdev( );

    std::vector<Transponder *> epg_transponders;

    Avahi_Client *avahi_client;

    Mutex device_list_mutex;
    struct device_t {
      device_t( const std::string& an, const std::string& fn, const std::string& ui, const int aid, const int fid ) :
        adapter_name(an), frontend_name(fn), uid(ui), adapter_id(aid), frontend_id(fid) { }
      std::string adapter_name;
      std::string frontend_name;
      std::string uid;
      int adapter_id;
      int frontend_id;
    };
    std::list<device_t> device_list;
};

#endif

