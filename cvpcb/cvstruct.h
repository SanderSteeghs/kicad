/***********************************************************/
/*                      cvstruct.h  :                      */
/* descriptions des principales classes derivees utilisees */
/***********************************************************/

#ifndef CVSTRUCT_H
#define CVSTRUCT_H

#include "wx/listctrl.h"
#include <wx/filename.h>
#include "param_config.h"
#include "cvpcb.h"

/*  Forward declarations of all top-level window classes. */
class FOOTPRINTS_LISTBOX;
class COMPONENTS_LISTBOX;
class WinEDA_DisplayFrame;


#include "id.h"


/**
 * Command IDs for the component library viewer.
 *
 * Please add IDs that are unique to the component library viewer here and
 * not in the global id.h file.  This will prevent the entire project from
 * being rebuilt when adding new commands to the component library viewer.
 */
enum id_cvpcb_frm
{
    ID_CVPCB_QUIT = ID_END_LIST,
    ID_CVPCB_READ_INPUT_NETLIST,
    ID_CVPCB_SAVEQUITCVPCB,
    ID_CVPCB_CREATE_CONFIGWINDOW,
    ID_CVPCB_CREATE_SCREENCMP,
    ID_CVPCB_GOTO_FIRSTNA,
    ID_CVPCB_GOTO_PREVIOUSNA,
    ID_CVPCB_DEL_ASSOCIATIONS,
    ID_CVPCB_AUTO_ASSOCIE,
    ID_CVPCB_COMPONENT_LIST,
    ID_CVPCB_FOOTPRINT_LIST,
    ID_CVPCB_CREATE_STUFF_FILE,
    ID_CVPCB_SHOW3D_FRAME,
    ID_CVPCB_FOOTPRINT_DISPLAY_FULL_LIST,
    ID_CVPCB_FOOTPRINT_DISPLAY_FILTERED_LIST,
    ID_CVPCB_CONFIG_KEEP_OPEN_ON_SAVE
};


/**
 * The CVPcb application main window.
 */
class WinEDA_CvpcbFrame : public WinEDA_BasicFrame
{
public:

    bool m_KeepCvpcbOpen;
    FOOTPRINTS_LISTBOX*  m_FootprintList;
    COMPONENTS_LISTBOX*  m_ListCmp;
    WinEDA_DisplayFrame* DrawFrame;
    WinEDA_Toolbar*      m_HToolBar; // Toolbar horizontal haut d'ecran
    wxFileName           m_NetlistFileName;
    wxArrayString        m_ModuleLibNames;
    wxArrayString        m_AliasLibNames;
    wxString             m_UserLibraryPath;
    wxString             m_NetlistFileExtension;
    wxString             m_DocModulesFileName;
    FOOTPRINT_LIST       m_footprints;
    COMPONENT_LIST       m_components;

protected:
    int             m_undefinedComponentCnt;
    bool            m_modified;
    bool            m_rightJustify;
    bool            m_isEESchemaNetlist;
    PARAM_CFG_ARRAY m_projectFileParams;

    // Constructor and destructor
public:
    WinEDA_CvpcbFrame( const wxString& title,
                       long            style = KICAD_DEFAULT_DRAWFRAME_STYLE );
    ~WinEDA_CvpcbFrame();

    void             OnLeftClick( wxListEvent& event );
    void             OnLeftDClick( wxListEvent& event );
    void             OnSelectComponent( wxListEvent& event );

    void             Update_Config( wxCommandEvent& event ); /* enregistrement de la config */
    void             OnQuit( wxCommandEvent& event );
    void             OnCloseWindow( wxCloseEvent& Event );
    void             OnSize( wxSizeEvent& SizeEvent );
    void             OnChar( wxKeyEvent& event );
    void             ReCreateHToolbar();
    virtual void     ReCreateMenuBar();
    void             SetLanguage( wxCommandEvent& event );

    void             ToFirstNA( wxCommandEvent& event );
    void             ToPreviousNA( wxCommandEvent& event );
    void             DelAssociations( wxCommandEvent& event );
    void             SaveQuitCvpcb( wxCommandEvent& event );
    void             LoadNetList( wxCommandEvent& event );
    void             ConfigCvpcb( wxCommandEvent& event );
    void             OnKeepOpenOnSave( wxCommandEvent& event );
    void             DisplayModule( wxCommandEvent& event );
    void             AssocieModule( wxCommandEvent& event );
    void             WriteStuffList( wxCommandEvent& event );
    void             DisplayDocFile( wxCommandEvent& event );
    void             OnSelectFilteringFootprint( wxCommandEvent& event );

    void             OnUpdateKeepOpenOnSave( wxUpdateUIEvent& event );

    void             SetNewPkg( const wxString& package );
    void             BuildCmpListBox();
    void             BuildFOOTPRINTS_LISTBOX();
    void             CreateScreenCmp();
    int              SaveNetList( const wxString& FullFileName );
    int              SaveComponentList( const wxString& FullFileName );
    bool             ReadNetList();
    int              rdpcad();
    int              ReadSchematicNetlist();
    int              ReadFootprintFilterList( FILE* f );
    int              ReadViewlogicWirList();
    int              ReadViewlogicNetList();
    void             LoadProjectFile( const wxString& FileName );
    void             SaveProjectFile( const wxString& fileName );
    virtual void     LoadSettings();
    virtual void     SaveSettings();

    PARAM_CFG_ARRAY& GetProjectFileParameters( void );

    DECLARE_EVENT_TABLE()
};


