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
#include <libudev.h>
#include <pthread.h>

#define SCANFILE_DIR "/usr/share/dvb/"

#define HTTPDPORT    7777

class Adapter;
class Source;
class Channel;
class HTTPServer;

class TVDaemon : public ConfigObject, public HTTPDynamicHandler, public ThreadBase
{
  public:
    static TVDaemon *Instance( );
    bool Create( std::string configdir );
    virtual ~TVDaemon( );

    bool Start( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    std::vector<std::string> GetAdapterList( );
    Adapter *GetAdapter( int id );

    Source *CreateSource( std::string name, Source::Type type, std::string scanfile = "" );
    std::vector<std::string> GetSourceList( );
    Source *GetSource( int id ) const;

    Channel *CreateChannel( std::string name );
    std::vector<std::string> GetChannelList( );
    Channel *GetChannel( int id );

    std::string GetDir( ) { return dir; }

    // RPC
    virtual bool HandleDynamicHTTP( const HTTPRequest &request );
    bool RPC        ( const HTTPRequest &request, const std::string &cat, const std::string &action );
    bool RPC_Source ( const HTTPRequest &request, const std::string &cat, const std::string &action );
    bool RPC_Adapter( const HTTPRequest &request, const std::string &cat, const std::string &action );

  private:
    TVDaemon( );
    int FindAdapters( );
    void MonitorAdapters( );

    static TVDaemon *instance;
    std::vector<Adapter *> adapters;
    std::vector<Source *>  sources;
    std::vector<Channel *> channels;

    bool up;
    std::string dir;

    // udev
    Adapter *UdevAdd( struct udev_device *dev, const char *path );
    void UdevRemove( const char *path );
    struct udev *udev;
    struct udev_monitor *udev_mon;
    int udev_fd;

    HTTPServer *httpd;

    Thread *thread_udev;
    void Thread_udev( );

};

#endif
