
function getJSON( url, callback )
{
  $.ajax( {
    url: url,
    dataType: 'json',
    success: callback,
    timeout: 3000,
    error: function( jqXHR, status, errorThrown ) {
      if( !callback( null, jqXHR.responseText ) && jqXHR.responseText != "" && errorThrown != "undefined" )
        alert( jqXHR.responseText );
    }
  });
}


/* Server Side Table */

var SST_NUMERIC = 1;

function ServerSideTable( name, url, page_size )
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
  paginator.prop( "id", "sst_paginator_" + context['name'] );
  paginator.prop( "class", "sst_paginator" );

  info = $('<span>');
  info.prop( "id", "sst_info_" + context['name'] );
  info.prop( "class", "sst_info" );
  paginator.append( info );

  context['func_info'] = function( context, data ) {
    if( data["count"] > 0 ) return (data["start"]+1) + " - " + data["end"] + " / " + data["count"];
  };

  scroll_up = function( ) {
    this['start'] -= this['page_size'];
    if( this['start'] < 0 ) this['start'] = 0;
    this['load']( );
  };
  prev = $('<button>');
  prev.html( "&#x21e7;" );
  prev.bind( 'click', scroll_up.bind( context ));
  paginator.append( prev );

  scroll_down = function( ) {
    if( this['start'] + this['page_size'] < this['count'] )
      this['start'] += this['page_size'];
    this['load']( );
  };

  next = $('<button>');
  next.html( "&#x21e9;" );
  next.bind( 'click', scroll_down.bind( context ));
  paginator.append( next );

  search_func = function ( ) {
    search = $('#sst_search_' + this['name']).val( );
    if( search == this['search'] )
      return;
    this['search'] = search;
    this['start'] = 0;
    this['load']( );
  };

  search = $('<input>');
  search.prop( 'id', "sst_search_" + context['name'] );
  search.bind( 'keyup', search_func.bind( context ));
  search.val( context['search'] );
  paginator.append( search );

  sst = $('<div>');
  sst.prop( "class", "sst" );
  //sst.html( "loading data ..." );

  $( '#' + context['name'] ).empty( );
  sst.append( paginator );
  table = $('<table>');
  table.attr( 'id', 'sst_' + context['name'] );
  sst.append( table );
  sst.appendTo( '#' + context['name'] );
  return context;
}

function renderTable( data, errmsg )
{
  context = this;
  table = $( '#sst_' + context['name'] );
  table.empty( );
  if( !data )
  {
    sst.html( "error loading data: " + errmsg );
    return true;
  }

  context['count'] = data["count"];
  $( '#sst_info_' + context['name'] ).html( context['func_info']( context, data ));
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
        cell.attr( "class", "sst_" + key );
      else if( render == 1 )
        cell.attr( "class", "sst_numeric" );

      if( render instanceof Function )
      {
        cell.attr( "class", "sst_" + key );
        cell.html( render( entry ));
      }
      else
        cell.html( entry[key] );
      row.append( cell );
    }

    table.append( row );
  }
  table.appendTo( sst );
  return true;
}

/* Menu */

function Menu( selected )
{
  menu = { Setup   : "gui.html",
           Transponders: "transponders.html",
           Services: "services.html",
           Channels: "channels.html",
           EPG     : "epg.html",
           Recorder: "recorder.html"
         };

  ul = $('<ul>');
  for( m in menu )
  {
    li = $('<li>');
    li.html( m );
    if( m == selected )
      li.attr( 'class', 'selected' );
    click = function( ) { window.location = this; }
    li.bind( 'click', click.bind( menu[m] ));
    ul.append( li );
  }
  div = $('#menu');
  div.append( ul );
}

/* misc */

function print_time( time )
{
  start = "";
  start_hour = time["start_hour"];
  start_min  = time["start_min"];
  start_day  = time["start_day"];
  start_month  = time["start_month"];
  if( start_hour < 10 ) start_hour = "0" + start_hour;
  if( start_min < 10 ) start_min = "0" + start_min;
  if( start_day < 10 ) start_day = "0" + start_day;
  if( start_month < 10 ) start_month = "0" + start_month;

  if( time["start_istoday"] == 1 )
    start = start_hour + ":" + start_min;
  else if( time["start_istomorrow"] == 1 )
    start = "tomorrow " + start_hour + ":" + start_min;
  else
    start = start_day + "." + start_month + " " + start_hour + ":" + start_min;
  return start;
}

function epg_state( row )
{
  switch( row["epg_state"] )
  {
    case 0: return "Missing";
    case 1: return "ok";
    case 2: return "Updating";
    case 3: return "N/A";
  }
}
