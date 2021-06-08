/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2004 Jean-Pierre Charras, jaen-pierre.charras@gipsa-lab.inpg.com
 * Copyright (C) 2004-2021 KiCad Developers, see change_log.txt for contributors.
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
 * @file sch_bus_entry.h
 *
 */

#ifndef _SCH_BUS_ENTRY_H_
#define _SCH_BUS_ENTRY_H_

#include <gal/color4d.h>
#include <sch_item.h>

#define TARGET_BUSENTRY_RADIUS Mils2iu( 12 )   // Circle diameter drawn at the ends


/**
 * Base class for a bus or wire entry.
 */
class SCH_BUS_ENTRY_BASE : public SCH_ITEM
{
public:
    SCH_BUS_ENTRY_BASE( KICAD_T aType, const wxPoint& pos = wxPoint( 0, 0 ), bool aFlipY = false );

    bool IsDanglingStart() const { return m_isDanglingStart; }
    bool IsDanglingEnd() const { return m_isDanglingEnd; }

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    ~SCH_BUS_ENTRY_BASE() { }

    /**
     * Return true for items which are moved with the anchor point at mouse cursor
     *  and false for items moved with no reference to anchor
     * @return false for a bus entry
     */
    bool IsMovableFromAnchorPoint() const override { return false; }

    wxPoint GetEnd() const;

    wxSize GetSize() const { return m_size; }
    void SetSize( const wxSize& aSize ) { m_size = aSize; }

    void SetPenWidth( int aWidth ) { m_stroke.SetWidth( aWidth ); }

    virtual bool HasLineStroke() const override { return true; }
    virtual STROKE_PARAMS GetStroke() const override { return m_stroke; }
    virtual void SetStroke( const STROKE_PARAMS& aStroke ) override { m_stroke = aStroke; }

    PLOT_DASH_TYPE GetStrokeStyle() const;
    void SetStrokeStyle( PLOT_DASH_TYPE aStyle ) { m_stroke.SetPlotStyle( aStyle ); }

    COLOR4D GetStrokeColor() const;
    void SetStrokeColor( const COLOR4D& aColor ) { m_stroke.SetColor( aColor ); }

    void SwapData( SCH_ITEM* aItem ) override;

    void ViewGetLayers( int aLayers[], int& aCount ) const override;

    void Print( const RENDER_SETTINGS* aSettings, const wxPoint& aOffset ) override;

    const EDA_RECT GetBoundingBox() const override;

    void Move( const wxPoint& aMoveVector ) override
    {
        m_pos += aMoveVector;
    }

    void MirrorHorizontally( int aCenter ) override;
    void MirrorVertically( int aCenter ) override;
    void Rotate( const wxPoint& aCenter ) override;

    bool IsDangling() const override;

    bool IsConnectable() const override { return true; }

    std::vector<wxPoint> GetConnectionPoints() const override;

    wxPoint GetPosition() const override { return m_pos; }
    void SetPosition( const wxPoint& aPosition ) override { m_pos = aPosition; }

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override;
    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    void Plot( PLOTTER* aPlotter ) const override;

    void GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList ) override;

    bool operator <( const SCH_ITEM& aItem ) const override;

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif

private:
    bool doIsConnected( const wxPoint& aPosition ) const override;

protected:
    wxPoint       m_pos;
    wxSize        m_size;
    bool          m_isDanglingStart;
    bool          m_isDanglingEnd;
    STROKE_PARAMS m_stroke;
};

/**
 * Class for a wire to bus entry.
 */
class SCH_BUS_WIRE_ENTRY : public SCH_BUS_ENTRY_BASE
{
public:
    SCH_BUS_WIRE_ENTRY( const wxPoint& pos = wxPoint( 0, 0 ), bool aFlipY = false );

    ~SCH_BUS_WIRE_ENTRY() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_BUS_WIRE_ENTRY_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_BUS_WIRE_ENTRY" );
    }

    int GetPenWidth() const override;

    void GetEndPoints( std::vector< DANGLING_END_ITEM >& aItemList ) override;

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return aItem->Type() == SCH_LINE_T &&
                ( aItem->GetLayer() == LAYER_WIRE || aItem->GetLayer() == LAYER_BUS );
    }

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    EDA_ITEM* Clone() const override;

    virtual bool ConnectionPropagatesTo( const EDA_ITEM* aItem ) const override;

    BITMAPS GetMenuImage() const override;

    bool UpdateDanglingState( std::vector<DANGLING_END_ITEM>& aItemList,
                              const SCH_SHEET_PATH* aPath = nullptr ) override;

    /**
     * Pointer to the bus item (usually a bus wire) connected to this bus-wire
     * entry, if it is connected to one.
     */
    SCH_ITEM* m_connected_bus_item;
};

/**
 * Class for a bus to bus entry.
 */
class SCH_BUS_BUS_ENTRY : public SCH_BUS_ENTRY_BASE
{
public:
    SCH_BUS_BUS_ENTRY( const wxPoint& pos = wxPoint( 0, 0 ), bool aFlipY = false );

    ~SCH_BUS_BUS_ENTRY() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_BUS_BUS_ENTRY_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_BUS_BUS_ENTRY" );
    }

    int GetPenWidth() const override;

    void GetEndPoints( std::vector< DANGLING_END_ITEM >& aItemList ) override;

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return aItem->Type() == SCH_LINE_T && aItem->GetLayer() == LAYER_BUS;
    }

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    EDA_ITEM* Clone() const override;

    BITMAPS GetMenuImage() const override;

    bool UpdateDanglingState( std::vector<DANGLING_END_ITEM>& aItemList,
                              const SCH_SHEET_PATH* aPath = nullptr ) override;

    /**
     * Pointer to the bus items (usually bus wires) connected to this bus-bus
     * entry (either or both may be nullptr)
     */
    SCH_ITEM* m_connected_bus_items[2];
};

#endif    // _SCH_BUS_ENTRY_H_
