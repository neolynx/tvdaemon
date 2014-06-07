/*
 *  tvdaemon
 *
 *  Activity_Stream class
 *
 *  Copyright (C) 2013 André Roth
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

#ifndef _Activity_Stream_
#define _Activity_Stream_

#include "Activity.h"
#include "Thread.h"

#include <ccrtp/rtp.h>

class Channel;
class Activity_Record;

class Activity_Stream : public Activity
{
  public:
    Activity_Stream( Channel *channel, ost::RTPSession *session );
    Activity_Stream( Activity_Record *recording, ost::RTPSession *session );
    virtual ~Activity_Stream( );

    virtual std::string GetTitle( ) const;

    virtual void Stop( );

  private:
    virtual bool Perform( );
    virtual void Failed( ) { }

    bool StreamChannel( );
    bool StreamRecording( );

    bool GetTimestamp( const uint8_t *buf, double &ts );

    void UpdateRTPTimestamp( );
    void SendRTP( const uint8_t *data, int length );

    Activity_Record *recording;
    ost::RTPSession *session;
    uint64_t rtp_timestamp;
    double bigbang;
    struct timespec start_time;

    Condition cond;
};


#endif
