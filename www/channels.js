$(document).ready( ready );

var channel_table;

function ready( )
{
  channel_table = ServerSideTable( 'channels', 'tvd?c=tvdaemon&a=get_channels', 20 );
  channel_table["columns"] = {
    "number"    : "#",
    "name"      : "Channel",
  };
  channel_table.load( );
}

