/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015-2016 Mario Luzeiro <mrluzeiro@ua.pt>
 * Copyright (C) 2014 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2011 Wayne Stambaugh <stambaughw@gmail.com>
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
 * @file  eda_3d_viewer.h
 * @brief Declaration of the eda_3d_viewer class
 */

#ifndef EDA_3D_VIEWER_H
#define EDA_3D_VIEWER_H

#include "3d_canvas/board_adapter.h"
#include "3d_canvas/eda_3d_canvas.h"
#include "3d_rendering/ctrack_ball.h"
#include <kiway_player.h>
#include <wx/colourdata.h>
#include <../common/dialogs/dialog_color_picker.h>  // for CUSTOM_COLORS_LIST definition


/// A variable name whose value holds the path of 3D shape files.
/// Currently an environment variable, eventually a project variable.
#define KISYS3DMOD wxT( "KISYS3DMOD" )


#define KICAD_DEFAULT_3D_DRAWFRAME_STYLE    (wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS)

#define VIEWER3D_FRAMENAME wxT( "Viewer3DFrameName" )
#define QUALIFIED_VIEWER3D_FRAMENAME( parent ) \
                    ( wxString( VIEWER3D_FRAMENAME ) + wxT( ":" ) + parent->GetName() )


enum EDA_3D_VIEWER_STATUSBAR
{
    STATUS_TEXT = 0,
    X_POS,
    Y_POS
};

/**
 * Create and handle a window for the 3d viewer connected to a Kiway and a pcbboard
 */
class EDA_3D_VIEWER : public EDA_3D_BOARD_HOLDER, public KIWAY_PLAYER
{
public:
    EDA_3D_VIEWER( KIWAY* aKiway, PCB_BASE_FRAME* aParent, const wxString& aTitle,
                   long style = KICAD_DEFAULT_3D_DRAWFRAME_STYLE );

    ~EDA_3D_VIEWER();

    PCB_BASE_FRAME* Parent() const { return (PCB_BASE_FRAME*)GetParent(); }

    BOARD* GetBoard() { return Parent()->GetBoard(); }

    wxWindow* GetToolCanvas() const override { return m_canvas; }

    /**
     * Request reloading the 3D view.
     *
     * However the request will be executed only when the 3D canvas is refreshed.  It allows
     * one to prepare changes and request for 3D rebuild only when all changes are committed.
     * This is made because the 3D rebuild can take a long time, and this rebuild cannot
     * always made after each change, for calculation time reason.
     */
    void ReloadRequest();

    // !TODO: review this function: it need a way to tell what changed,
    // to only reload/rebuild things that have really changed
    /**
     * Reload and refresh (rebuild) the 3D scene.
     *
     * @warning Rebuilding the 3D scene can take a bit of time, so rebuilding the scene can
     *          be immediate, or made later, during the next 3D canvas refresh (on zoom for
     *           instance).
     *
     * @param aForceImmediateRedraw true to immediately rebuild the 3D scene or false to wait
     *                              refresh later.
     */
    void NewDisplay( bool aForceImmediateRedraw = false );

    void Redraw();

    BOARD_ADAPTER& GetAdapter() override { return m_boardAdapter; }
    CCAMERA& GetCurrentCamera() override { return m_currentCamera; }

    EDA_3D_CANVAS* GetCanvas()  { return m_canvas; }

    /**
     * Get a SFVEC3D from a color dialog.
     *
     * @param aColor is the SFVEC3D to change.
     * @param aTitle is the title displayed in the colordialog selector.
     * @param aPredefinedColors is a reference to a CUSTOM_COLOR_ITEM list which contains.
     *                          a few predefined colors
     */
    bool Set3DColorFromUser( SFVEC4F& aColor, const wxString& aTitle,
                             CUSTOM_COLORS_LIST* aPredefinedColors,
                             bool aAllowOpacityControl = false,
                             KIGFX::COLOR4D aDefaultColor = KIGFX::COLOR4D( 1.0, 1.0, 1.0, 1.0 ) );

    /**
     * Set the solder mask color from a set of colors.
     *
     * @return true if a new color is chosen, false if no change or aborted by user.
     */
    bool Set3DSolderMaskColorFromUser();

    /**
     * Set the solder mask color from a set of colors.
     *
     * @return true if a new color is chosen, false if no change or aborted by user.
     */
    bool Set3DSolderPasteColorFromUser();

    /**
     * Set the copper color from a set of colors.
     *
     * @return true if a new color is chosen, false if no change or aborted by user.
     */
    bool Set3DCopperColorFromUser();

    /**
     * Set the copper color from a set of colors.
     *
     * @return true if a new color is chosen, false if no change or aborted by user.
     */
    bool Set3DBoardBodyColorFromUser();

    /**
     * Set the silkscreen color from a set of colors.
     *
     * @return true if a new color is chosen, false if no change or aborted by user.
     */
    bool Set3DSilkScreenColorFromUser();

    /**
     * Notification that common settings are updated.
     *
     * This would be private (and only called by the Kiway), but we need to do this manually
     * from the PCB frame because the 3D viewer isn't updated via the #KIWAY.
     */
    void CommonSettingsChanged( bool aEnvVarsChanged, bool aTextVarsChanged ) override;


    void SynchroniseColoursWithBoard();

protected:
    void setupUIConditions() override;

private:
    /// Called when user press the File->Exit
    void Exit3DFrame( wxCommandEvent& event );

    void OnCloseWindow( wxCloseEvent& event );

    void Process_Special_Functions( wxCommandEvent& event );

    void OnRenderEngineSelection( wxCommandEvent& event );
    void OnDisableRayTracing( wxCommandEvent& aEvent );

    void OnActivate( wxActivateEvent& event );

    void OnSetFocus( wxFocusEvent& event );

    void Install3DViewOptionDialog( wxCommandEvent& event );

    void CreateMenuBar();
    void ReCreateMainToolbar();

    void SaveSettings( APP_SETTINGS_BASE* aCfg ) override;

    void LoadSettings( APP_SETTINGS_BASE* aCfg ) override;

    /**
     *  Create a Screenshot of the current 3D view.
     *  Output file format is png or jpeg, or image is copied to the clipboard
     */
    void takeScreenshot( wxCommandEvent& event );

    /**
     * @brief RenderEngineChanged - Update toolbar icon and call canvas RenderEngineChanged
     */
    void RenderEngineChanged();

    void refreshRender();

    DECLARE_EVENT_TABLE()

    /**
     * Load configuration from common settings.
     */
    void loadCommonSettings();

    wxFileName       m_defaultSaveScreenshotFileName;

    ACTION_TOOLBAR*  m_mainToolBar;
    EDA_3D_CANVAS*   m_canvas;
    BOARD_ADAPTER    m_boardAdapter;
    CCAMERA&         m_currentCamera;
    CTRACK_BALL      m_trackBallCamera;

    bool             m_disable_ray_tracing;

    /**
     *  Trace mask used to enable or disable the trace output of this class.
     *  The debug output can be turned on by setting the WXTRACE environment variable to
     *  "KI_TRACE_EDA_3D_VIEWER".  See the wxWidgets documentation on wxLogTrace for
     *  more information.
     */
    static const wxChar *m_logTrace;

};

#endif // EDA_3D_VIEWER_H
