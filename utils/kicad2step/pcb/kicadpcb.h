/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2016 Cirilo Bernardo <cirilo.bernardo@gmail.com>
 * Copyright (C) 2021-2022 KiCad Developers, see AUTHORS.txt for contributors.
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
 * @file kicadpcb.h
 * declares the main PCB object
 */

#ifndef KICADPCB_H
#define KICADPCB_H

#include <wx/string.h>
#include <wx/colour.h>
#include <string>
#include <vector>
#include "3d_resolver.h"
#include "base.h"

#ifdef SUPPORTS_IGES
#undef SUPPORTS_IGES
#endif

extern void ReportMessage( const wxString& aMessage );

namespace SEXPR
{
    class SEXPR;
}

class KICADFOOTPRINT;
class KICADCURVE;
class PCBMODEL;

class KICADPCB
{
public:
    KICADPCB( const wxString& aPcbName );
    virtual ~KICADPCB();

    int GetLayerId( std::string& aLayerName );

    void SetOrigin( double aXOrigin, double aYOrigin )
    {
        m_origin.x = aXOrigin;
        m_origin.y = aYOrigin;
    }

    void UseGridOrigin( bool aUseOrigin )
    {
        m_useGridOrigin = aUseOrigin;
    }

    void UseDrillOrigin( bool aUseOrigin )
    {
        m_useDrillOrigin = aUseOrigin;
    }

    void SetMinDistance( double aDistance )
    {
        m_minDistance = aDistance;
    }

    bool ReadFile( const wxString& aFileName );
    bool ComposePCB( bool aComposeVirtual = true, bool aSubstituteModels = true );
    bool WriteSTEP( const wxString& aFileName );

#ifdef SUPPORTS_IGES
    bool WriteIGES( const wxString& aFileName );
#endif

private:
    bool parsePCB( SEXPR::SEXPR* data );
    bool parseGeneral( SEXPR::SEXPR* data );
    bool parseSetup( SEXPR::SEXPR* data );
    bool parseStackup( SEXPR::SEXPR* data );
    bool parseStackupLayer( SEXPR::SEXPR* data );
    bool parseLayers( SEXPR::SEXPR* data );
    bool parseModule( SEXPR::SEXPR* data );
    bool parseCurve( SEXPR::SEXPR* data, CURVE_TYPE aCurveType );
    bool parseRect( SEXPR::SEXPR* data );
    bool parsePolygon( SEXPR::SEXPR* data );

private:
    S3D_RESOLVER                 m_resolver;
    wxString                     m_filename;
    PCBMODEL*                    m_pcb_model;
    DOUBLET                      m_origin;
    DOUBLET                      m_gridOrigin;
    DOUBLET                      m_drillOrigin;
    bool                         m_useGridOrigin;
    bool                         m_useDrillOrigin;
    bool                         m_hasGridOrigin;    // indicates origin found in source file
    bool                         m_hasDrillOrigin;   // indicates origin found in source file

    // minimum distance between points to treat them as separate entities (mm)
    double                       m_minDistance;

    // the names of layers in use, and the internal layer ID
    std::map<std::string, int>   m_layersNames;

    // PCB parameters/entities
    double                       m_thickness;
    wxColour                     m_topSolderMask;
    wxColour                     m_bottomSolderMask;    // Not currently used.
    wxColour                     m_topSilk;             // Not currently used.
    wxColour                     m_bottomSilk;          // Not currently used.
    wxColour                     m_copperFinish;        // Not currently used.
    std::vector<KICADFOOTPRINT*> m_footprints;
    std::vector<KICADCURVE*>     m_curves;
    wxString                     m_pcbName;
};


#endif  // KICADPCB_H
