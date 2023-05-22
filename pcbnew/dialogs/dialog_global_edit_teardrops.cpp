/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <widgets/unit_binder.h>
#include <pcb_edit_frame.h>
#include <board.h>
#include <board_design_settings.h>
#include <pcb_track.h>
#include <pcb_group.h>
#include <footprint.h>
#include <teardrop/teardrop.h>
#include <zone_filler.h>
#include <connectivity/connectivity_data.h>
#include <pcb_layer_box_selector.h>
#include <tool/tool_manager.h>
#include <tools/global_edit_tool.h>
#include "dialog_global_edit_teardrops_base.h"


// Globals to remember control settings during a session
static bool         g_vias = true;
static bool         g_pthPads = true;
static bool         g_smdPads = true;
static bool         g_trackToTrack = false;
static bool         g_filterByNetclass;
static wxString     g_netclassFilter;
static bool         g_filterByNet;
static wxString     g_netFilter;
static bool         g_filterByLayer;
static int          g_layerFilter;
static bool         g_filterRoundPads = false;
static bool         g_filterSelected = false;
static int          g_action = 1;


class DIALOG_GLOBAL_EDIT_TEARDROPS : public DIALOG_GLOBAL_EDIT_TEARDROPS_BASE
{
public:
    DIALOG_GLOBAL_EDIT_TEARDROPS( PCB_EDIT_FRAME* aParent );
    ~DIALOG_GLOBAL_EDIT_TEARDROPS() override;

protected:
    void onSpecifiedValuesUpdateUi( wxUpdateUIEvent& event ) override
    {
        event.Enable( m_specifiedValues->GetValue() );
    }
    void onCurvedEdgesUpdateUi( wxUpdateUIEvent& event ) override
    {
        event.Enable( m_specifiedValues->GetValue() && m_rbCurved->GetValue() );
    }
    void onFilterUpdateUi( wxUpdateUIEvent& event ) override
    {
        event.Enable( !m_trackToTrack->GetValue() );
    }

    // We can't use standard wxWidgets radio button processing for these because you must
    // be able to turn them both off to set a "don't modify" state
    void onStraightLines( wxMouseEvent& event ) override
    {
        if( !m_rbStraightLines->GetValue() )
        {
            m_rbStraightLines->SetValue( true );
            m_rbAntiCurved->SetValue( true );
            m_rbCurved->SetValue( false );
        }
        else
        {
            m_rbAntiStraight->SetValue( true );
            m_rbStraightLines->SetValue( false );
        }
    }
    void onCurvedLines( wxMouseEvent& event ) override
    {
        if( !m_rbCurved->GetValue() )
        {
            m_rbCurved->SetValue( true );
            m_rbAntiStraight->SetValue( true );
            m_rbStraightLines->SetValue( false );
        }
        else
        {
            m_rbAntiCurved->SetValue( true );
            m_rbCurved->SetValue( false );
        }
    }

    // Track-to-track teardrops always follow the document-wide settings (there are no teardrop
    // properties on individual track segments as they're too ephemeral).  Therefore we disable
    // Set-to-specified-values when track-to-track is selected.
    void onTrackToTrack( wxCommandEvent& event ) override
    {
        if( event.IsChecked() && m_specifiedValues->GetValue() )
        {
            m_specifiedValues->SetValue( false );
            m_addTeardrops->SetValue( true );
        }
    }

    // These just improve usability so that you don't have to click twice to enable a filter.
    void OnNetclassFilterSelect( wxCommandEvent& event ) override
    {
        m_netclassFilterOpt->SetValue( true );
    }
    void OnLayerFilterSelect( wxCommandEvent& event ) override
    {
        m_layerFilterOpt->SetValue( true );
    }
    void OnNetFilterSelect( wxCommandEvent& event )
    {
        m_netFilterOpt->SetValue( true );
    }

    // Remove "add" terminology when updating only existing teardrops.
    void OnExistingFilterSelect( wxCommandEvent& event ) override
    {
        if( event.IsChecked() )
        {
            m_addTeardrops->SetLabel( _( "Set teardrops to default values for shape" ) );
            m_specifiedValues->SetLabel( _( "Set teardrops to specified values:" ) );
        }
        else
        {
            m_addTeardrops->SetLabel( _( "Add teardrops with default values for shape" ) );
            m_specifiedValues->SetLabel( _( "Add teardrops with specified values:" ) );
        }
    }

