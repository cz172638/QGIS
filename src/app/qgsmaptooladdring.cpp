/***************************************************************************
    qgsmaptooladdring.cpp  - map tool to cut rings in polygon and multipolygon features
    ---------------------
    begin                : April 2007
    copyright            : (C) 2007 by Marco Hugentobler
    email                : marco dot hugentobler at karto dot baug dot ethz dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QMouseEvent>

#include "qgsmaptooladdring.h"
#include "qgsgeometry.h"
#include "qgslinestring.h"
#include "qgsmapcanvas.h"
#include "qgsproject.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
#include "qgisapp.h"


QgsMapToolAddRing::QgsMapToolAddRing( QgsMapCanvas *canvas )
  : QgsMapToolCapture( canvas, QgisApp::instance()->cadDockWidget(), QgsMapToolCapture::CapturePolygon )
{
  mToolName = tr( "Add ring" );
  connect( QgisApp::instance(), &QgisApp::newProject, this, &QgsMapToolAddRing::stopCapturing );
  connect( QgisApp::instance(), &QgisApp::projectRead, this, &QgsMapToolAddRing::stopCapturing );
}

void QgsMapToolAddRing::cadCanvasReleaseEvent( QgsMapMouseEvent *e )
{

  emit messageDiscarded();

  //check if we operate on a vector layer
  QgsVectorLayer *vlayer = currentVectorLayer();

  if ( !vlayer )
  {
    notifyNotVectorLayer();
    return;
  }

  if ( !vlayer->isEditable() )
  {
    notifyNotEditableLayer();
    return;
  }

  //add point to list and to rubber band
  if ( e->button() == Qt::LeftButton )
  {
    int error = addVertex( e->mapPoint(), e->mapPointMatch() );
    if ( error == 1 )
    {
      //current layer is not a vector layer
      return;
    }
    else if ( error == 2 )
    {
      //problem with coordinate transformation
      emit messageEmitted( tr( "Cannot transform the point to the layers coordinate system." ), QgsMessageBar::WARNING );
      return;
    }

    startCapturing();
  }
  else if ( e->button() == Qt::RightButton )
  {
    if ( !isCapturing() )
      return;

    deleteTempRubberBand();

    closePolygon();

    vlayer->beginEditCommand( tr( "Ring added" ) );

    //does compoundcurve contain circular strings?
    //does provider support circular strings?
    bool hasCurvedSegments = captureCurve()->hasCurvedSegments();
    bool providerSupportsCurvedSegments = vlayer->dataProvider()->capabilities() & QgsVectorDataProvider::CircularGeometries;

    QgsCurve *curveToAdd = nullptr;
    if ( hasCurvedSegments && providerSupportsCurvedSegments )
    {
      curveToAdd = captureCurve()->clone();
    }
    else
    {
      curveToAdd = captureCurve()->curveToLine();
    }

    int addRingReturnCode = vlayer->addRing( curveToAdd );
    if ( addRingReturnCode != 0 )
    {
      QString errorMessage;
      //todo: open message box to communicate errors
      if ( addRingReturnCode == 1 )
      {
        errorMessage = tr( "a problem with geometry type occurred" );
      }
      else if ( addRingReturnCode == 2 )
      {
        errorMessage = tr( "the inserted ring is not closed" );
      }
      else if ( addRingReturnCode == 3 )
      {
        errorMessage = tr( "the inserted ring is not a valid geometry" );
      }
      else if ( addRingReturnCode == 4 )
      {
        errorMessage = tr( "the inserted ring crosses existing rings" );
      }
      else if ( addRingReturnCode == 5 )
      {
        errorMessage = tr( "the inserted ring is not contained in a feature" );
      }
      else
      {
        errorMessage = tr( "an unknown error occurred" );
      }
      emit messageEmitted( tr( "Could not add ring since %1." ).arg( errorMessage ), QgsMessageBar::CRITICAL );
      vlayer->destroyEditCommand();
    }
    else
    {
      vlayer->endEditCommand();
    }

    stopCapturing();
  }
}
