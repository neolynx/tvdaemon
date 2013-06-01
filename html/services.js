$(document).ready( ready );

var sources = [];
var service_types = [];

function readServiceTypes( data, errmsg )
{
  if( !data )
    return false;
  service_types = [];
  for( s in data )
    service_types[s] = data[s];
  return true;
}

var service_table;

function ready( )
{
  Menu( "Services" );
  getJSON( 'tvd?c=tvdaemon&a=get_service_types', readServiceTypes );
  t = ServerSideTable( 'services', 'tvd?c=tvdaemon&a=get_services', 20 );
  t["columns"] = {
    "channel"   : [ "", function( service ) { if( service["channel"] == 1 ) return "C"; } ],
    "scrambled" : [ "", function( service ) { if( service["scrambled"] == 1 ) return "&#9911;"; } ],
    "name"      : "Service",
    "provider"  : "Provider",
    "type"      : [ "Type", function( service ) { return service_types[service['type']]; } ],
    "id"        : [ "PID", SST_NUMERIC ]
  };
  t["click"] = function( row ) {
    getJSON( 'tvd?c=service&a=add_channel&source_id=' + this["source_id"] +
        '&transponder_id=' + this["transponder_id"] + '&service_id=' + this["id"], addChannel );
  };
  t.filters( [ "search" ] );
  t.load( );
  service_table = t;
}

function addChannel( data, errmsg )
{
  service_table.load( );
}

