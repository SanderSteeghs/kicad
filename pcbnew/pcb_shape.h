/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 Jean-Pierre Charras jp.charras at wanadoo.fr
 * Copyright (C) 1992-2018 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef PCB_SHAPE_H
#define PCB_SHAPE_H

#include <class_board_item.h>
#include <eda_units.h>
#include <convert_to_biu.h>
#include <math/util.h>      // for KiROUND
#include <math_for_graphics.h>
#include <trigo.h>
#include <geometry/shape_poly_set.h>


class LINE_READER;
class EDA_DRAW_FRAME;
class FOOTPRINT;
class MSG_PANEL_ITEM;


class PCB_SHAPE : public BOARD_ITEM
{
protected:
    int                  m_width;        // thickness of lines ...
    bool                 m_filled;       // Pretty much what it says on the tin...
    wxPoint              m_start;        // Line start point or Circle and Arc center
    wxPoint              m_end;          // Line end point or circle and arc start point
    wxPoint              m_thirdPoint;   // Used only for Arcs: arc end point

    PCB_SHAPE_TYPE_T     m_shape;        // Shape: line, Circle, Arc
    double               m_angle;        // Used only for Arcs: Arc angle in 1/10 deg
    wxPoint              m_bezierC1;     // Bezier Control Point 1
    wxPoint              m_bezierC2;     // Bezier Control Point 2

    std::vector<wxPoint> m_bezierPoints;
    SHAPE_POLY_SET       m_poly;         // Stores the S_POLYGON shape

    // Computes the bounding box for an arc
    void computeArcBBox( EDA_RECT& aBBox ) const;

    const std::vector<wxPoint> buildBezierToSegmentsPointsList( int aMinSegLen  ) const;

public:
    PCB_SHAPE( BOARD_ITEM* aParent = NULL, KICAD_T idtype = PCB_SHAPE_T );

    // Do not create a copy constructor & operator=.
    // The ones generated by the compiler are adequate.

