/*****************************************************************************
 * playlist.m: MacOS X interface module
 *****************************************************************************
* Copyright (C) 2002-2006 the VideoLAN team
 * $Id: 93929e4ed85fbbdcc30e1ec55dd238db446e9fdc $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videola/n dot org>
 *          Benjamin Pracht <bigben at videolab dot org>
 *          Felix K�hne <fkuehne at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/* TODO
 * add 'icons' for different types of nodes? (http://www.cocoadev.com/index.pl?IconAndTextInTableCell)
 * reimplement enable/disable item
 * create a new 'tool' button (see the gear button in the Finder window) for 'actions'
   (adding service discovery, other views, new node/playlist, save node/playlist) stuff like that
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#include <math.h>
#include <sys/mount.h>
#include <vlc_keys.h>

#import "intf.h"
#import "wizard.h"
#import "bookmarks.h"
#import "playlistinfo.h"
#import "playlist.h"
#import "controls.h"
#import "vlc_osd.h"
#import "misc.h"

/*****************************************************************************
 * VLCPlaylistView implementation 
 *****************************************************************************/
@implementation VLCPlaylistView

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    return( [[self delegate] menuForEvent: o_event] );
}

- (void)keyDown:(NSEvent *)o_event
{
    unichar key = 0;

    if( [[o_event characters] length] )
    {
        key = [[o_event characters] characterAtIndex: 0];
    }

    switch( key )
    {
        case NSDeleteCharacter:
        case NSDeleteFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSBackspaceCharacter:
            [[self delegate] deleteItem:self];
            break;

        case NSEnterCharacter:
        case NSCarriageReturnCharacter:
            [(VLCPlaylist *)[[VLCMain sharedInstance] getPlaylist]
                                                            playItem:self];
            break;

        default:
            [super keyDown: o_event];
            break;
    }
}

@end


/*****************************************************************************
 * VLCPlaylistCommon implementation
 *
 * This class the superclass of the VLCPlaylist and VLCPlaylistWizard.
 * It contains the common methods and elements of these 2 entities.
 *****************************************************************************/
@implementation VLCPlaylistCommon

- (id)init
{
    self = [super init];
    if ( self != nil )
    {
        o_outline_dict = [[NSMutableDictionary alloc] init];
    }
    return self;
}
- (void)awakeFromNib
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    i_current_view = VIEW_CATEGORY;
    playlist_ViewUpdate( p_playlist, i_current_view );

    [o_outline_view setTarget: self];
    [o_outline_view setDelegate: self];
    [o_outline_view setDataSource: self];

    vlc_object_release( p_playlist );
    [self initStrings];
}

- (void)initStrings
{
    [[o_tc_name headerCell] setStringValue:_NS("Name")];
    [[o_tc_author headerCell] setStringValue:_NS("Author")];
    [[o_tc_duration headerCell] setStringValue:_NS("Duration")];
}

- (NSOutlineView *)outlineView
{
    return o_outline_view;
}

- (playlist_item_t *)selectedPlaylistItem
{
    return [[o_outline_view itemAtRow: [o_outline_view selectedRow]]
                                                                pointerValue];
}

@end

@implementation VLCPlaylistCommon (NSOutlineViewDataSource)

/* return the number of children for Obj-C pointer item */ /* DONE */
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    int i_return = 0;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
        return 0;
    if( outlineView != o_outline_view )
    {
        vlc_object_release( p_playlist );
        return 0;
    }

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, i_current_view );
        if( p_view && p_view->p_root )
        {
            i_return = p_view->p_root->i_children;

            if( i_current_view == VIEW_CATEGORY )
            {
                i_return--; /* remove the GENERAL item from the list */
                i_return += p_playlist->p_general->i_children; /* add the items of the general node */
            }
        }
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item )
            i_return = p_item->i_children;
    }
    vlc_object_release( p_playlist );
    
    if( i_return <= 0 )
        i_return = 0;
    
    return i_return;
}

/* return the child at index for the Obj-C pointer item */ /* DONE */
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    playlist_item_t *p_return = NULL;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    NSValue *o_value;

    if( p_playlist == NULL )
        return nil;

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, i_current_view );
        if( p_view && p_view->p_root ) p_return = p_view->p_root->pp_children[index];

        if( i_current_view == VIEW_CATEGORY )
        {
            if( p_playlist->p_general->i_children && index >= 0 && index < p_playlist->p_general->i_children )
            {
                p_return = p_playlist->p_general->pp_children[index];
            }
            else if( p_view && p_view->p_root && index >= 0 && index - p_playlist->p_general->i_children < p_view->p_root->i_children )
            {
                p_return = p_view->p_root->pp_children[index - p_playlist->p_general->i_children + 1];
            }
        }
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item && index < p_item->i_children && index >= 0 )
            p_return = p_item->pp_children[index];
    }
    

    vlc_object_release( p_playlist );

    o_value = [o_outline_dict objectForKey:[NSString stringWithFormat: @"%p", p_return]];
    if( o_value == nil )
    {
        o_value = [[NSValue valueWithPointer: p_return] retain];
    }
    return o_value;
}

/* is the item expandable */
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    int i_return = 0;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
        return NO;

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, i_current_view );
        if( p_view && p_view->p_root ) i_return = p_view->p_root->i_children;

        if( i_current_view == VIEW_CATEGORY )
        {
            i_return--;
            i_return += p_playlist->p_general->i_children;
        }
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item )
            i_return = p_item->i_children;
    }
    vlc_object_release( p_playlist );

    if( i_return <= 0 )
        return NO;
    else
        return YES;
}

/* retrieve the string values for the cells */
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)o_tc byItem:(id)item
{
    id o_value = nil;
    intf_thread_t *p_intf = VLCIntf;
    playlist_t *p_playlist;
    playlist_item_t *p_item;
    
    if( item == nil || ![item isKindOfClass: [NSValue class]] ) return( @"error" );
    
    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return( @"error" );
    }

    p_item = (playlist_item_t *)[item pointerValue];

    if( p_item == NULL )
    {
        vlc_object_release( p_playlist );
        return( @"error");
    }
