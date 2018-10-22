//
//  LandscapePeople.cpp
//  midnight
//
//  Created by Chris Wild on 17/10/2018.
//  Copyright © 2018 Chilli Hugger Software. All rights reserved.
//
#include "cocos2d.h"
//#include "ui/CocosGUI.h"

#include "LandscapePeople.h"
#include "ILandscape.h"
#include "../system/resolutionmanager.h"
#include "../system/tmemanager.h"
#include "../ui/uihelper.h"

#include "../tme_interface.h"

using namespace tme;

#define TRANSITION_DURATION     0.5f
#define adjusty                 RES(8)

bool LandscapePeople::init()
{
    if ( !LandscapeNode::init() )
    {
        return false;
    }

    setLocalZOrder(ZORDER_DEFAULT);
    setPosition(0,adjusty);
    setCascadeOpacityEnabled(true);
    setOpacity(ALPHA(1.0));
    return true;
}

void LandscapePeople::Init(LandscapeOptions* options)
{
    this->options = options;
    
    characters=0;
    CLEARARRAY(columns);
    
#if defined(_DDR_)
    islookingdowntunnel=false;
    isintunnel=false;
    time = sv_time_dawn;
#endif
    //DisableMouse();
    
    loc = options->currentLocation;
    
}

void LandscapePeople::Initialise( const character& c)
{
    loc = c.location;
    
#if defined(_DDR_)
    isintunnel = Character_IsInTunnel(c);
    time  = c.time;
    Initialise(isintunnel);
#else
    Initialise(c.looking, false);
#endif
}

void LandscapePeople::Initialise( mxdir_t dir, bool isintunnel )
{
    mxgridref ref(loc);
    ref.AddDirection(dir);
    
    loc = ref;
    Initialise(isintunnel);
}

void LandscapePeople::Initialise( bool isintunnel )
{
    u32           ii;
    LPCSTR        person=NULL;
    c_mxid        objects;
    
    this->removeAllChildren();
    
//    auto background = LayerColor::create(Color4B(_clrRed));
//    background->setPosition(0,0);
//    background->setContentSize( Size(Director::getInstance()->getVisibleSize().width,RES(PEOPLE_WINDOW_HEIGHT)));
//    background->setAnchorPoint(uihelper::AnchorBottomLeft);
//    background->setOpacity(ALPHA(0.25f));
//    addChild(background);
    
    setOpacity(ALPHA(1.0f));
    setPosition(0,adjusty);
    setScale(1.0f);
    
    mxid            locid = MAKE_LOCID( loc.x, loc.y );
    
    TME_GetLocationInfo(loc);
    
#if defined(_DDR_)
    maplocation m;
    TME_GetLocation(m, locid);
    
    if ( m.flags.Is(lf_tunnel) && isintunnel  )
        islookingdowntunnel=true;
#endif
    
    // we have not printed anyone yet
    characters = 0;
    CLEARARRAY ( columns );
    
    
    // get the characters infront of us
#if defined(_LOM_)
    u32 recruited;
    TME_GetCharacters ( locid, objects, recruited );
#endif
    
#if defined(_DDR_)
    TME_GetCharactersAtLocation(locid,characters, TRUE, isintunnel);
#endif
    
    // if there are characters in front of us then
    // print them on screen
    for (ii = 0; ii < objects.Count(); ii++) {
        character c;
        TME_GetCharacter ( c, objects[ii] );
        
#if defined(_DDR_)
        if ( islookingdowntunnel && !Character_IsInTunnel(c) )
            continue;
        if ( Character_IsDead(c) ||  Character_IsHidden(c) )
            continue;
#endif
        person = GetCharacterImage(c);
        add(person,DEFAULT_PRINT_CHARACTER /*, NULL &hotspot*/);
    }
    
    
    s32 objectid = GET_ID(location_object) ;
#if defined(_DDR_)
    if ( isintunnel )
        objectid = GET_ID(location_object_tunnel);
#endif
    //
    // get location infront map
    //
    
    int foe_warriors = location_armies.foe_warriors ;
    int foe_riders = location_armies.foe_riders ;
    
    // if there are riders
    if ( foe_riders )    {
        // get RA_DOOMGUARD
        person = GetRaceImage(MAKE_ID(IDT_RACEINFO,RA_DOOMGUARD),TRUE);
        add( person, DEFAULT_PRINT_RIDERS/*, NULL*/);
    } else if ( foe_warriors ) {
        person = GetRaceImage(MAKE_ID(IDT_RACEINFO,RA_DOOMGUARD),FALSE);
        add( person,DEFAULT_PRINT_WARRIORS /*, NULL*/);
        return;
    } else     if ( objectid >= OB_WOLVES && objectid <= OB_WILDHORSES)    {
        //hotspot.type = HOTSPOT_OBJECT;
        //hotspot.thing = (thing_t)GET_ID(location_infront_object);
        
        person = GetObjectBig(MAKE_ID(IDT_OBJECT, objectid));
        if ( objectid == OB_DRAGONS ) {
            add(person,DEFAULT_PRINT_DRAGONS /*, NULL*/);
        }else{
            add(person,DEFAULT_PRINT_OTHER /*, NULL*/);
        }
    }
    
}

