
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

var SRVT_NUMERIC = 1;

function createSRVTable( name, url, page_size )
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
    if( data["count"] > 0 ) return (data["start"]+1) + " - " + data["end"] + " / " + data["count"];
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

  srvtable = $('<div>');
  srvtable.prop( "id", context['name'] + "_srvtable" );
  srvtable.prop( "class", "srvtable" );

  $( '#' + context['name'] ).empty( );
  paginator.appendTo( '#' + context['name'] );
  srvtable.appendTo( '#' + context['name'] );
  return context;
}

function renderTable( data, errmsg )
{
  context = this;
  if( !data ) return;

  context['count'] = data["count"];

  $( '#' + context['name'] + "_srvtable" ).empty( );

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
  table.appendTo( '#' + context['name'] + "_srvtable" );
}

