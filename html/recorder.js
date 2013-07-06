$(document).ready( ready );

var epg_table;

function ready( )
{
  t = ServerSideTable( 'recorder', 'tvd?c=tvdaemon&a=get_recordings', 10 );
  t["columns"] = {
    "start"    : [ "Start", print_time ],
    "channel"  :   "Channel",
    "name"     : [ "Name", function ( row ) { return "<b>" + row["name"] + "</b>"; } ],
    "state"    : [ "State", recording_state ],
    ""         : [ "", print_remove ],
  };
  t.load( );
  epg_table = t;
  Menu( "Recorder" );
}

function duration( row )
{
  d = row["duration"] / 60;
  return Math.floor( d ) + "'";
}

function print_remove( row )
{
  return "<a href=\"javascript: remove( " + row["id"] + ", '" + row["name"] + "' );\">X</a>";
}

function remove( id, name )
{
  if( confirm( "Are you sure to remove '" + name + "' ?\nRecorded files will not be deleted." ))
    getJSON( 'tvd?c=recorder&a=remove&id=' + id, rethandler );
}

function rethandler( data, errmsg )
{
  epg_table.load( );
}
