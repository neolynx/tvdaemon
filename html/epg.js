$(document).ready( ready );

function ready( )
{
  Menu( "EPG" );
  t = ServerSideTable( 'epg', 'tvd?c=tvdaemon&a=get_epg', 10 );
  t["columns"] = {
    "start"    : [ "", print_start ],
    "name"     : [ "", print_event ],
  };
  t["click"] = function( ) {
    if( confirm( "Record " + this["name"] + " ?" ))
      getJSON( 'tvd?c=channel&a=schedule&channel_id=' + this["channel_id"] + "&event_id=" + this["id"], schedule );
  };
  t.filters( [ "search" ] );
  t.load( );
}

function schedule( data, errmsg )
{
  if( !data )
    return false;
  return true;
}

function duration( row )
{
  d = row["duration"] / 60;
  return Math.floor( d ) + "'";
}

function print_event( row )
{
  cell = "<div><strong>" + row["name"] + "</strong></div>" +
         "<div>" + row["channel"] + "</div>" +
         "<div><strong>" + row["description"] + "</strong></div>";

    if(row["description_extended"] != "")
         cell += "<div>" + row["description_extended"].replace(/\n/g, '<br/>') + "</div>";
    if(row["description_items"])
         for( item in row["description_items"]) {
            //alert( JSON.stringify(item, null, 4));
            cell += "<div>" + item + ":<ul>";
            for( desc in row["description_items"][item] ) {
                cell += "<li>" + row["description_items"][item][desc] + "</li>";
            }
            cell += "</ul><div>";
         }
    return cell;
}

function print_start( row, field )
{
  r = print_time( row, field );
  r += "<br/>" + duration( row );
  return r;
}

