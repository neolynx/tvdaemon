$(document).ready( ready );

//window.setInterval( function( ) { update( ); }, 2000 );

var sources = {};
var adapters = {};
var no_update = false;
var dialog;

var adapter_menu = [
{
  name: 'delete Adapter',
    //        img: 'images/delete.png',
    title: 'delete adapter',
    fun: function () {
      delete_adapter( this );
    }
}];

var frontend_menu = [
{
  name: 'add Port',
    //        img: 'images/delete.png',
    title: 'add a Port to this frontend',
    fun: function () {
      console.log( "add port", this );
      dialog = $( "#port-form" ).dialog( {
        title: "Add Port",
        autoOpen: false,
        height: 300,
        width: 350,
        modal: true,
        buttons: {
          "Add": editPort,
          Cancel: function() { dialog.dialog( "close" ); }
        },
      });
      adapter = adapters[this.adapter];
      frontend = adapter.frontends[this.frontend];
      port_form( adapter, frontend );
      dialog.dialog( "open" );
    }
}];

var port_menu = [
  { name: 'edit Port',
    //        img: 'images/delete.png',
    title: 'edit this port',
    fun: function () {
      console.log( "edit port", this );
      dialog = $( "#port-form" ).dialog( {
        title: "Edit Port",
        autoOpen: false,
        height: 300,
        width: 350,
        modal: true,
        buttons: {
          "Edit": editPort,
          Cancel: function() { dialog.dialog( "close" ); }
        },
      });
      adapter = adapters[this.adapter];
      frontend = adapter.frontends[this.frontend];
      port = frontend.ports[this.port];
      port_form( adapter, frontend, port );
      dialog.dialog( "open" );
    }
  },
  { name: 'delete Port',
    //        img: 'images/delete.png',
    title: 'delete this port',
    fun: function () {
      console.log( "delete port", this );
    }
  },
];

var source_menu = [
{
  name: 'delete',
    //        img: 'images/delete.png',
    title: 'delete this source',
    fun: function () {
      delete_source( this );
    }
}];

function ready( )
{
  theme = "";
  $('#setup').attr( 'class', 'setup' );

  Menu( "Setup" );
  update( );
}

function update( )
{
  if( no_update )
    return;
  getJSON('tvd?c=tvdaemon&a=get_sources', readSources );
}

function readSources( data, errmsg )
{
  if( data )
  {
    sources = [];
    for( s in data["data"] )
    {
      sources.push( data["data"][s] );
    }
  }
  getJSON('tvd?c=tvdaemon&a=get_devices', readDevices );
}

function readDevices( data, errmsg )
{
  $( '#setup' ).empty( );
  if( !data ) return;
  table = $('<table>');
  table.attr( 'class', 'setup' );
  adapters = data;
  for( a in adapters )
  {
    adapter = data[a];
    row = $('<tr>');
    cell = $('<td>');
    cell.append( getAdapter( adapter ));
    cell.append( "<br/>" + adapter["name"] );
    if( a != adapters.length - 1 )
      cell.attr( "class", "adapter" );
    else
      cell.attr( "class", "adapter_last" );
    row.append( cell );

    adapterports = 0;

    frontends = adapter["frontends"]
      for( f in frontends )
      {
        frontend = frontends[f];
        frontend["adapter_id"] = adapter["id"];

        cell2 = $('<td>');
        cell2.append( getFrontend( frontend ));
        if( f != frontends.length - 1 || a != adapters.length - 1 )
          cell2.attr( "class", "frontend" );
        else
          cell2.attr( "class", "frontend_last" );
        //cell2.bind( "click", function( ) { alert ( 'bla' ); } );
        row.append( cell2 );

        ports = Object.keys( frontend["ports"] ).length;
        adapterports += ports;
        cell2.attr( "rowspan", ports );

        ports = frontend["ports"];
        for( p in ports )
        {
          port = ports[p];
          port["frontend_id"] = frontend["id"];
          port["adapter_id"] = adapter["id"];

          cell3 = $('<td>');
          cell3.append( getPort( port ));
          if( p != ports.length - 1 || a != adapters.length - 1 || f != frontends.length -1 )
            cell3.attr( "class", "port" );
          else
            cell3.attr( "class", "port_last" );
          row.append( cell3 );

          cell4 = $('<td>');
          cell4.html( getSource( port ));
          if( p != ports.length - 1 || a != adapters.length - 1 || f != frontends.length -1 )
            cell4.attr( "class", "source" );
          else
            cell4.attr( "class", "source_last" );
          row.append( cell4 );


          row.appendTo( table );
          row = $('<tr>');
        }

      }
    cell.attr( "rowspan", adapterports );
  }

  // end row
  row = $('<tr>');
  cell = $('<td>');
  cell.html( "<div id=\"button1\"> <a href=\"javascript: scan( );\" class=\"button\">Scan</a></div>" );
  row.append( cell );

  cell2 = $('<td>');
  row.append( cell2 );

  cell3 = $('<td>');
  row.append( cell3 );

  cell4 = $('<td>');
  row.append( cell4 );

  row.appendTo( table );
  table.appendTo( '#setup' );

}

