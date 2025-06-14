/*
 * FILE:    item_character.cpp
 * 
 * PROJECT: MidnightEngine
 * 
 * froupedD:
 * 
 * AUTHOR:  Chris Wild
 * 
 * Copyright 2011 Chilli Hugger. All rights reserved.
 * 
 * PURPOSE: 
 * 
 * 
 */

#include "../baseinc/tme_internal.h"
#include "../scenario/helpers/race_terrain_movement_modifier.h"

#include <memory>
#include <string>

#if defined(_DDR_)
#include "../scenarios/ddr/scenario_ddr_internal.h"
#endif

#define ENABLE_GROUPING 1

namespace tme {

        mxcharacter::mxcharacter() 
        {
            mxentity::idType = IDT_CHARACTER ;
            //memory=NULL;
        }

        mxcharacter::~mxcharacter()
        {
        }

        void mxcharacter::Serialize ( archive& ar )
        {
            
            mxitem::Serialize(ar);
            if ( ar.IsStoring() ) {
                ar << longname;
                ar << shortname;
                WRITE_ENUM( looking );    
                WRITE_ENUM( time );
                ar << riders;
                ar << warriors;
                ar << battleloc;
                ar << battleslew;
                ar << reckless;
                ar << energy;
                ar << strength;
                ar << cowardess;
                ar << courage;
                ar << fear;
                ar << RecruitingKey;
                ar << RecruitedBy;
                WRITE_ENUM( race );
                ar << carrying ;
                ar << killedbyobject ;
                WRITE_ENUM( gender ) ;
                WRITE_ENUM( loyalty );
                ar << liege ;
                ar << foe ;
                WRITE_ENUM( wait );

                WRITE_ENUM(orders);
                ar << despondency;
                ar << traits ;
                
                ar << following ;
                ar << followers ;
                
            }else{
                ar >> longname;
                ar >> shortname;
                READ_ENUM ( looking );
                READ_ENUM ( time );
                ar >> riders;
                ar >> warriors;
                ar >> battleloc;
                ar >> battleslew;
                ar >> reckless;
                ar >> energy;
                ar >> strength;
                ar >> cowardess;
                ar >> courage;
                ar >> fear;
                ar >> RecruitingKey;
                ar >> RecruitedBy;
                READ_ENUM ( race );
                ar >> carrying ;
                ar >> killedbyobject ;
                READ_ENUM ( gender );
                READ_ENUM ( loyalty );
                ar >> liege ;
                ar >> foe ;
                READ_ENUM ( wait );

                READ_ENUM(orders) ;
                
                ar >> despondency;
                ar >> traits ;
            
                if ( tme::mx->SaveGameVersion() > 2 )
                    ar >> following ;
                else
                    following = NULL ;
             
                if ( tme::mx->SaveGameVersion() > 4 )
                    ar >> followers ;
                else
                    followers = 0 ;
                
                // temp bug fix
                if ( (s32)energy < 0 )
                    energy= 0;
            }

        }

        void mxcharacter::ForEachFollower(const std::function<void(mxcharacter*)> &callback)
        {
            if (!HasFollowers()) return;
            for ( auto follower : mx->scenario->GetCharacterFollowers(this) ) {
                callback(follower);
            }
        }

        mxcharacter* mxcharacter::FindFollower(const std::function<bool(mxcharacter*)> &callback)
        {
            if (!HasFollowers()) return nullptr;
            for ( auto follower : mx->scenario->GetCharacterFollowers(this) ) {
                if (callback(follower)) {
                    return follower;
                }
            }
            return nullptr;
        }

        void mxcharacter::RefreshLocationBasedVariables ( s32 icefear )
        {
            fear = icefear ;
            s32 temp  = cowardess-(fear/7);
            temp = std::max<int>( temp, 0 ) / 8 ;
            courage = std::min<int>( temp, 7 ) ;
        }

        void mxcharacter::ForcedVariableRefresh () const
        {
            // NOTE: This breaks the rules of this function being const
            // but i am only modifying temporary variables, so this should be okay
            mxcharacter* newcharacter = mx->CharacterById(Id());
            std::unique_ptr<mxlocinfo> info(new mxlocinfo( Location(), newcharacter, slf_none ));
            newcharacter->RefreshLocationBasedVariables ( info->adj_fear );
        }

