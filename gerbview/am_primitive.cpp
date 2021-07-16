/**
 * @file aperture_macro.cpp
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2017 Jean-Pierre Charras <jp.charras at wanadoo.fr>
 * Copyright (C) 2010 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 1992-2021 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <trigo.h>
#include <convert_to_biu.h>
#include <convert_basic_shapes_to_polygon.h>
#include <math/util.h>      // for KiROUND

#include <gerbview.h>
#include <gerber_file_image.h>


/**
 * Convert a distance given in floating point to our internal units.
 */
extern int scaletoIU( double aCoord, bool isMetric );


/**
 * Translate a point from the aperture macro coordinate system to our
 * deci-mils coordinate system.
 *
 * @return The GerbView coordinate system vector.
 */
static wxPoint mapPt( double x, double y, bool isMetric )
{
    wxPoint ret( scaletoIU( x, isMetric ), scaletoIU( y, isMetric ) );

    return ret;
}


bool AM_PRIMITIVE::IsAMPrimitiveExposureOn( const GERBER_DRAW_ITEM* aParent ) const
{
    /*
     * Some but not all primitives use the first parameter as an exposure control.
     * Others are always ON.
     * In a aperture macro shape, a basic primitive with exposure off is a hole in the shape
     * it is NOT a negative shape
    */
    wxASSERT( params.size() );

    switch( primitive_id )
    {
    case AMP_CIRCLE:
    case AMP_LINE2:
    case AMP_LINE20:
    case AMP_LINE_CENTER:
    case AMP_LINE_LOWER_LEFT:
    case AMP_OUTLINE:
    case AMP_POLYGON:
        // All have an exposure parameter and can return a value (0 or 1)
        return params[0].GetValue( aParent->GetDcodeDescr() ) != 0;
        break;

    case AMP_THERMAL:   // Exposure is always on
    case AMP_MOIRE:     // Exposure is always on
    case AMP_EOF:
    case AMP_UNKNOWN:
    default:
        return 1;       // All have no exposure parameter and are always 0N return true
        break;
    }
}


// TODO(snh): Remove hard coded count
const int seg_per_circle = 64;   // Number of segments to approximate a circle