//NSLog( @"values for %p", p_item ); 

    if( [[o_tc identifier] isEqualToString:@"1"] )
    {
        /* sanity check to prevent the NSString class from crashing */
        if( p_item->input.psz_name != NULL )
        {
            o_value = [NSString stringWithUTF8String:
                p_item->input.psz_name];
            if( o_value == NULL )
                o_value = [NSString stringWithCString:
                    p_item->input.psz_name];
        }
    }
    else if( [[o_tc identifier] isEqualToString:@"2"] )
    {
        char *psz_temp;
        psz_temp = vlc_input_item_GetInfo( &p_item->input ,_("Meta-information"),_("Artist") );

        if( psz_temp == NULL )
            o_value = @"";
        else
        {
            o_value = [NSString stringWithUTF8String: psz_temp];
            if( o_value == NULL )
            {
                o_value = [NSString stringWithCString: psz_temp];
            }
            free( psz_temp );
        }
    }
    else if( [[o_tc identifier] isEqualToString:@"3"] )
    {
        char psz_duration[MSTRTIME_MAX_SIZE];
        mtime_t dur = p_item->input.i_duration;
        if( dur != -1 )
        {
            secstotimestr( psz_duration, dur/1000000 );
            o_value = [NSString stringWithUTF8String: psz_duration];
        }
        else
        {
            o_value = @"-:--:--";
        }
    }
    vlc_object_release( p_playlist );

    return( o_value );
}

@end

/*****************************************************************************
 * VLCPlaylistWizard implementation
 *****************************************************************************/
@implementation VLCPlaylistWizard

- (IBAction)reloadOutlineView
{
    /* Only reload the outlineview if the wizard window is open since this can
       be quite long on big playlists */
    if( [[o_outline_view window] isVisible] )
    {
        [o_outline_view reloadData];
    }
}

@end

/*****************************************************************************
* extension to NSOutlineView's interface to fix compilation warnings
* and let us access these 2 functions properly
* this uses a private Apple-API, but works finely on all current OSX releases
* keep checking for compatiblity with future releases though
*****************************************************************************/

@interface NSOutlineView (UndocumentedSortImages)
+ (NSImage *)_defaultTableHeaderSortImage;
+ (NSImage *)_defaultTableHeaderReverseSortImage;
@end

/*****************************************************************************
 * VLCPlaylist implementation
 *****************************************************************************/
@implementation VLCPlaylist

- (id)init
{
    self = [super init];
    if ( self != nil )
    {
        o_nodes_array = [[NSMutableArray alloc] init];
        o_items_array = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)awakeFromNib
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    vlc_list_t *p_list = vlc_list_find( p_playlist, VLC_OBJECT_MODULE,
                                        FIND_ANYWHERE );

    int i_index;

    [super awakeFromNib];

    [o_outline_view setDoubleAction: @selector(playItem:)];

    [o_outline_view registerForDraggedTypes:
        [NSArray arrayWithObjects: NSFilenamesPboardType,
        @"VLCPlaylistItemPboardType", nil]];
    [o_outline_view setIntercellSpacing: NSMakeSize (0.0, 1.0)];

    /* this uses private Apple API which works fine on all releases incl. 10.4, 
        * but keep checking in the future!
        * These methods were added artificially to NSOutlineView's public interface above */
    o_ascendingSortingImage = [[NSOutlineView class] _defaultTableHeaderSortImage];
    o_descendingSortingImage = [[NSOutlineView class] _defaultTableHeaderReverseSortImage];

    o_tc_sortColumn = nil;

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        vlc_bool_t  b_enabled;
        char        *objectname;
        NSMenuItem  *o_lmi;
        module_t    *p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability, "services_discovery" ) )
        {
            /* Check for submodules */
            int i = -1;
            while( p_parser->pp_shortcuts[++i] != NULL ); i--;

            /* Check whether to enable these menuitems */
            objectname = i>=0 ? p_parser->pp_shortcuts[i] : p_parser->psz_object_name;
            b_enabled = playlist_IsServicesDiscoveryLoaded( p_playlist, objectname );
            
            /* Create the menu entries used in the playlist menu */
            o_lmi = [[o_mi_services submenu] addItemWithTitle:
                     [NSString stringWithUTF8String:
                     p_parser->psz_longname ? p_parser->psz_longname :
                     ( p_parser->psz_shortname ? p_parser->psz_shortname:
                     objectname)]
                                             action: @selector(servicesChange:)
                                             keyEquivalent: @""];
            [o_lmi setTarget: self];
            [o_lmi setRepresentedObject: [NSString stringWithCString: objectname]];
            if( b_enabled ) [o_lmi setState: NSOnState];
                
            /* Create the menu entries for the main menu */
            o_lmi = [[o_mm_mi_services submenu] addItemWithTitle:
                     [NSString stringWithUTF8String:
                     p_parser->psz_longname ? p_parser->psz_longname :
                     ( p_parser->psz_shortname ? p_parser->psz_shortname:
                     objectname)]
                                             action: @selector(servicesChange:)
                                             keyEquivalent: @""];
            [o_lmi setTarget: self];
            [o_lmi setRepresentedObject: [NSString stringWithCString:objectname]];
            if( b_enabled ) [o_lmi setState: NSOnState];
        }
    }
    vlc_list_release( p_list );
    vlc_object_release( p_playlist );

    //[self playlistUpdated];
}

- (void)searchfieldChanged:(NSNotification *)o_notification
{
    [o_search_field setStringValue:[[o_notification object] stringValue]];
}

- (void)initStrings
{
    [super initStrings];

    [o_mi_save_playlist setTitle: _NS("Save Playlist...")];
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_delete setTitle: _NS("Delete")];
    [o_mi_recursive_expand setTitle: _NS("Expand Node")];
    [o_mi_selectall setTitle: _NS("Select All")];
    [o_mi_info setTitle: _NS("Information")];
    [o_mi_preparse setTitle: _NS("Get Stream Information")];
    [o_mi_sort_name setTitle: _NS("Sort Node by Name")];
    [o_mi_sort_author setTitle: _NS("Sort Node by Author")];
    [o_mi_services setTitle: _NS("Services discovery")];
    [o_status_field setStringValue: [NSString stringWithFormat:
                        _NS("No items in the playlist")]];

#if 0
    [o_search_button setTitle: _NS("Search")];
#endif
    [o_search_field setToolTip: _NS("Search in Playlist")];
    [o_mi_addNode setTitle: _NS("Add Folder to Playlist")];

    [o_save_accessory_text setStringValue: _NS("File Format:")];
    [[o_save_accessory_popup itemAtIndex:0] setTitle: _NS("Extended M3U")];
    [[o_save_accessory_popup itemAtIndex:1] setTitle: _NS("XML Shareable Playlist Format (XSPF)")];
}

