//
//  storymanager.cpp
//  midnight
//
//  Created by Chris Wild on 15/12/2017.
//
//

#include "../ui/uihelper.h"
#include "storymanager.h"
#include "settingsmanager.h"
#include "configmanager.h"
#include "../tme/tme_interface.h"
#include "moonring.h"


using namespace chilli::os;
using namespace chilli::lib;

storymanager::storymanager() :
    loadsave(nullptr),
    currentStory(STORY_NONE),
    last_save(SAVE_NONE),
    undo_last_available(false)
{
    CLEARARRAY(used);
    last_night.Clear();
    last_morning.Clear();
}


storymanager::~storymanager()
{
}

storyinfo_t* storymanager::getStoriesInfo()
{
    
    int used = 0;
    
    for ( u32 ii=0; ii<MAX_STORIES; ii++ ) {
        if ( filemanager::Exists( getPath(ii+1) ) ) {
            getDescription(ii+1, stories.chapter[used].description);
            stories.chapter[used].id = ii + 1;
            stories.chapter[used].slot = ii;
            used++;
        }
    }
    
    stories.count = used;
    
    return &stories;
}


void storymanager::checkStories( void )
{
    for ( u32 ii=0; ii<MAX_STORIES; ii++ ) {
        used[ii] = filemanager::Exists( getPath(ii+1) ) ;
    }
}

int storymanager::stories_used()
{
    u32 count=0;
    checkStories();
    for ( u32 ii=0; ii<MAX_STORIES; ii++ ) {
        if ( used[ii])
            count++;
    }
    return count;
    
}

storyid_t storymanager::next_free_story()
{
    checkStories();
    for ( u32 ii=0; ii<MAX_STORIES; ii++ ) {
        if ( !used[ii])
            return ii+1;
    }
    return STORY_NONE;
}

storyid_t storymanager::first_used_story()
{
    checkStories();
    for ( u32 ii=0; ii<MAX_STORIES; ii++ ) {
        if ( used[ii])
            return ii+1;
    }
    return STORY_NONE;
}

storyid_t storymanager::alloc( void )
{
    storyid_t id = next_free_story();
    used[id-1]=true;
    return id;
}


void storymanager::SetLoadSave( tme::PFNSERIALIZE function )
{
    loadsave = function;
}

LPCSTR storymanager::DiscoveryMapFilename()
{
    static char filename[MAX_PATH]={0};
    
    sprintf(filename, "%s/story/discovery.map", mr->getWritablePath());
    
    return filename;
    
}

bool storymanager::create ( storyid_t id )
{
    TME_DeInit();
    TME_Init();
    currentStory=id;
    last_save=SAVE_NONE;
    last_night.Clear();
    last_morning.Clear();
    
    cocos2d::FileUtils::getInstance()->createDirectory(getFolder(id));
    
    TME_LoadDiscoveryMap( (char*)DiscoveryMapFilename() );
    
    return save(id,savemode_dawn);
}

LPCSTR storymanager::getFolder ( storyid_t id )
{
    static char folder[MAX_PATH]={0};
    
    sprintf(folder, "%s/story/%d", mr->getWritablePath(), (int)id);

    return folder;
    
}

LPCSTR storymanager::getPath( storyid_t id )
{
    static char filename[MAX_PATH]={0};
    
    sprintf(filename, "%s/current", getFolder(id) );
    
    return filename;
    
}

LPCSTR storymanager::getPath(storyid_t id, u32 save)
{
    static char filename[MAX_PATH]={0};
    
    sprintf(filename, "%s/%03d", getFolder(id), (int)save );
    
    return filename;
    
}

bool storymanager::save( savemode_t mode )
{
    if ( currentStory == STORY_NONE )
        return false;
    return save(currentStory, mode );
}


bool storymanager::save ( storyid_t id, savemode_t mode )
{
    undo_last_available=true;
    
    if ( mode == savemode_night ) {
        last_night.Add( last_save );
        undo_last_available=false;
        return true;
    }
    else if ( mode == savemode_dawn ) {
        last_morning.Add( last_save+1 );
        undo_last_available=false;
    }
    
    char* file = (char*)getPath(id) ;
    
    // copy save file to last
    if ( last_save  ) {
        char* cpy = (char*)getPath(id, last_save);
        if ( !filemanager::Copy( file, cpy ) ) {
            UIDEBUG("Unable to create undo file");
        }
    }
    
    last_save++;
    
    if ( filemanager::Exists(file) )
        filemanager::Remove(file);
    
    if ( !TME_Save( file,loadsave ) ) {
        
        last_save--;
        
        return false ;
    }
    
    TME_SaveDiscoveryMap( (char*)DiscoveryMapFilename() );
    
    cleanup();
    
    return true;
}

