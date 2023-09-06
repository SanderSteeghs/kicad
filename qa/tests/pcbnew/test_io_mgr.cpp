/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Roberto Fernandez Bautista <roberto.fer.bau@gmail.com>
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


#include <pcbnew_utils/board_test_utils.h>
#include <pcbnew_utils/board_file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <pcbnew/io_mgr.h>



BOOST_AUTO_TEST_SUITE( IOMGR )


struct PCB_IO_PLUGIN_CASE
{
    std::string        m_case_name;
    std::string        m_file_rel_path;
    IO_MGR::PCB_FILE_T m_expected_type;
};


// clang-format off
static const std::vector<PCB_IO_PLUGIN_CASE> BoardPluginCases = {

    //
    // FAKE Boards (should return FILE_TYPE_NONE):
    //
    {
        "Fake Board file (KiCad *Legacy* / EAGLE file ext)",
        "plugins/fakeboard.brd",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Fake Board file (KiCad file ext)",
        "plugins/fakeboard.kicad_pcb",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Fake Board file (PCAD file ext)",
        "plugins/fakeboard.pcb",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Fake Board file (CADSTAR file ext)",
        "plugins/fakeboard.cpa",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Fake Board file (Altium Circuit Studio file ext)",
        "plugins/fakeboard.CSPcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Fake Board file (Altium Circuit Maker file ext)",
        "plugins/fakeboard.CMPcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Fake Board file (Altium Designer file ext)",
        "plugins/fakeboard.PcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },

    {
        "Fake Board file (Solid Works PCB file ext)",
        "plugins/fakeboard.SWPcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },

    //
    // REAL Boards:
    //

    {
        "Basic KiCad *Legacy* board file",
        "plugins/legacy_demos/flat_hierarchy/flat_hierarchy.brd",
        IO_MGR::LEGACY
    },
    {
        "Basic KiCad board file",
        "complex_hierarchy.kicad_pcb",
        IO_MGR::KICAD_SEXP
    },
    {
        "Basic Eagle board file",
        "plugins/eagle/Adafruit-AHT20-PCB/Adafruit AHT20 Temperature & Humidity.brd",
        IO_MGR::EAGLE
    },
    {
        "Basic PCAD board file",
        "plugins/pcad/pcad_4layer_glyph_test_ascii.PCB",
        IO_MGR::PCAD
    },
    {
        "Basic CADSTAR board file",
        "plugins/cadstar/route_offset/minimal_route_offset_curved_track.cpa",
        IO_MGR::CADSTAR_PCB_ARCHIVE
    },
    {
        "Basic Altium Designer board file",
        "plugins/altium/eDP_adapter_dvt1_source/eDP_adapter_dvt1.PcbDoc",
        IO_MGR::ALTIUM_DESIGNER
    }
    // Todo: Add Altium derivatives and Fabmaster tests
};


static const std::vector<PCB_IO_PLUGIN_CASE> LibraryPluginCases = {

    //
    // NOT libraries (should return FILE_TYPE_NONE):
    //
    {
        "Non-Library file (KiCad *Legacy* / EAGLE file ext)",
        "plugins/fakeboard.brd",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Non-Library file (KiCad file ext)",
        "plugins/fakeboard.kicad_pcb",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Non-Library file (PCAD file ext)",
        "plugins/fakeboard.pcb",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Non-Library file (CADSTAR file ext)",
        "plugins/fakeboard.cpa",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Non-Library file (Altium Circuit Studio file ext)",
        "plugins/fakeboard.CSPcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Non-Library file (Altium Circuit Maker file ext)",
        "plugins/fakeboard.CMPcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },
    {
        "Non-Library file (Altium Designer file ext)",
        "plugins/fakeboard.PcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },

    {
        "Non-Library file (Solid Works PCB file ext)",
        "plugins/fakeboard.SWPcbDoc",
        IO_MGR::FILE_TYPE_NONE
    },

    //
    // REAL Libraries:
    //

    {
        "Basic KiCad footprint .pretty library",
        "plugins/eagle/lbr/SparkFun-GPS.pretty",
        IO_MGR::KICAD_SEXP
    },
    {
        "Basic Eagle library file",
        "plugins/eagle/lbr/SparkFun-GPS.lbr",
        IO_MGR::EAGLE
    },
    {
        "Basic CADSTAR PCB Archive library file",
        "plugins/cadstar/lib/footprint-with-thermal-pad.cpa",
        IO_MGR::CADSTAR_PCB_ARCHIVE
    },
    {
        "Altium Designer 'Espressif ESP32-WROOM-32.PcbLib' library file",
        "plugins/altium/pcblib/Espressif ESP32-WROOM-32.PcbLib",
        IO_MGR::ALTIUM_DESIGNER
    },
    {
        "Altium Designer 'Tracks.v6.PcbLib' library file",
        "plugins/altium/pcblib/Tracks.v6.PcbLib",
        IO_MGR::ALTIUM_DESIGNER
    }
    // Todo: Add Altium derivatives and Fabmaster tests
};
// clang-format on


BOOST_AUTO_TEST_CASE( FindBoardPluginType )
{
    for( auto& c : BoardPluginCases )
    {
        BOOST_TEST_CONTEXT( c.m_case_name )
        {
            std::string dataPath = KI_TEST::GetPcbnewTestDataDir() + c.m_file_rel_path;

            BOOST_CHECK_EQUAL( IO_MGR::FindPluginTypeFromBoardPath( dataPath ),
                               c.m_expected_type );

            // Todo add tests to check if it still works with upper/lower case ext.
            // ( FindPluginTypeFromBoardPath should be case insensitive)
        }
    }
}


BOOST_AUTO_TEST_CASE( GuessLibraryPluginType )
{
    for( auto& c : LibraryPluginCases )
    {
        BOOST_TEST_CONTEXT( c.m_case_name )
        {
            std::string dataPath = KI_TEST::GetPcbnewTestDataDir() + c.m_file_rel_path;

            BOOST_CHECK_EQUAL( IO_MGR::GuessPluginTypeFromLibPath( dataPath ), c.m_expected_type );

            // Todo add tests to check if it still works with upper/lower case ext.
            // ( GuessPluginTypeFromLibPath should be case insensitive)
        }
    }
}


BOOST_AUTO_TEST_CASE( CheckCanReadBoard )
{
    for( auto& c : BoardPluginCases )
    {
        BOOST_TEST_CONTEXT( c.m_case_name )
        {
            std::string dataPath = KI_TEST::GetPcbnewTestDataDir() + c.m_file_rel_path;

            auto& pluginEntries = IO_MGR::PLUGIN_REGISTRY::Instance()->AllPlugins();

            for( auto& entry : pluginEntries )
            {
                BOOST_TEST_CONTEXT( entry.m_name )
                {
                    auto plugin = PLUGIN::RELEASER( IO_MGR::PluginFind( entry.m_type ) );
                    bool expectValidHeader = c.m_expected_type == entry.m_type;

                    BOOST_CHECK_EQUAL( plugin->CanReadBoard( dataPath ), expectValidHeader );
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()