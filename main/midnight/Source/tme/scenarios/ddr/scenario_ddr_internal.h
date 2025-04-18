#ifndef _DDRSCENARIO_INTERNAL_H_INCLUDED_
#define _DDRSCENARIO_INTERNAL_H_INCLUDED_

#include "../../baseinc/tme_internal.h"
#include "../default/default_scenario_internal.h"

#if defined(_DDR_)
namespace tme {

    FORWARD_REFERENCE(ddr_character);
    FORWARD_REFERENCE(ddr_object);
    
    namespace utils {
        FORWARD_REFERENCE(UpdateDDRObjects);
    }

    class ddr_x  : public mxscenario
    {
    public:
        ddr_x();
        virtual ~ddr_x();

        virtual scenarioinfo_t* GetInfoBlock() const override;
        virtual MXRESULT Register ( mxengine* midnightx ) override;
        virtual MXRESULT UnRegister ( mxengine* midnightx ) override;

        virtual void NightStop(void) override;

        virtual void initialise( u32 version ) override;
        virtual void initialiseAfterCreate( u32 version ) override;
        virtual void updateAfterLoad ( u32 version ) override;

        virtual void GiveGuidance(mxcharacter *character, s32 hint) override;
        
        virtual mxcharacter* WhoHasObject( mxobject* object ) const override;
        virtual bool DropObject ( mxgridref loc, mxobject* obj ) override;
        virtual mxobject* PickupObject ( mxgridref loc ) override;

        virtual void PlaceObjectsOnMap ( void );
        virtual mxobject* FindObjectAtLocation ( mxgridref loc ) override;
        virtual mxstronghold* StrongholdFromLocation ( mxgridref loc );
        virtual mxterrain_t toScenarioTerrain( mxterrain_t t) const override;
        virtual mxcharacter* IsEnemyAtLocation( mxgridref loc, const ddr_character* character) const;
        virtual bool IsLoyalToTheMoonprince(mxrace_t race) const ;
        virtual void CleanupBattleContinuesForCharacter(ddr_character* character);
        virtual void CleanupBattleContinuesForLocation(mxgridref loc);
    
    public:
        ddr_character*  morkin;
        ddr_character*  tarithel;
        ddr_character*  rorthron;
        ddr_character*  shareth;
        mxplace*        cityofglireon;
        ddr_object*     crownofvarenand;
        ddr_object*     crownofcarudrium;
        ddr_object*     spellofthigrorn;
        ddr_object*     runesoffinorn;
        ddr_object*     crownofimiriel;
    };

    class ddr_entityfactory : public mxentityfactory
    {
    public:
        virtual mxentity* Create(id_type_t type) override;
    };
    
    class ddr_object : public mxobject
    {
    public:
        ddr_object();
        virtual ~ddr_object();
        
        virtual MXRESULT FillExportData ( info_t* data );
        virtual void Serialize ( archive& ar );

        FLAG_PROPERTY( CanHelpRecruitment,  of_recruitment )
        FLAG_PROPERTY( IsRandomStart,       of_randomstart )
    
        bool IsSpecial() const;
        
        virtual u32 FightHP() const;
        virtual u32 FightSuccess() const;
        virtual u32 KillRate( u32 hp ) const;
        
    public:
        mxobjtype_t   type;
        mxobjpower_t  power;
        
        friend tme::utils::UpdateDDRObjects;
    };
    
    class ddr_stronghold : public mxstronghold
    {
    public:
        ddr_stronghold();
        virtual ~ddr_stronghold();
        virtual MXRESULT FillExportData ( info_t* data ) override;
        virtual void Serialize ( archive& ar ) override;
        virtual void MakeChangeSides( mxrace_t newrace, mxcharacter* newoccupier ) override;
        virtual void OnRespawn();
        virtual mxrace_t Loyalty() const override ;
        virtual bool CanCharacterRecruitOrPost(const mxcharacter* character) const override;

        PROPERTY ( u32, Energy, energy )
    protected:
        u32 energy;
    };
    
