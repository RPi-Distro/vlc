/*****************************************************************************
 * plugins.hpp : Plug-ins and extensions listing
 ****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN team
 * $Id: 5199c95b7c6a3ffa71ccad4dd469156c20a25a9b $
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
 *          Jean-Philippe André <jpeg (at) videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "plugins.hpp"

#include "util/customwidgets.hpp"
#include "extensions_manager.hpp"

#include <assert.h>

//#include <vlc_modules.h>

#include <QTreeWidget>
#include <QStringList>
#include <QTabWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QListView>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QKeyEvent>


PluginDialog::PluginDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Plugins and extensions" ) );
    setWindowRole( "vlc-plugins" );

    QVBoxLayout *layout = new QVBoxLayout( this );
    tabs = new QTabWidget( this );
    tabs->addTab( extensionTab = new ExtensionTab( p_intf ),
                  qtr( "Extensions" ) );
    tabs->addTab( pluginTab = new PluginTab( p_intf ),
                  qtr( "Plugins" ) );
    layout->addWidget( tabs );

    QDialogButtonBox *box = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( qtr( "&Close" ), this );
    box->addButton( okButton, QDialogButtonBox::AcceptRole );
    layout->addWidget( box );
    BUTTONACT( okButton, close() );
    readSettings( "PluginsDialog", QSize( 435, 280 ) );
}

PluginDialog::~PluginDialog()
{
    writeSettings( "PluginsDialog" );
}

/* Plugins tab */

PluginTab::PluginTab( intf_thread_t *p_intf )
        : QVLCFrame( p_intf )
{
    QGridLayout *layout = new QGridLayout( this );

    /* Main Tree for modules */
    treePlugins = new QTreeWidget;
    layout->addWidget( treePlugins, 0, 0, 1, -1 );

    /* Users cannot move the columns around but we need to sort */
    treePlugins->header()->setMovable( false );
    treePlugins->header()->setSortIndicatorShown( true );
    //    treePlugins->header()->setResizeMode( QHeaderView::ResizeToContents );
    treePlugins->setAlternatingRowColors( true );
    treePlugins->setColumnWidth( 0, 200 );

    QStringList headerNames;
    headerNames << qtr("Name") << qtr("Capability" ) << qtr( "Score" );
    treePlugins->setHeaderLabels( headerNames );

    FillTree();

    /* Set capability column to the correct Size*/
    treePlugins->resizeColumnToContents( 1 );
    treePlugins->header()->restoreState(
            getSettings()->value( "Plugins/Header-State" ).toByteArray() );

    treePlugins->setSortingEnabled( true );
    treePlugins->sortByColumn( 1, Qt::AscendingOrder );

    QLabel *label = new QLabel( qtr("&Search:"), this );
    edit = new SearchLineEdit( this );
    label->setBuddy( edit );

    layout->addWidget( label, 1, 0 );
    layout->addWidget( edit, 1, 1, 1, 1 );
    CONNECT( edit, textChanged( const QString& ),
            this, search( const QString& ) );

    setMinimumSize( 500, 300 );
    readSettings( "Plugins", QSize( 540, 400 ) );
}

inline void PluginTab::FillTree()
{
    module_t **p_list = module_list_get( NULL );
    module_t *p_module;

    for( unsigned int i = 0; (p_module = p_list[i] ) != NULL; i++ )
    {
        QStringList qs_item;
        qs_item << qfu( module_get_name( p_module, true ) )
                << qfu( module_get_capability( p_module ) )
                << QString::number( module_get_score( p_module ) );
#ifndef DEBUG
        if( qs_item.at(1).isEmpty() ) continue;
#endif

        QTreeWidgetItem *item = new PluginTreeItem( qs_item );
        treePlugins->addTopLevelItem( item );
    }
    module_list_free( p_list );
}