- (void)playlistUpdated
{
    unsigned int i;

    /* Clear indications of any existing column sorting */
    for( i = 0 ; i < [[o_outline_view tableColumns] count] ; i++ )
    {
        [o_outline_view setIndicatorImage:nil inTableColumn:
                            [[o_outline_view tableColumns] objectAtIndex:i]];
    }

    [o_outline_view setHighlightedTableColumn:nil];
    o_tc_sortColumn = nil;
    // TODO Find a way to keep the dict size to a minimum
    //[o_outline_dict removeAllObjects];
    [o_outline_view reloadData];
    [[[[VLCMain sharedInstance] getWizard] getPlaylistWizard] reloadOutlineView];
    [[[[VLCMain sharedInstance] getBookmarks] getDataTable] reloadData];

    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    if(! p_playlist )
        return;

    if( p_playlist->i_size >= 2 )
    {
        [o_status_field setStringValue: [NSString stringWithFormat:
                    _NS("%i items in the playlist"), p_playlist->i_size]];
    }
    else
    {
        if( p_playlist->i_size == 0 )
        {
            [o_status_field setStringValue: _NS("No items in the playlist")];
        }
        else
        {
            [o_status_field setStringValue: _NS("1 item in the playlist")];
        }
    }
    vlc_object_release( p_playlist );
}

- (void)playModeUpdated
{
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    vlc_value_t val, val2;

    if( p_playlist == NULL )
    {
        return;
    }

    var_Get( p_playlist, "loop", &val2 );
    var_Get( p_playlist, "repeat", &val );
    if( val.b_bool == VLC_TRUE )
    {
        [[[VLCMain sharedInstance] getControls] repeatOne];
   }
    else if( val2.b_bool == VLC_TRUE )
    {
        [[[VLCMain sharedInstance] getControls] repeatAll];
    }
    else
    {
        [[[VLCMain sharedInstance] getControls] repeatOff];
    }

    [[[VLCMain sharedInstance] getControls] shuffle];

    vlc_object_release( p_playlist );
}

- (playlist_item_t *)parentOfItem:(playlist_item_t *)p_item
{
    int i;
    for( i = 0 ; i < p_item->i_parents; i++ )
    {
        if( p_item->pp_parents[i]->i_view == i_current_view )
        {
            return p_item->pp_parents[i]->p_parent;
        }
    }
    return NULL;
}

- (void)updateRowSelection
{
    int i_row;
    unsigned int j;

    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    playlist_item_t *p_item, *p_temp_item;
    NSMutableArray *o_array = [NSMutableArray array];

    if( p_playlist == NULL )
        return;

    p_item = p_playlist->status.p_item;
    if( p_item == NULL )
    {
        vlc_object_release(p_playlist);
        return;
    }

    p_temp_item = p_item;
    while( p_temp_item->i_parents > 0 )
    {
        [o_array insertObject: [NSValue valueWithPointer: p_temp_item] atIndex: 0];

        p_temp_item = [self parentOfItem: p_temp_item];
        /*for (i = 0 ; i < p_temp_item->i_parents ; i++)
        {
            if( p_temp_item->pp_parents[i]->i_view == i_current_view )
            {
                p_temp_item = p_temp_item->pp_parents[i]->p_parent;
                break;
            }
        }*/
    }

    for (j = 0 ; j < [o_array count] - 1 ; j++)
    {
        id o_item;
        if( ( o_item = [o_outline_dict objectForKey:
                            [NSString stringWithFormat: @"%p",
                            [[o_array objectAtIndex:j] pointerValue]]] ) != nil )
            [o_outline_view expandItem: o_item];

    }

    i_row = [o_outline_view rowForItem:[o_outline_dict
            objectForKey:[NSString stringWithFormat: @"%p", p_item]]];

    [o_outline_view selectRow: i_row byExtendingSelection: NO];
    [o_outline_view scrollRowToVisible: i_row];

    vlc_object_release(p_playlist);

    /* update our info-panel to reflect the new item */
    [[[VLCMain sharedInstance] getInfo] updatePanel];
}

/* Check if p_item is a child of p_node recursively. We need to check the item
   existence first since OSX sometimes tries to redraw items that have been
   deleted. We don't do it when not required  since this verification takes
   quite a long time on big playlists (yes, pretty hacky). */
- (BOOL)isItem: (playlist_item_t *)p_item
                    inNode: (playlist_item_t *)p_node
                    checkItemExistence:(BOOL)b_check

{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    playlist_item_t *p_temp_item = p_item;

    if( p_playlist == NULL )
    {
        return NO;
    }

    if( p_node == p_item )
    {
        vlc_object_release(p_playlist);
        return YES;
    }

    if( p_node->i_children < 1)
    {
        vlc_object_release(p_playlist);
        return NO;
    }

    if ( p_temp_item )
    {
        int i;
        vlc_mutex_lock( &p_playlist->object_lock );

        if( b_check )
        {
        /* Since outlineView: willDisplayCell:... may call this function with
           p_items that don't exist anymore, first check if the item is still
           in the playlist. Any cleaner solution welcomed. */
            for( i = 0; i < p_playlist->i_all_size; i++ )
            {
                if( p_playlist->pp_all_items[i] == p_item ) break;
                else if ( i == p_playlist->i_all_size - 1 )
                {
                    vlc_object_release( p_playlist );
                    vlc_mutex_unlock( &p_playlist->object_lock );
                    return NO;
                }
            }
        }

        while( p_temp_item->i_parents > 0 )
        {
            p_temp_item = [self parentOfItem: p_temp_item];
            if( p_temp_item == p_node )
            {
                 vlc_mutex_unlock( &p_playlist->object_lock );
                 vlc_object_release( p_playlist );
                 return YES;
            }

/*            for( i = 0; i < p_temp_item->i_parents ; i++ )
            {
                if( p_temp_item->pp_parents[i]->i_view == i_current_view )
                {
                    if( p_temp_item->pp_parents[i]->p_parent == p_node )
                    {
                        vlc_mutex_unlock( &p_playlist->object_lock );
                        vlc_object_release( p_playlist );
                        return YES;
                    }
                    else
                    {
                        p_temp_item = p_temp_item->pp_parents[i]->p_parent;
                        break;
                    }
                }
            }*/
        }
        vlc_mutex_unlock( &p_playlist->object_lock );
    }

    vlc_object_release( p_playlist );
    return NO;
}

/* This method is usefull for instance to remove the selected children of an
   already selected node */
