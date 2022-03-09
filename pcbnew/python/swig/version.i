/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2022 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>
 */

/**
 * @file version.i
 * @brief KiCad version routine
 */

#pragma SWIG nowarn=305

%include kicad_build_version.h

%pythoncode
%{
    def Version():
        """Return the semantic version of KiCad"""
        return KICAD_SEMANTIC_VERSION

    def FullVersion():
        """Return the full, git-based version of KiCad"""
        return KICAD_VERSION_FULL
%}