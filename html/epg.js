$(document).ready( ready );

function ready( )
{
  Menu( "EPG" );
  t = ServerSideTable( 'epg', 'tvd?c=tvdaemon&a=get_epg', 10 );
  t["columns"] = {
    "start"    : [ "Start", print_time ],
    "name"     : [ "Event", print_event ],
    "channel"  :   "Channel",
  };
  t["click"] = function( ) {
    if( confirm( "Record " + this["name"] + " ?" ))
      getJSON( 'tvd?c=channel&a=schedule&channel_id=' + this["channel_id"] + "&event_id=" + this["id"], schedule );
  };
  t.filters( [ "search" ] );
  t.load( );
}

function schedule( data, errmsg )
{
  if( !data )
    return false;
  return true;
}

function duration( row )
{
  d = row["duration"] / 60;
  return Math.floor( d ) + "'";
}

function print_event( row )
{
  return "<b>" + row["name"] + "</b>&nbsp;" + duration( row ) + "<br/>" + row["description"];
}