- (void)removeItemsFrom:(id)o_items ifChildrenOf:(id)o_nodes
{
    unsigned int i, j;
    for( i = 0 ; i < [o_items count] ; i++ )
    {
        for ( j = 0 ; j < [o_nodes count] ; j++ )
        {
            if( o_items == o_nodes)
            {
                if( j == i ) continue;
            }
            if( [self isItem: [[o_items objectAtIndex:i] pointerValue]
                    inNode: [[o_nodes objectAtIndex:j] pointerValue]
                    checkItemExistence: NO] )
            {
                [o_items removeObjectAtIndex:i];
                /* We need to execute the next iteration with the same index
                   since the current item has been deleted */
                i--;
                break;
            }
        }
    }

}

- (IBAction)savePlaylist:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    NSSavePanel *o_save_panel = [NSSavePanel savePanel];
    NSString * o_name = [NSString stringWithFormat: @"%@", _NS("Untitled")];

    //[o_save_panel setAllowedFileTypes: [NSArray arrayWithObjects: @"m3u", @"xpf", nil] ];
    [o_save_panel setTitle: _NS("Save Playlist")];
    [o_save_panel setPrompt: _NS("Save")];
    [o_save_panel setAccessoryView: o_save_accessory_view];

    if( [o_save_panel runModalForDirectory: nil
            file: o_name] == NSOKButton )
    {
        NSString *o_filename = [o_save_panel filename];

        if( [o_save_accessory_popup indexOfSelectedItem] == 1 )
        {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".xspf" length];
            range.length = [@".xspf" length];

            if( [o_filename compare:@".xspf" options: NSCaseInsensitiveSearch
                                             range: range] != NSOrderedSame )
            {
                o_real_filename = [NSString stringWithFormat: @"%@.xspf", o_filename];
            }
            else
            {
                o_real_filename = o_filename;
            }
            playlist_Export( p_playlist, [o_real_filename fileSystemRepresentation], "export-xspf" );
        }
        else
        {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".m3u" length];
            range.length = [@".m3u" length];

            if( [o_filename compare:@".m3u" options: NSCaseInsensitiveSearch
                                             range: range] != NSOrderedSame )
            {
                o_real_filename = [NSString stringWithFormat: @"%@.m3u", o_filename];
            }
            else
            {
                o_real_filename = o_filename;
            }
            playlist_Export( p_playlist, [o_real_filename fileSystemRepresentation], "export-m3u" );
        }
    }
    vlc_object_release( p_playlist );
}

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)playItem:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        playlist_item_t *p_item;
        playlist_item_t *p_node = NULL;

        p_item = [[o_outline_view itemAtRow:[o_outline_view selectedRow]] pointerValue];

        if( p_item )
        {
            if( p_item->i_children == -1 )
            {
                p_node = [self parentOfItem: p_item];

/*                for( i = 0 ; i < p_item->i_parents ; i++ )
                {
                    if( p_item->pp_parents[i]->i_view == i_current_view )
                    {
                        p_node = p_item->pp_parents[i]->p_parent;
                    }
                }*/
            }
            else
            {
                p_node = p_item;
                if( p_node->i_children > 0 && p_node->pp_children[0]->i_children == -1 )
                {
                    p_item = p_node->pp_children[0];
                }
                else
                {
                    p_item = NULL;
                }
            }
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, i_current_view, p_node, p_item );
        }
        vlc_object_release( p_playlist );
    }
}

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)preparseItem:(id)sender
{
    int i_count;
    NSMutableArray *o_to_preparse;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
                                                       
    o_to_preparse = [NSMutableArray arrayWithArray:[[o_outline_view selectedRowEnumerator] allObjects]];
    i_count = [o_to_preparse count];

    if( p_playlist != NULL )
    {
        int i, i_row;
        NSNumber *o_number;
        playlist_item_t *p_item = NULL;

        for( i = 0; i < i_count; i++ )
        {
            o_number = [o_to_preparse lastObject];
            i_row = [o_number intValue];
            p_item = [[o_outline_view itemAtRow:i_row] pointerValue];
            [o_to_preparse removeObject: o_number];
            [o_outline_view deselectRow: i_row];

            if( p_item )
            {
                if( p_item->i_children == -1 )
                {
                    playlist_PreparseEnqueue( p_playlist, &p_item->input );
                }
                else
                {
                    msg_Dbg( p_intf, "preparse of nodes not yet implemented" );
                }
            }
        }
        vlc_object_release( p_playlist );
    }
    [self playlistUpdated];
}

- (IBAction)servicesChange:(id)sender
{
    NSMenuItem *o_mi = (NSMenuItem *)sender;
    NSString *o_string = [o_mi representedObject];
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    if( !playlist_IsServicesDiscoveryLoaded( p_playlist, [o_string cString] ) )
        playlist_ServicesDiscoveryAdd( p_playlist, [o_string cString] );
    else
        playlist_ServicesDiscoveryRemove( p_playlist, [o_string cString] );

    [o_mi setState: playlist_IsServicesDiscoveryLoaded( p_playlist,
                                          [o_string cString] ) ? YES : NO];

    i_current_view = VIEW_CATEGORY;
    playlist_ViewUpdate( p_playlist, i_current_view );
    vlc_object_release( p_playlist );
    [self playlistUpdated];
    return;
}

- (IBAction)selectAll:(id)sender
{
    [o_outline_view selectAll: nil];
}