void AM_PRIMITIVE::DrawBasicShape( const GERBER_DRAW_ITEM* aParent, SHAPE_POLY_SET& aShapeBuffer,
                                   wxPoint aShapePos )
{
#define TO_POLY_SHAPE                                                                          \
    {                                                                                          \
        if( polybuffer.size() > 1 )                                                            \
        {                                                                                      \
            aShapeBuffer.NewOutline();                                                         \
            for( unsigned jj = 0; jj < polybuffer.size(); jj++ )                               \
                aShapeBuffer.Append( polybuffer[jj].x, polybuffer[jj].y );                     \
            aShapeBuffer.Append( polybuffer[0].x, polybuffer[0].y );                           \
        }                                                                                      \
    }

    // Draw the primitive shape for flashed items.
    // Create a static buffer to avoid a lot of memory reallocation.
    static std::vector<wxPoint> polybuffer;
    polybuffer.clear();

    wxPoint curPos = aShapePos;
    D_CODE* tool   = aParent->GetDcodeDescr();
    double rotation;

    switch( primitive_id )
    {
    case AMP_CIRCLE:        // Circle, given diameter and position
    {
        /* Generated by an aperture macro declaration like:
         * "1,1,0.3,0.5, 1.0*"
         * type (1), exposure, diameter, pos.x, pos.y, <rotation>
         * <rotation> is a optional parameter: rotation from origin.
         * type is not stored in parameters list, so the first parameter is exposure
         */
        ConvertShapeToPolygon( aParent, polybuffer );

        // shape rotation (if any):
        if( params.size() >= 5 )
        {
            rotation = params[4].GetValue( tool ) * 10.0;

            if( rotation != 0)
            {
                for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
                    RotatePoint( &polybuffer[ii], -rotation );
            }
        }


        // Move to current position:
        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            polybuffer[ii] += curPos;
            polybuffer[ii] = aParent->GetABPosition( polybuffer[ii] );
        }

        TO_POLY_SHAPE;
        break;
    }

    case AMP_LINE2:
    case AMP_LINE20:        // Line with rectangle ends. (Width, start and end pos + rotation)
    {
        /* Vector Line, Primitive Code 20.
         * A vector line is a rectangle defined by its line width, start and end points.
         * The line ends are rectangular.
        */
        /* Generated by an aperture macro declaration like:
         * "2,1,0.3,0,0, 0.5, 1.0,-135*"
         * type (2), exposure, width, start.x, start.y, end.x, end.y, rotation
         * type is not stored in parameters list, so the first parameter is exposure
         */
        ConvertShapeToPolygon( aParent, polybuffer );

        // shape rotation:
        rotation = params[6].GetValue( tool ) * 10.0;

        if( rotation != 0)
        {
            for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
                RotatePoint( &polybuffer[ii], -rotation );
        }

        // Move to current position:
        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            polybuffer[ii] += curPos;
            polybuffer[ii] = aParent->GetABPosition( polybuffer[ii] );
        }

        TO_POLY_SHAPE;
        break;
    }

    case AMP_LINE_CENTER:
    {
        /* Center Line, Primitive Code 21
         * A center line primitive is a rectangle defined by its width, height, and center point
         */
        /* Generated by an aperture macro declaration like:
         * "21,1,0.3,0.03,0,0,-135*"
         * type (21), exposure, ,width, height, center pos.x, center pos.y, rotation
         * type is not stored in parameters list, so the first parameter is exposure
         */
        ConvertShapeToPolygon( aParent, polybuffer );

        // shape rotation:
        rotation = params[5].GetValue( tool ) * 10.0;

        if( rotation != 0 )
        {
            for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
                RotatePoint( &polybuffer[ii], -rotation );
        }

        // Move to current position:
        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            polybuffer[ii] += curPos;
            polybuffer[ii] = aParent->GetABPosition( polybuffer[ii] );
        }

        TO_POLY_SHAPE;
        break;
    }

    case AMP_LINE_LOWER_LEFT:
    {
        /* Generated by an aperture macro declaration like:
         * "22,1,0.3,0.03,0,0,-135*"
         * type (22), exposure, ,width, height, corner pos.x, corner pos.y, rotation
         * type is not stored in parameters list, so the first parameter is exposure
         */
        ConvertShapeToPolygon( aParent, polybuffer );

        // shape rotation:
        rotation = params[5].GetValue( tool ) * 10.0;
        if( rotation != 0)
        {
            for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
                RotatePoint( &polybuffer[ii], -rotation );
        }

        // Move to current position:
        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            polybuffer[ii] += curPos;
            polybuffer[ii] = aParent->GetABPosition( polybuffer[ii] );
        }

        TO_POLY_SHAPE;
        break;
    }

    case AMP_THERMAL:
    {
        /* Generated by an aperture macro declaration like:
         * "7, 0,0,1.0,0.3,0.01,-13*"
         * type (7), center.x , center.y, outside diam, inside diam, crosshair thickness, rotation
         * type is not stored in parameters list, so the first parameter is center.x
         *
         * The thermal primitive is a ring (annulus) interrupted by four gaps. Exposure is always
         * on.
         */
        std::vector<wxPoint> subshape_poly;
        curPos += mapPt( params[0].GetValue( tool ), params[1].GetValue( tool ), m_GerbMetric );
        ConvertShapeToPolygon( aParent, subshape_poly );

        // shape rotation:
        rotation = params[5].GetValue( tool ) * 10.0;

        // Because a thermal shape has 4 identical sub-shapes, only one is created in subshape_poly.
        // We must draw 4 sub-shapes rotated by 90 deg
        for( int ii = 0; ii < 4; ii++ )
        {
            polybuffer = subshape_poly;
            double sub_rotation = rotation + 900 * ii;

            for( unsigned jj = 0; jj < polybuffer.size(); jj++ )
                RotatePoint( &polybuffer[jj], -sub_rotation );

            // Move to current position:
            for( unsigned jj = 0; jj < polybuffer.size(); jj++ )
            {
                polybuffer[jj] += curPos;
                polybuffer[jj] = aParent->GetABPosition( polybuffer[jj] );
            }

            TO_POLY_SHAPE;
        }
    }
    break;

    case AMP_MOIRE:
    {
        /* Moire, Primitive Code 6
         * The moire primitive is a cross hair centered on concentric rings (annuli).
         * Exposure is always on.
         */
        curPos += mapPt( params[0].GetValue( tool ), params[1].GetValue( tool ),
                         m_GerbMetric );

        /* Generated by an aperture macro declaration like:
         * "6,0,0,0.125,.01,0.01,3,0.003,0.150,0"
         * type(6), pos.x, pos.y, diam, penwidth, gap, circlecount, crosshair thickness,
         * crosshair len, rotation.  The type is not stored in parameters list, so the first
         * parameter is pos.x.
         */
        int outerDiam    = scaletoIU( params[2].GetValue( tool ), m_GerbMetric );
        int penThickness = scaletoIU( params[3].GetValue( tool ), m_GerbMetric );
        int gap = scaletoIU( params[4].GetValue( tool ), m_GerbMetric );
        int numCircles = KiROUND( params[5].GetValue( tool ) );

        // Draw circles:
        wxPoint center = aParent->GetABPosition( curPos );

        // adjust outerDiam by this on each nested circle
        int diamAdjust = ( gap + penThickness ) * 2;

        for( int i = 0; i < numCircles; ++i, outerDiam -= diamAdjust )
        {
            if( outerDiam <= 0 )
                break;

            // Note: outerDiam is the outer diameter of the ring.
            // the ring graphic diameter is (outerDiam - penThickness)
            if( outerDiam <= penThickness )
            {   // No room to draw a ring (no room for the hole):
                // draw a circle instead (with no hole), with the right diameter
                TransformCircleToPolygon( aShapeBuffer, center, outerDiam / 2, ARC_HIGH_DEF,
                                          ERROR_INSIDE );
            }
            else
            {
                TransformRingToPolygon( aShapeBuffer, center, ( outerDiam - penThickness ) / 2,
                                        penThickness, ARC_HIGH_DEF, ERROR_INSIDE );
            }
        }

        // Draw the cross:
        ConvertShapeToPolygon( aParent, polybuffer );

        rotation = params[8].GetValue( tool ) * 10.0;

        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            // shape rotation:
            RotatePoint( &polybuffer[ii], -rotation );

            // Move to current position:
            polybuffer[ii] += curPos;
            polybuffer[ii] = aParent->GetABPosition( polybuffer[ii] );
        }

        TO_POLY_SHAPE;
        break;
    }

    case AMP_OUTLINE:
    {
        /* Outline, Primitive Code 4
         * An outline primitive is an area enclosed by an n-point polygon defined by its start
         * point and n
         * subsequent points. The outline must be closed, i.e. the last point must be equal to
         * the start point. There must be at least one subsequent point (to close the outline).
         * The outline of the primitive is actually the contour (see 2.6) that consists of linear
         * segments only, so it must conform to all the requirements described for contours.
         * Warning: Make no mistake: n is the number of subsequent points, being the number of
         * vertices of the outline or one less than the number of coordinate pairs.
        */
        /* Generated by an aperture macro declaration like:
         * "4,1,3,0.0,0.0,0.0,0.5,0.5,0.5,0.5,0.0,-25"
         * type(4), exposure, corners count, corner1.x, corner.1y, ..., corner1.x, corner.1y,
         * rotation
         * type is not stored in parameters list, so the first parameter is exposure
         */
        // params[0] is the exposure and params[1] is the corners count after the first corner
        int numCorners = (int) params[1].GetValue( tool );

        // the shape rotation is the last param of list, after corners
        int last_prm = params.size() - 1;
        rotation  = params[last_prm].GetValue( tool ) * 10.0;
        wxPoint pos;

        // Read points.
        // Note: numCorners is the polygon corner count, following the first corner
        // * the polygon is always closed,
        // * therefore the last XY coordinate is the same as the first
        int prm_idx = 2;    //  params[2] is the first X coordinate

        for( int i = 0; i <= numCorners; ++i )
        {
            pos.x = scaletoIU( params[prm_idx].GetValue( tool ), m_GerbMetric );
            prm_idx++;
            pos.y = scaletoIU( params[prm_idx].GetValue( tool ), m_GerbMetric );
            prm_idx++;
            polybuffer.push_back(pos);

            // Guard: ensure prm_idx < last_prm
            // I saw malformed gerber files with numCorners = number
            // of coordinates instead of number of coordinates following the first point
            if( prm_idx >= last_prm )
                break;
        }

        // rotate polygon and move it to the actual position shape rotation:
        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            RotatePoint( &polybuffer[ii], -rotation );
        }

        // Move to current position:
        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            polybuffer[ii] += curPos;
            polybuffer[ii] = aParent->GetABPosition( polybuffer[ii] );
        }

        break;
        TO_POLY_SHAPE;
    }

    case AMP_POLYGON:
        /* Polygon, Primitive Code 5
         * A polygon primitive is a regular polygon defined by the number of vertices n, the
         * center point and the diameter of the circumscribed circle
        */
        /* Generated by an aperture macro declaration like:
         * "5,1,0.6,0,0,0.5,25"
         * type(5), exposure, vertices count, pox.x, pos.y, diameter, rotation
         * type is not stored in parameters list, so the first parameter is exposure
         */
        curPos += mapPt( params[2].GetValue( tool ), params[3].GetValue( tool ), m_GerbMetric );

        // Creates the shape:
        ConvertShapeToPolygon( aParent, polybuffer );

        // rotate polygon and move it to the actual position
        rotation  = params[5].GetValue( tool ) * 10.0;
        for( unsigned ii = 0; ii < polybuffer.size(); ii++ )
        {
            RotatePoint( &polybuffer[ii], -rotation );
            polybuffer[ii] += curPos;
            polybuffer[ii] = aParent->GetABPosition( polybuffer[ii] );
        }

        TO_POLY_SHAPE;

        break;

    case AMP_EOF:
        // not yet supported, waiting for you.
        break;

    case AMP_UNKNOWN:
    default:
        break;
    }
}


