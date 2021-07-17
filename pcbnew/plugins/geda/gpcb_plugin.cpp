/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 Wayne Stambaugh <stambaughw@gmail.com>
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

/**
 * @file gpcb_plugin.cpp
 * @brief Geda PCB file plugin implementation file.
 */

#include <wildcards_and_files_ext.h>
#include <trace_helpers.h>
#include <math/util.h>      // for KiROUND

#include <board.h>
#include <footprint.h>
#include <pad.h>
#include <locale_io.h>
#include <macros.h>
#include <pcb_text.h>
#include <pcb_shape.h>
#include <fp_shape.h>
#include <plugins/geda/gpcb_plugin.h>
#include <wx_filename.h>

#include <wx/dir.h>
#include <wx/log.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <boost/ptr_container/ptr_map.hpp>


static inline long parseInt( const wxString& aValue, double aScalar )
{
    double value = std::numeric_limits<double>::max();

    /*
     * In 2011 gEDA/pcb introduced values with units, like "10mm" or "200mil".
     * Unit-less values are still centimils (100000 units per inch), like with
     * the previous format.
     *
     * Distinction between the even older format (mils, 1000 units per inch)
     * and the pre-2011 format is done in ::parseFOOTPRINT already; the
     * distinction is by whether an object definition opens with '(' or '['.
     * All values with explicit unit open with a '[' so there's no need to
     * consider this distinction when parsing them.
     *
     * The solution here is to watch for a unit and, if present, convert the
     * value to centimils. All unit-less values are read unaltered. This way
     * the code below can continue to consider all read values to be in mils or
     * centimils. It also matches the strategy gEDA/pcb uses for backwards
     * compatibility with its own layouts.
     *
     * Fortunately gEDA/pcb allows only units 'mil' and 'mm' in files, see
     * definition of ALLOW_READABLE in gEDA/pcb's pcb_printf.h. So we don't
     * have to test for all 11 units gEDA/pcb allows in user dialogs.
     */
    if( aValue.EndsWith( wxT( "mm" ) ) )
    {
        aScalar *= 100000.0 / 25.4;
    }
    else if( aValue.EndsWith( wxT( "mil" ) ) )
    {
        aScalar *= 100.;
    }

    // This conversion reports failure on strings as simple as "1000", still
    // it returns the right result in &value. Thus, ignore the return value.
    aValue.ToCDouble(&value);
    if( value == std::numeric_limits<double>::max() ) // conversion really failed
    {
        THROW_IO_ERROR( wxString::Format( _( "Cannot convert '%s' to an integer." ),
                                          aValue.GetData() ) );
        return 0;
    }

    return KiROUND( value * aScalar );
}


/**
 * helper class for creating a footprint library cache.
 *
 * The new footprint library design is a file path of individual footprint files
 * that contain a single footprint per file.  This class is a helper only for the
 * footprint portion of the PLUGIN API, and only for the #PCB_IO plugin.  It is
 * private to this implementation file so it is not placed into a header.
 */
class GPCB_FPL_CACHE_ITEM
{
public:
    GPCB_FPL_CACHE_ITEM( FOOTPRINT* aFootprint, const WX_FILENAME& aFileName );

    WX_FILENAME  GetFileName() const  { return m_filename; }
    FOOTPRINT*      GetFootprint() const { return m_footprint.get(); }

private:
    WX_FILENAME m_filename;       ///< The full file name and path of the footprint to cache.
    std::unique_ptr<FOOTPRINT> m_footprint;
};


GPCB_FPL_CACHE_ITEM::GPCB_FPL_CACHE_ITEM( FOOTPRINT* aFootprint, const WX_FILENAME& aFileName ) :
        m_filename( aFileName ),
        m_footprint( aFootprint )
{
}


typedef boost::ptr_map< std::string, GPCB_FPL_CACHE_ITEM >  FOOTPRINT_MAP;


class GPCB_FPL_CACHE
{
public:
    GPCB_FPL_CACHE( GPCB_PLUGIN* aOwner, const wxString& aLibraryPath );

    wxString GetPath() const { return m_lib_path.GetPath(); }
    bool IsWritable() const { return m_lib_path.IsOk() && m_lib_path.IsDirWritable(); }
    FOOTPRINT_MAP& GetFootprints() { return m_footprints; }

    // Most all functions in this class throw IO_ERROR exceptions.  There are no
    // error codes nor user interface calls from here, nor in any PLUGIN.
    // Catch these exceptions higher up please.

    /// Save not implemented for the Geda PCB footprint library format.

    void Load();

