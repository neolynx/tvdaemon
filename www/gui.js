$(document).ready( ready );

window.setInterval( function( ) { update( ); }, 2000 );

var sources = [];
var adapters = [];
var no_update = false;

function getJSON( url, callback )
{
  $.ajax( {
    url: url,
    dataType: 'json',
    success: callback,
    timeout: 3000,
    error: function(jqXHR, status, errorThrown ) { callback( null ); }
  });
}

function ready( )
{
  theme = "";
  $('#popup_port').jqxWindow({ width: "auto", resizable: false, theme: theme, isModal: true, autoOpen: false, cancelButton: $("#port_cancel"), modalOpacity: 0.30 });
  $('#popup_port').bind( 'closed', function ( event ) { no_update = false; } );
  $('#port_ok').bind( 'click', function ( event ) { savePort( ); } );
  update( );
}

function update( )
{
  if( no_update )
    return;
  getJSON('tvd?c=tvdaemon&a=list_sources', readSources );
}

function readSources( data )
{
  if( data )
  {
    for( s in data["data"] )
    {
      sources.push( data["data"][s]["name"] );
    }
  }
  getJSON('tvd?c=tvdaemon&a=list_devices', readDevices );
}

function readDevices( data )
{
  $( '#config' ).empty( );
  if( !data ) return;
  adapters = data;
  for( a in adapters )
  {
    adapter = data[a];
    row = $('<tr>');
    cell = $('<td>');
    cell.html( getAdapter( adapter ));
    cell.attr( "class", "adapter" );
    row.append( cell );

    adapterports = 0;

    for( f in adapter["frontends"] )
    {
      frontend = adapter["frontends"][f];
      frontend["adapter_id"] = adapter["id"];

      cell2 = $('<td>');
      cell2.html( getFrontend( frontend ));
      cell2.attr( "class", "frontend" );
      row.append( cell2 );

      ports = Object.keys( frontend["ports"] ).length;
      adapterports += ports;
      cell2.attr( "rowspan", ports );

      for( p in frontend["ports"] )
      {
        port = frontend["ports"][p];
        port["frontend_id"] = frontend["id"];
        port["adapter_id"] = adapter["id"];

        cell3 = $('<td>');
        cell3.html( getPort( port ));
        cell3.attr( "class", "port" );
        row.append( cell3 );

        cell4 = $('<td>');
        cell4.html( getSource( port["source_id"] ));
        cell4.attr( "class", "source" );
        row.append( cell4 );


        row.appendTo( '#config' );
        row = $('<tr>');
      }

    }
    cell.attr( "rowspan", adapterports );
  }

  // end row
  row = $('<tr>');
  cell = $('<td>');
  cell.html( "<div id=\"button1\"> <a href=\"#\" class=\"button\">Add Tuner </a></div>" );
  row.append( cell );

  cell2 = $('<td>');
  row.append( cell2 );

  cell3 = $('<td>');
  cell3.html( "<div id=\"return port\"> <a href=\"\"class=\"button\">Add Port</a></div>" );
  row.append( cell3 );

  cell4 = $('<td>');
  cell4.html( "<div id=\"return sat\"> <a href=\"\"class=\"button\">Add Source</a></div>" );
  row.append( cell4 );

  row.appendTo( '#config' );
}

function getAdapter( adapter )
{
  imgstyle = "";
  if( adapter["present"] != 1 )
    imgstyle = "alt=\"not present\" class=\"grey\";";
  if( adapter["path"].indexOf( "/usb" ) != -1 )
    icon = "<img src=\"images/usb-icon.png\" style=\"" + imgstyle + "\"/>";
  else if ( adapter["path"].indexOf( "/pci" ) != -1 )
    icon = "<img src=\"images/pci-icon.png\" style=\"" + imgstyle + "\"/>";
  return icon + '<br/>' + adapter["name"];
}

function getFrontend( frontend )
{
  return "Type: " + frontend["type"] + " " + frontend["name"] + "<br/><a href=\"\"class=\"button\">Add port</a>";
}

function getPort( port )
{
  frontend = adapters[port["adapter_id"]]["frontends"][port["frontend_id"]];
  if( frontend["type"] == 0 ) // Sat
    return "<a href=\"javascript: editPort( " + port["adapter_id"] + ", " + port["frontend_id"] + ",  " + port["id"] + " );\">" + port["name"] + "</a>";
  return "";
}

function getSource( source_id )
{
  if( source_id >= 0 )
    return sources[source_id];
  else
    return " - ";
}

function editPort( adapter_id, frontend_id, port_id )
{
  no_update = true;
  $("#popup_port").jqxWindow('show');
  port = adapters[adapter_id]["frontends"][frontend_id]["ports"][port_id];
  $("#port_name").focus( );
  $("#port_name").val( port["name"] );
  $("#port_num").val( port["port_num"] );
  $("#adapter_id").val( adapter_id );
  $("#frontend_id").val( frontend_id );
  $("#port_id").val( port_id );

}

function savePort( )
{
  $.ajax( {
            type: 'POST',
            cache: false,
            url: 'tvd?c=port&a=set',
            data: $('#port_form').serialize( ),
            success: function(msg) { $("#popup_port").jqxWindow( 'hide' ); },
            error: function( jqXHR, status, errorThrown ) { alert( jqXHR.responseText ); }
          } );
}