void AM_PRIMITIVE::ConvertShapeToPolygon( const GERBER_DRAW_ITEM* aParent,
                                          std::vector<wxPoint>& aBuffer )
{
    D_CODE* tool = aParent->GetDcodeDescr();

    switch( primitive_id )
    {
    case AMP_CIRCLE:
    {
        /* Generated by an aperture macro declaration like:
         * "1,1,0.3,0.5, 1.0*"
         * type (1), exposure, diameter, pos.x, pos.y, <rotation>
         * <rotation> is a optional parameter: rotation from origin.
         * type is not stored in parameters list, so the first parameter is exposure
         */
        int radius = scaletoIU( params[1].GetValue( tool ), m_GerbMetric ) / 2;

        // A circle primitive can have a 0 size (for instance when used in roundrect macro),
        // so skip it
        if( radius <= 0 )
            break;

        wxPoint center = mapPt( params[2].GetValue( tool ), params[3].GetValue( tool ),
                                m_GerbMetric );
        wxPoint corner;
        const int delta = 3600 / seg_per_circle;    // rot angle in 0.1 degree

        for( int angle = 0; angle < 3600; angle += delta )
        {
            corner.x   = radius;
            corner.y   = 0;
            RotatePoint( &corner, angle );
            corner += center;
            aBuffer.push_back( corner );
        }

        break;
    }

    case AMP_LINE2:
    case AMP_LINE20:        // Line with rectangle ends. (Width, start and end pos + rotation)
    {
        int     width = scaletoIU( params[1].GetValue( tool ), m_GerbMetric );
        wxPoint start = mapPt( params[2].GetValue( tool ),
                               params[3].GetValue( tool ), m_GerbMetric );
        wxPoint end = mapPt( params[4].GetValue( tool ),
                             params[5].GetValue( tool ), m_GerbMetric );
        wxPoint delta = end - start;
        int     len   = KiROUND( EuclideanNorm( delta ) );

        // To build the polygon, we must create a horizontal polygon starting to "start"
        // and rotate it to have the end point to "end"
        wxPoint currpt;
        currpt.y += width / 2;          // Upper left
        aBuffer.push_back( currpt );
        currpt.x = len;                 // Upper right
        aBuffer.push_back( currpt );
        currpt.y -= width;              // lower right
        aBuffer.push_back( currpt );
        currpt.x = 0;                   // lower left
        aBuffer.push_back( currpt );

        // Rotate rectangle and move it to the actual start point
        double angle = ArcTangente( delta.y, delta.x );

        for( unsigned ii = 0; ii < 4; ii++ )
        {
            RotatePoint( &aBuffer[ii], -angle );
            aBuffer[ii] += start;
        }

        break;
    }

    case AMP_LINE_CENTER:
    {
        wxPoint size = mapPt( params[1].GetValue( tool ), params[2].GetValue( tool ),
                              m_GerbMetric );
        wxPoint pos  = mapPt( params[3].GetValue( tool ), params[4].GetValue( tool ),
                              m_GerbMetric );

        // Build poly:
        pos.x -= size.x / 2;
        pos.y -= size.y / 2;        // Lower left
        aBuffer.push_back( pos );
        pos.y += size.y;            // Upper left
        aBuffer.push_back( pos );
        pos.x += size.x;            // Upper right
        aBuffer.push_back( pos );
        pos.y -= size.y;            // lower right
        aBuffer.push_back( pos );
        break;
    }

    case AMP_LINE_LOWER_LEFT:
    {
        wxPoint size = mapPt( params[1].GetValue( tool ), params[2].GetValue( tool ),
                              m_GerbMetric );
        wxPoint lowerLeft = mapPt( params[3].GetValue( tool ), params[4].GetValue( tool ),
                                   m_GerbMetric );

        // Build poly:
        aBuffer.push_back( lowerLeft );
        lowerLeft.y += size.y;          // Upper left
        aBuffer.push_back( lowerLeft );
        lowerLeft.x += size.x;          // Upper right
        aBuffer.push_back( lowerLeft );
        lowerLeft.y -= size.y;          // lower right
        aBuffer.push_back( lowerLeft );
        break;
    }

    case AMP_THERMAL:
    {
        // Only 1/4 of the full shape is built, because the other 3 shapes will be draw from
        // this first rotated by 90, 180 and 270 deg.
        // params = center.x (unused here), center.y (unused here), outside diam, inside diam,
        // crosshair thickness.
        int outerRadius   = scaletoIU( params[2].GetValue( tool ), m_GerbMetric ) / 2;
        int innerRadius   = scaletoIU( params[3].GetValue( tool ), m_GerbMetric ) / 2;

        // Safety checks to guarantee no divide-by-zero
        outerRadius = std::max( 1, outerRadius );
        innerRadius = std::max( 1, innerRadius );

        int halfthickness  = scaletoIU( params[4].GetValue( tool ), m_GerbMetric ) / 2;
        double angle_start = RAD2DECIDEG( asin( (double) halfthickness / innerRadius ) );

        // Draw shape in the first quadrant (X and Y > 0)
        wxPoint pos, startpos;

        // Inner arc
        startpos.x = innerRadius;
        double angle_end = 900 - angle_start;

        for( double angle = angle_start; angle < angle_end; angle += 100 )
        {
            pos = startpos;
            RotatePoint( &pos, angle );
            aBuffer.push_back( pos );
        }

        // Last point
        pos = startpos;
        RotatePoint( &pos, angle_end );
        aBuffer.push_back( pos );

        // outer arc
        startpos.x  = outerRadius;
        startpos.y  = 0;
        angle_start = RAD2DECIDEG( asin( (double) halfthickness / outerRadius ) );
        angle_end   = 900 - angle_start;

        // First point, near Y axis, outer arc
        for( double angle = angle_end; angle > angle_start; angle -= 100 )
        {
            pos = startpos;
            RotatePoint( &pos, angle );
            aBuffer.push_back( pos );
        }

        // last point
        pos = startpos;
        RotatePoint( &pos, angle_start );
        aBuffer.push_back( pos );

        aBuffer.push_back( aBuffer[0] );  // Close poly
    }
    break;

    case AMP_MOIRE:
    {
        // A cross hair with n concentric circles. Only the cross is built as
        // polygon because circles can be drawn easily
        int crossHairThickness = scaletoIU( params[6].GetValue( tool ), m_GerbMetric );
        int crossHairLength    = scaletoIU( params[7].GetValue( tool ), m_GerbMetric );

        // Create cross. First create 1/4 of the shape.
        // Others point are the same, rotated by 90, 180 and 270 deg
        wxPoint pos( crossHairThickness / 2, crossHairLength / 2 );
        aBuffer.push_back( pos );
        pos.y = crossHairThickness / 2;
        aBuffer.push_back( pos );
        pos.x = -crossHairLength / 2;
        aBuffer.push_back( pos );
        pos.y = -crossHairThickness / 2;
        aBuffer.push_back( pos );

        // Copy the 4 shape, rotated by 90, 180 and 270 deg
        for( int jj = 1; jj <= 3; jj ++ )
        {
            for( int ii = 0; ii < 4; ii++ )
            {
                pos = aBuffer[ii];
                RotatePoint( &pos, jj*900 );
                aBuffer.push_back( pos );
            }
        }

        break;
    }

    case AMP_OUTLINE:
        // already is a polygon. Do nothing
        break;

    case AMP_POLYGON:   // Creates a regular polygon
    {
        int vertexcount = KiROUND( params[1].GetValue( tool ) );
        int radius    = scaletoIU( params[4].GetValue( tool ), m_GerbMetric ) / 2;

        // rs274x said: vertex count = 3 ... 10, and the first corner is on the X axis
        if( vertexcount < 3 )
            vertexcount = 3;

        if( vertexcount > 10 )
            vertexcount = 10;

        for( int ii = 0; ii <= vertexcount; ii++ )
        {
            wxPoint pos( radius, 0);
            RotatePoint( &pos, ii * 3600 / vertexcount );
            aBuffer.push_back( pos );
        }

        break;
    }

    case AMP_COMMENT:
    case AMP_UNKNOWN:
    case AMP_EOF:
        break;
    }
}


