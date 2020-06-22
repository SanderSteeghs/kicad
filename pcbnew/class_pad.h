/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 1992-2020 KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file class_pad.h
 * @brief Pad object description
 */

#ifndef PAD_H_
#define PAD_H_

#include "zones.h"
#include <board_connected_item.h>
#include <class_board_item.h>
#include <convert_to_biu.h>
#include <geometry/shape_poly_set.h>
#include <pad_shapes.h>
#include <pcbnew.h>

class DRAWSEGMENT;
class PARAM_CFG;
class SHAPE;
class SHAPE_SEGMENT;

enum CUST_PAD_SHAPE_IN_ZONE
{
    CUST_PAD_SHAPE_IN_ZONE_OUTLINE,
    CUST_PAD_SHAPE_IN_ZONE_CONVEXHULL
};

class LINE_READER;
class EDA_3D_CANVAS;
class MODULE;
class EDGE_MODULE;
class TRACK;

namespace KIGFX
{
    class VIEW;
}

/** Helper class to handle a primitive (basic shape: polygon, segment, circle or arc)
 * to build a custom pad full shape from a set of primitives
 */
class PAD_CS_PRIMITIVE
{
public:
    STROKE_T m_Shape;   /// S_SEGMENT, S_RECT, S_ARC, S_CIRCLE, S_POLYGON only (same as DRAWSEGMENT)
    int m_Thickness;    /// thickness of segment or outline
                        /// For filled S_CIRCLE shape, thickness = 0.
                        // if thickness is not = 0 S_CIRCLE shape is a ring
    int m_Radius;       /// radius of a circle
    double m_ArcAngle;  /// angle of an arc, from its starting point, in 0.1 deg
    wxPoint m_Start;    /// is also the center of the circle and arc
    wxPoint m_End;      /// is also the start point of the arc
    wxPoint m_Ctrl1;    /// Bezier Control point 1
    wxPoint m_Ctrl2;    /// Bezier Control point 2
    std::vector<wxPoint> m_Poly;

    PAD_CS_PRIMITIVE( STROKE_T aShape ):
        m_Shape( aShape ), m_Thickness( 0 ), m_Radius( 0 ), m_ArcAngle( 0 )
    {
    }

    // Accessors (helpers for arc and circle shapes)
    wxPoint GetCenter() { return m_Start; }     /// returns the center of a circle or arc
    wxPoint GetArcStart() { return m_End; }     /// returns the start point of an arc

    // Geometric transform
    /** Move the primitive
     * @param aMoveVector is the deplacement vector
     */
    void Move( wxPoint aMoveVector );

    /**
     * Rotates the primitive about a point
     * @param aRotCentre center of rotation
     * @param aAngle angle in tenths of degree
     */
    void Rotate( const wxPoint& aRotCentre, double aAngle );

    /** Export the PAD_CS_PRIMITIVE parameters to a DRAWSEGMENT
     * useful to draw a primitive shape
     * @param aTarget is the DRAWSEGMENT to initialize
     */
    void ExportTo( DRAWSEGMENT* aTarget );

    /** Export the PAD_CS_PRIMITIVE parameters to a EDGE_MODULE
     * useful to convert a primitive shape to a EDGE_MODULE shape for editing in footprint editor
     * @param aTarget is the EDGE_MODULE to initialize
     */
    void ExportTo( EDGE_MODULE* aTarget );
};


class D_PAD : public BOARD_CONNECTED_ITEM
{
public:
    D_PAD( MODULE* parent );

    // Do not create a copy constructor & operator=.
    // The ones generated by the compiler are adequate.

