/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2021-2023 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <settings/settings_manager.h>

#include <pcbnew/pad.h>
#include <pcbnew/pcb_track.h>

#include <router/pns_node.h>
#include <router/pns_router.h>
#include <router/pns_item.h>
#include <router/pns_via.h>
#include <router/pns_kicad_iface.h>

static bool isCopper( const PNS::ITEM* aItem )
{
    if( !aItem )
        return false;

    BOARD_ITEM* parent = aItem->Parent();

    if ( !parent )
    {
        return LSET::AllCuMask().Contains( (PCB_LAYER_ID) aItem->Layer() );
    }

    if( parent && parent->Type() == PCB_PAD_T )
    {
        PAD* pad = static_cast<PAD*>( parent );
        return pad->IsOnCopperLayer() && pad->GetAttribute() != PAD_ATTRIB::NPTH;
    }

    return true;
}


static bool isHole( const PNS::ITEM* aItem )
{
    if( !aItem )
        return false;

    return aItem->OfKind( PNS::ITEM::HOLE_T );
}


static bool isEdge( const PNS::ITEM* aItem )
{
    if( !aItem )
        return false;

    const BOARD_ITEM* parent = aItem->Parent();

    if ( !parent )
    {
        return aItem->Layer() == Edge_Cuts;
    }

    return parent && ( parent->IsOnLayer( Edge_Cuts ) || parent->IsOnLayer( Margin ) );
}


class MOCK_RULE_RESOLVER : public PNS::RULE_RESOLVER
{
public:
    MOCK_RULE_RESOLVER() : m_clearanceEpsilon( 10 )
    {
    }

    virtual ~MOCK_RULE_RESOLVER() {}

    virtual int Clearance( const PNS::ITEM* aA, const PNS::ITEM* aB,
                           bool aUseClearanceEpsilon = true ) override
    {
        PNS::CONSTRAINT constraint;
        int             rv = -1;
        int             layer;

        if( !aA->Layers().IsMultilayer() || !aB || aB->Layers().IsMultilayer() )
            layer = aA->Layer();
        else
            layer = aB->Layer();

        if( isHole( aA ) && isHole( aB ) )
        {
            if( QueryConstraint( PNS::CONSTRAINT_TYPE::CT_HOLE_TO_HOLE, aA, aB, layer,
                                 &constraint ) )
                rv = constraint.m_Value.Min();
        }
        else if( isHole( aA ) || isHole( aB ) )
        {
            if( QueryConstraint( PNS::CONSTRAINT_TYPE::CT_HOLE_CLEARANCE, aA, aB, layer,
                                 &constraint ) )
                rv = constraint.m_Value.Min();
        }
        else if( isCopper( aA ) && ( !aB || isCopper( aB ) ) )
        {
            if( QueryConstraint( PNS::CONSTRAINT_TYPE::CT_CLEARANCE, aA, aB, layer, &constraint ) )
                rv = constraint.m_Value.Min();
        }
        else if( isEdge( aA ) || ( aB && isEdge( aB ) ) )
        {
            if( QueryConstraint( PNS::CONSTRAINT_TYPE::CT_EDGE_CLEARANCE, aA, aB, layer,
                                 &constraint ) )
            {
                if( constraint.m_Value.Min() > rv )
                    rv = constraint.m_Value.Min();
            }
        }

        return rv;
    }


    virtual int  DpCoupledNet( int aNet ) override { return -1; }
    virtual int  DpNetPolarity( int aNet ) override { return -1; }
    virtual bool DpNetPair( const PNS::ITEM* aItem, int& aNetP, int& aNetN ) override
    {
        return false;
    }
    virtual bool IsDiffPair( const PNS::ITEM* aA, const PNS::ITEM* aB ) override { return false; }

    virtual bool QueryConstraint( PNS::CONSTRAINT_TYPE aType, const PNS::ITEM* aItemA,
                                  const PNS::ITEM* aItemB, int aLayer,
                                  PNS::CONSTRAINT* aConstraint ) override
    {
        ITEM_KEY key;

        key.a = aItemA;
        key.b = aItemB;
        key.type = aType;

        auto it = m_ruleMap.find( key );
        if( it == m_ruleMap.end() )
        {
            int cl;
            switch( aType )
            {
            case PNS::CONSTRAINT_TYPE::CT_CLEARANCE: cl = m_defaultClearance; break;
            case PNS::CONSTRAINT_TYPE::CT_HOLE_TO_HOLE: cl = m_defaultHole2Hole; break;
            case PNS::CONSTRAINT_TYPE::CT_HOLE_CLEARANCE: cl = m_defaultHole2Copper; break;
            default: return false;
            }

            //printf("GetDef %s %s %d cl %d\n", aItemA->KindStr().c_str(), aItemB->KindStr().c_str(), aType, cl );

            aConstraint->m_Type = aType;
            aConstraint->m_Value.SetMin( cl );

            return true;
        } else {
            *aConstraint = it->second;
        }

        return true;
    }

    virtual wxString NetName( int aNet ) override { return wxT( "noname" ); }

    int ClearanceEpsilon() const override { return m_clearanceEpsilon; }

    struct ITEM_KEY
    {
        const PNS::ITEM*     a = nullptr;
        const PNS::ITEM*     b = nullptr;
        PNS::CONSTRAINT_TYPE type;

        bool operator==( const ITEM_KEY& other ) const
        {
            return a == other.a && b == other.b && type == other.type;
        }