    void Remove( const wxString& aFootprintName );

    /**
     * Generate a timestamp representing all source files in the cache (including the
     * parent directory).
     *
     * Timestamps should not be considered ordered.  They either match or they don't.
     */
    static long long GetTimestamp( const wxString& aLibPath );

    /**
     * Return true if the cache is not up-to-date.
     */
    bool IsModified();

private:
    FOOTPRINT* parseFOOTPRINT( LINE_READER* aLineReader );

    /**
     * Test \a aFlag for \a aMask or \a aName.
     *
     * @param aFlag is a list of flags to test against: can be a bit field flag or a list name flag
     *              a bit field flag is an hexadecimal value: Ox00020000 a list name flag is a
     *              string list of flags, comma separated like square,option1.
     * @param aMask is the flag list to test.
     * @param aName is the flag name to find in list.
     * @return true if found.
     */
    bool testFlags( const wxString& aFlag, long aMask, const wxChar* aName );

    /**
     * Extract parameters and tokens from \a aLineReader and adds them to \a aParameterList.
     *
     * Delimiter characters are:
     * [ ] ( )  Begin and end of parameter list and units indicator
     * " is a string delimiter
     * space is the param separator
     * The first word is the keyword
     * the second item is one of ( or [
     * other are parameters (number or delimited string)
     * last parameter is ) or ]
     *
     * @param aParameterList This list of parameters parsed.
     * @param aLineReader The line reader object to parse.
     */
    void parseParameters( wxArrayString& aParameterList, LINE_READER* aLineReader );

    GPCB_PLUGIN*    m_owner;            ///< Plugin object that owns the cache.
    wxFileName      m_lib_path;         ///< The path of the library.
    FOOTPRINT_MAP   m_footprints;       ///< Map of footprint file name to FOOTPRINT*.

    bool            m_cache_dirty;      ///< Stored separately because it's expensive to check
                                        ///< m_cache_timestamp against all the files.
    long long       m_cache_timestamp;  ///< A hash of the timestamps for all the footprint
                                        ///< files.
};


GPCB_FPL_CACHE::GPCB_FPL_CACHE( GPCB_PLUGIN* aOwner, const wxString& aLibraryPath )
{
    m_owner = aOwner;
    m_lib_path.SetPath( aLibraryPath );
    m_cache_timestamp = 0;
    m_cache_dirty = true;
}


void GPCB_FPL_CACHE::Load()
{
    m_cache_dirty = false;
    m_cache_timestamp = 0;

    // Note: like our .pretty footprint libraries, the gpcb footprint libraries are folders,
    // and the footprints are the .fp files inside this folder.

    wxDir dir( m_lib_path.GetPath() );

    if( !dir.IsOpened() )
    {
        THROW_IO_ERROR( wxString::Format( _( "Footprint library '%s' not found." ),
                                          m_lib_path.GetPath().GetData() ) );
    }

    wxString fullName;
    wxString fileSpec = wxT( "*." ) + GedaPcbFootprintLibFileExtension;

    // wxFileName construction is egregiously slow.  Construct it once and just swap out
    // the filename thereafter.
    WX_FILENAME fn( m_lib_path.GetPath(), wxT( "dummyName" ) );

    if( !dir.GetFirst( &fullName, fileSpec ) )
        return;

    wxString cacheErrorMsg;

    do
    {
        fn.SetFullName( fullName );

        // Queue I/O errors so only files that fail to parse don't get loaded.
        try
        {
            // reader now owns fp, will close on exception or return
            FILE_LINE_READER reader( fn.GetFullPath() );
            std::string      name = TO_UTF8( fn.GetName() );
            FOOTPRINT*       footprint = parseFOOTPRINT( &reader );

            // The footprint name is the file name without the extension.
            footprint->SetFPID( LIB_ID( wxEmptyString, fn.GetName() ) );
            m_footprints.insert( name, new GPCB_FPL_CACHE_ITEM( footprint, fn ) );
        }
        catch( const IO_ERROR& ioe )
        {
            if( !cacheErrorMsg.IsEmpty() )
                cacheErrorMsg += "\n\n";

            cacheErrorMsg += ioe.What();
        }
    } while( dir.GetNext( &fullName ) );

    if( !cacheErrorMsg.IsEmpty() )
        THROW_IO_ERROR( cacheErrorMsg );
}