- (IBAction)deleteItem:(id)sender
{
    int i, i_count, i_row;
    NSMutableArray *o_to_delete;
    NSNumber *o_number;

    playlist_t * p_playlist;
    intf_thread_t * p_intf = VLCIntf;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );

    if ( p_playlist == NULL )
    {
        return;
    }
    o_to_delete = [NSMutableArray arrayWithArray:[[o_outline_view selectedRowEnumerator] allObjects]];
    i_count = [o_to_delete count];

    for( i = 0; i < i_count; i++ )
    {
        o_number = [o_to_delete lastObject];
        i_row = [o_number intValue];
        id o_item = [o_outline_view itemAtRow: i_row];
        playlist_item_t *p_item = [o_item pointerValue];
        [o_to_delete removeObject: o_number];
        [o_outline_view deselectRow: i_row];

        if( [[o_outline_view dataSource] outlineView:o_outline_view
                                        numberOfChildrenOfItem: o_item]  > 0 )
        //is a node and not an item
        {
            if( p_playlist->status.i_status != PLAYLIST_STOPPED &&
                [self isItem: p_playlist->status.p_item inNode:
                        ((playlist_item_t *)[o_item pointerValue])
                        checkItemExistence: NO] == YES )
            {
                // if current item is in selected node and is playing then stop playlist
                playlist_Stop( p_playlist );
            }
            vlc_mutex_lock( &p_playlist->object_lock );
            playlist_NodeDelete( p_playlist, p_item, VLC_TRUE, VLC_FALSE );
            vlc_mutex_unlock( &p_playlist->object_lock );
        }
        else
        {
            if( p_playlist->status.i_status != PLAYLIST_STOPPED &&
                p_playlist->status.p_item == [[o_outline_view itemAtRow: i_row] pointerValue] )
            {
                playlist_Stop( p_playlist );
            }
            vlc_mutex_lock( &p_playlist->object_lock );
            playlist_Delete( p_playlist, p_item->input.i_id );
            vlc_mutex_unlock( &p_playlist->object_lock );
        }
    }
    [self playlistUpdated];
    vlc_object_release( p_playlist );
}

- (IBAction)sortNodeByName:(id)sender
{
    [self sortNode: SORT_TITLE];
}

- (IBAction)sortNodeByAuthor:(id)sender
{
    [self sortNode: SORT_AUTHOR];
}

- (void)sortNode:(int)i_mode
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    playlist_item_t * p_item;

    if (p_playlist == NULL)
    {
        return;
    }

    if( [o_outline_view selectedRow] > -1 )
    {
        p_item = [[o_outline_view itemAtRow: [o_outline_view selectedRow]]
                                                                pointerValue];
    }
    else
    /*If no item is selected, sort the whole playlist*/
    {
        playlist_view_t * p_view = playlist_ViewFind( p_playlist, i_current_view );
        p_item = p_view->p_root;
    }

    if( p_item->i_children > -1 ) // the item is a node
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        playlist_RecursiveNodeSort( p_playlist, p_item, i_mode, ORDER_NORMAL );
        vlc_mutex_unlock( &p_playlist->object_lock );
    }
    else
    {
        int i;

        for( i = 0 ; i < p_item->i_parents ; i++ )
        {
            if( p_item->pp_parents[i]->i_view == i_current_view )
            {
                vlc_mutex_lock( &p_playlist->object_lock );
                playlist_RecursiveNodeSort( p_playlist,
                        p_item->pp_parents[i]->p_parent, i_mode, ORDER_NORMAL );
                vlc_mutex_unlock( &p_playlist->object_lock );
                break;
            }
        }
    }
    vlc_object_release( p_playlist );
    [self playlistUpdated];
}

- (playlist_item_t *)createItem:(NSDictionary *)o_one_item
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return NULL;
    }
    playlist_item_t *p_item;
    int i;
    BOOL b_rem = FALSE, b_dir = FALSE;
    NSString *o_uri, *o_name;
    NSArray *o_options;
    NSURL *o_true_file;

    /* Get the item */
    o_uri = (NSString *)[o_one_item objectForKey: @"ITEM_URL"];
    o_name = (NSString *)[o_one_item objectForKey: @"ITEM_NAME"];
    o_options = (NSArray *)[o_one_item objectForKey: @"ITEM_OPTIONS"];

    /* Find the name for a disc entry ( i know, can you believe the trouble?) */
    if( ( !o_name || [o_name isEqualToString:@""] ) && [o_uri rangeOfString: @"/dev/"].location != NSNotFound )
    {
        int i_count, i_index;
        struct statfs *mounts = NULL;

        i_count = getmntinfo (&mounts, MNT_NOWAIT);
        /* getmntinfo returns a pointer to static data. Do not free. */
        for( i_index = 0 ; i_index < i_count; i_index++ )
        {
            NSMutableString *o_temp, *o_temp2;
            o_temp = [NSMutableString stringWithString: o_uri];
            o_temp2 = [NSMutableString stringWithCString: mounts[i_index].f_mntfromname];
            [o_temp replaceOccurrencesOfString: @"/dev/rdisk" withString: @"/dev/disk" options:nil range:NSMakeRange(0, [o_temp length]) ];
            [o_temp2 replaceOccurrencesOfString: @"s0" withString: @"" options:nil range:NSMakeRange(0, [o_temp2 length]) ];
            [o_temp2 replaceOccurrencesOfString: @"s1" withString: @"" options:nil range:NSMakeRange(0, [o_temp2 length]) ];

            if( strstr( [o_temp fileSystemRepresentation], [o_temp2 fileSystemRepresentation] ) != NULL )
            {
                o_name = [[NSFileManager defaultManager] displayNameAtPath: [NSString stringWithCString:mounts[i_index].f_mntonname]];
            }
        }
    }
    /* If no name, then make a guess */
    if( !o_name) o_name = [[NSFileManager defaultManager] displayNameAtPath: o_uri];

    if( [[NSFileManager defaultManager] fileExistsAtPath:o_uri isDirectory:&b_dir] && b_dir &&
        [[NSWorkspace sharedWorkspace] getFileSystemInfoForPath: o_uri isRemovable: &b_rem
                isWritable:NULL isUnmountable:NULL description:NULL type:NULL] && b_rem   )
    {
        /* All of this is to make sure CD's play when you D&D them on VLC */
        /* Converts mountpoint to a /dev file */
        struct statfs *buf;
        char *psz_dev;
        NSMutableString *o_temp;

        buf = (struct statfs *) malloc (sizeof(struct statfs));
        statfs( [o_uri fileSystemRepresentation], buf );
        psz_dev = strdup(buf->f_mntfromname);
        o_temp = [NSMutableString stringWithCString: psz_dev ];
        [o_temp replaceOccurrencesOfString: @"/dev/disk" withString: @"/dev/rdisk" options:nil range:NSMakeRange(0, [o_temp length]) ];
        [o_temp replaceOccurrencesOfString: @"s0" withString: @"" options:nil range:NSMakeRange(0, [o_temp length]) ];
        [o_temp replaceOccurrencesOfString: @"s1" withString: @"" options:nil range:NSMakeRange(0, [o_temp length]) ];
        o_uri = o_temp;
    }

    p_item = playlist_ItemNew( p_intf, [o_uri fileSystemRepresentation], [o_name UTF8String] );
    if( !p_item )
       return NULL;

    if( o_options )
    {
        for( i = 0; i < (int)[o_options count]; i++ )
        {
            playlist_ItemAddOption( p_item, strdup( [[o_options objectAtIndex:i] UTF8String] ) );
        }
    }

    /* Recent documents menu */
    o_true_file = [NSURL fileURLWithPath: o_uri];
    if( o_true_file != nil )
    {
        [[NSDocumentController sharedDocumentController]
            noteNewRecentDocumentURL: o_true_file];
    }

    vlc_object_release( p_playlist );
    return p_item;
}

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue
{
    int i_item;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                            FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    for( i_item = 0; i_item < (int)[o_array count]; i_item++ )
    {
        playlist_item_t *p_item;
        NSDictionary *o_one_item;

        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        p_item = [self createItem: o_one_item];
        if( !p_item )
        {
            continue;
        }

        /* Add the item */
        playlist_AddItem( p_playlist, p_item, PLAYLIST_INSERT, i_position == -1 ? PLAYLIST_END : i_position + i_item );

        if( i_item == 0 && !b_enqueue )
        {
            playlist_Control( p_playlist, PLAYLIST_ITEMPLAY, p_item );
        }
    }
    vlc_object_release( p_playlist );
}