    /* Default layers used for pads, according to the pad type.
     * this is default values only, they can be changed for a given pad
     */
    static LSET StandardMask();     ///< layer set for a through hole pad
    static LSET SMDMask();          ///< layer set for a SMD pad on Front layer
    static LSET ConnSMDMask();      ///< layer set for a SMD pad on Front layer
                                    ///< used for edge board connectors
    static LSET UnplatedHoleMask(); ///< layer set for a mechanical unplated through hole pad
    static LSET ApertureMask();     ///< layer set for an aperture pad

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && PCB_PAD_T == aItem->Type();
    }

    bool IsType( const KICAD_T aScanTypes[] ) const override
    {
        if( BOARD_CONNECTED_ITEM::IsType( aScanTypes ) )
            return true;

        for( const KICAD_T* p = aScanTypes; *p != EOT; ++p )
        {
            if( m_Drill.x > 0 && m_Drill.y > 0 )
            {
                if( *p == PCB_LOCATE_HOLE_T )
                    return true;
                else if( *p == PCB_LOCATE_PTH_T && m_Attribute != PAD_ATTRIB_HOLE_NOT_PLATED )
                    return true;
                else if( *p == PCB_LOCATE_NPTH_T && m_Attribute == PAD_ATTRIB_HOLE_NOT_PLATED )
                    return true;
            }
        }

        return false;
    }

    MODULE* GetParent() const { return (MODULE*) m_Parent; }

    /**
     * Imports the pad settings from aMasterPad.
     * The result is "this" has the same settinds (sizes, shapes ... )
     * as aMasterPad
     * @param aMasterPad = the template pad
     */
    void ImportSettingsFrom( const D_PAD& aMasterPad );

    /**
     * @return true if the pad has a footprint parent flipped
     * (on the back/bottom layer)
     */
    bool IsFlipped() const;

    /**
     * Set the pad name (sometimes called pad number, although
     * it can be an array reference like AA12).
     */
    void SetName( const wxString& aName ) { m_name = aName; }
    const wxString& GetName() const { return m_name; }

    /**
     * Set the pad function (pin name in schematic)
     */
    void SetPinFunction( const wxString& aName ) { m_pinFunction = aName; }
    const wxString& GetPinFunction() const { return m_pinFunction; }

    bool PadNameEqual( const D_PAD* other ) const
    {
        return m_name == other->m_name; // hide tricks behind sensible API
    }

    /**
     * Function GetShape
     * @return the shape of this pad.
     */
    void SetShape( PAD_SHAPE_T aShape ) { m_padShape = aShape; m_shapesDirty = true; }
    PAD_SHAPE_T GetShape() const { return m_padShape; }

    void SetPosition( const wxPoint& aPos ) override { m_Pos = aPos; }
    const wxPoint GetPosition() const override { return m_Pos; }

    /**
     * Function GetAnchorPadShape
     * @return the shape of the anchor pad shape, for custom shaped pads.
     */
    PAD_SHAPE_T GetAnchorPadShape() const       { return m_anchorPadShape; }

    /**
     * @return the option for the custom pad shape to use as clearance area
     * in copper zones
     */
    CUST_PAD_SHAPE_IN_ZONE GetCustomShapeInZoneOpt() const
    {
        return m_customShapeClearanceArea;
    }

    /**
     * Set the option for the custom pad shape to use as clearance area
     * in copper zones
     * @param aOption is the clearance area shape CUST_PAD_SHAPE_IN_ZONE option
     */
    void SetCustomShapeInZoneOpt( CUST_PAD_SHAPE_IN_ZONE aOption )
    {
        m_customShapeClearanceArea = aOption;
    }

    /**
     * Function SetAnchorPadShape
     * Set the shape of the anchor pad for custm shped pads.
     * @param the shape of the anchor pad shape( currently, only
     * PAD_SHAPE_RECT or PAD_SHAPE_CIRCLE.
     */
    void SetAnchorPadShape( PAD_SHAPE_T aShape )
    {
        m_anchorPadShape = ( aShape ==  PAD_SHAPE_RECT ) ? PAD_SHAPE_RECT : PAD_SHAPE_CIRCLE;
        m_shapesDirty = true;
    }

    /**
     * @return true if the pad is on any copper layer, false otherwise.
     * pads can be only on tech layers to build special pads.
     * they are therefore not always on a copper layer
     */
    bool IsOnCopperLayer() const override
    {
        return ( GetLayerSet() & LSET::AllCuMask() ) != 0;
    }

    void SetY( int y )                          { m_Pos.y = y; m_shapesDirty = true; }
    void SetX( int x )                          { m_Pos.x = x; m_shapesDirty = true; }

    void SetPos0( const wxPoint& aPos )         { m_Pos0 = aPos; }
    const wxPoint& GetPos0() const              { return m_Pos0; }

    void SetY0( int y )                         { m_Pos0.y = y; }
    void SetX0( int x )                         { m_Pos0.x = x; }

    void SetSize( const wxSize& aSize )         { m_Size = aSize; m_shapesDirty = true; }
    const wxSize& GetSize() const               { return m_Size; }

    void SetDelta( const wxSize& aSize )        { m_DeltaSize = aSize; m_shapesDirty = true; }
    const wxSize& GetDelta() const              { return m_DeltaSize; }

    void SetDrillSize( const wxSize& aSize )    { m_Drill = aSize; m_shapesDirty = true; }
    const wxSize& GetDrillSize() const          { return m_Drill; }

    void SetOffset( const wxPoint& aOffset )    { m_Offset = aOffset; m_shapesDirty = true; }
    const wxPoint& GetOffset() const            { return m_Offset; }

    /**
     * Has meaning only for free shape pads.
     * add a free shape to the shape list.
     * the shape can be
     *   a polygon (outline can have a thickness)
     *   a thick segment
     *   a filled circle or ring ( if thickness == 0, this is a filled circle, else a ring)
     *   a arc
     *   a curve
     */
    void AddPrimitivePoly( const SHAPE_POLY_SET& aPoly, int aThickness );
    void AddPrimitivePoly( const std::vector<wxPoint>& aPoly, int aThickness );
    void AddPrimitiveSegment( const wxPoint& aStart, const wxPoint& aEnd, int aThickness );
    void AddPrimitiveCircle( const wxPoint& aCenter, int aRadius, int aThickness );
    void AddPrimitiveRect( const wxPoint& aStart, const wxPoint& aEnd, int aThickness );
    void AddPrimitiveArc( const wxPoint& aCenter, const wxPoint& aStart, int aArcAngle,
                          int aThickness );
    void AddPrimitiveCurve( const wxPoint& aStart, const wxPoint& aEnd, const wxPoint& aCtrl1,
                            const wxPoint& aCtrl2, int aThickness );


    bool GetBestAnchorPosition( VECTOR2I& aPos );

    /**
     * Merge all basic shapes to a SHAPE_POLY_SET
     * Note: The corners coordinates are relative to the pad position, orientation 0,
     */
    void MergePrimitivesAsPolygon( SHAPE_POLY_SET* aMergedPolygon ) const;

    /**
     * clear the basic shapes list
     */
    void DeletePrimitivesList();

    /**
     * Accessor to the basic shape list
     */
    const std::vector<PAD_CS_PRIMITIVE>& GetPrimitives() const { return m_basicShapes; }

    void Flip( const wxPoint& aCentre, bool aFlipLeftRight ) override;

    /**
     * Flip the basic shapes, in custom pads
     */
    void FlipPrimitives();

    /**
     * Mirror the primitives about a coordinate
     */
    void MirrorXPrimitives( int aX );

    /**
     * Import to the basic shape list
     */
    void SetPrimitives( const std::vector<PAD_CS_PRIMITIVE>& aPrimitivesList );

    /**
     * Add to the basic shape list
     */
    void AddPrimitives( const std::vector<PAD_CS_PRIMITIVE>& aPrimitivesList );


    /**
     * Function SetOrientation
     * sets the rotation angle of the pad.
     * @param aAngle is tenths of degrees, but will soon be degrees.  If it is
     *  outside of 0 - 3600, then it will be normalized before being saved.
     */
    void SetOrientation( double aAngle );

    /**
     * Set orientation in degrees
     */
    void SetOrientationDegrees( double aOrientation ) { SetOrientation( aOrientation*10.0 ); }

    /**
     * Function GetOrientation
     * returns the rotation angle of the pad in tenths of degrees, but soon degrees.
     */
    double GetOrientation() const { return m_Orient; }
    double GetOrientationDegrees() const   { return m_Orient/10.0; }
    double GetOrientationRadians() const   { return m_Orient*M_PI/1800; }

    void SetDrillShape( PAD_DRILL_SHAPE_T aShape ) { m_drillShape = aShape; m_shapesDirty = true; }
    PAD_DRILL_SHAPE_T GetDrillShape() const     { return m_drillShape; }

    /**
     * JEY TODO: temporary until Tom is done with DRC stuff....
     */
    void GetOblongGeometry( const wxSize& aDrillOrPadSize,
                            wxPoint* aStartPoint, wxPoint* aEndPoint, int* aWidth ) const;

    void SetLayerSet( LSET aLayerMask )         { m_layerMask = aLayerMask; }
    LSET GetLayerSet() const override           { return m_layerMask; }

    void SetAttribute( PAD_ATTR_T aAttribute );
    PAD_ATTR_T GetAttribute() const             { return m_Attribute; }

    void SetProperty( PAD_PROP_T aProperty );
    PAD_PROP_T GetProperty() const              { return m_Property; }

    // We don't currently have an attribute for APERTURE, and adding one will change the file
    // format, so for now just infer a copper-less pad to be an APERTURE pad.
    bool IsAperturePad() const                  { return ( m_layerMask & LSET::AllCuMask() ).none(); }

    void SetPadToDieLength( int aLength )       { m_LengthPadToDie = aLength; }
    int GetPadToDieLength() const               { return m_LengthPadToDie; }

    int GetLocalSolderMaskMargin() const        { return m_LocalSolderMaskMargin; }
    void SetLocalSolderMaskMargin( int aMargin ) { m_LocalSolderMaskMargin = aMargin; }

    int GetLocalClearance( wxString* aSource = nullptr ) const override;
    void SetLocalClearance( int aClearance )    { m_LocalClearance = aClearance; }

    int GetLocalSolderPasteMargin() const       { return m_LocalSolderPasteMargin; }
    void SetLocalSolderPasteMargin( int aMargin ) { m_LocalSolderPasteMargin = aMargin; }

    double GetLocalSolderPasteMarginRatio() const { return m_LocalSolderPasteMarginRatio; }
    void SetLocalSolderPasteMarginRatio( double aRatio ) { m_LocalSolderPasteMarginRatio = aRatio; }

    /**
     * Function TransformShapeWithClearanceToPolygon
     * Convert the pad shape to a closed polygon. Circles and arcs are approximated by segments.
     * @param aCornerBuffer = a buffer to store the polygon
     * @param aClearanceValue = the clearance around the pad
     * @param aMaxError = maximum error from true when converting arcs
     * @param ignoreLineWidth = used for edge cuts where the line width is only for visualization
     */
    void TransformShapeWithClearanceToPolygon( SHAPE_POLY_SET& aCornerBuffer, int aClearanceValue,
                                               int aMaxError = ARC_HIGH_DEF,
                                               bool ignoreLineWidth = false ) const override;

    /**
     * Function TransformHoleWithClearanceToPolygon
     * Build the Corner list of the polygonal drill shape in the board coordinate system.
     * @param aCornerBuffer = a buffer to fill.
     * @param aInflateValue = the clearance or margin value.
     * @param aError = maximum deviation of an arc from the polygon approximation
     * @return false if the pad has no hole, true otherwise
     */
    bool TransformHoleWithClearanceToPolygon( SHAPE_POLY_SET& aCornerBuffer, int aInflateValue,
                                              int aError = ARC_HIGH_DEF ) const;

    /**
     * Function GetEffectiveShapes
     * Returns a list of SHAPE objects representing the pad's copper.
     */
    const std::vector<std::shared_ptr<SHAPE>>& GetEffectiveShapes() const;

    /**
     * Function GetEffectiveHoleShape
     * Returns a list of SHAPE objects representing the pad's hole.
     */
    const std::shared_ptr<SHAPE_SEGMENT>& GetEffectiveHoleShape() const;

    /**
     * Function GetBoundingRadius
     * returns the radius of a minimum sized circle which fully encloses this pad.
     * The center is the pad position NOT THE SHAPE POS!
     */
    int GetBoundingRadius() const;

    /**
     * Function GetLocalClearanceOverrides
     * returns any local clearance overrides set in the "classic" (ie: pre-rule) system.
     * @param aSource [out] optionally reports the source as a user-readable string
     * @return int - the clearance in internal units.
     */
    int GetLocalClearanceOverrides( wxString* aSource = nullptr ) const override;

   // Mask margins handling:

    /**
     * Function GetSolderMaskMargin
     * @return the margin for the solder mask layer
     * usually > 0 (mask shape bigger than pad
     * For pads also on copper layers, the value (used to build a default shape) is
     * 1 - the local value
     * 2 - if 0, the parent footprint value
     * 3 - if 0, the global value
     * For pads NOT on copper layers, the value is the local value because there is
     * not default shape to build
     */
    int GetSolderMaskMargin() const;

    /**
     * Function GetSolderPasteMargin
     * @return the margin for the solder mask layer
     * usually < 0 (mask shape smaller than pad)
     * because the margin can be dependent on the pad size, the margin has a x and a y value
     *
     * For pads also on copper layers, the value (used to build a default shape) is
     * 1 - the local value
     * 2 - if 0, the parent footprint value
     * 3 - if 0, the global value
     *
     * For pads NOT on copper layers, the value is the local value because there is
     * not default shape to build
    */
    wxSize GetSolderPasteMargin() const;

    void SetZoneConnection( ZONE_CONNECTION aType ) { m_ZoneConnection = aType; }
    ZONE_CONNECTION GetZoneConnection() const { return m_ZoneConnection; }

    /**
     * Return the zone connection in effect (either locally overridden or overridden in the
     * parent module).
     */
    ZONE_CONNECTION GetEffectiveZoneConnection() const;

    void SetThermalWidth( int aWidth ) { m_ThermalWidth = aWidth; }
    int GetThermalWidth() const;

    void SetThermalGap( int aGap ) { m_ThermalGap = aGap; }
    int GetThermalGap() const;

    /**
     * Function SetRoundRectCornerRadius
     * Has meaning only for rounded rect pads
     * @return The radius of the rounded corners for this pad.
     */
    void SetRoundRectCornerRadius( double aRadius );
    int GetRoundRectCornerRadius() const;

    wxPoint ShapePos() const;

    /**
     * has meaning only for rounded rect pads
     * @return the scaling factor between the smaller Y or Y size and the radius
     * of the rounded corners.
     * Cannot be > 0.5
     * the normalized IPC-7351C value is 0.25
     */
    void SetRoundRectRadiusRatio( double aRadiusScale );
    double GetRoundRectRadiusRatio() const { return m_padRoundRectRadiusScale; }

    /**
     * has meaning only for chamfered rect pads
     * Set the ratio between the smaller Y or Y size and the radius
     * of the rounded corners.
     * Cannot be < 0.5 and obviously must be > 0
     */
    void SetChamferRectRatio( double aChamferScale );
    double GetChamferRectRatio() const { return m_padChamferRectScale; }

    /**
     * has meaning only for chamfered rect pads
     * set the position of the chamfer for a 0 orientation, one of
     * RECT_CHAMFER_TOP_LEFT, RECT_CHAMFER_TOP_RIGHT,
     * RECT_CHAMFER_BOTTOM_LEFT, RECT_CHAMFER_BOTTOM_RIGHT
     */
    void SetChamferPositions( int aPositions ) { m_chamferPositions = aPositions; }
    int GetChamferPositions() const { return m_chamferPositions; }

    /**
     * Function GetSubRatsnest
     * @return int - the netcode
     */
    int GetSubRatsnest() const                  { return m_SubRatsnest; }
    void SetSubRatsnest( int aSubRatsnest )     { m_SubRatsnest = aSubRatsnest; }

    void GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList ) override;

    bool IsOnLayer( PCB_LAYER_ID aLayer ) const override
    {
        return m_layerMask[aLayer];
    }

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override;
    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    wxString GetClass() const override
    {
        return wxT( "PAD" );
    }

    // Virtual function:
    const EDA_RECT GetBoundingBox() const override;

    ///> Set absolute coordinates.
    void SetDrawCoord();

    //todo: Remove SetLocalCoord along with m_pos
    ///> Set relative coordinates.
    void SetLocalCoord();

    /**
     * Function Compare
     * compares two pads and return 0 if they are equal.
     * @return int - <0 if left less than right, 0 if equal, >0 if left greater than right.
     */
    static int Compare( const D_PAD* padref, const D_PAD* padcmp );

    void Move( const wxPoint& aMoveVector ) override
    {
        m_Pos += aMoveVector;
        SetLocalCoord();
    }

    void Rotate( const wxPoint& aRotCentre, double aAngle ) override;

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    BITMAP_DEF GetMenuImage() const override;

    /**
     * Function ShowPadShape
     * @return the name of the shape
     */
    wxString ShowPadShape() const;

    /**
     * Function ShowPadAttr
     * @return the name of the pad type (attribute) : STD, SMD ...
     */
    wxString ShowPadAttr() const;

    /**
     * Function AppendConfigs
     * appends to @a aResult the configuration setting accessors which will later
     * allow reading or writing of configuration file information directly into
     * this object.
     */
    void AppendConfigs( std::vector<PARAM_CFG*>* aResult );

    EDA_ITEM* Clone() const override;

    /**
     * same as Clone, but returns a D_PAD item.
     * Useful mainly for python scripts, because Clone returns an EDA_ITEM.
     */
    D_PAD* ClonePad() const
    {
        return (D_PAD*) Clone();
    }

    /**
     * A pad whose hole is the same size as the pad is a NPTH.  However, if the user
     * fails to mark this correctly then the pad will become invisible on the board.
     * This check allows us to special-case this error-condition.
     */
    bool PadShouldBeNPTH() const;

    virtual void ViewGetLayers( int aLayers[], int& aCount ) const override;

    virtual unsigned int ViewGetLOD( int aLayer, KIGFX::VIEW* aView ) const override;

    virtual const BOX2I ViewBBox() const override;

    virtual void SwapData( BOARD_ITEM* aImage ) override;

