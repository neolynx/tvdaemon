$(document).ready( ready );

var sources = [];
var service_types = [];

function readServiceTypes( data, errmsg )
{
  if( !data )
    return;
  service_types = [];
  for( s in data )
    service_types[s] = data[s];
}

var service_table;

function ready( )
{
  getJSON( 'tvd?c=tvdaemon&a=get_service_types', readServiceTypes );
  service_table = createSRVTable( 'services', 'tvd?c=tvdaemon&a=get_services', 20 );
  service_table["columns"] = {
    "channel"   : [ "", function( service ) { if( service["channel"] == 1 ) return "C"; } ],
    "scrambled" : [ "", function( service ) { if( service["scrambled"] == 1 ) return "&#9911;"; } ],
    "name"      : "Service",
    "provider"  : "Provider",
    "type"      : [ "Type", function( service ) { return service_types[service['type']]; } ],
    "id"        : [ "PID", SRVT_NUMERIC ]
  };
  service_table["click"] = function( event ) {
    getJSON( 'tvd?c=service&a=add_channel&source_id=' + this["source_id"] +
        '&transponder_id=' + this["transponder_id"] + '&service_id=' + this["id"], addChannel );
  };
  service_table.load( );
}

function addChannel( data, errmsg )
{
  service_table.load( );
}