        std::unique_ptr<mxlocinfo> mxcharacter::GetLocInfo()
        {
            // create a new info class
            // for our current location and the direction
            // we are looking
            auto flags = IsInTunnel() ? slf_tunnel : slf_none;
            
            std::unique_ptr<mxlocinfo> info( new mxlocinfo ( Location(), Looking(), this, flags ) );

            if ( IsNight() ) {
                info->flags.Reset(lif_enter_tunnel);
            }

            if ( IsDead() )
                return info;

            // set the characters courage and fear
            RefreshLocationBasedVariables ( info->adj_fear );

            // if the character is hidden  then
            // nothing else is possible
            info->flags.Set(lif_hide); // = TRUE ;
            if ( IsHidden()  ) {
                info->flags.Reset(lif_enterbattle); // = FALSE ;
                info->flags.Set(lif_unhide);

                if( mx->Difficulty() > DF_EASY && IsNight())
                    info->flags.Reset(lif_unhide);
                return info;
            }

            // can we seek?
            info->flags.Set(lif_seek); // = TRUE ;

            // can we hide ?
            // 1. Only if we are allowed to an we have no armies
            // 2. TODO how about allowing hiding when armies
            // are below a certain number? - maybe warriors only
            if ( !IsAllowedHide() || HasArmy() || IsFollowing() || HasFollowers() || IsInTunnel() )
                info->flags.Reset(lif_hide); // = FALSE ;

            // can't hide or seek at night
            if( mx->Difficulty() > DF_EASY  && IsNight())
                info->flags.Reset(lif_hide|lif_seek); // = false ;

#if !defined(_DDR_)
            // if in battle there is nothing more we can do
            if ( info->foe.armies )
                return info; 
#endif
            
            // make a list of all other recruitable
            // characters available
            // TODO in LOM this can only be one
            if ( mx->scenario->IsFeature(SF_APPROACH_DDR) ) {
                if(!mx->scenario->isLocationImpassable(info->loc_infront, info->owner)) {
                    info->FindApproachCharactersInfront();
                }
            } else {
                info->FindRecruitCharactersHere();
            }

            // can we recruit ?
            if ( info->objRecruit.Count() ) {
                info->flags.Set(lif_recruitchar); // = TRUE ;
            
                if( mx->Difficulty() > DF_EASY  && IsNight())
                    info->flags.Reset(lif_recruitchar); // = false ;
            }
            
            // can we recruitmen or guardmen ?
            // Recruit:
            // 1. only if there is a stronghold here
            // 2. Is the stronghold race the same as us?
            // 3. Are we allowed an army?
            // 4. does the stronghold have more than its minimum in?
            // 5. can we take any more soldiers?

            // guard:
            // 1. only if there is a stronghold here
            // 2. Is the stronghold race the same as us?
            // 3. does the stronghold have less than its maximum in?
            // 4. do we have enough soldiers of the required type?

            // TODO this is too restrictive, the stronghold::add routine
            // allows for arbitruary values to be added, but this
            // uses the LOM model of having to place 100 at a time
            // should this change?

            if ( info->objStrongholds.Count() ) {
                mxstronghold* stronghold = (mxstronghold*)info->objStrongholds[0] ;

                if ( stronghold->CanCharacterRecruitOrPost(this) ) {
                    // can we recruit men ?
                    // this is not an exact check, a stronghold CAN have less than the minimum
                    // in, however you can no longer recruit after the min has been reached
                    if ( IsAllowedArmy() && stronghold->TotalTroops() > stronghold->MinTroops() ) {
                        if ( stronghold->Type() == UT_WARRIORS && IsAllowedWarriors() ) {
                            if ( warriors.total + sv_character_recruit_amount <= sv_character_max_warriors) {
                                info->flags.Set(lif_recruitmen) ;
                            }
                        }
                    
                        if ( stronghold->Type() == UT_RIDERS && IsAllowedRiders() ) {
                            if ( riders.total + sv_character_recruit_amount <= sv_character_max_riders ) {
                                info->flags.Set(lif_recruitmen);
                            }
                        }
                    }

                    // can we guard men ?
                    if ( stronghold->TotalTroops() + sv_character_guard_amount <= stronghold->MaxTroops() ) {
                        if ( (stronghold->Type()==UT_WARRIORS) && (warriors.total >= sv_character_guard_amount ) )
                            info->flags.Set(lif_guardmen); // = TRUE ;
                        if ( (stronghold->Type()==UT_RIDERS) && (riders.total >= sv_character_guard_amount ) )
                            info->flags.Set(lif_guardmen); // = TRUE ;
                    }

                }

            }
            
            // use
            // 1. are we carrying our object
            // 2. have we used it
            
            // take
            // 1. is there an object here?
            
            // give
            // 1. do we have an object
            // 2. is there another character here
            // 3. are we generous
                        
            auto thing = mx->gamemap->getLocationObject(this, Location());
            
            // can we fight ?
            // can we fight the object currently in this location?
            auto obj = mx->ObjectById(thing);
            if ( CheckFightObject ( obj )  ) {
                info->flags.Set(lif_fight); // = TRUE ;
                info->fightthing = thing ;
            }

            
            // attack
            if ( info->flags.Is(lif_enterbattle) ) {
            // if a battle is allowed and we don't have any courage
            // then we can't actually enter the battle
                if ( IsCoward() )
                    info->flags.Reset(lif_enterbattle) ;// = FALSE ;
                
                if ( IsFollowing()  )
                    info->flags.Reset(lif_enterbattle) ;// = FALSE ;

                // if we have followers then all my followers
                // must have courage
                info->stubborn_follower_battle = FIND_FOLLOWER(f) {
                    return f->IsCoward();
                });
            }
            
            // Farflame cannot move in to small tunnels
            // Armies cannot move in to small tunnels
            if (IsInTunnel()) {
                if (info->infront->mapsqr.HasTunnel() && info->infront->mapsqr.IsSmallTunnel()) {
                    if(Symbol() == "CH_FARFLAME" || HasArmy() || IsRiding()) {
                        info->flags.Reset(lif_moveforward);
                        info->flags.Set(lif_blocked);
                    }
                }
            }
            