function getAdapter( adapter )
{
  icon = "images/pci-icon.png"; // FIXME: used unknown icon
  if( adapter["path"].indexOf( "/usb" ) != -1 )
    icon = "images/usb-icon.png";
  else if ( adapter["path"].indexOf( "/pci" ) != -1 )
    icon = "images/pci-icon.png";

  img = $('<img />', {
    id:  "adapter" + adapter["id"],
      src: icon,
      alt: adapter["name"],
  });
  img.css( 'cursor', 'pointer' );
  if( adapter["present"] == 1 )
    img.attr('class', "icon");
  else
    img.attr('class', "icon_gray");

  img.contextMenu(adapter_menu, { userData: adapter["id"] });
  return img;
}

function getFrontend( frontend )
{
  adapter = adapters[frontend["adapter_id"]];
  span = $('<span />');
  span.css( 'cursor', 'pointer' );
  span.append( frontend["name"] );
  span.contextMenu(frontend_menu, { userData: { adapter: adapter["id"], frontend: frontend["id"] }});
  return span;
}

function getPort( port )
{
  icon = "images/antenna-oldstyle.png"; // FIXME: used unknown icon
  if( frontend["type"] == 0 )
    icon = "images/antenna-icon.png";
  else if( frontend["type"] == 1 )
    icon = "images/antenna-cable.smal.png";

  adapter = adapters[port["adapter_id"]];
  if( !adapter )
  {
    console.log( "adapter " + port["adapter_id"] + " not found for port " + port["name"] + "("+ port["id"] + ")" );
    return;
  }
  frontend = adapter["frontends"][port["frontend_id"]];

  img = $('<img />', {
    src: icon,
      alt: adapter["name"],
  });
  img.css( 'cursor', 'pointer' );

  if( adapter["present"] == 1 )
    img.attr('class', "icon");
  else
    img.attr('class', "icon_gray");

  img.contextMenu(port_menu, { userData: { adapter: adapter["id"], frontend: frontend["id"], port: port["id"] }});
  return img;
}

function getSource( port )
{
  source_id = port["source_id"];
  span = $('<span />');
  if( source_id >= 0 )
  {
    source = $('<b/>');
    source.css( 'cursor', 'pointer' );
    source.html( sources[source_id]["name"] );
    source.contextMenu(source_menu, { userData: { adapter: adapter["id"], frontend: frontend["id"], port: port["id"], source: source_id }});
    span.append( source );
    span.append( "<br/>Transponders: " + sources[source_id]["transponders"] +
        "<br/>Services: " + sources[source_id]["services"] );
    adapter = adapters[port["adapter_id"]];
    frontend = adapter["frontends"][port["frontend_id"]];
  }
  else
  {
    span.css( 'cursor', 'pointer' );
    span.html( "Undefined" );
    span.contextMenu(source_menu, { userData: { adapter: adapter["id"], frontend: frontend["id"], port: port["id"] }});
  }
  return span;
}

function delete_adapter( adapter_id )
{
  if( confirm( "Delete adapter ?" ))
    getJSON('tvd?c=tvdaemon&a=delete_adapter&id=' + adapter_id, reload );
}

function delete_source( source_id )
{
  if( confirm( "Delete source ?" ))
    getJSON('tvd?c=tvdaemon&a=delete_source&id=' + source_id, reload );
}

function reload( data, errmsg )
{
  update( );
}