    ~PCB_SHAPE();

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && PCB_SHAPE_T == aItem->Type();
    }

    bool IsType( const KICAD_T aScanTypes[] ) const override
    {
        if( BOARD_ITEM::IsType( aScanTypes ) )
            return true;

        for( const KICAD_T* p = aScanTypes; *p != EOT; ++p )
        {
            if( *p == PCB_LOCATE_GRAPHIC_T )
                return true;
            else if( *p == PCB_LOCATE_BOARD_EDGE_T )
                return m_Layer == Edge_Cuts;
        }

        return false;
    }

    void SetFilled( bool aFlag ) { m_filled = aFlag; }

    bool IsFilled() const
    {
        switch( m_shape )
        {
        case S_RECT:
        case S_CIRCLE:
        case S_POLYGON:
            return m_filled;

        case S_SEGMENT:
        case S_ARC:
        case S_CURVE:
        case S_LAST:
            return false;
        }

        return false;   // Make compil happy
    }

    void SetWidth( int aWidth ) { m_width = aWidth; }
    int GetWidth() const        { return m_width; }

    /**
     * Function SetAngle
     * sets the angle for arcs, and normalizes it within the range 0 - 360 degrees.
     * @param aAngle is tenths of degrees, but will soon be degrees.
     */
    virtual void SetAngle( double aAngle, bool aUpdateEnd = true );
    double GetAngle() const { return m_angle; }

    void SetShape( PCB_SHAPE_TYPE_T aShape )       { m_shape = aShape; }
    PCB_SHAPE_TYPE_T GetShape() const              { return m_shape; }

    void SetBezControl1( const wxPoint& aPoint )    { m_bezierC1 = aPoint; }
    const wxPoint& GetBezControl1() const           { return m_bezierC1; }

    void SetBezControl2( const wxPoint& aPoint )    { m_bezierC2 = aPoint; }
    const wxPoint& GetBezControl2() const           { return m_bezierC2; }

    void SetPosition( const wxPoint& aPos ) override;
    wxPoint GetPosition() const override;

    /**
     * Function GetStart
     * returns the starting point of the graphic
     */
    const wxPoint& GetStart() const         { return m_start; }
    int GetStartY()                         { return m_start.y; }
    int GetStartX()                         { return m_start.x; }
    void SetStart( const wxPoint& aStart )  { m_start = aStart; }
    void SetStartY( int y )                 { m_start.y = y; }
    void SetStartX( int x )                 { m_start.x = x; }

    /**
     * Function GetEnd
     * returns the ending point of the graphic
     */
    const wxPoint& GetEnd() const           { return m_end; }
    int GetEndY()                           { return m_end.y; }
    int GetEndX()                           { return m_end.x; }
    void SetEnd( const wxPoint& aEnd )      { m_end = aEnd; }
    void SetEndY( int y )                   { m_end.y = y; }
    void SetEndX( int x )                   { m_end.x = x; }

    /**
     * Function GetThirdPoint
     * returns the third point point of the graphic
     */
    const wxPoint& GetThirdPoint() const           { return m_thirdPoint; }
    int GetThirdPointY()                           { return m_thirdPoint.y; }
    int GetThirdPointX()                           { return m_thirdPoint.x; }
    void SetThirdPoint( const wxPoint& aPoint )    { m_thirdPoint = aPoint; }
    void SetThirdPointY( int y )                   { m_thirdPoint.y = y; }
    void SetThirdPointX( int x )                   { m_thirdPoint.x = x; }

    // Some attributes are read only, since they are "calculated" from
    // m_Start, m_End, and m_Angle.
    // No Set...() function for these attributes.

    wxPoint GetCenter() const override;
    wxPoint GetArcStart() const      { return m_end; }
    wxPoint GetArcEnd() const;
    wxPoint GetArcMid() const;
    std::vector<wxPoint> GetRectCorners() const;

    /**
     * function GetArcAngleStart()
     * @return the angle of the starting point of this arc, between 0 and 3600 in 0.1 deg
     */
    double GetArcAngleStart() const;

    /**
     * function GetArcAngleEnd()
     * @return the angle of the ending point of this arc, between 0 and 3600 in 0.1 deg
     */
    double GetArcAngleEnd() const;

    /**
     * Function GetRadius
     * returns the radius of this item
     * Has meaning only for arc and circle
     */
    int GetRadius() const
    {
        double radius = GetLineLength( m_start, m_end );

        // don't allow degenerate arcs
        return std::max( 1, KiROUND( radius ) );
    }

    /**
     * Initialize the start arc point. can be used for circles
     * to initialize one point of the cicumference
     */
    void SetArcStart( const wxPoint& aArcStartPoint )
    {
        m_end = aArcStartPoint;
    }

    /**
     * Initialize the end arc point. can be used for circles
     * to initialize one point of the cicumference
     */
    void SetArcEnd( const wxPoint& aArcEndPoint )
    {
        m_thirdPoint = aArcEndPoint;
    }

    /** For arcs and circles:
     */
    void SetCenter( const wxPoint& aCenterPoint ) { m_start = aCenterPoint; }

    const wxPoint GetFocusPosition() const override
    {
        return GetCenter();
    }

    /**
     * Function GetParentFootprint
     * returns a pointer to the parent footprint, or NULL if PCB_SHAPE does not
     * belong to a footprint.
     * @return FOOTPRINT* - pointer to the parent footprint or NULL.
     */
    FOOTPRINT* GetParentFootprint() const;

    // Accessors:
    const std::vector<wxPoint>& GetBezierPoints() const { return m_bezierPoints; }

    /** Build and return the list of corners in a std::vector<wxPoint>
     * It must be used only to convert the SHAPE_POLY_SET internal corner buffer
     * to a list of wxPoints, and nothing else, because it duplicates the buffer,
     * that is inefficient to know for instance the corner count
     */
    const std::vector<wxPoint> BuildPolyPointsList() const;

    /**
     * @return the number of corners of the polygonal shape
     */
    int GetPointCount() const;

    // Accessors to the polygonal shape
    SHAPE_POLY_SET& GetPolyShape() { return m_poly; }
    const SHAPE_POLY_SET& GetPolyShape() const { return m_poly; }

    /**
     * @return true if the polygonal shape is valid (has more than 2 points)
     */
    bool IsPolyShapeValid() const;

    void SetPolyShape( const SHAPE_POLY_SET& aShape ) { m_poly = aShape; }

    void SetBezierPoints( const std::vector<wxPoint>& aPoints )
    {
        m_bezierPoints = aPoints;
    }

    /** Rebuild the m_BezierPoints vertex list that approximate the Bezier curve
     * by a list of segments
     * Has meaning only for S_CURVE DRAW_SEGMENT shape
     * @param aMinSegLen is the min length of segments approximating the shape.
     * the last segment can be shorter
     * This param avoid having too many very short segment in list.
     * a good value is m_Width/2 to m_Width
     */
    void RebuildBezierToSegmentsPointsList( int aMinSegLen );

    void SetPolyPoints( const std::vector<wxPoint>& aPoints );

    /**
     * Makes a set of SHAPE objects representing the PCB_SHAPE.  Caller owns the objects.
     */
    std::vector<SHAPE*> MakeEffectiveShapes() const; // fixme: move to shape_compound
    virtual std::shared_ptr<SHAPE> GetEffectiveShape( PCB_LAYER_ID aLayer = UNDEFINED_LAYER ) const override;

    void GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList ) override;

    const EDA_RECT GetBoundingBox() const override;

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override;
    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    wxString GetClass() const override
    {
        return wxT( "PCB_SHAPE" );
    }

    /**
     * Function GetLength
     * returns the length of the track using the hypotenuse calculation.
     * @return double - the length of the track
     */
    double  GetLength() const;

    virtual void Move( const wxPoint& aMoveVector ) override;

    virtual void Rotate( const wxPoint& aRotCentre, double aAngle ) override;

    virtual void Flip( const wxPoint& aCentre, bool aFlipLeftRight ) override;

    void Scale( double aScale );

    /**
     * Function TransformShapeWithClearanceToPolygon
     * Convert the draw segment to a closed polygon
     * Used in filling zones calculations
     * Circles and arcs are approximated by segments
     * @param aCornerBuffer = a buffer to store the polygon
     * @param aClearanceValue = the clearance around the pad
     * @param aError = the maximum deviation from a true arc
     * @param ignoreLineWidth = used for edge cut items where the line width is only
     * for visualization
     */
    void TransformShapeWithClearanceToPolygon( SHAPE_POLY_SET& aCornerBuffer,
                                               PCB_LAYER_ID aLayer, int aClearanceValue,
                                               int aError, ERROR_LOC aErrorLoc,
                                               bool ignoreLineWidth = false ) const override;

    virtual wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    virtual BITMAP_DEF GetMenuImage() const override;

    virtual EDA_ITEM* Clone() const override;

    virtual const BOX2I ViewBBox() const override;

    virtual void SwapData( BOARD_ITEM* aImage ) override;

    struct cmp_drawings
    {
        bool operator()( const BOARD_ITEM* aFirst, const BOARD_ITEM* aSecond ) const;
    };

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif
};

#endif  // PCB_SHAPE_H
