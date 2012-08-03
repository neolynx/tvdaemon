/*
 *  transpondereadend
 *
 *  DVB Service class
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

#include "Service.h"

#include <algorithm> // find

#include "Transponder.h"
#include "Stream.h"
#include "Log.h"

Service::Service( Transponder &transponder, uint16_t service_id, uint16_t pid, int config_id ) :
  ConfigObject( transponder, "service", config_id ),
  transponder(transponder),
  service_id(service_id),
  pid(pid)
{
  //Log( "Created Service with pno %d", pno );
}

Service::Service( Transponder &transponder, std::string configfile ) :
  ConfigObject( transponder, configfile ),
  transponder(transponder)
{
}

Service::~Service( )
{
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
  {
    delete it->second;
  }
}

bool Service::SaveConfig( )
{
  Lookup( "ServiceID",    Setting::TypeInt )    = service_id;
  Lookup( "PID",          Setting::TypeInt )    = pid;
  Lookup( "Name",         Setting::TypeString ) = name;
  Lookup( "Provider",     Setting::TypeString ) = provider;

  //DeleteConfig( "Audio-Streams" );
  //Setting &n = Lookup( "Audio-Streams", Setting::TypeArray );
  //for( std::vector<uint16_t>::iterator it = audiostreams.begin( ); it != audiostreams.end( ); it++ )
  //{
    //n.add( Setting::TypeInt ) = *it;
  //}
  //DeleteConfig( "Video-Streams" );
  //Setting &n2 = Lookup( "Video-Streams", Setting::TypeArray );
  //for( std::vector<uint16_t>::iterator it = videostreams.begin( ); it != videostreams.end( ); it++ )
  //{
    //n2.add( Setting::TypeInt ) = *it;
  //}

  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
  {
    it->second->SaveConfig( );
  }

  return WriteConfig( );
}

bool Service::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;

  service_id = (int) Lookup( "ServiceID", Setting::TypeInt );
  pid        = (int) Lookup( "PID", Setting::TypeInt );
  const char *t = Lookup( "Name", Setting::TypeString );
  if( t ) name = t;
  t =             Lookup( "Provider", Setting::TypeString );
  if( t ) provider = t;

  if( !CreateFromConfig<Stream, uint16_t, Service>( *this, "stream", streams ))
    return false;

  //Setting &n = Lookup( "Audio-Streams", Setting::TypeArray );
  //for( int i = 0; i < n.getLength( ); i++ )
  //{
    //audiostreams.push_back((int) n[i] );
    //printf( "  Found configured audio stream %d\n", (int) n[i]);
  //}
  //Setting &n2 = Lookup( "Video-Streams", Setting::TypeArray );
  //for( int i = 0; i < n2.getLength( ); i++ )
  //{
    //videostreams.push_back((int) n2[i] );
    //printf( "  Found configured video stream %d\n", (int) n2[i]);
  //}
  return true;
}

bool Service::UpdateStream( int id, Stream::Type type )
{
  std::map<uint16_t, Stream *>::iterator it = streams.find( id );
  Stream *s;
  if( it == streams.end( ))
  {
    s = new Stream( *this, id, type, streams.size( ));
    streams[id] = s;
  }
  else
  {
    LogWarn( "Already known Stream %d", id );
    s = it->second;
  }

  return s->Update( type );

  //switch( type )
  //{
    //case dtag_mpeg_video_stream:
    //case dtag_mpeg_4_video:
      //if( std::find( videostreams.begin( ), videostreams.end( ), pid ) != videostreams.end( ))
        //printf( "Already known video stream %d\n", pid );
      //else
      //{
        //printf( "Creating video stream %d\n", pid );
        //videostreams.push_back( pid );
      //}
      //break;
    //case dtag_mpeg_audio_stream:
    //case dtag_mpeg_4_audio:
    //case dtag_dvb_ac3:
    //case dtag_dvb_aac_descriptor:
      //if( std::find( audiostreams.begin( ), audiostreams.end( ), pid ) != audiostreams.end( ))
        //printf( "Already known audio stream %d\n", pid );
      //else
      //{
        //printf( "Creating audio stream %d\n", pid );
        //audiostreams.push_back( pid );
      //}
      //break;
    //default:
      //break;
  //}
}

std::map<uint16_t, Stream *> &Service::GetStreams() // FIXME: const
{
  return streams;
}

bool Service::Tune( )
{
  return transponder.Tune( service_id );
}