int AM_PRIMITIVE::GetShapeDim( const GERBER_DRAW_ITEM* aParent )
{
    int dim = -1;
    D_CODE* tool = aParent->GetDcodeDescr();

    switch( primitive_id )
    {
    case AMP_CIRCLE:
        // params = exposure, diameter, pos.x, pos.y
        dim = scaletoIU( params[1].GetValue( tool ), m_GerbMetric );     // Diameter
        break;

    case AMP_LINE2:
    case AMP_LINE20:        // Line with rectangle ends. (Width, start and end pos + rotation)
        dim  = scaletoIU( params[1].GetValue( tool ), m_GerbMetric );   // line width
        break;

    case AMP_LINE_CENTER:
    {
        wxPoint size = mapPt( params[1].GetValue( tool ), params[2].GetValue( tool ),
                              m_GerbMetric );
        dim = std::min(size.x, size.y);
        break;
    }

    case AMP_LINE_LOWER_LEFT:
    {
        wxPoint size = mapPt( params[1].GetValue( tool ), params[2].GetValue( tool ),
                              m_GerbMetric );
        dim = std::min(size.x, size.y);
        break;
    }

    case AMP_THERMAL:
    {
        // Only 1/4 of the full shape is built, because the other 3 shapes will be draw from
        // this first rotated by 90, 180 and 270 deg.
        // params = center.x (unused here), center.y (unused here), outside diam, inside diam,
        // crosshair thickness.
        dim   = scaletoIU( params[2].GetValue( tool ), m_GerbMetric ) / 2;  // Outer diam
        break;
    }

    case AMP_MOIRE:     // A cross hair with n concentric circles.
        dim = scaletoIU( params[7].GetValue( tool ), m_GerbMetric );    // = cross hair len
        break;

    case AMP_OUTLINE:   // a free polygon :
    {
        // dim = min side of the bounding box (this is a poor criteria, but what is a good
        // criteria b?)
        // exposure, corners count, corner1.x, corner.1y, ..., rotation
        // note: corners count is the count of corners following corner1
        int numPoints = (int) params[1].GetValue( tool );

        // Read points. numPoints does not include the starting point, so add 1.
        // and calculate the bounding box;
        wxSize pos_min, pos_max, pos;
        int prm_idx = 2;    //  params[2] is the first X coordinate
        int last_prm = params.size() - 1;

        for( int i = 0; i<= numPoints; ++i )
        {
            pos.x = scaletoIU( params[prm_idx].GetValue( tool ), m_GerbMetric );
            prm_idx++;
            pos.y = scaletoIU( params[prm_idx].GetValue( tool ), m_GerbMetric );
            prm_idx++;

            if( i == 0 )
            {
                pos_min = pos_max = pos;
            }
            else
            {
                // upper right corner:
                if( pos_min.x > pos.x )
                    pos_min.x = pos.x;

                if( pos_min.y > pos.y )
                    pos_min.y = pos.y;

                // lower left corner:
                if( pos_max.x < pos.x )
                    pos_max.x = pos.x;

                if( pos_max.y < pos.y )
                    pos_max.y = pos.y;
            }

            // Guard: ensure prm_idx < last_prm (last prm is orientation)
            // I saw malformed gerber files with numCorners = number
            // of coordinates instead of number of coordinates following the first point
            if( prm_idx >= last_prm )
                break;
        }

        // calculate dim
        wxSize size;
        size.x = pos_max.x - pos_min.x;
        size.y = pos_max.y - pos_min.y;
        dim = std::min( size.x, size.y );
        break;
    }

    case AMP_POLYGON:   // Regular polygon
        dim = scaletoIU( params[4].GetValue( tool ), m_GerbMetric ) / 2;      // Radius
        break;

    case AMP_COMMENT:
    case AMP_UNKNOWN:
    case AMP_EOF:
        break;
    }

    return dim;
}