void PluginTab::search( const QString& qs )
{
    QList<QTreeWidgetItem *> items = treePlugins->findItems( qs, Qt::MatchContains );
    items += treePlugins->findItems( qs, Qt::MatchContains, 1 );

    QTreeWidgetItem *item = NULL;
    for( int i = 0; i < treePlugins->topLevelItemCount(); i++ )
    {
        item = treePlugins->topLevelItem( i );
        item->setHidden( !items.contains( item ) );
    }
}

PluginTab::~PluginTab()
{
    writeSettings( "Plugins" );
    getSettings()->setValue( "Plugins/Header-State",
                             treePlugins->header()->saveState() );
}

bool PluginTreeItem::operator< ( const QTreeWidgetItem & other ) const
{
    int col = treeWidget()->sortColumn();
    if( col == 2 )
        return text( col ).toInt() < other.text( col ).toInt();
    return text( col ) < other.text( col );
}

/* Extensions tab */
ExtensionTab::ExtensionTab( intf_thread_t *p_intf )
        : QVLCFrame( p_intf )
{
    // Layout
    QVBoxLayout *layout = new QVBoxLayout( this );

    // ListView
    extList = new QListView( this );
    CONNECT( extList, activated( const QModelIndex& ),
             this, moreInformation() );
    layout->addWidget( extList );

    // List item delegate
    ExtensionItemDelegate *itemDelegate = new ExtensionItemDelegate( p_intf,
                                                                     extList );
    extList->setItemDelegate( itemDelegate );

    // Extension list look & feeling
    extList->setAlternatingRowColors( true );
    extList->setSelectionMode( QAbstractItemView::SingleSelection );

    // Model
    ExtensionListModel *model = new ExtensionListModel( extList, p_intf );
    extList->setModel( model );

    // Buttons' layout
    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addItem( new QSpacerItem( 1, 1, QSizePolicy::Expanding,
                                    QSizePolicy::Fixed ) );

    // More information button
    butMoreInfo = new QPushButton( QIcon( ":/menu/info" ),
                                   qtr( "More information..." ),
                                   this );
    CONNECT( butMoreInfo, clicked(),
             this, moreInformation() );
    hbox->addWidget( butMoreInfo );

    // Reload button
    ExtensionsManager *EM = ExtensionsManager::getInstance( p_intf );
    QPushButton *reload = new QPushButton( QIcon( ":/update" ),
                                           qtr( "Reload extensions" ),
                                           this );
    CONNECT( reload, clicked(),
             EM, reloadExtensions() );
    hbox->addWidget( reload );

    // Add buttons hbox
    layout->addItem( hbox );
}

ExtensionTab::~ExtensionTab()
{
}

// Do not close on ESC or ENTER
void ExtensionTab::keyPressEvent( QKeyEvent *keyEvent )
{
    keyEvent->ignore();
}

// Show more information
void ExtensionTab::moreInformation()
{
    if( !extList->selectionModel() ||
        extList->selectionModel()->selectedIndexes().isEmpty() )

    {
        return;
    }

    QModelIndex index = extList->selectionModel()->selectedIndexes().first();
    ExtensionCopy *ext = (ExtensionCopy*) index.internalPointer();
    if( !ext )
        return;

    ExtensionInfoDialog dlg( *ext, p_intf, this );
    dlg.exec();
}

/* Safe copy of the extension_t struct */
class ExtensionCopy
{
public:
    ExtensionCopy( extension_t *p_ext )
    {
        name = qfu( p_ext->psz_name );
        description = qfu( p_ext->psz_description );
        shortdesc = qfu( p_ext->psz_shortdescription );
        if( description.isEmpty() )
            description = shortdesc;
        if( shortdesc.isEmpty() && !description.isEmpty() )
            shortdesc = description;
        title = qfu( p_ext->psz_title );
        author = qfu( p_ext->psz_author );
        version = qfu( p_ext->psz_version );
        url = qfu( p_ext->psz_url );
    }
    ~ExtensionCopy() {}

    QString name, title, description, shortdesc, author, version, url;
};

/* Extensions list model for the QListView */

