/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RC_JSON_SCHEMA_H
#define RC_JSON_SCHEMA_H

#include <nlohmann/json.hpp>
#include <wx/string.h>
#include <vector>

/**
 * Contains the json serialization structs for DRC and ERC reports
 * If you are trying to change the output schema
 * Please update the schemas located in /resources/schemas/ as both documentation
 * and use by end user implementations
 */
namespace RC_JSON
{
struct COORDINATE
{
    double x;
    double y;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( COORDINATE, x, y )

struct AFFECTED_ITEM
{
    wxString uuid;
    wxString description;
    COORDINATE pos;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( AFFECTED_ITEM, uuid, description, pos )

struct VIOLATION
{
    wxString                   type;
    wxString                   description;
    wxString                   severity;
    std::vector<AFFECTED_ITEM> items;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( VIOLATION, type, description, severity, items )

struct REPORT_BASE
{
    wxString source;
    wxString date;
    wxString kicad_version;
    wxString type;
    wxString coordinate_units;
};

struct DRC_REPORT : REPORT_BASE
{
    DRC_REPORT() { type = wxS( "drc" ); }

    std::vector<VIOLATION>                 violations;
    std::vector<VIOLATION>                 unconnected_items;
    std::vector<VIOLATION>                 schematic_parity;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( DRC_REPORT, source, date, kicad_version, violations,
                                    unconnected_items, schematic_parity, coordinate_units )

struct ERC_SHEET
{
    wxString               uuid;
    wxString               path;
    std::vector<VIOLATION> violations;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( ERC_SHEET, uuid, path, violations )

struct ERC_REPORT : REPORT_BASE
{
    ERC_REPORT() { type = wxS( "erc" ); }

    std::vector<ERC_SHEET> sheets;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( ERC_REPORT, source, date, kicad_version, sheets,
                                    coordinate_units )

} // namespace RC_JSON

#endif