    void setSpecifiedParams( TEARDROP_PARAMETERS* targetParams );
    void visitItem( BOARD_COMMIT* aCommit, BOARD_CONNECTED_ITEM* aItem );
    void processItem( BOARD_COMMIT* aCommit, BOARD_CONNECTED_ITEM* aItem );

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;


    void onShowBoardSetup( wxHyperlinkEvent& event ) override
    {
        m_parent->ShowBoardSetupDialog( _( "Teardrops" ) );
    }

    void buildFilterLists();

private:
    PCB_EDIT_FRAME* m_parent;
    BOARD*          m_brd;
    PCB_SELECTION   m_selection;

    UNIT_BINDER     m_teardropHDPercent;
    UNIT_BINDER     m_teardropLenPercent;
    UNIT_BINDER     m_teardropMaxLen;
    UNIT_BINDER     m_teardropHeightPercent;
    UNIT_BINDER     m_teardropMaxHeight;
    UNIT_BINDER     m_curvePoints;
};


DIALOG_GLOBAL_EDIT_TEARDROPS::DIALOG_GLOBAL_EDIT_TEARDROPS( PCB_EDIT_FRAME* aParent ) :
        DIALOG_GLOBAL_EDIT_TEARDROPS_BASE( aParent ),
        m_teardropHDPercent( aParent, m_stHDRatio, m_tcHDRatio, m_stHDRatioUnits ),
        m_teardropLenPercent( aParent, m_stLenPercentLabel, m_tcLenPercent, m_stLenPercentUnits ),
        m_teardropMaxLen( aParent, m_stMaxLen, m_tcTdMaxLen, m_stMaxLenUnits ),
        m_teardropHeightPercent( aParent, m_stHeightPercentLabel, m_tcHeightPercent, m_stHeightPercentUnits ),
        m_teardropMaxHeight( aParent, m_stMaxHeight, m_tcMaxHeight, m_stMaxHeightUnits ),
        m_curvePoints( aParent, m_curvePointsLabel, m_curvePointsCtrl, nullptr )
{
    m_parent = aParent;
    m_brd = m_parent->GetBoard();

    m_bitmapTeardrop->SetBitmap( KiBitmap( BITMAPS::teardrop_sizes ) );

    m_teardropHDPercent.SetUnits( EDA_UNITS::PERCENT );
    m_teardropLenPercent.SetUnits( EDA_UNITS::PERCENT );
    m_teardropHeightPercent.SetUnits( EDA_UNITS::PERCENT );
    m_curvePoints.SetUnits( EDA_UNITS::UNSCALED );

    m_minTrackWidthHint->SetFont( KIUI::GetInfoFont( this ).Italic() );

    buildFilterLists();

    SetupStandardButtons();

    m_netFilter->Connect( NET_SELECTED,
                          wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEARDROPS::OnNetFilterSelect ),
                          nullptr, this );

    finishDialogSettings();
}


DIALOG_GLOBAL_EDIT_TEARDROPS::~DIALOG_GLOBAL_EDIT_TEARDROPS()
{
    g_vias = m_vias->GetValue();
    g_pthPads = m_pthPads->GetValue();
    g_smdPads = m_smdPads->GetValue();
    g_trackToTrack = m_trackToTrack->GetValue();
    g_filterByNetclass = m_netclassFilterOpt->GetValue();
    g_netclassFilter = m_netclassFilter->GetStringSelection();
    g_filterByNet = m_netFilterOpt->GetValue();
    g_netFilter = m_netFilter->GetSelectedNetname();
    g_filterByLayer = m_layerFilterOpt->GetValue();
    g_layerFilter = m_layerFilter->GetLayerSelection();
    g_filterRoundPads = m_roundPadsFilter->GetValue();
    g_filterSelected = m_selectedItemsFilter->GetValue();

    if( m_removeTeardrops->GetValue() )
        g_action = 0;
    else if( m_specifiedValues->GetValue() )
        g_action = 2;
    else
        g_action = 1;

    m_netFilter->Disconnect( NET_SELECTED,
                             wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEARDROPS::OnNetFilterSelect ),
                             nullptr, this );
}


