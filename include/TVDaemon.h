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
#include <queue>
#include <libudev.h>

#define SCANFILE_DIR "/usr/share/dvb/"

#define HTTPDPORT    7777

class Adapter;
class Source;
class Channel;
class Recorder;
class Event;
class StreamingHandler;
class Server_Avahi;

class TVDaemon : public ConfigObject, public HTTPDynamicHandler, public RTSPHandler, public Thread
{
  public:
    static TVDaemon *Instance( );
    bool Create( std::string configdir );
    virtual ~TVDaemon( );

    bool Start( const char* httpRoot );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    std::vector<std::string> GetAdapterList( );
    Adapter *GetAdapter( int id );

    Source *CreateSource( std::string name, Source::Type type, std::string scanfile = "" );
    std::vector<std::string> GetSourceList( );
    Source *GetSource( int id ) const;

    std::vector<std::string> GetChannelList( );
    Channel *GetChannel( int id );
    Channel *CreateChannel( Service *service );

    void LockFrontends( );
    void UnlockFrontends( );
    void LockChannels( );
    void UnlockChannels( );

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

    // RTSP
    virtual StreamingHandler *GetStreamingHandler( std::string url );


  private:
    TVDaemon( );
    int FindAdapters( );
    void MonitorAdapters( );

    static TVDaemon *instance;
    int epg_update_interval;

    std::vector<Adapter *> adapters;
    std::map<int, Source *>  sources;
    std::vector<Channel *> channels;

    bool up;

    // udev
    Adapter *UdevAdd( struct udev_device *dev, const char *path );
    void UdevRemove( const char *path );
    struct udev *udev;
    struct udev_monitor *udev_mon;
    int udev_fd;

    // non-udev
    Adapter *NonUdevAdd( int adapter_id, int frontend_id );

    HTTPServer *httpd;

    virtual void Run( );
    void HandleUdev( );

    Recorder *recorder;

    std::vector<Transponder *> epg_transponders;

    std::map<Channel *, StreamingHandler *> streaming_handlers;

    Mutex frontends_mutex;
    Mutex channels_mutex;

    Server_Avahi* srv_avahi;
};

#endif

