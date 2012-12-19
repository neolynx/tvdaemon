$(document).ready( ready );

window.setInterval( function( ) { ready( ); }, 2000 );

var sources = [];

function ready( )
{
  $.getJSON('tvd?c=tvdaemon&a=list_sources', readSources );
}

function readSources( data )
{
  for( s in data["data"] )
  {
    sources.push( data["data"][s]["name"] );
  }
  $.getJSON('tvd?c=tvdaemon&a=list_devices', readDevices );
}

function readDevices( data )
{
  $( '#plant' ).empty( );
  for( a in data )
  {
    adapter = data[a];
    row = $('<tr>');
    cell = $('<td>');
    cell.html( getAdapter( adapter ));
    row.append( cell );

    adapterports = 0;
    for( f in adapter["frontends"] )
    {
      frontend = adapter["frontends"][f];

      cell2 = $('<td>');
      cell2.html( getFrontend( frontend ));
      row.append( cell2 );

      ports = Object.keys( frontend["ports"] ).length;
      adapterports += ports;
      cell2.attr( "rowspan", ports );

      for( p in frontend["ports"] )
      {
        port = frontend["ports"][p];

        cell3 = $('<td>');
        cell3.html( getPort( port ));
        row.append( cell3 );

        cell4 = $('<td>');
        cell4.html( getSource( port["source_id"] ));
        row.append( cell4 );

        row.appendTo( '#plant' );
        row = $('<tr>');
      }

    }
    cell.attr( "rowspan", adapterports );
  }
}

function getAdapter( adapter )
{
  if( adapter["present"] == 1 )
    imgstyle = "";
  else
    imgstyle = "opacity:0.2";
  if( adapter["path"].indexOf( "/usb" ) != -1 )
    icon = "<img src=\"images/usb-icon.png\" style=\"" + imgstyle + "\"/>";
  else if ( adapter["path"].indexOf( "/pci" ) != -1 )
    icon = "<img src=\"images/pci-icon.png\" style=\"" + imgstyle + "\"/>";
  return icon + '<br/>' + adapter["name"];
}

function getFrontend( frontend )
{
  return frontend["name"];
}

function getPort( port )
{
  return port["name"];
}

function getSource( source_id )
{
  if( source_id >= 0 )
    return sources[source_id];
  else
    return " - ";
}