void DIALOG_GLOBAL_EDIT_TEARDROPS::buildFilterLists()
{
    // Populate the net filter list with net names
    m_netFilter->SetBoard( m_brd );
    m_netFilter->SetNetInfo( &m_brd->GetNetInfo() );

    if( !m_brd->GetHighLightNetCodes().empty() )
        m_netFilter->SetSelectedNetcode( *m_brd->GetHighLightNetCodes().begin() );

    // Populate the netclass filter list with netclass names
    wxArrayString                  netclassNames;
    std::shared_ptr<NET_SETTINGS>& settings = m_brd->GetDesignSettings().m_NetSettings;

    netclassNames.push_back( settings->m_DefaultNetClass->GetName() );

    for( const auto& [ name, netclass ] : settings->m_NetClasses )
        netclassNames.push_back( name );

    m_netclassFilter->Set( netclassNames );
    m_netclassFilter->SetStringSelection( m_brd->GetDesignSettings().GetCurrentNetClassName() );

    // Populate the layer filter list
    m_layerFilter->SetBoardFrame( m_parent );
    m_layerFilter->SetLayersHotkeys( false );
    m_layerFilter->SetNotAllowedLayerSet( LSET::AllNonCuMask() );
    m_layerFilter->Resync();
    m_layerFilter->SetLayerSelection( m_parent->GetActiveLayer() );
}


bool DIALOG_GLOBAL_EDIT_TEARDROPS::TransferDataToWindow()
{
    PCB_SELECTION_TOOL* selTool = m_parent->GetToolManager()->GetTool<PCB_SELECTION_TOOL>();
    m_selection                 = selTool->GetSelection();
    BOARD_CONNECTED_ITEM* item  = dynamic_cast<BOARD_CONNECTED_ITEM*>( m_selection.Front() );

    m_vias->SetValue( g_vias );
    m_pthPads->SetValue( g_pthPads );
    m_smdPads->SetValue( g_smdPads );
    m_trackToTrack->SetValue( g_trackToTrack );

    if( g_filterByNetclass && m_netclassFilter->SetStringSelection( g_netclassFilter ) )
        m_netclassFilterOpt->SetValue( true );
    else if( item )
        m_netclassFilter->SetStringSelection( item->GetNet()->GetNetClass()->GetName() );

    if( g_filterByNet && m_brd->FindNet( g_netFilter ) != nullptr )
    {
        m_netFilter->SetSelectedNet( g_netFilter );
        m_netFilterOpt->SetValue( true );
    }
    else if( item )
    {
        m_netFilter->SetSelectedNetcode( item->GetNetCode() );
    }

    if( g_filterByLayer && m_layerFilter->SetLayerSelection( g_layerFilter ) != wxNOT_FOUND )
    {
        m_layerFilterOpt->SetValue( true );
    }
    else if( item )
    {
        if( item->Type() == PCB_ZONE_T ) // a zone can be on more than one layer
            m_layerFilter->SetLayerSelection( static_cast<ZONE*>(item)->GetFirstLayer() );
        else
            m_layerFilter->SetLayerSelection( item->GetLayer() );
    }

    m_roundPadsFilter->SetValue( g_filterRoundPads );
    m_selectedItemsFilter->SetValue( g_filterSelected );

    if( g_action == 0 )
        m_removeTeardrops->SetValue( true );
    else if( g_action == 1 )
        m_addTeardrops->SetValue( true );
    else
        m_specifiedValues->SetValue( true );

    m_cbPreferZoneConnection->Set3StateValue( wxCHK_UNDETERMINED );
    m_cbTeardropsUseNextTrack->Set3StateValue( wxCHK_UNDETERMINED );
    m_teardropHDPercent.SetValue( INDETERMINATE_ACTION );
    m_teardropLenPercent.SetValue( INDETERMINATE_ACTION );
    m_teardropMaxLen.SetValue( INDETERMINATE_ACTION );
    m_teardropHeightPercent.SetValue( INDETERMINATE_ACTION );
    m_teardropMaxHeight.SetValue( INDETERMINATE_ACTION );
    m_curvePoints.SetValue( INDETERMINATE_ACTION );

    return true;
}