SHAPE_POLY_SET* APERTURE_MACRO::GetApertureMacroShape( const GERBER_DRAW_ITEM* aParent,
                                                       wxPoint aShapePos )
{
    SHAPE_POLY_SET holeBuffer;
    bool hasHole = false;

    m_shape.RemoveAllContours();

    for( AM_PRIMITIVES::iterator prim_macro = primitives.begin();
         prim_macro != primitives.end(); ++prim_macro )
    {
        if( prim_macro->primitive_id == AMP_COMMENT )
            continue;

        if( prim_macro->IsAMPrimitiveExposureOn( aParent ) )
        {
            prim_macro->DrawBasicShape( aParent, m_shape, aShapePos );
        }
        else
        {
            prim_macro->DrawBasicShape( aParent, holeBuffer, aShapePos );

            if( holeBuffer.OutlineCount() )     // we have a new hole in shape: remove the hole
            {
                m_shape.BooleanSubtract( holeBuffer, SHAPE_POLY_SET::PM_FAST );
                holeBuffer.RemoveAllContours();
                hasHole = true;
            }
        }
    }

    // Merge and cleanup basic shape polygons
    m_shape.Simplify( SHAPE_POLY_SET::PM_FAST );

    // If a hole is defined inside a polygon, we must fracture the polygon
    // to be able to drawn it (i.e link holes by overlapping edges)
    if( hasHole )
        m_shape.Fracture( SHAPE_POLY_SET::PM_FAST );

    m_boundingBox = EDA_RECT( wxPoint( 0, 0 ), wxSize( 1, 1 ) );
    auto bb = m_shape.BBox();
    wxPoint center( bb.Centre().x, bb.Centre().y );
    m_boundingBox.Move( aParent->GetABPosition( center ) );
    m_boundingBox.Inflate( bb.GetWidth() / 2, bb.GetHeight() / 2 );

    return &m_shape;
}


