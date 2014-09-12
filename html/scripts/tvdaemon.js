
function getJSON( url, callback )
{
  $.ajax( {
    url: url,
    dataType: 'json',
    success: callback,
    timeout: 3000,
    error: function( jqXHR, status, errorThrown ) {
      if(( !callback || !callback( null, jqXHR.responseText )) && jqXHR.responseText != "" && errorThrown != "undefined" )
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
  context['filters'] = [];
  context['search'] = '';
  context['renderer'] = renderTable.bind( context );

  search_func = function ( ) {
    this['start'] = 0;
    this['load']( );
  };

  //search = $('<input>');
  //search.prop( 'id', "sst_search_" + context['name'] );
  //search.bind( 'keyup', search_func.bind( context ));
  //search.val( context['search'] );
  //paginator.append( search );

  load = function ( ) {
    url = this['url'];
    url += (url.split('?')[1] ? '&':'?') + 'page_size=' + this['page_size'];
    url += '&start=' + this['start'];

    filters = this["filters"];
    for( f in filters )
    {
      filter = $('#' + filters[f]);
      url += '&' + filters[f] + "=" + filter.val( );
    }

    getJSON( url, this['renderer'] );
  };
  context['load'] = load.bind( context );

  filters = function ( filters ) {
    this["filters"] = filters;
    for( f in filters )
    {
      filter = $('#' + filters[f]);
      filter.bind( 'keyup', search_func.bind( context ));
      if( f == 0 )
        filter.focus( );
      filter.attr( "tabindex", f );
    }
  };
  context['filters'] = filters.bind( context );

  entry = function ( idx ) {
    return this["data"].data[idx];
  };
  context['entry'] = entry.bind( context );

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

  paginator.append( $('#search_' + context['name']) );

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

  context['data'] = data;
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
        cell.html( render( entry, key, i ));
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
  menu = { Setup   : "index.html",
           Transponders: "transponders.html",
           Services: "services.html",
           Channels: "channels.html",
           EPG     : "epg.html",
           Recorder: "recorder.html",
           CAMClients: "camclients.html",
         };

  ul = $('#menu');
  for( m in menu )
  {
    li = $('<li>');
    if( m == selected )
      li.attr( 'class', 'active' );
    a = $('<a>');
    a.html( m );
    a.attr( 'href', menu[m] );
    li.append( a );
    ul.append( li );
  }
  div = $('#menu');
  div.append( ul );
}

/* misc */

function print_time( time, key )
{
  t = "";
  t_hour = time[key + "_hour"];
  t_min  = time[key + "_min"];
  t_day  = time[key + "_day"];
  t_month  = time[key + "_month"];
  if( t_hour  < 10 ) t_hour = "0"  + t_hour;
  if( t_min   < 10 ) t_min = "0"   + t_min;
  if( t_day   < 10 ) t_day = "0"   + t_day;
  if( t_month < 10 ) t_month = "0" + t_month;

  if( time[key + "_istoday"] == 1 )
    t = t_hour + ":" + t_min;
  else if( time[key + "_istomorrow"] == 1 )
    t = "tomorrow " + t_hour + ":" + t_min;
  else
    t = t_day + "." + t_month + " " + t_hour + ":" + t_min;
  return t;
}

function epg_state( row, key )
{
  switch( row[key] )
  {
    case 0: return "Missing";
    case 1: return "ok";
    case 2: return "Updating";
    case 3: return "N/A";
  }
  return "?";
}

function recording_state( row, key )
{
  switch( row[key] )
  {
    case 0: return "New";
    case 1: return "Scheduled";
    case 2: return "Starting";
    case 3: return "Running";
    case 4: return "Done";
    case 5: return "Aborted";
    case 6: return "Failed";
    case 7: return "Missed";
  }
  return "?";
}