void DIALOG_GLOBAL_EDIT_TEARDROPS::setSpecifiedParams( TEARDROP_PARAMETERS* targetParams )
{
    if( m_cbPreferZoneConnection->Get3StateValue() != wxCHK_UNDETERMINED )
        targetParams->m_TdOnPadsInZones = !m_cbPreferZoneConnection->GetValue();

    if( m_cbTeardropsUseNextTrack->Get3StateValue() != wxCHK_UNDETERMINED )
        targetParams->m_AllowUseTwoTracks = m_cbTeardropsUseNextTrack->GetValue();

    if( !m_teardropHDPercent.IsIndeterminate() )
        targetParams->m_WidthtoSizeFilterRatio = m_teardropHDPercent.GetDoubleValue() / 100.0;

    if( !m_teardropLenPercent.IsIndeterminate() )
        targetParams->m_BestLengthRatio = m_teardropLenPercent.GetDoubleValue() / 100.0;

    if( !m_teardropMaxLen.IsIndeterminate() )
        targetParams->m_TdMaxLen = m_teardropMaxLen.GetIntValue();

    if( !m_teardropHeightPercent.IsIndeterminate() )
        targetParams->m_BestWidthRatio = m_teardropHeightPercent.GetDoubleValue() / 100.0;

    if( !m_teardropMaxHeight.IsIndeterminate() )
        targetParams->m_TdMaxWidth = m_teardropMaxHeight.GetIntValue();

    if( m_rbStraightLines->GetValue() )
    {
        targetParams->m_CurveSegCount = 0;
    }
    else if( m_rbCurved->GetValue() )
    {
        if( !m_curvePoints.IsIndeterminate() )
            targetParams->m_CurveSegCount = m_curvePoints.GetIntValue();
        else if( targetParams->m_CurveSegCount == 0 )
            targetParams->m_CurveSegCount = 5;
    }
}


void DIALOG_GLOBAL_EDIT_TEARDROPS::processItem( BOARD_COMMIT* aCommit, BOARD_CONNECTED_ITEM* aItem )
{
    BOARD_DESIGN_SETTINGS& brdSettings = m_brd->GetDesignSettings();
    TEARDROP_PARAMETERS*   targetParams = nullptr;

    if( aItem->Type() == PCB_PAD_T )
        targetParams = &static_cast<PAD*>( aItem )->GetTeardropParams();
    else if( aItem->Type() == PCB_VIA_T )
        targetParams = &static_cast<PCB_VIA*>( aItem )->GetTeardropParams();
    else
        return;

    aCommit->Stage( aItem, CHT_MODIFY );

    if( m_removeTeardrops->GetValue() )
    {
        targetParams->m_Enabled = false;
    }
    else if( m_addTeardrops->GetValue() )
    {
        if( TEARDROP_MANAGER::IsRound( aItem ) )
            *targetParams = *brdSettings.GetTeadropParamsList()->GetParameters( TARGET_ROUND );
        else
            *targetParams = *brdSettings.GetTeadropParamsList()->GetParameters( TARGET_RECT );

        targetParams->m_Enabled = true;
    }
    else if( m_specifiedValues->GetValue() )
    {
        setSpecifiedParams( targetParams );

        if( !m_existingFilter->GetValue() )
            targetParams->m_Enabled = true;
    }
}


void DIALOG_GLOBAL_EDIT_TEARDROPS::visitItem( BOARD_COMMIT* aCommit, BOARD_CONNECTED_ITEM* aItem )
{
    if( m_selectedItemsFilter->GetValue() )
    {
        if( !aItem->IsSelected() )
        {
            PCB_GROUP* group = aItem->GetParentGroup();

            while( group && !group->IsSelected() )
                group = group->GetParentGroup();

            if( !group )
                return;
        }
    }

    if( m_netFilterOpt->GetValue() && m_netFilter->GetSelectedNetcode() >= 0 )
    {
        if( aItem->GetNetCode() != m_netFilter->GetSelectedNetcode() )
            return;
    }

    if( m_netclassFilterOpt->GetValue() && !m_netclassFilter->GetStringSelection().IsEmpty() )
    {
        if( aItem->GetEffectiveNetClass()->GetName() != m_netclassFilter->GetStringSelection() )
            return;
    }

    if( m_layerFilterOpt->GetValue() && m_layerFilter->GetLayerSelection() != UNDEFINED_LAYER )
    {
        if( aItem->GetLayer() != m_layerFilter->GetLayerSelection() )
            return;
    }

    if( m_roundPadsFilter->GetValue() )
    {
        if( !TEARDROP_MANAGER::IsRound( aItem ) )
            return;
    }

    if( m_existingFilter->GetValue() )
    {
        if( aItem->Type() == PCB_PAD_T )
        {
            if( !static_cast<PAD*>( aItem )->GetTeardropParams().m_Enabled )
                return;
        }
        else if( aItem->Type() == PCB_VIA_T )
        {
            if( !static_cast<PCB_VIA*>( aItem )->GetTeardropParams().m_Enabled )
                return;
        }
    }

    processItem( aCommit, aItem );
}


