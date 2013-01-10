$(document).ready( ready );

var sources = [];
var service_types = [];

function getJSON( url, callback )
{
  $.ajax( {
    url: url,
    dataType: 'json',
    success: callback,
    timeout: 3000,
    error: function( jqXHR, status, errorThrown ) { callback( null, jqXHR.responseText ); }
  });
}

var NUMERIC = 1;

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
  service_table = createTable( 'services', 'tvd?c=tvdaemon&a=get_services', 20 );
  service_table["columns"] = { "name": "Service",
                       "provider": "Provider",
                       "type": [ "Type", function( service ) {
                         return service_types[service['type']];
                       } ],
                       "scrambled": [ "Scrambled", function( service ) {
                         if( service["scrambled"] == 1 )
                           return "&#9911;";
                       } ],
                       "id": [ "PID", NUMERIC ]
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

function createTable( name, url, page_size )
{
  context = { 'name': name, 'url': url }
  context['page_size'] = page_size || 10;
  context['start'] = 0;
  context['search'] = '';
  context['renderer'] = renderTable.bind( context );
  loader = function ( ) {
    url = this['url'];
    url += (url.split('?')[1] ? '&':'?') + 'page_size=' + this['page_size'];
    url += '&start=' + this['start'];
    url += '&search=' + this['search'];
    getJSON( url, this['renderer'] );
  };
  context['load'] = loader.bind( context );

  paginator = $('<div>');
  paginator.prop( "id", context['name'] + "_paginator" );
  paginator.prop( "class", "paginator" );

  info = $('<span>');
  info.prop( "id", context['name'] + "_info" );
  info.prop( "class", "info" );
  paginator.append( info );

  context['func_info'] = function( context, data ) {
    return (data["start"]+1) + " - " + data["end"] + " / " + data["count"];
  };

  scroll_up = function( ) {
    this['start'] -= this['page_size'];
    if( this['start'] < 0 ) this['start'] = 0;
    this['load']( );
  };
  prev = $('<a>');
  prev.html( "&#x21e7;" );
  prev.prop( 'href', 'javascript: void( );' );
  prev.bind( 'click', scroll_up.bind( context ));
  paginator.append( prev );

  scroll_down = function( ) {
    if( this['start'] + this['page_size'] < this['count'] )
      this['start'] += this['page_size'];
    this['load']( );
  };


  next = $('<a>');
  next.html( "&#x21e9;" );
  next.prop( 'href', 'javascript: void( );' );
  next.bind( 'click', scroll_down.bind( context ));
  paginator.append( next );

  search_func = function ( ) {
    search = $('#' + this['name'] + '_search').val( );
    if( search == this['search'] )
      return;
    this['search'] = search;
    this['start'] = 0;
    this['load']( );
  };

  search = $('<input>');
  search.prop( 'id', context['name'] + '_search' );
  search.bind( 'keyup', search_func.bind( context ));
  search.val( context['search'] );
  paginator.append( search );

  jsontable = $('<div>');
  jsontable.prop( "id", context['name'] + "_jsontable" );
  jsontable.prop( "class", "jsontable" );

  $( '#' + context['name'] ).empty( );
  paginator.appendTo( '#' + context['name'] );
  jsontable.appendTo( '#' + context['name'] );
  return context;
}

function renderTable( data, errmsg )
{
  context = this;
  if( !data ) return;

  context['count'] = data["count"];

  $( '#' + context['name'] + "_jsontable" ).empty( );

  $( '#' + context['name'] + "_info" ).html( context['func_info']( context, data ));
  table = $('<table>');
  row = $('<tr>');

  for( key in context["columns"] )
  {
    if( context["columns"][key] instanceof Array )
      colname = context["columns"][key][0];
    else
      colname = context["columns"][key];
    cell = $('<th>');
    cell.html( colname );
    row.append( cell );
  }

  table.append( row );

  for( i in data["data"] )
  {
    entry = data["data"][i];
    row = $('<tr>');
    if( context['click'] )
    {
      click = context['click'].bind( entry );
      row.bind( 'click', click );
    }

    for( key in context["columns"] )
    {
      cell = $('<td>');
      if( context["columns"][key] instanceof Array )
        render = context["columns"][key][1];
      else
        render = null;
      if( !render )
        cell.attr( "class", "column_" + key );
      else if( render == 1 )
        cell.attr( "class", "column_num" );

      if( render instanceof Function )
      {
        cell.attr( "class", "column_" + key );
        cell.html( render( entry ));
      }
      else
        cell.html( entry[key] );
      row.append( cell );
    }

    table.append( row );
  }
  table.appendTo( '#' + context['name'] + "_jsontable" );
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
  return "<b>" + adapter["name"] + "<br/>" + icon;
}

function getFrontend( frontend )
{
  adapter = adapters[frontend["adapter_id"]];
  imgstyle = "";
  if( adapter["present"] != 1 )
    imgstyle = "alt=\"not present\" class=\"grey\";";
  if( frontend["type"] == 0 )
    icon = "<img src=\"images/antenna-icon.png\" style=\"" + imgstyle + "\"/>";
  else if( frontend["type"] == 1 )
    icon = "<img src=\"images/antenna-cable.smal.png\" style=\"" + imgstyle + "\"/>";
  else if( frontend["type"] == 2 )
    icon = "<img src=\"images/antenna-oldstyle.png\" style=\"" + imgstyle + "\"/>";
  return icon;
}

function getPort( port )
{
  frontend = adapters[port["adapter_id"]]["frontends"][port["frontend_id"]];
  //if( frontend["type"] == 0 ) // Sat
  return "<a href=\"javascript: editPort( " + port["adapter_id"] + ", " + port["frontend_id"] + ",  " + port["id"] + " );\"><b>" + port["name"] + "</b></a>";
  //return "";
}

function getSource( port )
{
  source_id = port["source_id"];
  if( source_id >= 0 )
    return "<a href=\"tvd?c=source&a=show&source_id=" + source_id + "\"><b>" + sources[source_id]["name"]
      + "</b></a><br/>Type: " + sources[source_id]["type"]
      + "<br/>Transponders: " + sources[source_id]["transponders"]
      + "<br/>Services: " + sources[source_id]["services"];
  else
    return "Undefined";
}

function editPort( adapter_id, frontend_id, port_id )
{
  no_update = true;
  $("#port_popup").jqxWindow('show');
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
  $.ajax( {
    type: 'POST',
    cache: false,
    url: 'tvd?c=port&a=set',
    data: $('#port_form').serialize( ),
    success: function(msg) { $("#port_popup").jqxWindow( 'hide' ); },
    error: function( jqXHR, status, errorThrown ) { alert( jqXHR.responseText ); }
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

  $("#source_popup").jqxWindow('show');
}

function saveSource( )
{
  $.ajax( {
    type: 'POST',
    cache: false,
    url: 'tvd?c=tvdaemon&a=create_source',
    data: $('#source_form').serialize( ),
    success: function(msg) {
      $("#source_popup").jqxWindow( 'hide' );
      port["source_id"] = msg;
      getJSON('tvd?c=tvdaemon&a=get_sources', editPortGetSources );
      },
    error: function( jqXHR, status, errorThrown ) { alert( jqXHR.responseText ); }
  } );
}