void GPCB_FPL_CACHE::Remove( const wxString& aFootprintName )
{
    std::string footprintName = TO_UTF8( aFootprintName );

    FOOTPRINT_MAP::const_iterator it = m_footprints.find( footprintName );

    if( it == m_footprints.end() )
    {
        THROW_IO_ERROR( wxString::Format( _( "Library '%s' has no footprint '%s'." ),
                                          m_lib_path.GetPath().GetData(),
                                          aFootprintName.GetData() ) );
    }

    // Remove the footprint from the cache and delete the footprint file from the library.
    wxString fullPath = it->second->GetFileName().GetFullPath();
    m_footprints.erase( footprintName );
    wxRemoveFile( fullPath );
}


bool GPCB_FPL_CACHE::IsModified()
{
    m_cache_dirty = m_cache_dirty || GetTimestamp( m_lib_path.GetFullPath() ) != m_cache_timestamp;

    return m_cache_dirty;
}


long long GPCB_FPL_CACHE::GetTimestamp( const wxString& aLibPath )
{
    wxString fileSpec = wxT( "*." ) + GedaPcbFootprintLibFileExtension;

    return TimestampDir( aLibPath, fileSpec );
}


FOOTPRINT* GPCB_FPL_CACHE::parseFOOTPRINT( LINE_READER* aLineReader )
{
    #define TEXT_DEFAULT_SIZE  ( 40*IU_PER_MILS )
    #define OLD_GPCB_UNIT_CONV IU_PER_MILS

    // Old version unit = 1 mil, so conv_unit is 10 or 0.1
    #define NEW_GPCB_UNIT_CONV ( 0.01*IU_PER_MILS )

    int                        paramCnt;

    // GPCB unit = 0.01 mils and Pcbnew 0.1.
    double                     conv_unit = NEW_GPCB_UNIT_CONV;
    wxPoint                    textPos;
    wxString                   msg;
    wxArrayString              parameters;
    std::unique_ptr<FOOTPRINT> footprint = std::make_unique<FOOTPRINT>( nullptr );

    if( aLineReader->ReadLine() == nullptr )
    {
        msg = aLineReader->GetSource() + ": empty file";
        THROW_IO_ERROR( msg );
    }

    parameters.Clear();
    parseParameters( parameters, aLineReader );
    paramCnt = parameters.GetCount();

    /* From the Geda PCB documentation, valid Element definitions:
     *   Element [SFlags "Desc" "Name" "Value" MX MY TX TY TDir TScale TSFlags]
     *   Element (NFlags "Desc" "Name" "Value" MX MY TX TY TDir TScale TNFlags)
     *   Element (NFlags "Desc" "Name" "Value" TX TY TDir TScale TNFlags)
     *   Element (NFlags "Desc" "Name" TX TY TDir TScale TNFlags)
     *   Element ("Desc" "Name" TX TY TDir TScale TNFlags)
     */

    if( parameters[0].CmpNoCase( wxT( "Element" ) ) != 0 )
    {
        msg.Printf( _( "Unknown token '%s'" ), parameters[0] );
        THROW_PARSE_ERROR( msg, aLineReader->GetSource(), (const char *)aLineReader,
                           aLineReader->LineNumber(), 0 );
    }

    if( paramCnt < 10 || paramCnt > 14 )
    {
        msg.Printf( _( "Element token contains %d parameters." ), paramCnt );
        THROW_PARSE_ERROR( msg, aLineReader->GetSource(), (const char *)aLineReader,
                           aLineReader->LineNumber(), 0 );
    }

    // Test symbol after "Element": if [ units = 0.01 mils, and if ( units = 1 mil
    if( parameters[1] == wxT( "(" ) )
        conv_unit = OLD_GPCB_UNIT_CONV;

    if( paramCnt > 10 )
    {
        footprint->SetDescription( parameters[3] );
        footprint->SetReference( parameters[4] );
    }
    else
    {
        footprint->SetDescription( parameters[2] );
        footprint->SetReference( parameters[3] );
    }

    // Read value
    if( paramCnt > 10 )
        footprint->SetValue( parameters[5] );

    // With gEDA/pcb, value is meaningful after instantiation, only, so it's
    // often empty in bare footprints.
    if( footprint->Value().GetText().IsEmpty() )
        footprint->Value().SetText( wxT( "Val**" ) );


    if( paramCnt == 14 )
    {
        textPos = wxPoint( parseInt( parameters[8], conv_unit ),
                           parseInt( parameters[9], conv_unit ) );
    }
    else
    {
        textPos = wxPoint( parseInt( parameters[6], conv_unit ),
                           parseInt( parameters[7], conv_unit ) );
    }

    int orientation = parseInt( parameters[paramCnt-4], 1.0 );
    footprint->Reference().SetTextAngle(( orientation % 2) ? 900 : 0 );

    // Calculate size: default height is 40 mils, width 30 mil.
    // real size is:  default * ibuf[idx+3] / 100 (size in gpcb is given in percent of default size
    int thsize = parseInt( parameters[paramCnt-3], TEXT_DEFAULT_SIZE ) / 100;
    thsize = std::max( (int)( 5 * IU_PER_MILS ), thsize ); // Ensure a minimal size = 5 mils
    int twsize = thsize * 30 / 40;
    int thickness = thsize / 8;

    // gEDA/pcb aligns top/left, not pcbnew's default, center/center.
    // Compensate for this by shifting the insertion point instead of the
    // alignment, because alignment isn't changeable in the GUI.
    textPos.x = textPos.x + twsize * footprint->GetReference().Len() / 2;
    textPos.y += thsize / 2;

    // gEDA/pcb draws a bit too low/left, while pcbnew draws a bit too
    // high/right. Compensate for similar appearance.
    textPos.x -= thsize / 10;
    textPos.y += thsize / 2;

    footprint->Reference().SetTextPos( textPos );
    footprint->Reference().SetPos0( textPos );
    footprint->Reference().SetTextSize( wxSize( twsize, thsize ) );
    footprint->Reference().SetTextThickness( thickness );

    // gEDA/pcb shows only one of value/reference/description at a time. Which
    // one is selectable by a global menu setting. pcbnew needs reference as
    // well as value visible, so place the value right below the reference.
    footprint->Value().SetTextAngle( footprint->Reference().GetTextAngle() );
    footprint->Value().SetTextSize( footprint->Reference().GetTextSize() );
    footprint->Value().SetTextThickness( footprint->Reference().GetTextThickness() );
    textPos.y += thsize * 13 / 10;  // 130% line height
    footprint->Value().SetTextPos( textPos );
    footprint->Value().SetPos0( textPos );

    while( aLineReader->ReadLine() )
    {
        parameters.Clear();
        parseParameters( parameters, aLineReader );

        if( parameters.IsEmpty() || parameters[0] == wxT( "(" ) )
            continue;

        if( parameters[0] == wxT( ")" ) )
            break;

        paramCnt = parameters.GetCount();

        // Test units value for a string line param (more than 3 parameters : ident [ xx ] )
        if( paramCnt > 3 )
        {
            if( parameters[1] == wxT( "(" ) )
                conv_unit = OLD_GPCB_UNIT_CONV;
            else
                conv_unit = NEW_GPCB_UNIT_CONV;
        }

        wxLogTrace( traceGedaPcbPlugin, wxT( "%s parameter count = %d." ),
                    parameters[0], paramCnt );

        // Parse a line with format: ElementLine [X1 Y1 X2 Y2 Thickness]
        if( parameters[0].CmpNoCase( wxT( "ElementLine" ) ) == 0 )
        {
            if( paramCnt != 8 )
            {
                msg.Printf( wxT( "ElementLine token contains %d parameters." ), paramCnt );
                THROW_PARSE_ERROR( msg, aLineReader->GetSource(), (const char *)aLineReader,
                                   aLineReader->LineNumber(), 0 );
            }

            FP_SHAPE* shape = new FP_SHAPE( footprint.get(), SHAPE_T::SEGMENT );
            shape->SetLayer( F_SilkS );
            shape->SetStart0( wxPoint( parseInt( parameters[2], conv_unit ),
                                       parseInt( parameters[3], conv_unit ) ) );
            shape->SetEnd0( wxPoint( parseInt( parameters[4], conv_unit ),
                                     parseInt( parameters[5], conv_unit ) ) );
            shape->SetWidth( parseInt( parameters[6], conv_unit ) );
            shape->SetDrawCoord();
            footprint->Add( shape );
            continue;
        }

        // Parse an arc with format: ElementArc [X Y Width Height StartAngle DeltaAngle Thickness]
        if( parameters[0].CmpNoCase( wxT( "ElementArc" ) ) == 0 )
        {
            if( paramCnt != 10 )
            {
                msg.Printf( wxT( "ElementArc token contains %d parameters." ), paramCnt );
                THROW_PARSE_ERROR( msg, aLineReader->GetSource(), (const char *)aLineReader,
                                   aLineReader->LineNumber(), 0 );
            }

            // Pcbnew does know ellipse so we must have Width = Height
            FP_SHAPE* shape = new FP_SHAPE( footprint.get(), SHAPE_T::ARC );
            shape->SetLayer( F_SilkS );
            footprint->Add( shape );

            // for and arc: ibuf[3] = ibuf[4]. Pcbnew does not know ellipses
            int     radius = ( parseInt( parameters[4], conv_unit ) +
                               parseInt( parameters[5], conv_unit ) ) / 2;

            wxPoint centre( parseInt( parameters[2], conv_unit ),
                            parseInt( parameters[3], conv_unit ) );

            shape->SetStart0( centre );

            // Pcbnew start angles are inverted and 180 degrees from Geda PCB angles.
            double start_angle = parseInt( parameters[6], -10.0 ) + 1800.0;

            // Pcbnew delta angle direction is the opposite of Geda PCB delta angles.
            double sweep_angle = parseInt( parameters[7], -10.0 );

            // Geda PCB does not support circles.
            if( sweep_angle == -3600.0 )
                shape->SetShape( SHAPE_T::CIRCLE );

            // Angle value is clockwise in gpcb and Pcbnew.
            shape->SetAngle( sweep_angle );
            shape->SetEnd0( wxPoint( radius, 0 ) );

            // Calculate start point coordinate of arc
            wxPoint arcStart( shape->GetEnd0() );
            RotatePoint( &arcStart, -start_angle );
            shape->SetEnd0( centre + arcStart );
            shape->SetWidth( parseInt( parameters[8], conv_unit ) );
            shape->SetDrawCoord();
            continue;
        }

        // Parse a Pad with no hole with format:
        //   Pad [rX1 rY1 rX2 rY2 Thickness Clearance Mask "Name" "Number" SFlags]
        //   Pad (rX1 rY1 rX2 rY2 Thickness Clearance Mask "Name" "Number" NFlags)
        //   Pad (aX1 aY1 aX2 aY2 Thickness "Name" "Number" NFlags)
        //   Pad (aX1 aY1 aX2 aY2 Thickness "Name" NFlags)
        if( parameters[0].CmpNoCase( wxT( "Pad" ) ) == 0 )
        {
            if( paramCnt < 10 || paramCnt > 13 )
            {
                msg.Printf( wxT( "Pad token contains %d parameters." ), paramCnt );
                THROW_PARSE_ERROR( msg, aLineReader->GetSource(), (const char *)aLineReader,
                                   aLineReader->LineNumber(), 0 );
            }

            PAD* pad = new PAD( footprint.get() );

            static const LSET pad_front( 3, F_Cu, F_Mask, F_Paste );
            static const LSET pad_back(  3, B_Cu, B_Mask, B_Paste );

            pad->SetShape( PAD_SHAPE::RECT );
            pad->SetAttribute( PAD_ATTRIB::SMD );
            pad->SetLayerSet( pad_front );

            if( testFlags( parameters[paramCnt-2], 0x0080, wxT( "onsolder" ) ) )
                pad->SetLayerSet( pad_back );

            // Set the pad name:
            // Pcbnew pad name is used for electrical connection calculations.
            // Accordingly it should be mapped to gEDA's pin/pad number,
            // which is used for the same purpose.
            // gEDA also features a pin/pad "name", which is an arbitrary string
            // and set to the pin name of the netlist on instantiation. Many gEDA
            // bare footprints use identical strings for name and number, so this
            // can be a bit confusing.
            pad->SetNumber( parameters[paramCnt-3] );

            int x1 = parseInt( parameters[2], conv_unit );
            int x2 = parseInt( parameters[4], conv_unit );
            int y1 = parseInt( parameters[3], conv_unit );
            int y2 = parseInt( parameters[5], conv_unit );
            int width = parseInt( parameters[6], conv_unit );
            wxPoint delta( x2 - x1, y2 - y1 );
            double angle = atan2( (double)delta.y, (double)delta.x );

            // Get the pad clearance and the solder mask clearance.
            if( paramCnt == 13 )
            {
                int clearance = parseInt( parameters[7], conv_unit );
                // One of gEDA's oddities is that clearance between pad and polygon
                // is given as the gap on both sides of the pad together, so for
                // KiCad it has to halfed.
                pad->SetLocalClearance( clearance / 2 );

                // In GEDA, the mask value is the size of the hole in this
                // solder mask. In Pcbnew, it is a margin, therefore the distance
                // between the copper and the mask
                int maskMargin = parseInt( parameters[8], conv_unit );
                maskMargin = ( maskMargin - width ) / 2;
                pad->SetLocalSolderMaskMargin( maskMargin );
            }

            // Negate angle (due to Y reversed axis) and convert it to internal units
            angle = - RAD2DECIDEG( angle );
            pad->SetOrientation( KiROUND( angle ) );

            wxPoint padPos( (x1 + x2) / 2, (y1 + y2) / 2 );

            pad->SetSize( wxSize( KiROUND( EuclideanNorm( delta ) ) + width, width ) );

            // Set the relative position before adjusting the absolute position
            pad->SetPos0( padPos );
            padPos += footprint->GetPosition();
            pad->SetPosition( padPos );

            if( !testFlags( parameters[paramCnt-2], 0x0100, wxT( "square" ) ) )
            {
                if( pad->GetSize().x == pad->GetSize().y )
                    pad->SetShape( PAD_SHAPE::CIRCLE );
                else
                    pad->SetShape( PAD_SHAPE::OVAL );
            }

            footprint->Add( pad );
            continue;
        }

        // Parse a Pin with through hole with format:
        //    Pin [rX rY Thickness Clearance Mask Drill "Name" "Number" SFlags]
        //    Pin (rX rY Thickness Clearance Mask Drill "Name" "Number" NFlags)
        //    Pin (aX aY Thickness Drill "Name" "Number" NFlags)
        //    Pin (aX aY Thickness Drill "Name" NFlags)
        //    Pin (aX aY Thickness "Name" NFlags)
        if( parameters[0].CmpNoCase( wxT( "Pin" ) ) == 0 )
        {
            if( paramCnt < 8 || paramCnt > 12 )
            {
                msg.Printf( wxT( "Pin token contains %d parameters." ), paramCnt );
                THROW_PARSE_ERROR( msg, aLineReader->GetSource(), (const char *)aLineReader,
                                   aLineReader->LineNumber(), 0 );
            }

            PAD* pad = new PAD( footprint.get() );

            pad->SetShape( PAD_SHAPE::CIRCLE );

            static const LSET pad_set = LSET::AllCuMask() | LSET( 3, F_SilkS, F_Mask, B_Mask );

            pad->SetLayerSet( pad_set );

            if( testFlags( parameters[paramCnt-2], 0x0100, wxT( "square" ) ) )
                pad->SetShape( PAD_SHAPE::RECT );

            // Set the pad name:
            // Pcbnew pad name is used for electrical connection calculations.
            // Accordingly it should be mapped to gEDA's pin/pad number,
            // which is used for the same purpose.
            pad->SetNumber( parameters[paramCnt-3] );

            wxPoint padPos( parseInt( parameters[2], conv_unit ),
                            parseInt( parameters[3], conv_unit ) );

            int padSize = parseInt( parameters[4], conv_unit );

            pad->SetSize( wxSize( padSize, padSize ) );

            int drillSize = 0;

            // Get the pad clearance, solder mask clearance, and drill size.
            if( paramCnt == 12 )
            {
                int clearance = parseInt( parameters[5], conv_unit );
                // One of gEDA's oddities is that clearance between pad and polygon
                // is given as the gap on both sides of the pad together, so for
                // KiCad it has to halfed.
                pad->SetLocalClearance( clearance / 2 );

                // In GEDA, the mask value is the size of the hole in this
                // solder mask. In Pcbnew, it is a margin, therefore the distance
                // between the copper and the mask
                int maskMargin = parseInt( parameters[6], conv_unit );
                maskMargin = ( maskMargin - padSize ) / 2;
                pad->SetLocalSolderMaskMargin( maskMargin );

                drillSize = parseInt( parameters[7], conv_unit );
            }
            else
            {
                drillSize = parseInt( parameters[5], conv_unit );
            }

            pad->SetDrillSize( wxSize( drillSize, drillSize ) );

            // Set the relative position before adjusting the absolute position
            pad->SetPos0( padPos );
            padPos += footprint->GetPosition();
            pad->SetPosition( padPos );

            if( pad->GetShape() == PAD_SHAPE::CIRCLE  &&  pad->GetSize().x != pad->GetSize().y )
                pad->SetShape( PAD_SHAPE::OVAL );

            footprint->Add( pad );
            continue;
        }
    }

    return footprint.release();
}


