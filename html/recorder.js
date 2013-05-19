$(document).ready( ready );

function ready( )
{
  t = ServerSideTable( 'recorder', 'tvd?c=tvdaemon&a=get_recordings', 10 );
  t["columns"] = {
    "start"    : [ "Start", print_time ],
    "channel"  :   "Channel",
    "name"     : [ "Name", function ( row ) { return "<b>" + row["name"] + "</b>"; } ],
    "state"    : [ "State", recording_state ],
  };
  t.load( );
  Menu( "Recorder" );
}

function schedule( data, errmsg )
{
  if( !data )
    return false;
  return true;
}

function duration( event )
{
  d = event["duration"] / 60;
  return Math.floor( d ) + "'";
}