- (void)appendNodeArray:(NSArray*)o_array inNode:(playlist_item_t *)p_node atPos:(int)i_position inView:(int)i_view enqueue:(BOOL)b_enqueue
{
    int i_item;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                            FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    for( i_item = 0; i_item < (int)[o_array count]; i_item++ )
    {
        playlist_item_t *p_item;
        NSDictionary *o_one_item;

        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        p_item = [self createItem: o_one_item];
        if( !p_item )
        {
            continue;
        }

        /* Add the item */
        playlist_NodeAddItem( p_playlist, p_item, i_view, p_node, PLAYLIST_INSERT, i_position + i_item );

        if( i_item == 0 && !b_enqueue )
        {
            playlist_Control( p_playlist, PLAYLIST_ITEMPLAY, p_item );
        }
    }
    vlc_object_release( p_playlist );

}

- (NSMutableArray *)subSearchItem:(playlist_item_t *)p_item
{
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    playlist_item_t *p_selected_item;
    int i_current, i_selected_row;

    if( !p_playlist )
        return NULL;

    i_selected_row = [o_outline_view selectedRow];
    if (i_selected_row < 0)
        i_selected_row = 0;

    p_selected_item = (playlist_item_t *)[[o_outline_view itemAtRow:
                                            i_selected_row] pointerValue];

    for( i_current = 0; i_current < p_item->i_children ; i_current++ )
    {
        char *psz_temp;
        NSString *o_current_name, *o_current_author;

        vlc_mutex_lock( &p_playlist->object_lock );
        o_current_name = [NSString stringWithUTF8String:
            p_item->pp_children[i_current]->input.psz_name];
        psz_temp = vlc_input_item_GetInfo( &p_item->input ,
                   _("Meta-information"),_("Artist") );
        o_current_author = [NSString stringWithUTF8String: psz_temp];
        free( psz_temp);
        vlc_mutex_unlock( &p_playlist->object_lock );

        if( p_selected_item == p_item->pp_children[i_current] &&
                    b_selected_item_met == NO )
        {
            b_selected_item_met = YES;
        }
        else if( p_selected_item == p_item->pp_children[i_current] &&
                    b_selected_item_met == YES )
        {
            vlc_object_release( p_playlist );
            return NULL;
        }
        else if( b_selected_item_met == YES &&
                    ( [o_current_name rangeOfString:[o_search_field
                        stringValue] options:NSCaseInsensitiveSearch ].length ||
                      [o_current_author rangeOfString:[o_search_field
                        stringValue] options:NSCaseInsensitiveSearch ].length ) )
        {
            vlc_object_release( p_playlist );
            /*Adds the parent items in the result array as well, so that we can
            expand the tree*/
            return [NSMutableArray arrayWithObject: [NSValue
                            valueWithPointer: p_item->pp_children[i_current]]];
        }
        if( p_item->pp_children[i_current]->i_children > 0 )
        {
            id o_result = [self subSearchItem:
                                            p_item->pp_children[i_current]];
            if( o_result != NULL )
            {
                vlc_object_release( p_playlist );
                [o_result insertObject: [NSValue valueWithPointer:
                                p_item->pp_children[i_current]] atIndex:0];
                return o_result;
            }
        }
    }
    vlc_object_release( p_playlist );
    return NULL;
}

- (IBAction)searchItem:(id)sender
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    playlist_view_t * p_view;
    id o_result;

    unsigned int i;
    int i_row = -1;

    b_selected_item_met = NO;

    if( p_playlist == NULL )
        return;
    p_view = playlist_ViewFind( p_playlist, i_current_view );

    if( p_view )
    {
        /*First, only search after the selected item:*
         *(b_selected_item_met = NO)                 */
        o_result = [self subSearchItem:p_view->p_root];
        if( o_result == NULL )
        {
            /* If the first search failed, search again from the beginning */
            o_result = [self subSearchItem:p_view->p_root];
        }
        if( o_result != NULL )
        {
            int i_start;
            if( [[o_result objectAtIndex: 0] pointerValue] ==
                                                    p_playlist->p_general )
            i_start = 1;
            else
            i_start = 0;

            for( i = i_start ; i < [o_result count] - 1 ; i++ )
            {
                [o_outline_view expandItem: [o_outline_dict objectForKey:
                            [NSString stringWithFormat: @"%p",
                            [[o_result objectAtIndex: i] pointerValue]]]];
            }
            i_row = [o_outline_view rowForItem: [o_outline_dict objectForKey:
                            [NSString stringWithFormat: @"%p",
                            [[o_result objectAtIndex: [o_result count] - 1 ]
                            pointerValue]]]];
        }
        if( i_row > -1 )
        {
            [o_outline_view selectRow:i_row byExtendingSelection: NO];
            [o_outline_view scrollRowToVisible: i_row];
        }
    }
    vlc_object_release( p_playlist );
}