        bool operator<( const ITEM_KEY& other ) const
        {
            if( a < other.a )
            {
                return true;
            }
            else if ( a == other.a )
            {
                if( b < other.b )
                    return true;
                else if ( b == other.b )
                    return type < other.type;
            }

            return false;
        }
    };

    virtual bool IsInNetTie( const PNS::ITEM* aA ) override { return false; }
    virtual bool IsNetTieExclusion( const PNS::ITEM* aItem, const VECTOR2I& aCollisionPos,
                                    const PNS::ITEM* aCollidingItem ) override
    {
        return false;
    }

    void AddMockRule( PNS::CONSTRAINT_TYPE aType, const PNS::ITEM* aItemA, const PNS::ITEM* aItemB,
                      PNS::CONSTRAINT aConstraint )
    {
        ITEM_KEY key;

        key.a = aItemA;
        key.b = aItemB;
        key.type = aType;

        m_ruleMap[key] = aConstraint;
    }

    int m_defaultClearance = 200000;
    int m_defaultHole2Hole = 220000;
    int m_defaultHole2Copper = 210000;

private:
    std::map<ITEM_KEY, PNS::CONSTRAINT> m_ruleMap;
    int                                 m_clearanceEpsilon;
};

struct PNS_TEST_FIXTURE;

class MOCK_PNS_KICAD_IFACE : public PNS_KICAD_IFACE_BASE
{
public:
    MOCK_PNS_KICAD_IFACE( PNS_TEST_FIXTURE *aFixture ) :
        m_testFixture( aFixture )
    {}

    ~MOCK_PNS_KICAD_IFACE() {}

    void HideItem( PNS::ITEM* aItem ) override {};
    void DisplayItem( const PNS::ITEM* aItem, int aClearance, bool aEdit = false,
                      bool aIsHeadTrace = false ) override {};
    PNS::RULE_RESOLVER* GetRuleResolver() override;

private:
    PNS_TEST_FIXTURE* m_testFixture;
};


struct PNS_TEST_FIXTURE
{
    PNS_TEST_FIXTURE() :
        m_settingsManager( true /* headless */ )
    {
        m_router = new PNS::ROUTER;
        m_iface = new MOCK_PNS_KICAD_IFACE( this );
        m_router->SetInterface( m_iface );
    }

    SETTINGS_MANAGER      m_settingsManager;
    PNS::ROUTER*          m_router;
    MOCK_RULE_RESOLVER    m_ruleResolver;
    MOCK_PNS_KICAD_IFACE* m_iface;
    //std::unique_ptr<BOARD> m_board;
};


PNS::RULE_RESOLVER* MOCK_PNS_KICAD_IFACE::GetRuleResolver()
{
    return &m_testFixture->m_ruleResolver;
}

static void dumpObstacles( const PNS::NODE::OBSTACLES &obstacles )
{
    for( const PNS::OBSTACLE& obs : obstacles )
    {
        printf( "%p [%s] - %p [%s], clearance %d\n",
                obs.m_head, obs.m_head->KindStr().c_str(),
                obs.m_item, obs.m_item->KindStr().c_str(),
                obs.m_clearance );
    }
}

BOOST_FIXTURE_TEST_CASE( PNSHoleCollisions, PNS_TEST_FIXTURE )
{
    PNS::VIA* v1 = new PNS::VIA( VECTOR2I( 0, 1000000 ), LAYER_RANGE( F_Cu, B_Cu ), 500000, 100000 );
    PNS::VIA* v2 = new PNS::VIA( VECTOR2I( 0, 2000000 ), LAYER_RANGE( F_Cu, B_Cu ), 500000, 100000 );

    std::unique_ptr<PNS::NODE> world ( new PNS::NODE );

    world->SetMaxClearance( 10000000 );
    world->SetRuleResolver( &m_ruleResolver );

    world->AddRaw( v1 );
    world->AddRaw( v2 );

    BOOST_TEST_MESSAGE( "via to via, no violations" );
    {
        PNS::NODE::OBSTACLES obstacles;
        int count = world->QueryColliding( v1, obstacles );
        BOOST_CHECK_EQUAL( obstacles.size(), 0 );
        BOOST_CHECK_EQUAL( count, 0 );
    }

    BOOST_TEST_MESSAGE( "via to via, forced copper to copper violation" );
    {
        PNS::NODE::OBSTACLES obstacles;
        m_ruleResolver.m_defaultClearance = 1000000;
        world->QueryColliding( v1, obstacles );
        dumpObstacles( obstacles );

        BOOST_CHECK_EQUAL( obstacles.size(), 1 );
        const auto first = *obstacles.begin();

        BOOST_CHECK_EQUAL( first.m_head, v1 );
        BOOST_CHECK_EQUAL( first.m_item, v2 );
        BOOST_CHECK_EQUAL( first.m_clearance, m_ruleResolver.m_defaultClearance );
    }

    BOOST_TEST_MESSAGE( "via to via, forced copper to hole violation" );
    {
        PNS::NODE::OBSTACLES obstacles;
        m_ruleResolver.m_defaultClearance = 200000;
        m_ruleResolver.m_defaultHole2Copper = 1000000;

        world->QueryColliding( v1, obstacles );
        dumpObstacles( obstacles );

        BOOST_CHECK_EQUAL( obstacles.size(), 2 );
        auto iter = obstacles.begin();
        const auto first = *iter++;
        const auto second = *iter;

        BOOST_CHECK_EQUAL( first.m_head, v1 );
        BOOST_CHECK_EQUAL( first.m_item, v2 );
        BOOST_CHECK_EQUAL( first.m_clearance, m_ruleResolver.m_defaultHole2Copper );
    }


}

