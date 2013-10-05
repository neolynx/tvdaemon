/*
 *  tvdaemon
 *
 *  DVB StreamingHandler class
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

#ifndef _StreamingHandler_
#define _StreamingHandler_

#include <string>

#include <ccrtp/rtp.h> // FIXME: make interfase/impl

class Channel;
class Activity_Stream;

class StreamingHandler
{
  public:
    StreamingHandler( Channel *channel );
    virtual ~StreamingHandler( );

    void Setup( std::string server, std::string client, int port );
    bool Play( std::string url );

  private:
    Channel *channel;
    Activity_Stream *activity;
    std::string server;
    std::string client;
    int         port;

    ost::RTPSession *socket;
};

#endif