void GPCB_FPL_CACHE::parseParameters( wxArrayString& aParameterList, LINE_READER* aLineReader )
{
    char     key;
    wxString tmp;
    char*    line = aLineReader->Line();

    // Last line already ready in main parser loop.
    while( *line != 0 )
    {
        key = *line;
        line++;

        switch( key )
        {
        case '[':
        case '(':
            if( !tmp.IsEmpty() )
            {
                aParameterList.Add( tmp );
                tmp.Clear();
            }

            tmp.Append( key );
            aParameterList.Add( tmp );
            tmp.Clear();

            // Opening delimiter "(" after Element statement.  Any other occurrence is part
            // of a keyword definition.
            if( aParameterList.GetCount() == 1 )
            {
                wxLogTrace( traceGedaPcbPlugin, dump( aParameterList ) );
                return;
            }

            break;

        case ']':
        case ')':
            if( !tmp.IsEmpty() )
            {
                aParameterList.Add( tmp );
                tmp.Clear();
            }

            tmp.Append( key );
            aParameterList.Add( tmp );
            wxLogTrace( traceGedaPcbPlugin, dump( aParameterList ) );
            return;

        case '\n':
        case '\r':
            // Element descriptions can span multiple lines.
            line = aLineReader->ReadLine();
            KI_FALLTHROUGH;

        case '\t':
        case ' ':
            if( !tmp.IsEmpty() )
            {
                aParameterList.Add( tmp );
                tmp.Clear();
            }

            break;

        case '"':
            // Handle empty quotes.
            if( *line == '"' )
            {
                line++;
                tmp.Clear();
                aParameterList.Add( wxEmptyString );
                break;
            }

            while( *line != 0 )
            {
                key = *line;
                line++;

                if( key == '"' )
                {
                    aParameterList.Add( tmp );
                    tmp.Clear();
                    break;
                }
                else
                {
                    tmp.Append( key );
                }
            }

            break;

        case '#':
            line = aLineReader->ReadLine();
            break;

        default:
            tmp.Append( key );
            break;
        }
    }
}


