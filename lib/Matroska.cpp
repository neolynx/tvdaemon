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


Matroska::Matroska( std::string name ) : name(name), out(NULL), cluster(NULL)
{
  track_count = 0;
  timecode_scale = 1000000;  // default scale: 1 milisecond;
  last_block = NULL;
}

Matroska::~Matroska( )
{
  if( out )
  {
    CloseCluster( );
    curr_seg_size += dummy.ReplaceWith( seek, *out, false );
    if( segment.ForceSize( segment_size - segment.HeadSize( ) + curr_seg_size ))
      segment.OverwriteHead( *out );
    out->close( );
    Log( "test.mkv written" );
  }
}

void Matroska::WriteHeader( )
{

  try
  {
    std::string filename = name + ".mkv";
    out = new StdIOCallback( filename.c_str( ), MODE_CREATE );

    GetChildAs<EDocType,            EbmlString>  ( header ) = "matroska";
    GetChildAs<EDocTypeVersion,     EbmlUInteger>( header ) = MATROSKA_VERSION;
    GetChildAs<EDocTypeReadVersion, EbmlUInteger>( header ) = 2;
    header.Render( *out, false );

    segment_size = segment.WriteHead( *out, 5, false );

    dummy.SetSize( 512 );
    dummy.Render( *out, false );

    KaxInfo &info = GetChild<KaxInfo>( segment );
    UTFstring muxer;
    muxer.SetUTF8(std::string("libebml ") + EbmlCodeVersion + " & libmatroska " + KaxCodeVersion);
    *((EbmlUnicodeString *) &GetChildAs<KaxMuxingApp,    EbmlUnicodeString>( info )) = muxer;
    *((EbmlUnicodeString *) &GetChildAs<KaxWritingApp,   EbmlUnicodeString>( info )) = L"tvdaemon";
    *((EbmlUnicodeString *) &GetChildAs<KaxTitle,        EbmlUnicodeString>( info )) = L"test mkv";
    GetChildAs<KaxTimecodeScale, EbmlUInteger>( info ) = timecode_scale;

    GetChildAs<KaxDuration, EbmlFloat>( info ) = 0.0;
    GetChild<KaxDateUTC>( info ).SetEpochDate( time( NULL ));

    //TODO: seg uid
    //GetChild<KaxSegmentUID>( info ) = 0x77;

    curr_seg_size = info.Render( *out );

    seek.IndexThis( info, segment );

    KaxTags &tags = GetChild<KaxTags>( segment );
    curr_seg_size += tags.Render( *out );
    seek.IndexThis( tags, segment );

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

  track->SetGlobalTimecodeScale( 1000000 );

  GetChildAs<KaxTrackNumber, EbmlUInteger>( *track ) = track_count + 1;

  GetChildAs<KaxTrackUID, EbmlUInteger>( *track ) = track_count + 1;
  //*(static_cast<EbmlUInteger *>(&trackUID)) = random();

  //if (glob->outputTracks->at(i).theTrack)
  //GetTrackInfoCopy(glob, trackInfo, *track, glob->outputTracks->at(i));
  //else
  //GetTrackInfoEncode(glob, *track, glob->outputTracks->at(i));
  //}


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
    curr_seg_size += cluster->Render( *out, cues, false );
    cluster->ReleaseFrames( );
    seek.IndexThis( *cluster, segment);
    delete cluster;
    cluster = NULL;
  }
}

void Matroska::AddCluster( uint64_t ts )
{
  CloseCluster( );
  cluster = new KaxCluster( );
  cluster->SetParent( segment );
  double t = (double) ts;
  t *= (double) timecode_scale;
  //t /= 9000.0;
  //Log( "ts: %lld -> %f %lld", ts, t, (uint64_t) t );
  //static uint64_t iii = 0;
  cluster->SetPreviousTimecode( (uint64_t)t, timecode_scale );
  //iii += timecode_scale;
  cluster->EnableChecksum( );
}

void Matroska::AddFrame( uint8_t *data, size_t size )
{
  //ComponentResult err = noErr;
  //Media theMedia;
  //int trackNum = 0;
  //TimeValue64 decodeTime = glob->outputTracks->at(0).currentTime;
  //TimeValue64 decodeDuration, displayOffset;
  //MediaSampleFlags sampleFlags;
  //ByteCount sampleDataSize;

  //// find the track with the next earliest sample
  //for (int i = 1; i < glob->outputTracks->size(); i++) {
  //if (glob->outputTracks->at(i).currentTime < decodeTime &&
  //glob->outputTracks->at(i).currentTime >= 0) {

  //trackNum = i;
  //decodeTime = glob->outputTracks->at(i).currentTime;
  //}
  //}

  //theMedia = GetTrackMedia(glob->outputTracks->at(trackNum).theTrack);

  //// update the track's current time
  //GetMediaNextInterestingDecodeTime(theMedia, nextTimeMediaSample, decodeTime, fixed1,
  //&glob->outputTracks->at(trackNum).currentTime, NULL);

  //// get the sample size
  //err = GetMediaSample2(theMedia, NULL, 0, &sampleDataSize, decodeTime,
  //NULL, NULL, NULL, NULL, NULL, 1, NULL, NULL);
  //if (err) return err;

  //// make sure our buffer is large enough
  //if (sampleDataSize > GetPtrSize(glob->sampleBuffer))
  //SetPtrSize(glob->sampleBuffer, sampleDataSize);

  //// and actually fetch the sample
  //err = GetMediaSample2(theMedia, (UInt8 *) glob->sampleBuffer, GetPtrSize(glob->sampleBuffer),
  //NULL, decodeTime, NULL, &decodeDuration, &displayOffset,
  //NULL, NULL, 1, NULL, &sampleFlags);

  //DataBuffer data((binary *)glob->sampleBuffer, sampleDataSize);

  //// TODO: import with references to correct frames
  uint64_t ts = 0;
  KaxBlockGroup *block;
  DataBuffer frame( (binary *) data, size );
  cluster->AddFrame( *track, ts, frame, block );


  //return err;
}