void APERTURE_MACRO::DrawApertureMacroShape( GERBER_DRAW_ITEM* aParent, EDA_RECT* aClipBox,
                                             wxDC* aDC, COLOR4D aColor, wxPoint aShapePos,
                                             bool aFilledShape )
{
    SHAPE_POLY_SET* shapeBuffer = GetApertureMacroShape( aParent, aShapePos );

    if( shapeBuffer->OutlineCount() == 0 )
        return;

    for( int ii = 0; ii < shapeBuffer->OutlineCount(); ii++ )
    {
        SHAPE_LINE_CHAIN& poly = shapeBuffer->Outline( ii );

        GRClosedPoly( aClipBox, aDC, poly.PointCount(), (wxPoint*) &poly.CPoint( 0 ), aFilledShape,
                      aColor, aColor );
    }
}


int APERTURE_MACRO::GetShapeDim( GERBER_DRAW_ITEM* aParent )
{
    int dim = -1;

    for( AM_PRIMITIVES::iterator prim_macro = primitives.begin();
         prim_macro != primitives.end(); ++prim_macro )
    {
        int pdim = prim_macro->GetShapeDim( aParent );

        if( dim < pdim )
            dim = pdim;
    }

    return dim;
}


double APERTURE_MACRO::GetLocalParam( const D_CODE* aDcode, unsigned aParamId ) const
{
    // find parameter descr.
    const AM_PARAM * param = nullptr;

    for( unsigned ii = 0; ii < m_localparamStack.size(); ii ++ )
    {
        if( m_localparamStack[ii].GetIndex() == aParamId )
        {
            param = &m_localparamStack[ii];
            break;
        }
    }

    if ( param == nullptr )    // not found
        return 0.0;

    // Evaluate parameter
    double value = param->GetValue( aDcode );

    return value;
}