ExtensionListModel::ExtensionListModel( QListView *view, intf_thread_t *intf )
        : QAbstractListModel( view ), p_intf( intf )
{
    // Connect to ExtensionsManager::extensionsUpdated()
    ExtensionsManager* EM = ExtensionsManager::getInstance( p_intf );
    CONNECT( EM, extensionsUpdated(), this, updateList() );

    // Load extensions now if not already loaded
    EM->loadExtensions();
}

ExtensionListModel::~ExtensionListModel()
{
    // Clear extensions list
    while( !extensions.isEmpty() )
        delete extensions.takeLast();
}

void ExtensionListModel::updateList()
{
    ExtensionCopy *ext;

    // Clear extensions list
    while( !extensions.isEmpty() )
    {
        ext = extensions.takeLast();
        delete ext;
    }

    // Find new extensions
    ExtensionsManager *EM = ExtensionsManager::getInstance( p_intf );
    extensions_manager_t *p_mgr = EM->getManager();
    if( !p_mgr )
        return;

    vlc_mutex_lock( &p_mgr->lock );
    extension_t *p_ext;
    FOREACH_ARRAY( p_ext, p_mgr->extensions )
    {
        ext = new ExtensionCopy( p_ext );
        extensions.push_back( ext );
    }
    FOREACH_END()
    vlc_mutex_unlock( &p_mgr->lock );
    vlc_object_release( p_mgr );

    emit dataChanged( index( 0 ), index( rowCount() - 1 ) );
}

int ExtensionListModel::rowCount( const QModelIndex& parent ) const
{
    int count = 0;
    ExtensionsManager *EM = ExtensionsManager::getInstance( p_intf );
    extensions_manager_t *p_mgr = EM->getManager();
    if( !p_mgr )
        return 0;

    vlc_mutex_lock( &p_mgr->lock );
    count = p_mgr->extensions.i_size;
    vlc_mutex_unlock( &p_mgr->lock );
    vlc_object_release( p_mgr );

    return count;
}

QVariant ExtensionListModel::data( const QModelIndex& index, int role ) const
{
    if( !index.isValid() )
        return QVariant();

    switch( role )
    {
    default:
        return QVariant();
    }
}

QModelIndex ExtensionListModel::index( int row, int column,
                                       const QModelIndex& parent ) const
{
    if( column != 0 )
        return QModelIndex();
    if( row < 0 || row >= extensions.size() )
        return QModelIndex();

    return createIndex( row, 0, extensions.at( row ) );
}


/* Extension List Widget Item */
ExtensionItemDelegate::ExtensionItemDelegate( intf_thread_t *p_intf,
                                              QListView *view )
        : QStyledItemDelegate( view ), view( view ), p_intf( p_intf )
{
}

ExtensionItemDelegate::~ExtensionItemDelegate()
{
}

void ExtensionItemDelegate::paint( QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index ) const
{
    ExtensionCopy *ext = ( ExtensionCopy* ) index.internalPointer();
    assert( ext != NULL );

    int width = option.rect.width();
    int height = option.rect.height();

    // Pixmap: buffer where to draw
    QPixmap pix(option.rect.size());

    // Draw background
    pix.fill( Qt::transparent ); // FIXME

    // ItemView primitive style
    QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem,
                                          &option,
                                          painter );

    // Painter on the pixmap
    QPainter *pixpaint = new QPainter(&pix);

    // Text font & pen
    QFont font = painter->font();
    QPen pen = painter->pen();
    if( view->selectionModel()->selectedIndexes().contains( index ) )
    {
        pen.setBrush( option.palette.highlightedText() );
    }
    else
    {
        pen.setBrush( option.palette.text() );
    }
    pixpaint->setPen( pen );
    QFontMetrics metrics = option.fontMetrics;

    /// @todo Add extension's icon

    // Title: bold
    font.setBold( true );
    pixpaint->setFont( font );
    pixpaint->drawText( QRect( 10, 7, width - 70, metrics.height() ),
                        Qt::AlignLeft, ext->title );

    // Short description: normal
    font.setBold( false );
    pixpaint->setFont( font );
    pixpaint->drawText( QRect( 10, 7 + metrics.height(), width - 40,
                               metrics.height() ),
                        Qt::AlignLeft, ext->shortdesc );

    // Version: italic
    font.setItalic( true );
    pixpaint->setFont( font );
    pixpaint->drawText( width - 40, 7 + metrics.height(), ext->version );

    // Flush paint operations
    delete pixpaint;

    // Draw it on the screen!
    painter->drawPixmap( option.rect, pix );
}

