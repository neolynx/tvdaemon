$(document).ready( ready );

var table;

function ready( )
{
  Menu( "CAM Clients" );
  table = ServerSideTable( 'camclients', 'tvd?c=tvdaemon&a=get_camclients', 20 );
  table["columns"] = {
    "hostname"   : "Hostname",
  };
  table.load( );
  table.filters( [ "search" ] );
}