#if defined(DEBUG)
    virtual void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif


private:
    /**
     * Function calcBoundingRadius
     * returns a calculated radius of a bounding circle for this pad.
     */
    int calcBoundingRadius() const;

    void addCustomPadPrimitivesToPolygon( SHAPE_POLY_SET* aMergedPolygon, int aError ) const;

    void buildEffectiveShapes() const;

private:
    wxString    m_name;             ///< pad name (pin number in schematic)
    wxString    m_pinFunction;      ///< pin function in schematic

    wxPoint     m_Pos;              ///< pad Position on board

    PAD_SHAPE_T m_padShape;         ///< Shape: PAD_SHAPE_CIRCLE, PAD_SHAPE_RECT,
                                    ///< PAD_SHAPE_OVAL, PAD_SHAPE_TRAPEZOID,
                                    ///< PAD_SHAPE_ROUNDRECT, PAD_SHAPE_POLYGON

    mutable bool                                m_shapesDirty;
    mutable int                                 m_effectiveBoundingRadius;
    mutable std::vector<std::shared_ptr<SHAPE>> m_effectiveShapes;
    mutable std::shared_ptr<SHAPE_SEGMENT>      m_effectiveHoleShape;

    /** for free shape pads: a list of basic shapes,
     * in local coordinates, orient 0, coordinates relative to m_Pos
     * They are expected to define only one copper area.
     */
    std::vector<PAD_CS_PRIMITIVE> m_basicShapes;

    /**
     * How to build the custom shape in zone, to create the clearance area:
     * CUST_PAD_SHAPE_IN_ZONE_OUTLINE = use pad shape
     * CUST_PAD_SHAPE_IN_ZONE_CONVEXHULL = use the convex hull of the pad shape
     * other values are currently reserved
     */
    CUST_PAD_SHAPE_IN_ZONE  m_customShapeClearanceArea;

    int         m_SubRatsnest;      ///< variable used in rats nest computations
                                    ///< handle subnet (block) number in ratsnest connection

    wxSize      m_Drill;            ///< Drill diam (drill shape = PAD_CIRCLE) or drill size
                                    ///< (shape = OVAL) for drill shape = PAD_CIRCLE, drill
                                    ///< diam = m_Drill.x

    wxSize      m_Size;             ///< X and Y size ( relative to orient 0)

    PAD_DRILL_SHAPE_T m_drillShape; ///< PAD_DRILL_SHAPE_CIRCLE, PAD_DRILL_SHAPE_OBLONG

    double      m_padRoundRectRadiusScale;  ///< scaling factor from smallest m_Size coord
                                            ///< to corner radius, default 0.25
    double      m_padChamferRectScale;      ///< scaling factor from smallest m_Size coord
                                            ///< to chamfer value, default 0.25
    int         m_chamferPositions;         ///< the positions of the chamfers for a 0 orientation

    PAD_SHAPE_T m_anchorPadShape;           ///< for custom shaped pads: shape of pad anchor,
                                            ///< PAD_SHAPE_RECT, PAD_SHAPE_CIRCLE

    /**
     * m_Offset is useful only for oblong and rect pads (it can be used for other
     * shapes, but without any interest).
     * This is the offset between the pad hole and the pad shape (you must
     * understand here pad shape = copper area around the hole)
     * Most of cases, the hole is the center of the shape (m_Offset = 0).
     * But some board designers use oblong/rect pads with a hole moved to one of the
     * oblong/rect pad shape ends.
     * In all cases the pad position is the pad hole.
     * The physical shape position (used to draw it for instance) is pad
     * position (m_Pos) + m_Offset.
     * D_PAD::ShapePos() returns the physical shape position according to
     * the offset and the pad rotation.
     */
    wxPoint     m_Offset;

    LSET        m_layerMask;        ///< Bitwise layer :1= copper layer, 15= cmp,
                                    ///< 2..14 = internal layers
                                    ///< 16 .. 31 = technical layers

    wxSize      m_DeltaSize;        ///< delta on rectangular shapes

    wxPoint     m_Pos0;             ///< Initial Pad position (i.e. pad position relative to the
                                    ///< module anchor, orientation 0)

    PAD_ATTR_T  m_Attribute;        ///< PAD_ATTRIB_NORMAL, PAD_ATTRIB_SMD,
                                    ///< PAD_ATTRIB_CONN, PAD_ATTRIB_HOLE_NOT_PLATED
    PAD_PROP_T  m_Property;         ///< property in fab files (BGA, FIDUCIAL, TEST POINT, CASTELLATED)

    double      m_Orient;           ///< in 1/10 degrees

    int         m_LengthPadToDie;   ///< Length net from pad to die, inside the package

    /// Local clearance. When null, the module default value is used.
    /// when the module default value is null, the netclass value is used
    /// Usually the local clearance is null
    int         m_LocalClearance;

    /// Local mask margins: when 0, the parent footprint design values are used

    int         m_LocalSolderMaskMargin;        ///< Local solder mask margin
    int         m_LocalSolderPasteMargin;       ///< Local solder paste margin absolute value

    double      m_LocalSolderPasteMarginRatio;  ///< Local solder mask margin ratio value of pad size
                                                ///< The final margin is the sum of these 2 values
    /// how the connection to zone is made: no connection, thermal relief ...
    ZONE_CONNECTION m_ZoneConnection;

    int         m_ThermalWidth;
    int         m_ThermalGap;
};

#endif  // PAD_H_