- (IBAction)recursiveExpandNode:(id)sender
{
    int i;
    id o_item = [o_outline_view itemAtRow: [o_outline_view selectedRow]];
    playlist_item_t *p_item = (playlist_item_t *)[o_item pointerValue];

    if( ![[o_outline_view dataSource] outlineView: o_outline_view
                                                    isItemExpandable: o_item] )
    {
        for( i = 0 ; i < p_item->i_parents ; i++ )
        {
            if( p_item->pp_parents[i]->i_view == i_current_view )
            {
                o_item = [o_outline_dict objectForKey: [NSString
                    stringWithFormat: @"%p", p_item->pp_parents[i]->p_parent]];
                break;
            }
        }
    }

    /* We need to collapse the node first, since OSX refuses to recursively
       expand an already expanded node, even if children nodes are collapsed. */
    [o_outline_view collapseItem: o_item collapseChildren: YES];
    [o_outline_view expandItem: o_item expandChildren: YES];
}

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    NSPoint pt;
    vlc_bool_t b_rows;
    vlc_bool_t b_item_sel;

    pt = [o_outline_view convertPoint: [o_event locationInWindow]
                                                 fromView: nil];
    b_item_sel = ( [o_outline_view rowAtPoint: pt] != -1 &&
                   [o_outline_view selectedRow] != -1 );
    b_rows = [o_outline_view numberOfRows] != 0;

    [o_mi_play setEnabled: b_item_sel];
    [o_mi_delete setEnabled: b_item_sel];
    [o_mi_selectall setEnabled: b_rows];
    [o_mi_info setEnabled: b_item_sel];
    [o_mi_preparse setEnabled: b_item_sel];
    [o_mi_recursive_expand setEnabled: b_item_sel];
    [o_mi_sort_name setEnabled: b_item_sel];
    [o_mi_sort_author setEnabled: b_item_sel];

    return( o_ctx_menu );
}

- (void)outlineView: (NSTableView*)o_tv
                  didClickTableColumn:(NSTableColumn *)o_tc
{
    int i_mode = 0, i_type;
    intf_thread_t *p_intf = VLCIntf;
    playlist_view_t *p_view;

    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* Check whether the selected table column header corresponds to a
       sortable table column*/
    if( !( o_tc == o_tc_name || o_tc == o_tc_author ) )
    {
        vlc_object_release( p_playlist );
        return;
    }

    p_view = playlist_ViewFind( p_playlist, i_current_view );

    if( o_tc_sortColumn == o_tc )
    {
        b_isSortDescending = !b_isSortDescending;
    }
    else
    {
        b_isSortDescending = VLC_FALSE;
    }

    if( o_tc == o_tc_name )
    {
        i_mode = SORT_TITLE;
    }
    else if( o_tc == o_tc_author )
    {
        i_mode = SORT_AUTHOR;
    }

    if( b_isSortDescending )
    {
        i_type = ORDER_REVERSE;
    }
    else
    {
        i_type = ORDER_NORMAL;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    playlist_RecursiveNodeSort( p_playlist, p_view->p_root, i_mode, i_type );
    vlc_mutex_unlock( &p_playlist->object_lock );

    vlc_object_release( p_playlist );
    [self playlistUpdated];

    o_tc_sortColumn = o_tc;
    [o_outline_view setHighlightedTableColumn:o_tc];

    if( b_isSortDescending )
    {
        [o_outline_view setIndicatorImage:o_descendingSortingImage
                                                        inTableColumn:o_tc];
    }
    else
    {
        [o_outline_view setIndicatorImage:o_ascendingSortingImage
                                                        inTableColumn:o_tc];
    }
}


- (void)outlineView:(NSOutlineView *)outlineView
                                willDisplayCell:(id)cell
                                forTableColumn:(NSTableColumn *)tableColumn
                                item:(id)item
{
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );

    id o_playing_item;

    if( !p_playlist ) return;

    o_playing_item = [o_outline_dict objectForKey:
                [NSString stringWithFormat:@"%p",  p_playlist->status.p_item]];

    if( [self isItem: [o_playing_item pointerValue] inNode:
                        [item pointerValue] checkItemExistence: YES]
                        || [o_playing_item isEqual: item] )
    {
        [cell setFont: [NSFont boldSystemFontOfSize: 0]];
    }
    else
    {
        [cell setFont: [NSFont systemFontOfSize: 0]];
    }
    vlc_object_release( p_playlist );
}

- (IBAction)addNode:(id)sender
{
    /* simply adds a new node to the end of the playlist */
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    if( !p_playlist )
    {
        return;
    }

    playlist_item_t * p_item = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, 
        _("Empty Folder"), p_playlist->p_general );

    if(! p_item )
        msg_Warn( VLCIntf, "node creation failed" );
    
    playlist_ViewUpdate( p_playlist, VIEW_CATEGORY );
    
    vlc_object_release( p_playlist );
}

@end

@implementation VLCPlaylist (NSOutlineViewDataSource)

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    id o_value = [super outlineView: outlineView child: index ofItem: item];
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );

    if( !p_playlist ) return nil;

    if( p_playlist->i_size >= 2 )
    {
        [o_status_field setStringValue: [NSString stringWithFormat:
                    _NS("%i items in the playlist"), p_playlist->i_size]];
    }
    else
    {
        if( p_playlist->i_size == 0 )
        {
            [o_status_field setStringValue: _NS("No items in the playlist")];
        }
        else
        {
            [o_status_field setStringValue: _NS("1 item in the playlist")];
        }
    }
    vlc_object_release( p_playlist );

    [o_outline_dict setObject:o_value forKey:[NSString stringWithFormat:@"%p",
                                                    [o_value pointerValue]]];

    return o_value;

}