function editPort( adapter_id, frontend_id, port_id )
{
  no_update = true;
  frontend = adapters[adapter_id]["frontends"][frontend_id];
  port = frontend["ports"][port_id];
  $("#port_name").focus( );
  $("#port_name").val( port["name"] );
  $("#port_num").val( port["port_num"] );
  $("#port_adapter_id").val( adapter_id );
  $("#port_frontend_id").val( frontend_id );
  $("#port_port_id").val( port_id );
  $("#port_type").val( frontend["type"] );

  getJSON('tvd?c=tvdaemon&a=get_sources', editPortGetSources );
}

function editPortGetSources( data, errmsg )
{
  type = $("#port_type").val( );
  $("#port_source").empty( );
  $("#port_source").append( new Option( "Undefined", "-1" ));
  sources = [];
  for( s in data["data"] )
  {
    source = data["data"][s];
    if( source["type"] == type )
      $("#port_source").append( new Option( source["name"], source["id"] ));
    sources.push( source );
  }
  $("#port_source").val( port["source_id"] ).attr( 'selected', true );
  $("#port_source").append( new Option( "create new ...", "-2" ));
}

function savePort( )
{
  if( $("#port_port_id").val( ))
    u = 'tvd?c=port&a=set';
  else
    u = 'tvd?c=frontend&a=create_port';
  $.ajax( {
    type: 'POST',
    cache: false,
    url: u,
    data: $('#port_form').serialize( ),
    //success: function(msg) { $("#port_popup").jqxWindow( 'hide' ); },
    error: function( jqXHR, status, errorThrown ) { alert( 'Error: ' + jqXHR.responseText ); }
  } );
}

function editSource( adapter_id, frontend_id, port_id )
{
  no_update = true;
  adapter_id = $("#port_adapter_id").val( );
  frontend_id = $("#port_frontend_id").val( );
  frontend = adapters[adapter_id]["frontends"][frontend_id];
  $("#source_type").val( frontend["type"] );
  $("#source_name").val( "" );

  getJSON( 'tvd?c=tvdaemon&a=get_scanfiles&type=' + frontend["type"], loadScanfiles );
}

function loadScanfiles( scanfiles, errmsg )
{
  if( !scanfiles )
  {
    alert( errmsg );
    return;
  }

  $("#source_scanfile").empty( );
  $("#source_scanfile").append( new Option( "select ...", "-1" ));
  for( i in scanfiles )
    $("#source_scanfile").append( new Option( scanfiles[i], scanfiles[i] ));

  //$("#source_popup").jqxWindow('show');
}

function saveSource( )
{
  $.ajax( {
    type: 'POST',
  cache: false,
  url: 'tvd?c=tvdaemon&a=create_source',
  data: $('#source_form').serialize( ),
  success: function(msg) {
    //$("#source_popup").jqxWindow( 'hide' );
    //port["source_id"] = msg;
    //getJSON('tvd?c=tvdaemon&a=get_sources', editPortGetSources );
  },
  error: function( jqXHR, status, errorThrown ) { alert( jqXHR.responseText ); }
  } );
}

function scan( )
{
  $.ajax( {
    type: 'GET',
  cache: false,
  url: 'tvd?c=tvdaemon&a=scan',
  } );
}

function port_form( adapter, frontend, port )
{
  $("#port_adapter_id").val( adapter.id );
  $("#port_frontend_id").val( frontend.id );

  $("#port_source").empty( );
  $("#port_source").append( new Option( "Undefined", "-1" ));
  for( s in sources )
  {
    source = sources[s];
    if( source["type"] == frontend.type )
      $("#port_source").append( new Option( source["name"], source["id"] ));
  }
  $("#port_source").append( new Option( "create new ...", "-2" ));

  if( port )
  {
    $("#port_port_id").val( port.id );
    $("#port_name").val( port["name"] );
    $("#port_num").val( port["port_num"] );
    $("#port_source").val( port["source_id"] ).attr( 'selected', true );
  }
  else
  {
    $("#port_port_id").val( -1 );
    $("#port_name").val( "New LNB" );
    port_num = 0;
    for( p in frontend.ports )
      if( frontend.ports[p].id > port_num )
        port_num = frontend.ports[p].id;
    $("#port_num").val( ++port_num );
  }

  $("#port_name").focus( );
}
