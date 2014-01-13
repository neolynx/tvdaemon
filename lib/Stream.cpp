/*
 *  transpondereadend
 *
 *  DVB Stream class
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

#include "Stream.h"

#include "ConfigObject.h"
#include "Service.h"
#include "Log.h"
#include "RingBuffer.h"

#include <RPCObject.h>

Stream::Stream( Service &service, uint16_t id, Type type, int config_id ) :
  service(service),
  id(id),
  type(type),
  up(false)
{
}

Stream::Stream( Service &service ) :
  service(service)
{
}

Stream::~Stream( )
{
  up = false;
  JoinThread( );
}

bool Stream::SaveConfig( ConfigBase &config )
{
  config.WriteConfig( "ID",   id );
  config.WriteConfig( "Type", type );
  return true;
}

bool Stream::LoadConfig( ConfigBase &config )
{
  config.ReadConfig( "ID",   id );
  config.ReadConfig( "Type", (int &) type );
  return true;
}

const char *Stream::GetTypeName( Type type )
{
  switch( type )
  {
    case Type_Video:
      return "Video";
    case Type_Video_H262:
      return "Video H262";

    case Type_Video_H264:
      return "Video MPEG4";

    case Type_Audio:
    case Type_Audio_13818_3:
    case Type_Audio_ADTS:
    case Type_Audio_LATM:
      return "Audio";

    case Type_Audio_AC3:
      return "Audio AC3";

    case Type_Audio_AAC:
      return "Audio AAC";

    default:
      return "Unknown";
  }
}

bool Stream::Update( Type type )
{
  if( type != this->type )
  {
    LogWarn( "Stream %d type changed from %s (%d) to %s (%d)", id, GetTypeName( this->type ), this->type, GetTypeName( type ), type );
    this->type = type;
    return false;
  }
  return true;
}

void Stream::json( json_object *entry ) const
{
  json_object_object_add( entry, "id",        json_object_new_int( GetKey( )));
  json_object_object_add( entry, "pid",       json_object_new_int( id ));
  json_object_object_add( entry, "type",      json_object_new_int( type ));
}

bool Stream::GetSDPDescriptor( std::string &desc )
{
  AVFormatContext *avc;
  AVStream *avs = NULL;
  avc =  avformat_alloc_context( );
  avc->nb_streams = 1;
  av_dict_set( &avc->metadata, "title", "No Title", 0 );
  snprintf( avc->filename, 1024, "rtp://0.0.0.0" );
  avc->streams = (AVStream **) av_malloc( avc->nb_streams * sizeof( *avc->streams ));
  avs = (AVStream *) av_malloc( avc->nb_streams * sizeof( *avs ));
  avc->streams[0] = &avs[0];
  avcodec_register_all( );
  AVCodec *codec = NULL;
  switch( type )
  {
    case Type_Video:
      codec = avcodec_find_decoder( CODEC_ID_MPEG1VIDEO );
      break;
    case Type_Video_H262:
      codec = avcodec_find_decoder( CODEC_ID_MPEG2VIDEO );
      break;
    case Type_Video_H264:
      codec = avcodec_find_decoder( CODEC_ID_H264 );
      break;

    case Type_Audio:
    case Type_Audio_13818_3:
    case Type_Audio_ADTS:
    case Type_Audio_LATM:
    case Type_Audio_AC3:
    case Type_Audio_AAC:
    default:
      LogError( "Stream::GetSDPDescriptor codec not supported: %s", GetTypeName( type ));
      break;
  }
  if( codec )
  {
    avc->streams[0]->codec = avcodec_alloc_context3( codec );
    avc->streams[0]->codec->codec_id = codec->id;
    char *buf = (char *) av_mallocz( 2048 );
    av_sdp_create( &avc, 1, buf, 2048 );
    desc = buf;
    av_free( buf );
  }
  else
  {
    LogError( "Stream::GetSDPDescriptor unhandled codec: %s", GetTypeName( type ));
  }
  av_free( avc->streams );
  av_dict_free( &avc->metadata );
  av_free( avc );
  av_free( avs );
  return codec != NULL;
}

void Stream::HandleData( uint8_t *data, ssize_t len )
{
  ringbuffer->append( data, len );
  cond.Lock( );
  cond.Signal( );
  cond.Unlock( );

}

int Stream::read_packet( void *opaque, uint8_t *buf, int buf_size )
{
  Stream *s = (Stream *) opaque;
  while( s->up && s->ringbuffer->Count( ) < buf_size )
  {
    s->cond.Lock( );
    if( s->cond.Wait( ))
      continue;
    s->cond.Unlock( );
  }
  if( !s->up || s->ringbuffer->Count( ) < buf_size )
    return -1;
  Log( "Stream::read_packet %d", buf_size );
  return s->ringbuffer->read( buf, buf_size ) ? buf_size : -1;
}

int Stream::write_packet( void *opaque, uint8_t *buf, int buf_size )
{
  Stream *s = (Stream *) opaque;
  //if( s->timestamp == 0 )
  //{
    //s->timestamp = s->session->getCurrentTimestamp( );
    //s->session->putData( s->timestamp, buf + 8, buf_size - 8 );
    //s->timestamp = 1;
  //}
  //else
  //{
    //s->timestamp += s->session->getCurrentRTPClockRate();
    //s->session->putData( s->timestamp, buf + 8, buf_size - 8 );
  //}
  Utils::dump( buf, buf_size );
  return 0;
}

#define bsize 3000

bool Stream::Init( ost::RTPSession *session )
{
  timestamp = 0;
  this->session = session;
  ringbuffer = new RingBuffer( 2 * 1024 * 1024 );

  buffer  = (uint8_t *) av_malloc( bsize );
  buffer2 = (uint8_t *) av_malloc( bsize );

  av_register_all( );
  av_log_set_level( AV_LOG_DEBUG );




  ifc = avformat_alloc_context( );
  ictx = avio_alloc_context( buffer, bsize, 0, this, read_packet, NULL, NULL );
  ictx->seekable = 0;
  ictx->write_flag = 0;
  iformat = av_find_input_format( "mpegts" );
  if( !iformat )
  {
    LogError( "mpegts format not found" );
    return false;
  }
  ifc->iformat = iformat;
  ifc->pb = ictx;


  ofc = avformat_alloc_context( );
  octx = avio_alloc_context( buffer2, bsize, 1, this, NULL, write_packet, NULL );
  octx->max_packet_size = 1450; // ?? ehter frame ?
  oformat = av_guess_format( "rtp", NULL, NULL );
  if( !oformat )
  {
    LogError( "rtp format not found" );
    return false;
  }
  ofc->oformat = oformat;
  ofc->pb = octx;

  up = true;
  StartThread( );
  return true;
}

void Stream::Run( )
{
  Log( "Stream::Run av_open_input_stream" );

  if( avformat_open_input( &ifc, NULL, iformat, NULL ) < 0 )
  {
    LogError( "Stream::Run error opening input" );
    return;
  }
  Log( "Stream::Run input opened" );


  AVCodec *codec;
  AVCodecContext *c;

  codec = avcodec_find_decoder( CODEC_ID_MPEG2VIDEO );
  // int avcodec_get_context_defaults3(AVCodecContext *s, AVCodec *codec);
  c = avcodec_alloc_context3( codec );


  if( avcodec_open2( c, codec, NULL ) < 0 )
  {
    LogError( "Stream::Run avcodec_open2 failed" );
    return;
  }
  ///// from avserverc.c
        if (c->bit_rate == 0)
            c->bit_rate = 64000;
        if (c->time_base.num == 0){
            c->time_base.den = 5;
            c->time_base.num = 1;
        }
        if (c->width == 0 || c->height == 0) {
            c->width = 160;
            c->height = 128;
        }
         //Bitrate tolerance is less for streaming
        if (c->bit_rate_tolerance == 0)
            c->bit_rate_tolerance = FFMAX(c->bit_rate / 4,
                      (int64_t)c->bit_rate*c->time_base.num/c->time_base.den);
        if (c->qmin == 0)
            c->qmin = 3;
        if (c->qmax == 0)
            c->qmax = 31;
        if (c->max_qdiff == 0)
            c->max_qdiff = 3;
        c->qcompress = 0.5;
        c->qblur = 0.5;

        if (!c->nsse_weight)
            c->nsse_weight = 8;

        c->frame_skip_cmp = FF_CMP_DCTMAX;
        if (!c->me_method)
            c->me_method = ME_EPZS;
        c->rc_buffer_aggressivity = 1.0;

        if (!c->rc_eq)
            c->rc_eq = "tex^qComp";
        if (!c->i_quant_factor)
            c->i_quant_factor = -0.8;
        if (!c->b_quant_factor)
            c->b_quant_factor = 1.25;
        if (!c->b_quant_offset)
            c->b_quant_offset = 1.25;
        if (!c->rc_max_rate)
            c->rc_max_rate = c->bit_rate * 2;

        if (c->rc_max_rate && !c->rc_buffer_size) {
            c->rc_buffer_size = c->rc_max_rate;
        }
  /////

  //st = avformat_new_stream( ifc, codec );

  //ifc->streams = (AVStream **) av_malloc( sizeof( AVStream * ));
  //ifc->streams[0] = st;

  //ifc->priv_data = c->priv_data;

  //ifc->priv_data = calloc( 1, iformat->priv_data_size ); // FIXME: free
  //ifc->priv_data = av_mallocz( iformat->priv_data_size );

  Log( "Stream::Run ifc             %p", ifc );
  Log( "Stream::Run ifc->iformat    %p", ifc->iformat );
  Log( "Stream::Run ifc->priv_data  %p", ifc->priv_data );
  Log( "Stream::Run ifc->pb         %p", ifc->pb );
  Log( "Stream::Run ifc->streams    %p", ifc->streams );
  if( ifc->streams )
    Log( "Stream::Run ifc->streams[0] %p", ifc->streams[0] );

  if( avformat_find_stream_info( ifc, NULL ) < 0 )
  {
    LogError( "getting stream info failed" );
  }

  av_dump_format( ifc, 0, "", 0 );
  if( ifc->nb_streams != 1 )
  {
    LogError( "stream count %d instead of 1", ifc->nb_streams );
    //goto exit;
  }

  //Log( "Stream::Run avformat_find_stream_info" );
  //Log( "Stream::Run done" );

  //codec = avcodec_find_decoder( CODEC_ID_MPEG2VIDEO );
  //c = avcodec_alloc_context3( codec );
  //c->bits_per_raw_sample = 32;
  //Log( "time base: %d/%d", c->time_base.num, c->time_base.den );
  //Log( "bit rate : %d", c->bit_rate );
  //Log( "bit rate tol: %d", c->bit_rate_tolerance );


    //Duration: N/A, start: 59033.638156, bitrate: 15000 kb/s
     //Stream #0.0[0xd2], 131, 1/90000: Video: mpeg2video (Main), yuv420p, 720x576 [PAR 64:45 DAR 16:9], 1/50, 15000 kb/s, 25.80 fps, 25 tbr, 90k tbn, 50 tbc
     //


  ofc->oformat = oformat;
  ofc->pb = octx;
  ofc->streams = (AVStream **) av_malloc( sizeof( AVStream * ));

  codec = avcodec_find_encoder( CODEC_ID_MPEG2VIDEO );
  c = avcodec_alloc_context3( codec );
  c->pix_fmt = PIX_FMT_YUV420P;
  c->time_base.num = 1;
  c->time_base.den = 50;
  c->width = 720;
  c->height = 576;
  c->bit_rate = 200000;
  c->bit_rate_tolerance = 4000000;

  //c->bit_rate
  //c->sample_fmt
  //c->sample_rate = select_sample_rate(codec);
  //c->channel_layout = select_channel_layout(codec);
  //c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
  if( avcodec_open2( c, codec, NULL ) < 0 )
  {
    LogError( "Stream::Run avcodec_open2 failed" );
    return;
  }

  st = avformat_new_stream( ofc, codec );
  st->codec->codec_id = codec->id;
  st->codec->time_base.num = 1;
  st->codec->time_base.den = 5;
  st->codec->width = 720;
  st->codec->height = 576;
  ofc->priv_data = malloc( oformat->priv_data_size ); // FIXME: free

  Log( "output privdata: %p %d bytes", ofc->priv_data, oformat->priv_data_size );

  Log( "Stream::Run avformat_write_header" );
  if( avformat_write_header( ofc, NULL ) != 0 )
  {
    LogError( "Stream::Run error writing header" );
    return;
  }
  Log( "Stream::Run avformat_write_header done" );

  for(;;)
  {
    AVPacket pkt;
    if( av_read_frame( ifc, &pkt ) < 0 )
    {
      printf( "av_read_frame returned non zero\n" );
      break;
    }

    printf( "got packet size=%d\n", pkt.size );
    //dump( pkt.data, pkt.size );

    pkt.stream_index = 0; // FIXME: needed ? write video stream to our single stream

    if( av_interleaved_write_frame( ofc, &pkt ) < 0 )
    {
      printf( "error writing frame\n" );
    }
  }
exit:
  return;
}

