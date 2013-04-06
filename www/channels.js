$(document).ready( ready );

var channel_table;

function ready( )
{
  Menu( "Channels" );
  t = ServerSideTable( 'channels', 'tvd?c=tvdaemon&a=get_channels', 20 );
  t["columns"] = {
    "number"    : "#",
    "name"      : "Channel",
    "epg_state" : [ "EPG", epg_state ],
  };
  t["click"] = function( event ) {
    if( confirm( "Start recording " + this["name"] + " ?" ))
      getJSON( 'tvd?c=channel&a=record&channel_id=' + this["id"], record );
  };
  t.load( );
}

function record( data, errmsg )
{
  if( !data )
    return false;
  return true;
}