bool GPCB_FPL_CACHE::testFlags( const wxString& aFlag, long aMask, const wxChar* aName )
{
    wxString number;

    if( aFlag.StartsWith( wxT( "0x" ), &number ) || aFlag.StartsWith( wxT( "0X" ), &number ) )
    {
        long lflags;

        if( number.ToLong( &lflags, 16 ) && ( lflags & aMask ) )
            return true;
    }
    else if( aFlag.Contains( aName ) )
    {
        return true;
    }

    return false;
}


GPCB_PLUGIN::GPCB_PLUGIN() :
    m_cache( nullptr ),
    m_ctl( 0 )
{
    m_reader = nullptr;
    init( nullptr );
}


GPCB_PLUGIN::GPCB_PLUGIN( int aControlFlags ) :
    m_cache( nullptr ),
    m_ctl( aControlFlags )
{
    m_reader = nullptr;
    init( nullptr );
}


GPCB_PLUGIN::~GPCB_PLUGIN()
{
    delete m_cache;
}


void GPCB_PLUGIN::init( const PROPERTIES* aProperties )
{
    m_props = aProperties;
}


void GPCB_PLUGIN::validateCache( const wxString& aLibraryPath, bool checkModified  )
{
    if( !m_cache || ( checkModified && m_cache->IsModified() ) )
    {
        // a spectacular episode in memory management:
        delete m_cache;
        m_cache = new GPCB_FPL_CACHE( this, aLibraryPath );
        m_cache->Load();
    }
}