            // if we are leading
            // then the whole group needs to be able to move
            info->stubborn_follower_move = FIND_FOLLOWER(f) {
                return !f->CanWalkForward();
            });
            
            
            // Dismount
            if (mx->scenario->IsFeature(SF_DISMOUNT)) {
                if (IsRiding() && !IsInTunnel()) {
                    info->flags.Set(lif_dismount);
                }
            }
            
            return info ;

        }

        s32 mxcharacter::DistanceFromLoc ( mxgridref loc )
        {
            return Location() - loc ;
        }

        bool mxcharacter::CheckFightObject ( mxobject* obj ) const
        {
            return obj ? obj->CanFight() : FALSE ;
        }

        bool mxcharacter::CheckRecruitChar ( mxcharacter* character ) const
        {
            if ( character == this )
                return false ;

            if ( mx->scenario->IsFeature(SF_RECRUIT_DDR) ) {
                // TODO: recruiting logic
                return true ;
            }else{
                if ( character->Race() == RA_FEY) {
                    // recruiting of Feys 'off', cannot recruit
                    if (mx->isRuleEnabled(RF_LOM_FEY_RECRUIT_OFF)) {
                        return false;
                    }
                    
                    // In the novel, Feys only join the cause after 
                    // the Lord of Dreams joins.
                    // The Lord of Dreams is required for recruiting Fey, 
                    // he can always be recruited and other Fey can
                    // be recruited by all if he has already been recruited
                    if (mx->isRuleEnabled(RF_LOM_FEY_RECRUIT_NOVEL) && 
                        character->Id() != mx->scenario->dreams->Id()) {
                        // Fey, novel mode and not Lord of Dreams?
                        // Fey can be recruited by anyone if Lord of Dreams has been recruited
                        return mx->scenario->dreams->IsRecruited();
                    }
                }
                // check that 'this' character's RecruitingKey
                // can recruit 'character' by checking who can 
                // recruit it in flags 'RecruitedBy'
                if ( character->RecruitedBy & RecruitingKey )
                    return true ;
            }

            return false ;
        }

        bool mxcharacter::ShouldLoseHorse(s32 hint) const
        {
            return mxrandom() & 1;
        }

        bool mxcharacter::ShouldDieInFight() const
        {
            auto temp = (energy/2) - 64 + reckless;
            return mxrandom(255) >= temp;
        }

        void mxcharacter::Dismount()
        {
            flags.Reset ( cf_riding );
        }

        void mxcharacter::LostFight ( s32 hint )
        {
            if ( !IsDead() ) {
                if ( IsRiding() ) {
                    if (ShouldLoseHorse())
                        Dismount();
                }

                if (ShouldDieInFight()) {
                    Cmd_Dead();
                }
            }
        }

        void mxcharacter::Displace ( void )
        {
        mxgridref    loc;

            if ( IsDead() || IsFollowing() || HasFollowers() )
                return;
                
            for (;; ) {
                loc = Location() + (mxdir_t)mxrandom(0, 7);
                
                if ( !mx->scenario->isLocationImpassable( loc, this ) ) {
                    Flags().Reset(cf_battleover);
                    location = loc ;
                    if ( IsRecruited() ) {
                        EnterLocation ( loc );
                        mx->scenario->LookInDirection (Location(), Looking(), IsInTunnel());
                    }
                    break;
                }
            }
        }
        
        void mxcharacter::LostBattle(bool canFlee)
        {
            // we might not have displaced the lord due to grouping
            // so mark them as not being able to fight anyone else in this round
            Flags().Set(cf_battleover);

            if ( IsFollowing() ) {
                if ( mx->Difficulty() == DF_HARD ) {
                    // if we are a member of the group, then we need to leave the group
                    Cmd_UnFollow(Following());
                }else{
                    return;
                }
            }
            
            if ( HasFollowers() ) {
                if ( mx->Difficulty() == DF_HARD ) {
                    // if we are leading a group, then we must disband the group
                    Cmd_DisbandGroup();
                }else{
                    return;
                }
            }

            if (canFlee) {
                // lord flees
                Displace();
            }
        }

        void mxcharacter::SetLastCommand ( command_t cmd, mxid id )
        {
            lastcommand = cmd;
            lastcommandid = id;
        }
        
        s32 mxcharacter::CommandSuccessTime(command_t cmd)
        {
            auto command = mx->CommandById( cmd );
            if ( command == nullptr )
                return 0; // invalid command

            return command->SuccessTime();
        }

        void mxcharacter::CommandTakesTime ( bool success )
        {
            // ** CHEAT
            if ( sv_cheat_commands_free )
                return;
            
            mxcommand* command = mx->CommandById( GET_ID(lastcommand) );
            if ( command == nullptr )
                return; // invalid command

            int timetaken = success ? command->SuccessTime() : command->failuretime;

            time = (mxtime_t) BSub(time,timetaken,0);
        }    
    
        MXRESULT mxcharacter::Cmd_MoveForward ( void )
        {
            SetLastCommand ( CMD_MOVE, IDT_NONE);

            // lords who are following cannot be moved on their own
            if ( IsFollowing() )
                return MX_FAILED ;
            
            if ( !CanWalkForward() )
                return MX_FAILED ;

            // if we are leading
            // then the whole group needs to be able to move
            auto cantWalkForward = FIND_FOLLOWER(f) { return !f->CanWalkForward(); });
            if ( cantWalkForward != nullptr )
                return MX_FAILED;
                        
            // find where we will be moving to
            auto info = GetLocInfo();
            
            // something blocking our path!
            if ( !info->flags.Is(lif_moveforward) ) {
                return MX_FAILED;
            }

#if defined(_LOM_)
            // if there is an army here
            // you can only move first thing
            if ( !sv_cheat_armies_noblock ) {
                if ( !IsDawn() && info->foe.armies ) {
                    return MX_FAILED;
                }
            }
            
            // something needs killing!
            if ( !sv_cheat_nasties_noblock ) {
                if ( info->flags.Is(lif_fight) ) {
                    return MX_FAILED;
                }
            }