bool storymanager::load ( storyid_t id )
{
    currentStory = id;
    undo_last_available=false;
    
    if (!TME_Load( (char*)getPath(id), loadsave ) ){
        return false ;
    }
    
    TME_LoadDiscoveryMap( (char*)DiscoveryMapFilename() );
    
    return true ;
}

bool storymanager::canUndo ( savemode_t mode )
{
    saveid_t save=last_save-1;
    if ( mode == savemode_dawn ) {
        save = lastMorning() ;
        if ( save == 0 ) save = 1;
    } else if ( mode == savemode_night )
        save=lastNight();
    else if ( mode == savemode_last && (mr->config->undo_history == 1 && !mr->config->always_undo))
        return undo_last_available ;
    
    return filemanager::Exists(getPath(currentStory,save));
    
}


saveid_t storymanager::lastMorning() const
{
    saveid_t dawn = STORY_NONE;
    
    if ( last_morning.Count()>0 ) {
        
        dawn = last_morning.Get(last_morning.Count()-1);
        
        // are we at dawn?
        // then we want the previous dawn
        if ( dawn == last_save && last_morning.Count() > 1 ) {
            
            dawn = last_morning.Get(last_morning.Count()-2);
            
        }
        
        return dawn ;
    }

    return SAVE_NONE;
}

saveid_t storymanager::lastNight() const
{
    if ( last_night.Count()>0 )
        return last_night.Get(last_night.Count()-1);
    return SAVE_NONE;
}

bool storymanager::getDescription( storyid_t id, c_str& description )
{
    return TME_SaveDescription((char*)getPath(id), description);
}

bool storymanager::undo ( savemode_t mode )
{
    saveid_t save=last_save-1;
    if ( mode == savemode_dawn )
        save=lastMorning();
    else if ( mode == savemode_night )
        save=lastNight();
    
    saveid_t temp_save = last_save ;
    
    char* file = (char*)getPath(currentStory,save);
    
    if ( TME_Load( file , loadsave ) ) {
        
        // copy save entry over the current
        char* cpy = (char*)getPath(currentStory) ;
        filemanager::Copy( file, cpy );
        
        // delete all entries above the save entry
        for ( u32 ii=save; ii<temp_save; ii++ )  {
            LPCSTR file = getPath(currentStory,ii);
            if ( filemanager::Exists(file) )
                filemanager::Remove(file);
        }
        
        undo_last_available=false;
        
        return true;
    }
    
    return false;
}

bool storymanager::cleanup ( void )
{
    if ( mr->config->keep_full_save_history )
        return true;
    
    s32 final_save = MAX(0,last_save-mr->config->undo_history);
    for ( s32 ii=0; ii<final_save; ii++ ) {
        if ( !last_morning.IsInList(ii) ) {
            LPCSTR file = getPath(currentStory,ii) ;
            if ( filemanager::Exists(file) ) {
                filemanager::Remove(file);
            }
        }
    }
    
    return true;
}

bool storymanager::destroy( storyid_t id )
{
    currentStory = SAVE_NONE ;
    return filemanager::DestroyDirectory(getFolder(id)) ;
}

bool storymanager::Serialize( u32 version, archive& ar )
{
    u32 count = 0;
    u32 value = 0;
    
    if ( ar.IsStoring() ) {
        
        ar << last_save ;
        
        ar << last_night.Count();
        for ( int i =0; i<last_night.Count(); i++ )
            ar << last_night.Get(i) ;
        
        ar << last_morning.Count();
        for ( int i =0; i<last_morning.Count(); i++ )
            ar << last_morning.Get(i) ;
        
    }else{
        
        ar >> last_save ;
        
        last_night.Clear();
        last_morning.Clear();
        
        if ( version < 13 ) {
            
            ar >> value ;
            last_night.Add( value );
            
            ar >> value ;
            last_morning.Add( value );
            
        }else{
            
            ar >> count ;
            for ( int i =0; i<count; i++ ) {
                ar >> value ;
                last_night.Add(value);
            }
            
            ar >> count ;
            for ( int i =0; i<count; i++ ) {
                ar >> value ;
                last_morning.Add(value);
            }
            
        }
        
    }
    
    return true;
}


