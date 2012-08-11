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

class TVDaemon : public ConfigObject, public HTTPDynamicHandler
{
  public:
    TVDaemon( std::string configdir );
    virtual ~TVDaemon( );

    bool Start( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    enum SourceType
    {
      Source_ANY    = 0xff,
      Source_DVB_S  = 0,
      Source_DVB_C  = 1,
      Source_DVB_T  = 2,
      Source_ATSC   = 3,
    };

    std::vector<std::string> GetScanfileList( SourceType type = Source_ANY, std::string country = "" );
    std::vector<std::string> GetAdapterList( );
    Adapter *GetAdapter( int id );

    std::vector<std::string> GetSourceList( );
    Source *GetSource( int id ) const;
    Source *CreateSource( std::string name, std::string scanfile = "" );

    Channel *CreateChannel( std::string name );
    std::vector<std::string> GetChannelList( );
    Channel *GetChannel( int id );

    virtual bool HandleDynamicHTTP( const int client, const std::map<std::string, std::string> &parameters );

    bool RPC_Source( const int client, const std::map<std::string, std::string> &parameters );
    bool RPC_Transponder( const int client, const std::map<std::string, std::string> &parameters );

  private:
    int FindAdapters( );
    void MonitorAdapters( );

    std::vector<Adapter *> adapters;
    std::vector<Source *>  sources;
    std::vector<Channel *> channels;

    bool up;

    // udev
    Adapter *UdevAdd( struct udev_device *dev, const char *path );
    void UdevRemove( const char *path );
    struct udev *udev;
    struct udev_monitor *udev_mon;
    int udev_fd;
    pthread_t thread_udev;
    static void *run_udev( void *ptr );
    void Thread_udev( );

    HTTPServer *httpd;
};

#endif