    class ddr_character  : public mxcharacter
    {
    public:
        ddr_character();
        virtual ~ddr_character();
        
        virtual bool IsAllowedWarriors() const { return flags.Is(cf_allowedwarriors); }
        virtual bool IsAllowedRiders() const { return flags.Is(cf_allowedriders); }
                
        FLAG_PROPERTY ( IsPreparingForBattle, cf_preparesbattle )
        FLAG_PROPERTY ( IsApproaching, cf_approaching )
        
        GET_PROPERTY ( bool, IsCarryingDesiredObject, IsCarryingObject() && carrying == desired_object )

        virtual bool CheckRecruitChar ( mxcharacter* character )  const ;
        virtual bool Recruited ( mxcharacter* recruiter );
        virtual void Serialize ( archive& ar );
        virtual MXRESULT FillExportData ( info_t* data );

        virtual void DecreaseEnergy ( s32 amount );
        virtual void IncreaseEnergy ( s32 amount );
        
        virtual MXRESULT Cmd_WalkForward ( bool perform_seek, bool perform_approach );
        virtual mxobject* Cmd_Fight( void );
        virtual mxcharacter* Cmd_Approach ( mxcharacter* character );
                
        virtual MXRESULT Cmd_Use ( void ) ;
        virtual MXRESULT Cmd_Take ( void ) ;
        virtual MXRESULT Cmd_Give ( mxcharacter* character ) ;
        virtual mxcharacter* GetNextFoe() const;
        virtual mxcharacter* GetNextLiege() const;
        
        virtual void InitNightProcessing ( void );
        virtual void Turn ( void );
        
        virtual bool ShouldLoseHorse( s32 hint ) const;
        virtual void LostFight( s32 hint );
        
        virtual bool HasBattleObject() const;
        virtual bool IsProtected() const;
        virtual bool ShouldDieInFight() const;
        virtual bool KilledByBattleObject() const;
        
        virtual void Displace();
        virtual std::unique_ptr<mxlocinfo> GetLocInfo();
        virtual MXRESULT EnterBattle ();
        
        virtual void CheckKilledFoe ( void );
        virtual void AICheckRecruitSoldiers ( void );
        
        void Target ( const mxitem* newtarget );
        void whatIsCharacterDoing ( void );
        bool retarget ();
        void moveCharacterSomewhere ( void );
        
        virtual bool ShouldWeStopTurnForEnemy() const;
        virtual bool ShouldWeStayAndFight(const mxarmytotal* friends, const mxarmytotal* foe) const;
        virtual bool LookForInterestingLocationNearby();

        void IncreaseDespondency( s32 amount );
        void DecreaseDespondency( s32 amount );
        
        mxunit_t getArmyType() const ;
        mxunit* getUnit();
        s32 getArmySize() const;
        void setArmySize(s32 value);
        void setArmyLost(s32 value);
        void setArmyKilled(s32 value);
        
        virtual mxorders_t pickNewOrders() const;
        mxcharacter* AI_Approach ( mxcharacter* character );

        virtual bool IsLoyalToTheFoe(mxrace_t race) const;
        virtual bool InterestedInMoonprince() const ;
        virtual bool InterestedInFoe() const ;
        virtual bool InterestedInOthers() const ;

    protected:
        bool RecruitMorkin ( mxcharacter* recruiter );

        virtual void UseCrownOfPersuassion();
        virtual void UseSpellOfSwiftness();
        virtual void UseRunesOfProtection();
        virtual void UseAllOtherObjects();
        virtual bool DesiredObjectAtLocation();

    public:

        mxgridref       lastlocation;
        
        // not saved
        mxid            targetId;
        mxgridref       targetLocation;
        //
        
        mxobject*       desired_object;
        mxstronghold*   home_stronghold;
        ddr_character*  fighting_against;
        u32             battlelost;
        mxkilledby_t    killedby;
    };
    
    #define DDR_SCENARIO(x) static_cast<ddr_x*>(mx->scenario)->x

}
#endif // _DDR_

#endif //_CDDRSCENARIO_H_INCLUDED_
