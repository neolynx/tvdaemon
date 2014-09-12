$(document).ready( ready );

var camclients;

function ready( )
{
  Menu( "CAM Clients" );
  camclients = ServerSideTable( 'camclients', 'tvd?c=tvdaemon&a=get_camclients', 20 );
  camclients["columns"] = {
    "hostname"  : "Hostname",
    "service"   : "Service",
    "username"  : "Username",
    "u"         : [ "", print_edit ],
    "r"         : [ "", print_remove ],
  };
  camclients.load( );
  camclients.filters( [ "search" ] );
}

function print_edit( row, key, idx )
{
  return "<a href=\"javascript: edit_camclient( " + idx + " );\">edit</a>";
}

function print_remove( row, key, idx )
{
  return "<a href=\"javascript: remove_camclient( " + idx + " );\">X</a>";
}

function edit_camclient( idx )
{
  dialog = $( "#camclient-form" ).dialog( {
    title: "Edit CAM Client",
         autoOpen: false,
         height: 300,
         width: 350,
         modal: true,
         buttons: {
           "Edit": camclient_submit,
         Cancel: function() { dialog.dialog( "close" ); }
         },
  });
  camclient_form( idx );
  dialog.dialog( "open" );
}

function remove_camclient( idx )
{
  entry = camclients.entry( idx );
  if( confirm( "Remove " + entry.hostname + " ?" ))
    getJSON( 'tvd?c=tvdaemon&a=remove_camclient&id=' + entry.id, rethandler );
}

function rethandler( data, errmsg )
{
  camclients.load( );
}

function camclient_form( idx )
{
  entry = camclients.entry( idx );
  $("#camclient_id").val( entry.id );
  $("#hostname").val( entry.hostname );
  $("#service").val( entry.service );
  $("#username").val( entry.username );
  $("#password").val( "" );
  $("#key").val( entry.key );

  $("#hostname").focus( );
}

function camclient_submit()
{
}
