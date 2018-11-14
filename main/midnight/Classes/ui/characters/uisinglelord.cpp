//
//  uisinglelord.cpp
//  midnight
//
//  Created by Chris Wild on 29/10/2018.
//
//
#include "cocos2d.h"
#include "ui/CocosGUI.h"

#include "uisinglelord.h"
#include "../uihelper.h"
#include "../../tme_interface.h"
#include "../../system/tmemanager.h"
#include "../../system/resolutionmanager.h"

USING_NS_CC;
using namespace cocos2d::ui;



uisinglelord::uisinglelord() :
    buttonNode(nullptr),
    userdata(nullptr)
{
}

bool uisinglelord::init()
{
    if ( uilordselect::init() ) {
        setContentSize(Size(SELECT_ELEMENT_WIDTH, SELECT_ELEMENT_HEIGHT));
        auto bg = cocos2d::LayerColor::create(Color4B(0,0,0, 25));
        bg->setContentSize(this->getContentSize());
        this->addChild(bg);
        this->setTouchEnabled(true);
        return true;
    }
    return false;
}

uisinglelord* uisinglelord::createWithLord ( mxid characterid )
{
    auto select = uisinglelord::create();
    select->setLord( characterid );
    return select;
}

void uisinglelord::updateStatus(character& c)
{
    status.Clear();
    
#if defined(_DDR_)
    if ( Character_IsInTunnel(c))
        status.Set(LORD_STATUS::status_tunnel);
#endif
    if ( Character_IsDawn(c) )
        status.Set(LORD_STATUS::status_dawn);
    
    if ( Character_IsInBattle(c) )
        status.Set(LORD_STATUS::status_inbattle);
    
#if defined(_DDR_)
    if ( Character_IsPreparingForBattle(c) )
        status.Set(LORD_STATUS::status_inbattle);
#endif
    
    if ( Character_IsNight(c) )
        status.Set(LORD_STATUS::status_night);
    
    if ( Character_IsDead(c) )
        status.Set(LORD_STATUS::status_dead);
    
}

Sprite* uisinglelord::getStatusImage()
{
    // TODO: status_tunnel
    
    if ( status.Is(LORD_STATUS::status_inbattle))
        return Sprite::createWithSpriteFrameName("lord_battle");

    if ( status.Is(LORD_STATUS::status_night))
        return Sprite::createWithSpriteFrameName("lord_night");

    if ( status.Is(LORD_STATUS::status_dawn))
        return Sprite::createWithSpriteFrameName("lord_dawn");
    
    return nullptr;
}

Sprite* uisinglelord::getFaceImage()
{
    if ( status.Is(LORD_STATUS::status_dead))
        return Sprite::createWithSpriteFrameName("f_dead!");
    
    // TODO:
    // 1. start with get shield
    // 2. if there is no shield the get a face
    // 3. then get a race
    
#ifdef _DDR_
    mximage* face = NULL ;
    if ( data )
        face = data->face ;
    if ( face == NULL ) {
        race_data_t* r_data = (race_data_t*) TME_GetEntityUserData( c.race );
        if ( r_data )
            face = r_data->face;
    }
#else
    return Sprite::create( (LPCSTR)userdata->face );
#endif
    return nullptr;
}


bool uisinglelord::setLord( mxid characterid )
{
    userdata = (char_data_t*)TME_GetEntityUserData( characterid );
    
    character c;
    TME_GetCharacter(c, characterid );
    
    updateStatus(c);

    auto face = getFaceImage();
    if ( face == nullptr )
        face = Sprite::createWithSpriteFrameName("f_dead!");
    
    uihelper::AddCenter(this, face);
    
    // set name label
    auto title = Label::createWithTTF( uihelper::font_config_small, c.longname );
    title->getFontAtlas()->setAntiAliasTexParameters();
    title->setTextColor(Color4B(_clrWhite));
    title->enableOutline(Color4B(_clrBlack),RES(1));
    title->setAnchorPoint(uihelper::AnchorCenter);
    title->setWidth(getContentSize().width);
    title->setLineHeight(RES(10));
    title->setHeight(RES(32));
    title->setHorizontalAlignment(TextHAlignment::CENTER);
    title->setVerticalAlignment(TextVAlignment::CENTER);
    uihelper::AddBottomCenter(face, title, RES(0), RES(-16));
    

    // Add status image
    auto image = getStatusImage();
    if ( image!= nullptr ) {
        image->setScale(0.5f);
        face->addChild(image);
        uihelper::PositionCenterAnchor(image, uihelper::AnchorTopLeft);
    }
    
    buttonNode = face;
    return true;
}


void uisinglelord::onPressStateChangedToNormal()
{
    if ( buttonNode ) {
    buttonNode->setScale(1.0f);
    buttonNode->setOpacity(ALPHA(1.0f));
    }
}

void uisinglelord::onPressStateChangedToPressed()
{
    if ( buttonNode ) {
        buttonNode->setScale(0.75f);
        buttonNode->setOpacity(ALPHA(1.0f));
    }
}

void uisinglelord::onPressStateChangedToDisabled()
{
    if ( buttonNode ) {
        buttonNode->setScale(1.0f);
        buttonNode->setOpacity(ALPHA(0.25f));
    }
}