#endif
            bool approach = false;
            if ( sv_auto_approach ) {
                 info->infront->FindRecruitCharactersHere();
                 approach = info->infront->objRecruit.Count() == 1;
            }
            
            return Cmd_WalkForward(sv_auto_seek, approach);
        }

        bool mxcharacter::CanWalkForward ( void )
        {
            // dead men don't walk!
            if ( IsDead() ) {
                return false;
            }
            
            // completely and untterly shattered?
            if ( energy <= sv_energy_cannot_continue ) {
                return false;
            }
            
            // hidden under a rock?
            if ( IsHidden() ) {
                // if auto unhide turned on then we must unhide and carry on
                if ( !sv_auto_unhide )
                    return false;
            }
            
            // should be sleeping!
            if ( IsNight() ) {
                return false;
            }

            return true;
        }
        
    
        MXRESULT mxcharacter::Cmd_WalkForward ( bool perform_seek, bool perform_approach )
        {
        s32            TimeCost;
        mxrace*        rinfo;

            // hidden under a rock?
            // if auto unhide turned on then we must unhide and carry on
            if ( IsHidden() &&  sv_auto_unhide ) {
                Cmd_UnHide();
            }
            
            rinfo = mx->RaceById(Race());

            // remove army from location
            mx->gamemap->SetLocationArmy(Location(),0);
            mx->gamemap->SetLocationCharacter(Location(),0);
            
            // set the current location to visited, just to make sure
            mx->gamemap->SetLocationVisited(location, TRUE);
            
            // and move us there
            location = location + Looking();

            // set the new current location to visited
            mx->gamemap->SetLocationVisited(location, TRUE);

            // add armies and characters back to the map
            // this is overkill!!!!
            mx->scenario->SetMapArmies();

            EnterLocation ( location );
            
            // if this location has an exit then we must exit
            bool exit_tunnel = mx->gamemap->GetAt ( location ).HasTunnelExit() ;
            
            mx->scenario->LookInDirection (Location(), Looking(), IsInTunnel());
            
            // calculate our movement values
            TimeCost = rinfo->InitialMovementValue();

            // are we moving diagonally?
            if ( IS_DIAGONAL(Looking()) )
                TimeCost += rinfo->DiagonalMovementModifier();

            // are we on horseback?
            if ( !IsRiding() )
                TimeCost = (s32) ((f64)TimeCost * rinfo->RidingMovementMultiplier()) ;

            // adjust for terrain
            TimeCost += TME::helpers::RaceTerrainMovementModifier::Get(
                Race(),
                (mxterrain_t)mx->gamemap->GetAt ( location ).terrain
            );

            // check for max out
            TimeCost = std::min<int>( TimeCost, (int)rinfo->MovementMax() );

            // ** CHEATS
            if ( sv_cheat_movement_cheap )
                TimeCost=1;
            if ( sv_cheat_movement_free )
                TimeCost=0;
            // ** END CHEATS
            
            // time
            time = (mxtime_t) BSub(time,TimeCost,0);

            // energy
            DecreaseEnergy(TimeCost);

            CommandTakesTime(TRUE);
            
            // if we have moved, we are no longer in battle
            flags.Reset(cf_inbattle|cf_preparesbattle);
            
            // auto approach
            if ( perform_approach ) {
               // we need to recruit before anyone else joins us
                if ( Cmd_Approach() != nullptr) {
                    perform_seek = false;
                }
            }
            
            // auto seek
            CheckPerformSeek (perform_seek, exit_tunnel);
            
            WalkFollowersForward();
            
            // if this location has an exit then we must exit
            if ( exit_tunnel )
                Cmd_ExitTunnel();
            
            return MX_OK ;
        }
        
        void mxcharacter::CheckPerformSeek(bool seek, bool tunnelexit)
        {
            if ( !IsAIControlled() && seek ) {
                // quick fix
                if ( tunnelexit ) flags.Reset ( cf_tunnel );

                if ( mx->gamemap->getLocationObject(this, Location())!=OB_NONE ) {
                    Cmd_Seek();
                }

                if ( tunnelexit ) flags.Set ( cf_tunnel );
            }
        }

        MXRESULT mxcharacter::Cmd_EnterTunnel ( void )
        {
            if ( IsFollowing() )
                return MX_FAILED ;

            if ( IsInTunnel() )
                return MX_FAILED ;
                
            mxloc& mapsqr = mx->gamemap->GetAt ( Location() );
            if ( !mapsqr.HasTunnelEntrance() )
                return MX_FAILED ;
            
            // remove army from location
            mx->gamemap->SetLocationArmy(Location(),0);
            mx->gamemap->SetLocationCharacter(Location(),0);
            
            flags.Set ( cf_tunnel );

            EnterLocation(Location());
            
            // TODO:
            // we now need to cycle round the locations
            // looking for the location that connections to us
            // and drop us in there
            for ( int ii=DR_NORTH; ii<=DR_NORTHWEST; ii+=2 ) {
                mxdir_t d = (mxdir_t) ii ;
                loc_t l = Location() + d ;
                if ( mx->gamemap->GetAt ( l ).HasTunnel() ) {
                    location = l ;
                    looking=d;
                    break;
                }
                
            }
            
            // new location
            EnterLocation(Location());
            
            // group enter tunnel
            
            FOR_EACH_FOLLOWER(f) {
                f->Flags().Set(cf_tunnel);
                f->looking = Looking();
                f->Location(Location());
            });
            
            mx->scenario->SetMapArmies();
            
            CommandTakesTime(TRUE);
            
            return MX_OK ;
        }
        
        MXRESULT mxcharacter::Cmd_ExitTunnel ( void )
        {
            if ( IsFollowing() )
                return MX_FAILED ;
            
            if ( !IsInTunnel() )
                return MX_FAILED ;
            
            mxloc& mapsqr = mx->gamemap->GetAt ( Location() );
            
            if ( !mapsqr.HasTunnelExit() )
                return MX_FAILED ;
            
            flags.Reset ( cf_tunnel );
            
            // group exit tunnel
            FOR_EACH_FOLLOWER(f) {
                f->Flags().Reset(cf_tunnel);
                f->looking = Looking();
                f->Location(Location());
            });
            
            CommandTakesTime(TRUE);
            
            return MX_OK ;
        }

        void mxcharacter::WalkFollowersForward()
        {
            // if we are leading
            // then the whole group needs to be able to move
            FOR_EACH_FOLLOWER(f) {
                // must look the same direction to make them move in the
                // correct direction
                f->looking = Looking();
                f->Cmd_WalkForward();
            });
        }

        void mxcharacter::DecreaseEnergy ( s32 amount )
        {
            energy = BSub(energy,amount,0);
            riders.energy = BSub(riders.energy,amount,0);
            warriors.energy = BSub(warriors.energy,amount,0);
        }


        void mxcharacter::IncreaseEnergy ( s32 amount )
        {
        mxrace* rinfo = mx->RaceById(Race());

            s32 baserest = rinfo->BaseRestAmount() ;

            energy = 
                BAdd ( energy, amount + baserest, (u32)sv_character_max_energy );

            riders.energy = 
                BAdd ( riders.energy, amount + riders.BaseRestModifier(), (u32)sv_riders_max_energy );

            warriors.energy = 
                BAdd ( warriors.energy, amount  + warriors.BaseRestModifier(), (u32)sv_warriors_max_energy );

        }

        MXRESULT mxcharacter::Cmd_LookLeft ( void )
        { 
            //looking=(mxdir_t) ((looking-1)&7); 
            looking = PREV_DIRECTION(looking);
            
            mx->scenario->LookInDirection (Location(), Looking(), IsInTunnel());
            
#if defined(_DDR_)
            mx->scenario->MakeMapAreaVisible(Location(), this);
#endif

            return MX_OK ;
        }

        MXRESULT mxcharacter::Cmd_LookRight ( void )
        { 
            //looking=(mxdir_t)((looking+1)&7); 
            looking = NEXT_DIRECTION(looking);
            
            mx->scenario->LookInDirection (Location(), Looking(), IsInTunnel());
            
#if defined(_DDR_)
            mx->scenario->MakeMapAreaVisible(Location(), this);
#endif

            return MX_OK ;
        }

        MXRESULT mxcharacter::Cmd_LookDir ( mxdir_t dir )
        { 
            looking=(mxdir_t)((dir)&7); 
            
            mx->scenario->LookInDirection (Location(), Looking(), IsInTunnel());
            
#if defined(_DDR_)
            mx->scenario->MakeMapAreaVisible(Location(), this);
#endif
            return MX_OK ;
        }

        bool mxcharacter::EnterLocation ( mxgridref loc )
        {
            mx->scenario->MakeMapAreaVisible ( loc, this );

            return TRUE ;
        }

        MXRESULT mxcharacter::Cmd_Lookat(mxgridref loc)
        {
            return Cmd_LookDir ( location.DirFromHere(loc) );
        }

        MXRESULT mxcharacter::Cmd_Place(mxgridref loc)
        {
            Location(loc);
            EnterLocation( Location() );
            return MX_OK ;
        }
        
        MXRESULT mxcharacter::Cmd_Rest ( void )
        { 
            SetLastCommand ( CMD_REST, IDT_NONE );

            if ( IsNight() || IsResting() )
                return MX_FAILED ;

            flags.Set(cf_resting);
            
            FOR_EACH_FOLLOWER(f) {
                f->Cmd_Rest();
            });
            
            CommandTakesTime(TRUE);

            return MX_OK ;
        }

        MXRESULT mxcharacter::Cmd_RecruitMen ( mxstronghold* stronghold, u32 qty )
        {
        s32                    delta;
        mxstronghold*        def_stronghold = NULL ;

            SetLastCommand ( CMD_RECRUITMEN, IDT_NONE );

            // get location info
            auto info = GetLocInfo() ;
        
            if ( info->objStrongholds.Count() )
                    def_stronghold = (mxstronghold*)info->objStrongholds[0] ;

            // are we allowed to recruitmen ?
            if ( !info->flags.Is(lif_recruitmen) ) {
                return MX_FAILED ;
            }

            // get the default stronghold
            if ( stronghold == NULL ) {
                stronghold = def_stronghold ;
                if ( stronghold == NULL )
                    return MX_FAILED ;
            }

            if ( qty == 0 )
                qty = (u32)sv_character_recruit_amount;

            // try and recruit upto the RECRUIT_AMOUNT
            if ( stronghold->Type() == UT_RIDERS ) {
                delta = (s32)sv_character_max_riders - riders.total ;
                qty = stronghold->Remove( Race(), UT_RIDERS, std::min<int>((s32)qty,delta) );
                riders.total += qty;
            } else if ( stronghold->Type() == UT_WARRIORS ) {
                delta = (s32)sv_character_max_warriors - warriors.total ;
                qty = stronghold->Remove( Race(), UT_WARRIORS, std::min<int>((s32)qty,delta) );
                warriors.total += qty;
            }

            mx->SetLastActionMsg(mx->text->DescribeCharacterRecruitMen( this, stronghold, qty ));

            CommandTakesTime(TRUE);

            return MX_OK ;
        }

        MXRESULT mxcharacter::Cmd_PostMen ( mxstronghold* stronghold, u32 qty )
        {
        mxstronghold*    def_stronghold = NULL ;

            SetLastCommand ( CMD_POSTMEN, IDT_NONE );

            // get location info
            auto info = GetLocInfo() ;
        
            if ( info->objStrongholds.Count() )
                    def_stronghold = (mxstronghold*)info->objStrongholds[0] ;

            // are we allowed to postmen ?
            if ( !info->flags.Is(lif_guardmen) ) {
                return MX_FAILED ;
            }

            // get the default stronghold
            if ( stronghold == NULL ) {
                stronghold = def_stronghold ;
                if ( stronghold == NULL )
                    return MX_FAILED ;
            }

            if ( qty == 0 )
                qty = (u32)sv_character_guard_amount;
            // try and guard upto the GUARD_AMOUNT
            if ( stronghold->Type() == UT_RIDERS )
                riders.total -= stronghold->Add( Race(), UT_RIDERS, std::min<int>(riders.total,qty) );
            else if ( stronghold->Type() == UT_WARRIORS )
                warriors.total -= stronghold->Add( Race(), UT_WARRIORS, std::min<int>(warriors.total,qty));


            mx->SetLastActionMsg(mx->text->DescribeCharacterPostMen( this, stronghold, qty ));
            
            CommandTakesTime(TRUE);

            return MX_OK ;
        }

        mxcharacter* mxcharacter::Cmd_Approach ( mxcharacter* character )
        {
            SetLastCommand ( CMD_APPROACH, IDT_NONE );

            if ( character == nullptr ) {

                // get location info
                auto info = GetLocInfo() ;
    
                // are we allowed to approach ?
                if ( !info->flags.Is(lif_recruitchar) ) {
                    return nullptr ;
                }

                character = static_cast<mxcharacter*>(info->objRecruit.First()) ;
            }

            RETURN_IF_NULL(character) nullptr ;

            if ( character->Recruited ( this ) ) {
                SetLastCommand ( CMD_APPROACH, mxentity::SafeIdt(character) );
                character->SetLastCommand ( CMD_APPROACHED, mxentity::SafeIdt(this) );
                return character ;
            }

            CommandTakesTime(false);
            
            return nullptr ;
        }
            
        bool mxcharacter::Recruited ( mxcharacter* recruiter )
        {
            // set loyalty?
            flags.Set ( cf_recruited );
        
            // 1. set time of day to recruiting character
            SetRecruitmentTime(recruiter);
            
            // 2. our liege becomes the recruiting character
            liege = recruiter->Liege() ;
            // 3. our loyalty race becomes recruiting character loyalty race
            loyalty = recruiter->NormalisedLoyalty() ;
            // 4. get the my foe's loyalty, if they are loyal to the recruiting char's loyalty
            // then we need the recruiting characters foe
            //
            mxcharacter* foe = Foe();
            if ( foe->NormalisedLoyalty() == loyalty ) {
                foe = recruiter->Foe() ;
            }
            
            CommandTakesTime(true);

            return true ;
        }
            
        void mxcharacter::SetRecruitmentTime( mxcharacter* recruiter )
        {
            s32 TimeCost = 1 * sv_time_scale; // 1 hr
        
            if ( mx->scenario->IsFeature(SF_RECRUIT_TIME) )
                time = recruiter->Time() ;

            if ( mx->Difficulty() == DF_EASY )
                time = sv_time_dawn ;
            else if ( mx->Difficulty() == DF_MEDIUM ) {
                recruiter->time = (mxtime_t) BSub(recruiter->time,TimeCost,0);
                time = recruiter->Time() ;
            } else if ( mx->Difficulty() == DF_HARD ) {
                time = sv_time_night ;
                recruiter->time = time;
            }
        }
            
        MXRESULT mxcharacter::EnterBattle ( void )
        {
            Flags().Set(cf_inbattle); // force the battle flag early!
            
            return MX_OK;
        }

        MXRESULT mxcharacter::Cmd_Attack ( void )
        {
            if ( IsFollowing() )
                return MX_FAILED ;
            
            auto info = GetLocInfo() ;
        
            bool battle = info->flags.Is(lif_enterbattle) ;
            if ( !battle || info->stubborn_follower_battle )
                return MX_FAILED;
            
            SetLastCommand(CMD_ATTACK,IDT_NONE);
            MXRESULT result = Cmd_WalkForward(false) ;
            if ( result == MX_OK ) {
                EnterBattle();
            }
            return result;
        }

        bool mxcharacter::ShouldHaveOneToOneWithNasty() const
        {
            // allow game difficulty to have an effect
            // on the grouping
            if ( mx->Difficulty() == DF_EASY && followers>= 2 ) {
                return false;
            }
            if ( mx->Difficulty() == DF_MEDIUM ) {
                if ( followers >= 3 )
                    return false;
            }
            
            s32 total = warriors.total + riders.total ;
            if ( mx->Difficulty() == DF_EASY && total >= 250 )
                return false;
                
            if ( mx->Difficulty() == DF_MEDIUM && total >= 750 )
                return false;
            
            return true;
        }

        mxobject* mxcharacter::Cmd_Fight ( void )
        {
        bool needfight = true ;
        bool killedwithobject = false;

            SetLastCommand ( CMD_FIGHT, IDT_NONE );

            auto info = GetLocInfo() ;
            
            // are we allowed to fight
            if ( !info->flags.Is(lif_fight) )
               return nullptr ;
                
            // is there anything to fight
            auto fightobject = mx->ObjectById(info->fightthing) ;
            if ( fightobject == nullptr )
                return nullptr ;

            SetLastCommand ( CMD_FIGHT,SafeIdt(fightobject)) ;

            // if there is any friends here
            // then we win by default
            if (info->friends.armies)
                needfight = false;

            auto oinfo = Carrying();
            if (oinfo != nullptr && oinfo->CanDestroy(fightobject)) {
                needfight = false;
                killedwithobject = true;
            }
            
            if ( sv_cheat_always_win_fight )
                needfight = false;

            if ( !ShouldHaveOneToOneWithNasty() )
                needfight = false;
  
            if ( needfight ) {

                LostFight();

                if ( IsDead() ) {
                    killedbyobject = fightobject ;
                    return fightobject;
                }
            }

            // we have killed the nasty
            mx->text->oinfo = fightobject;

            // describe how
            u32 message = killedwithobject
                ? oinfo->usedescription
                : SS_FIGHT ;

            mx->SetLastActionMsg(mx->text->CookedSystemString(message, this));

            CommandTakesTime(true);

            // this does an auto write to the map!
            mx->gamemap->GetAt( Location() ).RemoveObject();

            return fightobject ;
        }

        MXRESULT mxcharacter::Cmd_Hide ( void )
        {
            SetLastCommand ( CMD_HIDE, IDT_NONE );

            if ( !IsAllowedHide() )
                return MX_FAILED;

            if ( IsHidden() )
                return MX_FAILED;

            flags.Set ( cf_hidden );
            
            if ( mx->Difficulty() == DF_MEDIUM ) {
                time = BSub(time, sv_time_scale, 0);
            }
            else if ( mx->Difficulty() == DF_HARD ) {
                time = sv_time_night;
            }
            
            CommandTakesTime(true);
            
            return MX_OK ;
        }

        MXRESULT mxcharacter::Cmd_UnHide ( void )
        {
            SetLastCommand ( CMD_UNHIDE, IDT_NONE );

            if ( !IsHidden() )
                return MX_FAILED;
                
            // should add an extra check here that unhiding is allowed
                
            flags.Reset ( cf_hidden );
            
            if ( mx->Difficulty() > DF_EASY ) {
                time = BSub(time, sv_time_scale, 0);
            }
            
            CommandTakesTime(true);
            
            return MX_OK ;
        }

        mxthing_t mxcharacter::LocationThing() const
        {
            mxloc& mapsqr = mx->gamemap->GetAt ( Location() );
            mxthing_t thing = (mxthing_t)mapsqr.object;
            if ( thing == OB_NONE ) return thing;
            mxobject* oinfo = mx->ObjectById ( thing );
            return  oinfo->CanSee() ? thing : OB_NONE ;
        }

        MXRESULT mxcharacter::Cmd_Follow ( mxcharacter* c )
        {
            if ( !CanFollow(c) )
                return MX_FAILED ;
            
            // we need to do a merge
            FOR_EACH_FOLLOWER(f) {
                RemoveFollower(f);
                c->AddFollower(f);
            });
            
            c->AddFollower( this );
            
            CommandTakesTime(TRUE);
            
            return MX_OK ;
        }
    
        MXRESULT mxcharacter::Cmd_UnFollow ( mxcharacter* c )
        {
            
            // are we following someone?
            if ( Following() != c )
                return MX_FAILED ;
            
            c->RemoveFollower( this );
            
            CommandTakesTime(true);
            
            return MX_OK ;
        }

        bool mxcharacter::AddFollower ( mxcharacter* character )
        {
            if ( character->Following() != nullptr )
                return false;
            followers+=1;
            if ( followers > 0 )
                flags.Set(cf_followers);
            character->following=this;
            return true;
        }

        bool mxcharacter::RemoveFollower ( mxcharacter* character )
        {
            if ( character->Following() != this )
                return false;
            followers-=1;
            if ( followers == 0 )
                flags.Reset(cf_followers);
            
            character->following = nullptr ;
            
            return true;
        }
    
        MXRESULT mxcharacter::Cmd_DisbandGroup ( void )
        {
            if ( !HasFollowers() )
                return MX_FAILED;
            
            FOR_EACH_FOLLOWER(follower) {
                RemoveFollower(follower);
            });
            
            CommandTakesTime(TRUE);
            
            return MX_OK;
        }
    
        MXRESULT mxcharacter::Cmd_SwapGroupLeader ( mxcharacter* character )
        {
            if ( !HasFollowers() )
                return MX_FAILED;
            
            auto followers = mx->scenario->GetCharacterFollowers(this);
            
            // remove them all first
            for ( auto f : followers ) {
                RemoveFollower(f);
            }
            
            // then add them back in the same order
            // with the old leader taking the same slot as the new leader
            for ( auto f : followers ) {
                if ( character == f ) {
                    // I join the group
                    character->AddFollower(this);
                }else{
                    character->AddFollower(f);
                }
            }
            
            CommandTakesTime(TRUE);
            
            return MX_OK;
        }

        bool mxcharacter::CanFollow( const mxcharacter* c ) const
        {
#ifdef ENABLE_GROUPING
            if ( c == NULL )
                return MX_FAILED ;
            
            if ( IsDead() || c->IsDead() )
                return FALSE;
            
            // if we are following someone, then we must unfollow
            // if they are following someone then they must unfollow
            //if ( IsFollowing() || c->IsFollowing() )
            //    return FALSE ;
            
            //if ( HasFollowers() )
            //    return FALSE;
            
            if ( IsHidden() || c->IsHidden() )
                return FALSE;
            
            if ( c->HasFollowers() && c->followers>=MAX_CHARACTERS_FOLLOWING)
                return FALSE;
            
            // problems because of const!!!!
            mxgridref l = c->Location();
            mxgridref l2 = Location();
            
            // we must be at the same location as the character
            if ( l != l2 )
                return FALSE ;
            
            // they both need to be in or out of a tunnel
            if ( c->IsInTunnel() != IsInTunnel() )
                return FALSE;
                
            // we must be loyal to the same character
            if ( !IsFriend(c) )
                return FALSE ;
            
            return TRUE ;
#else
            return FALSE ;
#endif
        }

        MXRESULT mxcharacter::Cmd_Dismount ( void )
        {
            if ( !IsRiding() || IsInTunnel() )
                return MX_FAILED ;
                
            Dismount();
            
            CommandTakesTime(true);
            
            return MX_OK ;
        }

        mxobject* mxcharacter::Cmd_Seek ( void )
        {
        mxthing_t    newobject;
        mxobject*    oinfo;
        u32 timeCost = 0;
        u32 additionalTimeCost = 0;
        bool removeObject = true;
        bool mikeseek = false;
        bool seekMsg = true;

            if ( mx->Difficulty() == DF_MEDIUM ) {
                timeCost = sv_time_scale / 2;       // half hr
            } else if ( mx->Difficulty() == DF_HARD ) {
                timeCost = sv_time_scale ;          // 1 hr
            }

            SetLastCommand ( CMD_SEEK, IDT_NONE );
            
            CommandTakesTime(true);

            mxloc& mapsqr = mx->gamemap->GetAt ( Location() );
            
            newobject = mx->gamemap->getLocationObject(this, Location());
            if ( !IsInTunnel() ) {
                if ( location.x == 4 && location.y == 10 ) {
                    mikeseek=TRUE;
                    newobject=OB_GUIDANCE;
                }
            }
            
            // reduce by standard seek time
            time = BSub(time, timeCost, 0);
            
            oinfo = mx->ObjectById ( newobject );
            if ( oinfo == nullptr ) {
                return FoundNothing();
            }

            //
            mx->text->oinfo = oinfo ;
            
            switch ( newobject ) {

                case OB_WILDHORSES:
                    if ( IsAllowedHorse() ) {
                        flags.Set ( cf_riding );
                        //
                    }else{
                        // display think
                    }
                    break;

                case OB_SHELTER:
                    additionalTimeCost = timeCost;
                    IncreaseEnergy( (s32)sv_object_energy_shelter );
                    break;

#if defined(_DDR_)
                case OB_CLAWS:
                    time = sv_time_night;
                    break;
                case OB_FLAMES:
                    time = sv_time_dawn;
                    break;
                case OB_THORNS:
                    despondency=MIN_DESPONDENCY;
                    break;
                case OB_BLOOD:
                    despondency=MAX_DESPONDENCY;
                    break;
                case OB_LANGUOR:
                    energy=sv_object_energy_shadowsofdeath;
                    break;
                case OB_SPRINGS:
                    energy=sv_object_energy_watersoflife;
                    break;
#endif
                    
                case OB_GUIDANCE:
                    additionalTimeCost = timeCost;
                    break;

                case OB_SHADOWSOFDEATH:
                    energy = (u32)sv_object_energy_shadowsofdeath;
                    warriors.energy = (u32)sv_object_energy_shadowsofdeath;
                    riders.energy = (u32)sv_object_energy_shadowsofdeath;
                    break;

                case OB_WATERSOFLIFE:
                    energy = (u32)sv_object_energy_watersoflife;
                    warriors.energy = (u32)sv_object_energy_watersoflife;
                    riders.energy = (u32)sv_object_energy_watersoflife;
                    break;

                case OB_HANDOFDARK:
                    time = sv_time_night;
                    break;

                case OB_CUPOFDREAMS:
                    time = sv_time_dawn;
                    break;

                case OB_ICECROWN:
                    if ( !IsAllowedIcecrown() ) {
                        return FoundNothing();
                    }
                
                    additionalTimeCost = timeCost * 2;
                    Cmd_PickupObject();
                    break;

                case OB_MOONRING:
                    if ( !IsAllowedMoonring() ) {
                        return FoundNothing();
                    }

                    additionalTimeCost = timeCost * 2;
                    Cmd_PickupObject();
                    break;

                default:

                    if ( oinfo->CanFight() ) {
                        // auto fight?
#if defined(_DDR_)
                        if ( !sv_cheat_nasties_noblock ) {
                            Cmd_Fight();
                            seekMsg = IsDead();
                        } else {
                            removeObject = false;
                        }
#endif  
                    } else

                    // if we can pickup the new object
                    if ( oinfo->CanPickup() ) {

                        // we can only pickup a new object if can we drop our current object
                        if ( carrying && !carrying->CanDrop() ) {
                            return FoundNothing();
                        }

                        Cmd_PickupObject();
                        
                        // we might have dropped an object
                        // and the pickup will have removed the object
                        
                        removeObject = false;
                    }
                    break;

            }

#if defined(_LOM_)
            if ( oinfo && oinfo->MapRemove() && removeObject ) {
                mapsqr.RemoveObject();
            }
#endif
            time = BSub(time, additionalTimeCost, 0);
            
            if ( newobject == OB_GUIDANCE ) {
                mx->scenario->GiveGuidance(this, (mikeseek?1:0) );
            } else {
                if (seekMsg) {
                    mx->text->oinfo = oinfo ;
                    mx->SetLastActionMsg( mx->text->CookedSystemString( SS_SEEK, this) );
                }
            }
            
            if ( removeObject )
                mapsqr.RemoveObject();

            SetLastCommand ( CMD_SEEK, MAKE_ID(IDT_OBJECT,newobject) );

            return mx->ObjectById(newobject) ;
        }

        mxobject* mxcharacter::FoundNothing() const
        {
            mx->SetLastActionMsg(mx->text->CookedSystemString(SS_SEEK_NOTHING, this));
            return nullptr;
        }

        MXRESULT mxcharacter::Cmd_DropObject ( void )
        {
            SetLastCommand ( CMD_DROPOBJECT, mxentity::SafeIdt(carrying) );

            mx->scenario->DropObject ( Location(), carrying );
#if defined(_DDR_)
            carrying->carriedby=NULL;
#endif
            carrying=NULL;
            
            return MX_OK ;
        }

        mxobject* mxcharacter::Cmd_PickupObject ( void )
        {
        mxobject* oldobject=carrying;

            carrying = mx->scenario->PickupObject( Location() ) ;
            if(carrying!=nullptr) {
                MXTRACE("    %s picks up object %s", Longname().c_str(), carrying->name.c_str());
            }

#if defined(_DDR_)
            if ( carrying==nullptr) {
                carrying = oldobject;
                return NULL;
            }
            
            carrying->carriedby=this;
            
            if ( oldobject ) {
                MXTRACE("    %s drops object %s", Longname().c_str(), oldobject->name.c_str());
                mx->scenario->DropObject ( Location(), oldobject );
                oldobject->carriedby=nullptr;
            }
#endif
            
#if defined(_LOM_)
            if( oldobject ) {
                MXTRACE("    %s drops object %s", Longname().c_str(), oldobject->name.c_str());
                mx->scenario->DropObject ( Location(), oldobject );
            }
#endif
            

            SetLastCommand ( CMD_TAKEOBJECT, mxentity::SafeIdt(carrying) );

            return carrying ;
        }
            
        void mxcharacter::InitNightProcessing ( void )
        {
            if (IsDead()) {
                return;
            }
        
            IncreaseEnergy( Time()/2 );

            time = sv_time_dawn; 

            flags.Reset ( cf_resting|cf_inbattle|cf_wonbattle|cf_battleover );
            battleloc= mxgridref(-1,-1);
            battleslew = 0;
            riders.lost = 0;
            riders.slew = 0;
            warriors.lost = 0;
            warriors.slew = 0;

            // reset the waiting mode
            if ( WaitMode() == WM_OVERNIGHT )
                Cmd_Wait(WM_NONE);
        }

        void mxcharacter::Cmd_Dead ( void )
        {
            
            // TODO we need to check if there is an object
            // we don't want to lose in the location underneath us
            if ( carrying && carrying->IsUnique() )
                Cmd_DropObject();

            flags.Reset(cf_alive);

            mx->scenario->CheckMoonringWearerDead();
#if defined(_DDR_)
            mx->scenario->ResetMapArmies();
#endif
        }

        bool mxcharacter::HasBattleInfo() const
        {
            mxgridref loc1 = battleloc ;
            mxgridref loc2 = mxgridref(-1,-1);
            return (loc1 !=  loc2);
        }

        void mxcharacter::CanSeeLocation ( mxgridref loc )
        {
            mxloc& mapsqr = mx->gamemap->GetAt( loc );

            mx->scenario->ClearFromMemory(loc);

            if ( mapsqr.HasArmy() ) {
                
                c_regiment regiments;
                if ( mx->CollectRegiments ( loc, regiments ) )
                    memory.Memorise ( regiments.First(), RA_DOOMGUARD );


                if ( mapsqr.IsStronghold() ) {
                    c_stronghold strongholds;
                    if ( mx->CollectStrongholds ( loc, strongholds ) ) {
                        auto stronghold = strongholds.First();
                        if ( stronghold->OccupyingRace() == RA_DOOMGUARD )
                            memory.Memorise ( stronghold, RA_DOOMGUARD );
                        else 
                            memory.Memorise ( stronghold, RA_FREE );
                    }
                }
            }

        }


        archive& operator<<(archive& ar, mxcharacter* ptr )
        {
            return ar << (u32) mxentity::SafeId(ptr);
        }

        archive& operator>>( archive& ar, mxcharacter*& ptr )
        {
        int temp;
            ar >> temp ; ptr = mx->CharacterById(temp);
            return ar ;
        }

        mxrace_t mxcharacter::NormalisedLoyalty() const
        {
            return loyalty == RA_NONE ? Race() : loyalty ;
        }

        bool mxcharacter::IsFriend( const mxcharacter* c ) const
        {
            if ( c == nullptr ) return false ;
            return NormalisedLoyalty() == c->NormalisedLoyalty() ;
        }

        const std::string& mxcharacter::HeOrShe() const
        { 
            return mx->GenderById ( gender )->pronoun1; 
        }

        const std::string& mxcharacter::HisOrHer() const
        { 
            return mx->GenderById ( gender )->pronoun2; 
        }

        const std::string& mxcharacter::HimOrHer() const
        { 
            return mx->GenderById ( gender )->pronoun3; 
        }

        MXRESULT mxcharacter::Cmd_Wait ( mxwait_t mode )
        {
            SetLastCommand ( CMD_WAIT, IDT_NONE );

            wait = mode ; 
            return MX_OK;
        }

        MXRESULT mxcharacter::FillExportData ( info_t* data )
        {
            // check if we are being asked to export a character
            // army
            if ( (u32)ID_TYPE(data->id) == (u32)INFO_ARMY ) {
                defaultexport::army_t* out = (defaultexport::army_t*)data;
                out->parent = SafeIdt(this);
                out->race = Race();
                out->type = AT_CHARACTER ;

                mxunit* unit = out->unit==UT_WARRIORS ? (mxunit*)&warriors : (mxunit*)&riders ;
                out->total = unit->Total() ;
                out->energy = unit->Energy();
                out->lost = unit->Lost();
                out->killed = unit->Killed();
                return MX_OK ;
            }

            defaultexport::character_t* out = (defaultexport::character_t*)data;
            VALIDATE_INFO_BLOCK(out,INFO_CHARACTER,defaultexport::character_t);

            out->looking = looking;
            out->time = Time();
            out->longname = std::string(longname);
            out->shortname = std::string(shortname);


#if defined(_LOM_)
            out->warriors = MAKE_ARMYID(UT_WARRIORS,SafeId(this));
            out->riders = MAKE_ARMYID(UT_RIDERS,SafeId(this));
#endif
#if defined(_DDR_)
            out->warriors =  MAKE_ARMYID((128|UT_WARRIORS),SafeId(this)) ;
            out->riders = MAKE_ARMYID((128|UT_RIDERS),SafeId(this)) ;
#endif
            
            
            out->battle.location.x = battleloc.x;
            out->battle.location.y = battleloc.y;
            out->battle.killed = battleslew;

            out->reckless = reckless;
            out->energy = energy;
            out->strength = strength;
            out->cowardess = cowardess;
            out->courage = courage;
            out->fear = fear;
            
            out->loyalty = loyalty;
            out->foe = SafeIdt(foe);
            out->liege = SafeIdt(liege);

            out->traits = traits;
            out->recruit.group = RecruitingKey;
            out->recruit.by = RecruitedBy;

            out->race = race;
            out->object = SafeIdt(carrying);
            out->killedby = SafeIdt(killedbyobject);
            out->gender = gender;
            out->wait = wait;
            
            out->following = SafeIdt(following);
            out->followers = followers ;
            
            out->lastcommand = lastcommand ;
            out->lastcommandid = lastcommandid ;

            return mxitem::FillExportData ( data );
        }
}
// namespace tme