QSize ExtensionItemDelegate::sizeHint( const QStyleOptionViewItem &option,
                                       const QModelIndex &index ) const
{
    if (index.isValid() && index.column() == 0)
    {
        QFontMetrics metrics = option.fontMetrics;
        return QSize( 200, 14 + 2 * metrics.height() );
    }
    else
        return QSize();
}

/* "More information" dialog */

ExtensionInfoDialog::ExtensionInfoDialog( const ExtensionCopy& extension,
                                          intf_thread_t *p_intf,
                                          QWidget *parent )
       : QVLCDialog( parent, p_intf ),
         extension( new ExtensionCopy( extension ) )
{
    // Let's be a modal dialog
    setWindowModality( Qt::WindowModal );

    // Window title
    setWindowTitle( qtr( "About" ) + " " + extension.title );

    // Layout
    QGridLayout *layout = new QGridLayout( this );

    // Icon
    /// @todo Use the extension's icon, when extensions will support icons :)
    QLabel *icon = new QLabel( this );
    QPixmap pix( ":/logo/vlc48.png" );
    icon->setPixmap( pix );
    layout->addWidget( icon, 1, 0, 2, 1 );

    // Title
    QLabel *label = new QLabel( extension.title, this );
    QFont font = label->font();
    font.setBold( true );
    font.setPointSizeF( font.pointSizeF() * 1.3f );
    label->setFont( font );
    layout->addWidget( label, 0, 0, 1, -1 );

    // Version
    label = new QLabel( "<b>" + qtr( "Version" ) + ":</b>", this );
    layout->addWidget( label, 1, 1, 1, 1, Qt::AlignBottom );
    label = new QLabel( extension.version, this );
    layout->addWidget( label, 1, 2, 1, 2, Qt::AlignBottom );

    // Author
    label = new QLabel( "<b>" + qtr( "Author" ) + ":</b>", this );
    layout->addWidget( label, 2, 1, 1, 1, Qt::AlignTop );
    label = new QLabel( extension.author, this );
    layout->addWidget( label, 2, 2, 1, 2, Qt::AlignTop );


    // Description
    label = new QLabel( this );
    label->setText( extension.description );
    label->setWordWrap( true );
    label->setOpenExternalLinks( true );
    layout->addWidget( label, 4, 0, 1, -1 );

    // URL
    label = new QLabel( "<b>" + qtr( "Website" ) + ":</b>", this );
    layout->addWidget( label, 5, 0, 1, 2 );
    QString txt = "<a href=\"";
    txt += extension.url;
    txt += "\">";
    txt += extension.url;
    txt += "</a>";
    label = new QLabel( txt, this );
    label->setText( txt );
    label->setOpenExternalLinks( true );
    layout->addWidget( label, 5, 2, 1, -1 );

    // Script file
    label = new QLabel( "<b>" + qtr( "File" ) + ":</b>", this );
    layout->addWidget( label, 6, 0, 1, 2 );
    QLineEdit *line = new QLineEdit( extension.name, this );
    layout->addWidget( line, 6, 2, 1, -1 );

    // Close button
    QDialogButtonBox *group = new QDialogButtonBox( this );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    group->addButton( closeButton, QDialogButtonBox::AcceptRole );
    BUTTONACT( closeButton, close() );

    layout->addWidget( group, 7, 0, 1, -1 );

    // Fix layout
    layout->setColumnStretch( 2, 1 );
    layout->setRowStretch( 4, 1 );
    setMinimumSize( 450, 350 );
}

ExtensionInfoDialog::~ExtensionInfoDialog()
{
    delete extension;
}
