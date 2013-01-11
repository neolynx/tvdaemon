$(document).ready( ready );

var channel_table;

function ready( )
{
  channel_table = createSRVTable( 'channels', 'tvd?c=tvdaemon&a=get_channels', 20 );
  channel_table["columns"] = {
    "number"    : [ "#" ],
    "name"      : "Channel",
    ""          : [ "", RecButton ]
  };
  channel_table.load( );
}

function RecButton( )
{
  return "R";
}
