/***************************************************************************
    qgis.r.in.cpp
    ---------------------
    begin                : May 2015
    copyright            : (C) 2015 by Radim Blazek
    email                : radim dot blazek at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

extern "C"
{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <grass/version.h>
#include <grass/gis.h>
#include <grass/raster.h>
#include <grass/display.h>

#ifdef _MSC_VER
#include <float.h>
#endif
}

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QIODevice>

#include "qgsrectangle.h"
#include "qgsrasterblock.h"
#include "qgsgrass.h"

#if GRASS_VERSION_MAJOR >= 7
#define G_allocate_raster_buf Rast_allocate_buf
#define G_close_cell Rast_close
#define G_get_raster_map_type Rast_get_map_type
#define G_get_raster_row Rast_get_row
#define G_is_null_value Rast_is_null_value
#define G_open_raster_new Rast_open_new
#define G_short_history Rast_short_history
#define G_command_history Rast_command_history
#define G_write_history Rast_write_history
#define G_set_raster_value_c Rast_set_c_value
#define G_set_raster_value_f Rast_set_f_value
#define G_set_raster_value_d Rast_set_d_value
#define G_put_raster_row Rast_put_row
#define G_raster_size Rast_cell_size
#endif

int main( int argc, char **argv )
{
  char *name;
  struct Option *map;
  struct Cell_head window;
  int cf;

  G_gisinit( argv[0] );

  G_define_module();

  map = G_define_standard_option( G_OPT_R_OUTPUT );

  if ( G_parser( argc, argv ) )
    exit( EXIT_FAILURE );

  name = map->answer;

  QFile stdinFile;
  stdinFile.open( 0, QIODevice::ReadOnly );

  QDataStream stdinStream( &stdinFile );

  QgsRectangle extent;
  qint32 rows, cols;
  stdinStream >> extent >> cols >> rows;

  QString err = QgsGrass::setRegion( &window, extent, rows, cols );
  if ( !err.isEmpty() )
  {
    G_fatal_error( "Cannot set region: %s", err.toUtf8().data() );
  }

  G_set_window( &window );

  QGis::DataType qgis_type;
  qint32 type;
  stdinStream >> type;
  qgis_type = ( QGis::DataType )type;

  RASTER_MAP_TYPE grass_type;
  switch ( qgis_type )
  {
    case QGis::Int32:
      grass_type = CELL_TYPE;
      break;
    case QGis::Float32:
      grass_type = FCELL_TYPE;
      break;
    case QGis::Float64:
      grass_type = DCELL_TYPE;
      break;
    default:
      G_fatal_error( "QGIS data type %d not supported", qgis_type );
      return 1;
  }

  cf = G_open_raster_new( name, grass_type );
  if ( cf < 0 )
  {
    G_fatal_error( "Unable to create raster map <%s>", name );
    return 1;
  }

  void *buf = G_allocate_raster_buf( grass_type );

  int expectedSize = cols * QgsRasterBlock::typeSize( qgis_type );
  QByteArray byteArray;
  for ( int row = 0; row < rows; row++ )
  {
    stdinStream >> byteArray;
    if ( byteArray.size() != expectedSize )
    {
      G_fatal_error( "Wrong byte array size, expected %d bytes, got %d", expectedSize, byteArray.size() );
      return 1;
    }

    qint32 *cell;
    float *fcell;
    double *dcell;
    if ( grass_type == CELL_TYPE )
      cell = ( qint32* ) byteArray.data();
    else if ( grass_type == FCELL_TYPE )
      fcell = ( float* ) byteArray.data();
    else if ( grass_type == DCELL_TYPE )
      dcell = ( double* ) byteArray.data();

    void *ptr = buf;
    for ( int col = 0; col < cols; col++ )
    {
      if ( grass_type == CELL_TYPE )
        G_set_raster_value_c( ptr, ( CELL )cell[col], grass_type );
      else if ( grass_type == FCELL_TYPE )
        G_set_raster_value_f( ptr, ( FCELL )fcell[col], grass_type );
      else if ( grass_type == DCELL_TYPE )
        G_set_raster_value_d( ptr, ( DCELL )dcell[col], grass_type );

      ptr = G_incr_void_ptr( ptr, G_raster_size( grass_type ) );
    }
    G_put_raster_row( cf, buf, grass_type );
  }

  G_close_cell( cf );
  struct History history;
  G_short_history( name, "raster", &history );
  G_command_history( &history );
  G_write_history( name, &history );

  exit( EXIT_SUCCESS );
}