void GPCB_PLUGIN::FootprintEnumerate( wxArrayString& aFootprintNames, const wxString& aLibraryPath,
                                      bool aBestEfforts, const PROPERTIES* aProperties )
{
    LOCALE_IO toggle;     // toggles on, then off, the C locale.
    wxDir     dir( aLibraryPath );
    wxString  errorMsg;

    if( !dir.IsOpened() )
    {
        if( aBestEfforts )
            return;
        else
        {
            THROW_IO_ERROR( wxString::Format( _( "Footprint library '%s' not found." ),
                                              aLibraryPath ) );
        }
    }

    init( aProperties );

    try
    {
        validateCache( aLibraryPath );
    }
    catch( const IO_ERROR& ioe )
    {
        errorMsg = ioe.What();
    }

    // Some of the files may have been parsed correctly so we want to add the valid files to
    // the library.

    for( const auto& footprint : m_cache->GetFootprints() )
        aFootprintNames.Add( FROM_UTF8( footprint.first.c_str() ) );

    if( !errorMsg.IsEmpty() && !aBestEfforts )
        THROW_IO_ERROR( errorMsg );
}


const FOOTPRINT* GPCB_PLUGIN::getFootprint( const wxString& aLibraryPath,
                                            const wxString& aFootprintName,
                                            const PROPERTIES* aProperties,
                                            bool checkModified )
{
    LOCALE_IO   toggle;     // toggles on, then off, the C locale.

    init( aProperties );

    validateCache( aLibraryPath, checkModified );

    const FOOTPRINT_MAP& mods = m_cache->GetFootprints();

    FOOTPRINT_MAP::const_iterator it = mods.find( TO_UTF8( aFootprintName ) );

    if( it == mods.end() )
        return nullptr;

    return it->second->GetFootprint();
}


