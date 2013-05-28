$(document).ready( ready );

function ready( )
{
  Menu( "Transponders" );
  t = ServerSideTable( 'transponders', 'tvd?c=tvdaemon&a=get_transponders', 20 );
  t["columns"] = {
    "delsys"    : [ "", print_delsys ],
    "frequency" : [ "Frequency", print_freq ],
    "state"     : [ "State", print_state ],
    "epg_state" : [ "EPG", epg_state ],
    "enabled"   : "Enabled",
    "tsid"      : "TS ID",
    "signal"    : [ "S/N", print_sn ],
    "services"  : "Services",
  };
  t.load( );
  $('#sst_paginator_transponders').append( $('#search') );
}

function print_freq( row )
{
  f = row["frequency"] / 1000000;
  f = Math.round( f * 1000 ) / 1000;
  return f.toFixed( 3 );
}

function print_sn( row )
{
  return row["signal"] + "/" + row["noise"];
}

function print_state( row )
{
  switch( row["state"] )
  {
    case 0: return "New";
    case 1: return "Selected";
    case 2: return "Tuning";
    case 3: return "Tuned";
    case 4: return "No Tune";
    case 5: return "Scanning";
    case 6: return "Scanned";
    case 7: return "Scanning failed";
    case 8: return "Idle";
    case 9: return "Duplicate";
  }
}

function print_delsys( row )
{
  switch( row["delsys"] )
  {
    case 3: return "DVB-T";
  }
}

