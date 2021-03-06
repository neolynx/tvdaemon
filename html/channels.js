$(document).ready( ready );

var channel_table;

function ready( )
{
  Menu( "Channels" );
  t = ServerSideTable( 'channels', 'tvd?c=tvdaemon&a=get_channels', 20 );
  t["columns"] = {
    "number"    : "#",
    "name"      : [ "Channel", print_channel ],
    "epg_state" : [ "EPG", epg_state ],
    "last_epg"  : [ "Last Update", print_time ],
    "u"         : [ "", print_update ],
    "r"         : [ "", print_remove ],
  };
  //t["click"] = function( ) {
    //location.href = 'rtsp://' + window.location.host + '/tvd?c=channel&a=stream&channel_id=' + this["id"];
  //};
  t.load( );
  channel_table = t;
}

function handle_message( data, errmsg )
{
  if( !data )
    return false;
  return true;
}

function print_update( row )
{
  return "<a href=\"javascript: update_epg( " + row["id"] + " );\">update</a>";
}

function print_remove( row )
{
  return "<a href=\"javascript: remove( " + row["id"] + ", '" + row["name"] + "' );\">X</a>";
}

function print_channel( row )
{
  name = encodeURIComponent( row["name"] );
  return "<a href=\"rtsp://" + window.location.host + "/channel/" + name + "\"><b>" + row["name"] + "</b></a>";
}

function update_epg( channel_id )
{
  if( typeof( channel_id ) === 'undefined' )
    getJSON( 'tvd?c=tvdaemon&a=update_epg', handle_message );
  else
    getJSON( 'tvd?c=channel&a=update_epg&channel_id=' + channel_id, handle_message );
}

function remove( id, name )
{
  if( confirm( "Are you sure to remove channel '" + name + "' ?" ))
    getJSON( 'tvd?c=tvdaemon&a=remove_channel&id=' + id, rethandler );
}

function rethandler( data, errmsg )
{
  channel_table.load( );
}