const FOOTPRINT* GPCB_PLUGIN::GetEnumeratedFootprint( const wxString& aLibraryPath,
                                                      const wxString& aFootprintName,
                                                      const PROPERTIES* aProperties )
{
    return getFootprint( aLibraryPath, aFootprintName, aProperties, false );
}


FOOTPRINT* GPCB_PLUGIN::FootprintLoad( const wxString& aLibraryPath,
                                       const wxString& aFootprintName,
                                       bool  aKeepUUID,
                                       const PROPERTIES* aProperties )
{
    const FOOTPRINT* footprint = getFootprint( aLibraryPath, aFootprintName, aProperties, true );

    if( footprint )
    {
        FOOTPRINT* copy = (FOOTPRINT*) footprint->Duplicate();
        copy->SetParent( nullptr );
        return copy;
    }

    return nullptr;
}


void GPCB_PLUGIN::FootprintDelete( const wxString& aLibraryPath, const wxString& aFootprintName,
                                   const PROPERTIES* aProperties )
{
    LOCALE_IO   toggle;     // toggles on, then off, the C locale.

    init( aProperties );

    validateCache( aLibraryPath );

    if( !m_cache->IsWritable() )
    {
        THROW_IO_ERROR( wxString::Format( _( "Library '%s' is read only." ),
                                          aLibraryPath.GetData() ) );
    }

    m_cache->Remove( aFootprintName );
}