void LandscapePeople::add( LPCSTR person, int number /*, void* hotspot */ )
{
    int ii;
    int column;
    
#if defined(_LOM_)
    static int xtable[] = { 3, 5, 4, 1, 2, 6, 0, 7 };
#endif
#if defined(_DDR_)
    // 0 1 2 3 4 5 6 7
    static int xtable[] = { 3, 4, 2, 5, 6, 1, 7, 0 };
#endif
    
    //lookhotspot_t*    newhotspot;
    
    if ( person == NULL )
        return;
    
    int max_chars = MAX_DISPLAY_CHARACTERS ;
    
#if defined(_DDR_)
    if ( islookingdowntunnel )
        max_chars = MAX_DISPLAY_CHARACTERS_TUNNEL ;
#endif
    
    for (ii = 0; ii < number; ii++)    {
        
        // make sure our printing position is free
        for(;characters<max_chars;characters++)
            if ( !columns[xtable[characters]].used)
                break;
        
        if ( characters >= max_chars )
            return;
        
        
        column = xtable[characters] ;
        
        
        auto image = Sprite::create(person);
        auto size = image->getContentSize();
        
        int x1 = 0 + ( column * RES(CHARACTER_COLUMN_WIDTH) );
        int y1 = 0; //size.height; //-ASP(person->Height());
        
        image->setAnchorPoint(uihelper::AnchorBottomLeft);
        image->setPosition(x1, y1);
        addChild(image);
        
        //uihelper::AddBottomLeft(this, image, x1, y1);
        
        columns[column].used=TRUE;
        columns[column].pos = point(x1,y1);
        columns[column].scale = 1.0f;
        columns[column].image = person ;
        
        // mark this printing position as used
        
        // and see if we have taken the one to the right of us
        if ( column <max_chars ) {
            //if ( RES(person->Width()) > RES(MAX_ALLOWED_CHARACTER_WIDTH) )
            //    m_columns[column+1].used = TRUE;
        }
        
        characters++;
        
        // add this into a mouse hotspot list
        //if ( hotspot ) {
        //    newhotspot = &hotspots[hotspot_count++];
        //    memcpy ( newhotspot, hotspot, sizeof(lookhotspot_t) );
        //    newhotspot->r = rect ( point(x1,y1), size(person->Width(),person->Height()) );
        //    newhotspot->r.ClipRect ( &panel_rect );
        //}
        
    }
    
}

void LandscapePeople::clear()
{
    characters=0;
    stopAnim();
}

void LandscapePeople::stopAnim()
{
    setPosition(0,adjusty);
    setScale(1.0f);
    setOpacity(ALPHA(1.0f));
    stopAllActions();
}

// Animation & Movement