bool DIALOG_GLOBAL_EDIT_TEARDROPS::TransferDataFromWindow()
{
    m_brd->SetLegacyTeardrops( false );

    BOARD_COMMIT commit( m_parent );
    wxBusyCursor dummy;

    if( m_vias->GetValue() )
    {
        for( PCB_TRACK* track : m_brd->Tracks() )
        {
            if ( track->Type() == PCB_VIA_T )
                visitItem( &commit, track );
        }
    }

    for( FOOTPRINT* footprint : m_brd->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            if( m_pthPads->GetValue() && pad->GetAttribute() == PAD_ATTRIB::PTH )
            {
                visitItem( &commit, pad );
            }
            else if( m_smdPads->GetValue() && ( pad->GetAttribute() == PAD_ATTRIB::SMD
                                                || pad->GetAttribute() == PAD_ATTRIB::CONN ) )
            {
                visitItem( &commit, pad );
            }
        }
    }

    if( m_trackToTrack->GetValue() )
    {
        TEARDROP_PARAMETERS_LIST* paramsList = m_brd->GetDesignSettings().GetTeadropParamsList();
        TEARDROP_PARAMETERS*      targetParams = paramsList->GetParameters( TARGET_TRACK );
        TEARDROP_MANAGER          teardropManager( m_brd, m_parent->GetToolManager() );

        teardropManager.DeleteTrackToTrackTeardrops( commit );

        if( m_removeTeardrops->GetValue() )
        {
            targetParams->m_Enabled = false;    // JEY TODO: how does this get undone/redone?
        }
        else if( m_addTeardrops->GetValue() )
        {
            targetParams->m_Enabled = true;     // JEY TODO: how does this get undone/redone?
            teardropManager.AddTeardropsOnTracks( commit, nullptr, true );
        }
    }

    // If there are no filters then a force-full-update is equivalent, and will be faster.
    if( !m_netFilterOpt->GetValue()
            && !m_netclassFilterOpt->GetValue()
            && !m_layerFilterOpt->GetValue()
            && !m_roundPadsFilter->GetValue()
            && !m_existingFilter->GetValue()
            && !m_selectedItemsFilter->GetValue() )
    {
        commit.Push( _( "Edit Teardrops" ), SKIP_TEARDROPS );

        TEARDROP_MANAGER teardropMgr( m_brd, m_parent->GetToolManager() );
        teardropMgr.UpdateTeardrops( commit, nullptr, nullptr, true /* forceFullUpdate */ );
        commit.Push( _( "Edit Teardrops" ), SKIP_TEARDROPS | APPEND_UNDO );
    }
    else
    {
        commit.Push( _( "Edit Teardrops" ) );
    }

    // Showing the unfilled, fully cross-hatched teardrops seems to be working fairly well, and
    // accurate fills can then be manually generated by doing a zone fill.
    //
    // But here's the old code which allowed for either "draft" fills or an automatic full zone
    // fill in case we decide the current situation isn't good enough:
#if 0
    if( aFillAfter )
    {
        ZONE_FILLER filler( m_board, aCommit );

        if( m_reporter )
            filler.SetProgressReporter( m_reporter );

        filler.Fill( m_board->Zones() );

        if( aCommit )
            aCommit->Push( _( "Edit Teardrops" ), APPEND_UNDO );
    }
    else
    {
        // Fill raw teardrop shapes. This is a rough calculation, just to show a filled
        // shape on screen without the (potentially large) performance hit of a zone refill
        int epsilon = pcbIUScale.mmToIU( 0.001 );
        int allowed_error = pcbIUScale.mmToIU( 0.005 );

        for( ZONE* zone: m_createdTdList )
        {
            int half_min_width = zone->GetMinThickness() / 2;
            int numSegs = GetArcToSegmentCount( half_min_width, allowed_error, FULL_CIRCLE );
            SHAPE_POLY_SET filledPolys = *zone->Outline();

            filledPolys.Deflate( half_min_width - epsilon, numSegs );

            // Re-inflate after pruning of areas that don't meet minimum-width criteria
            if( half_min_width - epsilon > epsilon )
                filledPolys.Inflate( half_min_width - epsilon, numSegs );

            zone->SetFilledPolysList( zone->GetFirstLayer(), filledPolys );
        }
    }
#endif

    return true;
}


int GLOBAL_EDIT_TOOL::EditTeardrops( const TOOL_EVENT& aEvent )
{
    PCB_EDIT_FRAME* editFrame = getEditFrame<PCB_EDIT_FRAME>();
    DIALOG_GLOBAL_EDIT_TEARDROPS dlg( editFrame );

    dlg.ShowQuasiModal();       // QuasiModal required for NET_SELECTOR
    return 0;
}