bool GPCB_PLUGIN::FootprintLibDelete( const wxString& aLibraryPath, const PROPERTIES* aProperties )
{
    wxFileName fn;
    fn.SetPath( aLibraryPath );

    // Return if there is no library path to delete.
    if( !fn.DirExists() )
        return false;

    if( !fn.IsDirWritable() )
    {
        THROW_IO_ERROR( wxString::Format( _( "Insufficient permissions to delete folder '%s'." ),
                                          aLibraryPath.GetData() ) );
    }

    wxDir dir( aLibraryPath );

    if( dir.HasSubDirs() )
    {
        THROW_IO_ERROR( wxString::Format( _( "Library folder '%s' has unexpected sub-folders." ),
                                          aLibraryPath.GetData() ) );
    }

    // All the footprint files must be deleted before the directory can be deleted.
    if( dir.HasFiles() )
    {
        unsigned      i;
        wxFileName    tmp;
        wxArrayString files;

        wxDir::GetAllFiles( aLibraryPath, &files );

        for( i = 0;  i < files.GetCount();  i++ )
        {
            tmp = files[i];

            if( tmp.GetExt() != KiCadFootprintFileExtension )
            {
                THROW_IO_ERROR( wxString::Format( _( "Unexpected file '%s' found in library '%s'." ),
                                                  files[i].GetData(),
                                                  aLibraryPath.GetData() ) );
            }
        }

        for( i = 0;  i < files.GetCount();  i++ )
        {
            wxRemoveFile( files[i] );
        }
    }

    wxLogTrace( traceGedaPcbPlugin, wxT( "Removing footprint library '%s'" ),
                aLibraryPath.GetData() );

    // Some of the more elaborate wxRemoveFile() crap puts up its own wxLog dialog
    // we don't want that.  we want bare metal portability with no UI here.
    if( !wxRmdir( aLibraryPath ) )
    {
        THROW_IO_ERROR( wxString::Format( _( "Footprint library '%s' cannot be deleted." ),
                                          aLibraryPath.GetData() ) );
    }

    // For some reason removing a directory in Windows is not immediately updated.  This delay
    // prevents an error when attempting to immediately recreate the same directory when over
    // writing an existing library.
#ifdef __WINDOWS__
    wxMilliSleep( 250L );
#endif

    if( m_cache && m_cache->GetPath() == aLibraryPath )
    {
        delete m_cache;
        m_cache = nullptr;
    }

    return true;
}


long long GPCB_PLUGIN::GetLibraryTimestamp( const wxString& aLibraryPath ) const
{
    return GPCB_FPL_CACHE::GetTimestamp( aLibraryPath );
}


bool GPCB_PLUGIN::IsFootprintLibWritable( const wxString& aLibraryPath )
{
    LOCALE_IO   toggle;

    init( nullptr );

    validateCache( aLibraryPath );

    return m_cache->IsWritable();
}
