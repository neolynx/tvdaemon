$(document).ready( ready );

function ready( )
{
  Menu( "Channels" );
  t = ServerSideTable( 'channels', 'tvd?c=tvdaemon&a=get_channels', 20 );
  t["columns"] = {
    "number"    : "#",
    "name"      : "Channel",
    "epg_state" : [ "EPG", epg_state ],
    "last_epg" : [ "Last Update", print_time ],
    "" : [ "", print_update ],
  };
  t["click"] = function( ) {
    location.href = 'rtsp://' + window.location.host + '/tvd?c=channel&a=stream&channel_id=' + this["id"];
  };
  t.load( );
}

function record( data, errmsg )
{
  if( !data )
    return false;
  return true;
}

function print_update( row )
{
  return "<a href=\"javascript: update_epg( " + row["id"] + " );\">update</a>";
}

function update_epg( channel_id )
{
  getJSON( 'tvd?c=channel&a=update_epg&channel_id=' + channel_id, record );
}