/* Required for drag & drop and reordering */
- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pboard
{
    unsigned int i;
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );

    /* First remove the items that were moved during the last drag & drop
       operation */
    [o_items_array removeAllObjects];
    [o_nodes_array removeAllObjects];

    if( !p_playlist ) return NO;

    for( i = 0 ; i < [items count] ; i++ )
    {
        id o_item = [items objectAtIndex: i];

        /* Refuse to move items that are not in the General Node
           (Service Discovery) */
        if( ![self isItem: [o_item pointerValue] inNode:
                        p_playlist->p_general checkItemExistence: NO])
        {
            vlc_object_release(p_playlist);
            return NO;
        }
        /* Fill the items and nodes to move in 2 different arrays */
        if( ((playlist_item_t *)[o_item pointerValue])->i_children > 0 )
            [o_nodes_array addObject: o_item];
        else
            [o_items_array addObject: o_item];
    }

    /* Now we need to check if there are selected items that are in already
       selected nodes. In that case, we only want to move the nodes */
    [self removeItemsFrom: o_nodes_array ifChildrenOf: o_nodes_array];
    [self removeItemsFrom: o_items_array ifChildrenOf: o_nodes_array];

    /* We add the "VLCPlaylistItemPboardType" type to be able to recognize
       a Drop operation coming from the playlist. */

    [pboard declareTypes: [NSArray arrayWithObjects:
        @"VLCPlaylistItemPboardType", nil] owner: self];
    [pboard setData:[NSData data] forType:@"VLCPlaylistItemPboardType"];

    vlc_object_release(p_playlist);
    return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    if( !p_playlist ) return NSDragOperationNone;

    /* Dropping ON items is not allowed if item is not a node */
    if( item )
    {
        if( index == NSOutlineViewDropOnItemIndex &&
                ((playlist_item_t *)[item pointerValue])->i_children == -1 )
        {
            vlc_object_release( p_playlist );
            return NSDragOperationNone;
        }
    }

    /* We refuse to drop an item in anything else than a child of the General
       Node. We still accept items that would be root nodes of the outlineview
       however, to allow drop in an empty playlist. */
    if( !([self isItem: [item pointerValue] inNode: p_playlist->p_general
                                    checkItemExistence: NO] || item == nil) )
    {
        vlc_object_release( p_playlist );
        return NSDragOperationNone;
    }

    /* Drop from the Playlist */
    if( [[o_pasteboard types] containsObject: @"VLCPlaylistItemPboardType"] )
    {
        unsigned int i;
        for( i = 0 ; i < [o_nodes_array count] ; i++ )
        {
            /* We refuse to Drop in a child of an item we are moving */
            if( [self isItem: [item pointerValue] inNode:
                    [[o_nodes_array objectAtIndex: i] pointerValue]
                    checkItemExistence: NO] )
            {
                vlc_object_release( p_playlist );
                return NSDragOperationNone;
            }
        }
        vlc_object_release(p_playlist);
        return NSDragOperationMove;
    }

    /* Drop from the Finder */
    else if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
    {
        vlc_object_release(p_playlist);
        return NSDragOperationGeneric;
    }
    vlc_object_release(p_playlist);
    return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index
{
    playlist_t * p_playlist =  vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    if( !p_playlist ) return NO;

    /* Drag & Drop inside the playlist */
    if( [[o_pasteboard types] containsObject: @"VLCPlaylistItemPboardType"] )
    {
        int i_row, i_removed_from_node = 0;
        unsigned int i;
        playlist_item_t *p_new_parent, *p_item = NULL;
        NSArray *o_all_items = [o_nodes_array arrayByAddingObjectsFromArray:
                                                                o_items_array];
        /* If the item is to be dropped as root item of the outline, make it a
           child of the General node.
           Else, choose the proposed parent as parent. */
        if( item == nil ) p_new_parent = p_playlist->p_general;
        else p_new_parent = [item pointerValue];

        /* Make sure the proposed parent is a node.
           (This should never be true) */
        if( p_new_parent->i_children < 0 )
        {
            vlc_object_release( p_playlist );
            return NO;
        }

        for( i = 0; i < [o_all_items count]; i++ )
        {
            playlist_item_t *p_old_parent = NULL;
            int i_old_index = 0;

            p_item = [[o_all_items objectAtIndex:i] pointerValue];
            p_old_parent = [self parentOfItem: p_item];
            if( !p_old_parent )
            continue;
            /* We may need the old index later */
            if( p_new_parent == p_old_parent )
            {
                int j;
                for( j = 0; j < p_old_parent->i_children; j++ )
                {
                    if( p_old_parent->pp_children[j] == p_item )
                    {
                        i_old_index = j;
                        break;
                    }
                }
            }

            vlc_mutex_lock( &p_playlist->object_lock );
            // Acually detach the item from the old position
            if( playlist_NodeRemoveItem( p_playlist, p_item, p_old_parent ) ==
                VLC_SUCCESS  &&
                playlist_NodeRemoveParent( p_playlist, p_item, p_old_parent ) ==
                VLC_SUCCESS )
            {
                int i_new_index;
                /* Calculate the new index */
                if( index == -1 )
                i_new_index = -1;
                /* If we move the item in the same node, we need to take into
                   account that one item will be deleted */
                else
                {
                    if ((p_new_parent == p_old_parent &&
                                   i_old_index < index + (int)i) )
                    {
                        i_removed_from_node++;
                    }
                    i_new_index = index + i - i_removed_from_node;
                }
                // Reattach the item to the new position
                playlist_NodeInsert( p_playlist, i_current_view, p_item,
                                                    p_new_parent, i_new_index );
            }
            vlc_mutex_unlock( &p_playlist->object_lock );
        }
        [self playlistUpdated];
        i_row = [o_outline_view rowForItem:[o_outline_dict
            objectForKey:[NSString stringWithFormat: @"%p",
            [[o_all_items objectAtIndex: 0] pointerValue]]]];

        if( i_row == -1 )
        {
            i_row = [o_outline_view rowForItem:[o_outline_dict
            objectForKey:[NSString stringWithFormat: @"%p", p_new_parent]]];
        }

        [o_outline_view deselectAll: self];
        [o_outline_view selectRow: i_row byExtendingSelection: NO];
        [o_outline_view scrollRowToVisible: i_row];

        vlc_object_release(p_playlist);
        return YES;
    }

    else if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
    {
        int i;
        playlist_item_t *p_node = [item pointerValue];

        NSArray *o_array = [NSArray array];
        NSArray *o_values = [[o_pasteboard propertyListForType:
                                        NSFilenamesPboardType]
                                sortedArrayUsingSelector:
                                        @selector(caseInsensitiveCompare:)];

        for( i = 0; i < (int)[o_values count]; i++)
        {
            NSDictionary *o_dic;
            o_dic = [NSDictionary dictionaryWithObject:[o_values
                        objectAtIndex:i] forKey:@"ITEM_URL"];
            o_array = [o_array arrayByAddingObject: o_dic];
        }

        if ( item == nil )
        {
            [self appendArray: o_array atPos: index enqueue: YES];
        }
        /* This should never occur */
        else if( p_node->i_children == -1 )
        {
            vlc_object_release( p_playlist );
            return NO;
        }
        else
        {
            [self appendNodeArray: o_array inNode: p_node
                atPos: index inView: i_current_view enqueue: YES];
        }
        vlc_object_release( p_playlist );
        return YES;
    }
    vlc_object_release( p_playlist );
    return NO;
}

/* Delegate method of NSWindow */
/*- (void)windowWillClose:(NSNotification *)aNotification
{
    [o_btn_playlist setState: NSOffState];
}
*/
@end