void LandscapePeople::startSlideFromRight()
{
    auto parentSize = getParent()->getContentSize();
    
    setScale(1.0f);
    setOpacity(ALPHA(1.0f));
    setPosition(parentSize.width,0+adjusty);
    amountmoved=0;
    amountmoving=parentSize.width*-1;
    startlocationx=getPosition().x;
}

void LandscapePeople::startSlideFromLeft()
{
    auto parentSize = getParent()->getContentSize();

    setScale(1.0f);
    setOpacity(ALPHA(1.0f));
    setPosition(-parentSize.width,0+adjusty);
    amountmoved=0;
    amountmoving=parentSize.width;
    startlocationx=getPosition().x;
}

void LandscapePeople::startSlideOffLeft()
{
    auto parentSize = getParent()->getContentSize();

    setOpacity(ALPHA(1.0f));
    amountmoved=0;
    amountmoving=parentSize.width*-1;
    startlocationx=getPosition().x;
}

void LandscapePeople::startSlideOffRight()
{
    auto parentSize = getParent()->getContentSize();

    setOpacity(ALPHA(1.0f));
    amountmoved=0;
    amountmoving=parentSize.width;
    startlocationx=getPosition().x;
}

void LandscapePeople::adjustMovement( f32 amount )
{
    amountmoved=amount;
    
    f32 totalmoved = amountmoving * amountmoved;
    setPositionX(startlocationx + totalmoved);
}


void LandscapePeople::startFadeOut()
{
    auto parentSize = getParent()->getContentSize();
    
    f32 startScale=1.0f;
    f32 height = RES(PEOPLE_WINDOW_HEIGHT)*startScale;
    f32 width = parentSize.width*startScale;
    //f32 adjust = 0; //RES(32) * startScale;
    f32 startY = 0;
    f32 targetY = -height*1.5f;
    f32 startX = (parentSize.width/2)-(width/2);
    setScale(startScale);
    setPosition( startX, startY );
    
    auto actionfloat = ActionFloat::create(TRANSITION_DURATION, 0.0f, 1.0f, [=](float value) {
        
        f32 alpha = 1.0f - (1.0f * value);
        f32 scale = startScale + (1.5f*value);
        
        //UIDEBUG("MovementForward  %f %f", alpha, value);
        
        setOpacity(ALPHA(alpha));
        setScale(scale);
        
        f32 y = targetY * value;
        
        f32 x = (parentSize.width/2)-((parentSize.width*scale)/2);
        setPosition(x,adjusty+y);
        
    });
    
    
    auto ease = new EaseSineInOut();
    ease->initWithAction(actionfloat);
    this->runAction(ease);
//    this->runAction(actionfloat);
    
}

void LandscapePeople::startFadeIn()
{
    auto parentSize = getParent()->getContentSize();

    f32 startScale=0.25f;
    f32 height = RES(PEOPLE_WINDOW_HEIGHT)*startScale;
    f32 width = parentSize.width*startScale;
    f32 adjust = RES(32) * startScale;
    f32 startY = options->generator->PanoramaHeight + options->generator->HorizonCentreY + (height*2) - adjust;
    f32 targetY = 0;
    f32 startX = (parentSize.width/2)-(width/2);
    setScale(startScale);
    setPosition( startX, startY );
    
    auto actionfloat = ActionFloat::create(TRANSITION_DURATION, 0.0f, 1.0f, [=](float value) {
        
        f32 alpha = 1.0f * value;
        f32 scale = startScale + ((1.0f-startScale)*value);
        
        //UIDEBUG("MovementForward  %f %f", alpha, value);
        
        
        setOpacity(ALPHA(alpha));
        setScale(scale);
        
        f32 y = targetY + (startY - (startY*value) );
        
        f32 x = (parentSize.width/2)-((parentSize.width*scale)/2);
        setPosition(x,adjusty+y);

    });
    
    
    auto ease = new EaseSineInOut();
    ease->initWithAction(actionfloat);
    this->runAction(ease);
    
//    this->runAction(actionfloat);
}