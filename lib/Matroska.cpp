/*
 *  tvdaemon
 *
 *  DVB Matroska class
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

#include "Matroska.h"

#include "TVDaemon.h"
#include "Utils.h"
#include "Log.h"

#include <ebml/EbmlSubHead.h>
//#include "matroska/FileKax.h"
#include "matroska/KaxTracks.h"
//#include "matroska/KaxTrackEntryData.h"
//#include "matroska/KaxTrackAudio.h"
//#include "matroska/KaxTrackVideo.h"
//#include "matroska/KaxClusterData.h"
#include <matroska/KaxInfo.h>
#include <matroska/KaxVersion.h>
//#include "matroska/KaxInfoData.h"
#include "matroska/KaxTags.h"
//#include "matroska/KaxTag.h"
//#include "matroska/KaxChapters.h"
//#include "matroska/KaxContentEncoding.h"

Matroska::Matroska( const std::string& name ) :
  name(name),
  timecode_scale(1000000),   // default scale: 1 milisecond;
  out(NULL),
  cluster(NULL),
  track_count(0),
  curr_seg_size(0),
  cluster_size(0)
{
}

Matroska::~Matroska( )
{
  if( out )
  {
    CloseCluster( );
    WriteSegment( );
    out->close( );
  }
}

void Matroska::WriteSegment( )
{
  dummy.ReplaceWith( seek, *out, false );
  //segment.SetSizeIsFinite( false );
  if( segment.ForceSize( segment_size - segment.HeadSize( ) + curr_seg_size ))
    segment.OverwriteHead( *out );
}

void Matroska::WriteHeader( )
{
  std::string dir; // FIXME
  Utils::EnsureSlash( dir );
  std::string filename = dir + name + ".mkv";

  try
  {
    out = new StdIOCallback( filename.c_str( ), MODE_CREATE );

    GetChildAs<EDocType,            EbmlString>  ( header ) = "matroska";
    GetChildAs<EDocTypeVersion,     EbmlUInteger>( header ) = MATROSKA_VERSION;
    GetChildAs<EDocTypeReadVersion, EbmlUInteger>( header ) = 2;
    header.Render( *out, false );

    segment_size = segment.WriteHead( *out, 5, false );

    dummy.SetSize( 512 );
    curr_seg_size += dummy.Render( *out, false );

    KaxInfo &info = GetChild<KaxInfo>( segment );
    UTFstring muxer;
    muxer.SetUTF8( std::string("libebml ") + EbmlCodeVersion + " & libmatroska " + KaxCodeVersion );
    *((EbmlUnicodeString *) &GetChildAs<KaxMuxingApp,    EbmlUnicodeString>( info )) = muxer;
    *((EbmlUnicodeString *) &GetChildAs<KaxWritingApp,   EbmlUnicodeString>( info )) = L"tvdaemon";
    *((EbmlUnicodeString *) &GetChildAs<KaxTitle,        EbmlUnicodeString>( info )) = L"test mkv";
    GetChildAs<KaxTimecodeScale, EbmlUInteger>( info ) = timecode_scale;

    GetChildAs<KaxDuration, EbmlFloat>( info ) = 0.0;
    GetChild<KaxDateUTC>( info ).SetEpochDate( time( NULL ));

    //TODO: seg uid
    //GetChild<KaxSegmentUID>( info ) = 0x77;

    curr_seg_size += info.Render( *out );

    seek.IndexThis( info, segment );

    //KaxTags &tags = GetChild<KaxTags>( segment );
    //curr_seg_size += tags.Render( *out );
    //seek.IndexThis( tags, segment );

  }
  catch( std::exception &e )
  {
    LogError( "Matroska: %s", e.what( ));
  }
}

void Matroska::AddTrack( )
{
  KaxTracks &tracks = GetChild<KaxTracks>( segment );
  track = &GetChild<KaxTrackEntry>( tracks );

  track->SetGlobalTimecodeScale( timecode_scale );

  GetChildAs<KaxTrackNumber, EbmlUInteger>( *track ) = track_count + 1;

  GetChildAs<KaxTrackUID, EbmlUInteger>( *track ) = track_count + 1;
  //*(static_cast<EbmlUInteger *>(&trackUID)) = random();

  //if (glob->outputTracks->at(i).theTrack)
  //GetTrackInfoCopy(glob, trackInfo, *track, glob->outputTracks->at(i));
  //else
  //GetTrackInfoEncode(glob, *track, glob->outputTracks->at(i));


  GetChildAs<KaxTrackType, EbmlUInteger>( *track ) = 1;
  GetChildAs<KaxCodecID, EbmlString>( *track ) = "V_MPEG2";
  //track.EnableLacing( false );



  KaxTrackVideo &video = GetChild<KaxTrackVideo>( *track );
  GetChildAs<KaxVideoPixelWidth, EbmlUInteger>( video ) = 720;
  GetChildAs<KaxVideoPixelHeight, EbmlUInteger>( video ) = 576;
  GetChildAs<KaxVideoDisplayUnit, EbmlUInteger>( video ) = 3;
  GetChildAs<KaxVideoDisplayWidth, EbmlUInteger>( video ) = 1024;
  GetChildAs<KaxVideoDisplayHeight, EbmlUInteger>( video ) = 576;

  //KaxCodecPrivate &codec = GetChild<KaxCodecPrivate>( track );
  //codec.CopyBuffer((const binary *) extradata, extradata_size );

  curr_seg_size += tracks.Render( *out, false );
  seek.IndexThis( tracks, segment );
}

void Matroska::CloseCluster( )
{
  if( cluster )
  {
    //Log( "Closing Cluster" );
    curr_seg_size += cluster->Render( *out, cues, false );
    cluster->ReleaseFrames( );
    //seek.IndexThis( *cluster, segment);
    delete cluster;
    cluster = NULL;
  }
}

void Matroska::AddCluster( uint64_t ts )
{
  CloseCluster( );
  cluster = new KaxCluster( );
  cluster->SetParent( segment );
  cluster->InitTimecode( ts, timecode_scale );
  cluster->EnableChecksum( );
  cluster_size = 0;
}

void Matroska::AddFrame( uint64_t ts, dvb_mpeg_es_frame_t type, uint8_t *data, size_t size )
{
  if( !track )
    return;
  //Log( "Adding Frame @%ld: %d bytes", ts, size );
  KaxBlockGroup *block;
  DataBuffer *frame = new DataBuffer( (binary *) data, size, NULL, true ); // internal buffer

  if( !cluster )
    AddCluster( 0 );

  //LogWarn( "delta: %ld * %ld - %ld", ts, timecode_scale, cluster->GlobalTimecode( ) );
  int64_t delta = ((int64_t)( ts * timecode_scale ) - (int64_t) cluster->GlobalTimecode( )) / (int64_t) timecode_scale;
  //LogWarn( "delta: %ld", (int64_t) delta );
  // if keyframe: 2000000/4
  if( delta > 32767ll || delta < -32768ll || cluster_size + size > 2000000 / 40 )
  {
    AddCluster( ts );
    delta = 0;
  }
  cluster_size += size;

  KaxSimpleBlock &simpleblock = AddNewChild<KaxSimpleBlock>( *cluster );
  //KaxSimpleBlock *simpleblock = new KaxSimpleBlock( );
  simpleblock.SetParent( *cluster );
  simpleblock.AddFrame( *track, ts * timecode_scale, *frame, LACING_EBML );
  simpleblock.SetKeyframe( type == DVB_MPEG_ES_FRAME_I );
  simpleblock.SetDiscardable( type == DVB_MPEG_ES_FRAME_B );

  //KaxBlockBlob *blob = new KaxBlockBlob( BLOCK_BLOB_ALWAYS_SIMPLE );
  //blob->SetParent( *cluster );
  //blob->AddFrameAuto( *track, ts * timecode_scale, *frame, LACING_EBML );
  ////blob->SetBlockDuration( 50 * timecode_scale );
  //cluster->AddBlockBlob( blob );

  //cues.AddBlockBlob( simpleblock );
  //WriteSegment( );
}