/*********************************************************************/
/* ListBox (base class) to display lists of components or footprints */
/*********************************************************************/
class ITEMS_LISTBOX_BASE : public wxListView
{
public:
    ITEMS_LISTBOX_BASE( WinEDA_CvpcbFrame* aParent, wxWindowID aId,
                        const wxPoint& aLocation, const wxSize& aSize );

    ~ITEMS_LISTBOX_BASE();

    int                        GetSelection();
    void                       OnSize( wxSizeEvent& event );

    virtual WinEDA_CvpcbFrame* GetParent();
};

/******************************************/
/* ListBox showing the list of footprints */
/******************************************/

class FOOTPRINTS_LISTBOX : public ITEMS_LISTBOX_BASE
{
private:
    wxArrayString  m_FullFootprintList;
    wxArrayString  m_FilteredFootprintList;
public:
    wxArrayString* m_ActiveFootprintList;
    bool           m_UseFootprintFullList;

public:
    FOOTPRINTS_LISTBOX( WinEDA_CvpcbFrame* parent, wxWindowID id,
                        const wxPoint& loc, const wxSize& size,
                        int nbitems, wxString choice[] );
    ~FOOTPRINTS_LISTBOX();

    int      GetCount();
    void     SetSelection( unsigned index, bool State = TRUE );
    void     SetString( unsigned linecount, const wxString& text );
    void     AppendLine( const wxString& text );
    void     SetFootprintFullList( FOOTPRINT_LIST& list );
    void     SetFootprintFilteredList( COMPONENT*      Component,
                                       FOOTPRINT_LIST& list );
    void     SetActiveFootprintList( bool FullList, bool Redraw = FALSE );

    wxString GetSelectedFootprint();
    wxString OnGetItemText( long item, long column ) const;

    // Events functions:
    void     OnLeftClick( wxListEvent& event );
    void     OnLeftDClick( wxListEvent& event );
    void     OnChar( wxKeyEvent& event );

    DECLARE_EVENT_TABLE()
};

/****************************************************/
/* ListBox showing the list of schematic components */
/****************************************************/

class COMPONENTS_LISTBOX : public ITEMS_LISTBOX_BASE
{
public:
    wxArrayString      m_ComponentList;
    WinEDA_CvpcbFrame* m_Parent;

public:

    COMPONENTS_LISTBOX( WinEDA_CvpcbFrame* parent, wxWindowID id,
                        const wxPoint& loc, const wxSize& size,
                        int nbitems, wxString choice[] );

    ~COMPONENTS_LISTBOX();

    void     Clear();
    int      GetCount();
    wxString OnGetItemText( long item, long column ) const;
    void     SetSelection( unsigned index, bool State = TRUE );
    void     SetString( unsigned linecount, const wxString& text );
    void     AppendLine( const wxString& text );

    // Events functions:
    void     OnChar( wxKeyEvent& event );

    DECLARE_EVENT_TABLE()
};


/*******************************************************/
/* class WinEDA_DisplayFrame: used to display footprints */
/*******************************************************/

class WinEDA_DisplayFrame : public WinEDA_BasePcbFrame
{
public:

public:
    WinEDA_DisplayFrame( WinEDA_CvpcbFrame* father,
                         const wxString& title,
                         const wxPoint& pos, const wxSize& size,
                         long style = KICAD_DEFAULT_DRAWFRAME_STYLE );

    ~WinEDA_DisplayFrame();

    void    OnCloseWindow( wxCloseEvent& Event );
    void    RedrawActiveWindow( wxDC* DC, bool EraseBg );
    void    ReCreateHToolbar();
    void    ReCreateVToolbar();
    void    RecreateMenuBar();
    void    OnLeftClick( wxDC* DC, const wxPoint& MousePos );
    void    OnLeftDClick( wxDC* DC, const wxPoint& MousePos );
    bool    OnRightClick( const wxPoint& MousePos, wxMenu* PopMenu );
    void    SetToolbars();
    void    GeneralControle( wxDC* DC, wxPoint Mouse );
    void    InstallOptionsDisplay( wxCommandEvent& event );
    MODULE* Get_Module( const wxString& CmpName );

    void    Process_Settings( wxCommandEvent& event );
    void    Show3D_Frame( wxCommandEvent& event );

    /* SaveCopyInUndoList() virtual
     * currently: do nothing in cvpcb.
     * but but be defined because it is a pure virtual in WinEDA_BasePcbFrame
     */
    virtual void SaveCopyInUndoList( BOARD_ITEM* aItemToCopy,
                                     UndoRedoOpType aTypeCommand = UR_UNSPECIFIED,
                                     const wxPoint& aTransformPoint = wxPoint( 0, 0 ) )
    {
    }


    /** Function SaveCopyInUndoList (overloaded).
     * Creates a new entry in undo list of commands.
     * add a list of pickers to handle a list of items
     * @param aItemsList = the list of items modified by the command to undo
     * @param aTypeCommand = command type (see enum UndoRedoOpType)
     * @param aTransformPoint = the reference point of the transformation, for commands like move
     */
    virtual void SaveCopyInUndoList( PICKED_ITEMS_LIST& aItemsList,
                                     UndoRedoOpType aTypeCommand,
                                     const wxPoint& aTransformPoint = wxPoint( 0, 0 ) )
    {
        // currently: do nothing in cvpcb.
    }


    DECLARE_EVENT_TABLE()
};


#endif  //#ifndef CVSTRUCT_H
