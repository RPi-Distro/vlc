/*****************************************************************************
 * preferences_widgets.cpp : Widgets for preferences displays
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id: c311afc3bef20994e52d8fc14e5144a83dc96e05 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

/**
 * Todo:
 *  - i_action handler for IntegerLists, but I don't see any module using it...
 *  - Validator for modulelist
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/preferences_widgets.hpp"
#include "util/customwidgets.hpp"
#include "util/searchlineedit.hpp"
#include "util/qt_dirs.hpp"
#include <vlc_keys.h>
#include <vlc_intf_strings.h>
#include <vlc_modules.h>

#include <QString>
#include <QVariant>
#include <QGridLayout>
#include <QSlider>
#include <QFileDialog>
#include <QGroupBox>
#include <QTreeWidgetItem>
#include <QSignalMapper>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QColorDialog>

#define MINWIDTH_BOX 90
#define LAST_COLUMN 10

QString formatTooltip(const QString & tooltip)
{
    QString text = tooltip;
    text.replace("\n", "<br/>");

    QString formatted =
    "<html><head><meta name=\"qrichtext\" content=\"1\" />"
    "<style type=\"text/css\"> p, li { white-space: pre-wrap; } </style></head>"
    "<body style=\" font-family:'Sans Serif'; "
    "font-style:normal; text-decoration:none;\">"
    "<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; "
    "margin-right:0px; -qt-block-indent:0; text-indent:0px;\">" +
    text + "</p></body></html>";
    return formatted;
}

ConfigControl *ConfigControl::createControl( vlc_object_t *p_this,
                                             module_config_t *p_item,
                                             QWidget *parent,
                                             QGridLayout *l, int line )
{
    ConfigControl *p_control = NULL;

    switch( p_item->i_type )
    {
    case CONFIG_ITEM_MODULE:
        p_control = new ModuleConfigControl( p_this, p_item, parent, false,
                                             l, line );
        break;
    case CONFIG_ITEM_MODULE_CAT:
        p_control = new ModuleConfigControl( p_this, p_item, parent, true,
                                             l, line );
        break;
    case CONFIG_ITEM_MODULE_LIST:
        p_control = new ModuleListConfigControl( p_this, p_item, parent, false,
                                             l, line );
        break;
    case CONFIG_ITEM_MODULE_LIST_CAT:
        p_control = new ModuleListConfigControl( p_this, p_item, parent, true,
                                             l, line );
        break;
    case CONFIG_ITEM_STRING:
        if( !p_item->i_list )
            p_control = new StringConfigControl( p_this, p_item, parent,
                                                 l, line, false );
        else
            p_control = new StringListConfigControl( p_this, p_item,
                                            parent, l, line );
        break;
    case CONFIG_ITEM_PASSWORD:
        if( !p_item->i_list )
            p_control = new StringConfigControl( p_this, p_item, parent,
                                                 l, line, true );
        else
            p_control = new StringListConfigControl( p_this, p_item,
                                            parent, l, line );
        break;
    case CONFIG_ITEM_RGB:
        p_control = new ColorConfigControl( p_this, p_item, parent, l, line );
        break;
    case CONFIG_ITEM_INTEGER:
        if( p_item->i_list )
            p_control = new IntegerListConfigControl( p_this, p_item,
                                            parent, false, l, line );
        else if( p_item->min.i || p_item->max.i )
            p_control = new IntegerRangeConfigControl( p_this, p_item, parent,
                                                       l, line );
        else
            p_control = new IntegerConfigControl( p_this, p_item, parent,
                                                  l, line );
        break;
    case CONFIG_ITEM_LOADFILE:
    case CONFIG_ITEM_SAVEFILE:
        p_control = new FileConfigControl( p_this, p_item, parent, l, line);
        break;
    case CONFIG_ITEM_DIRECTORY:
        p_control = new DirectoryConfigControl( p_this, p_item, parent, l,
                                                line );
        break;
    case CONFIG_ITEM_FONT:
        p_control = new FontConfigControl( p_this, p_item, parent, l,
                                           line);
        break;
    case CONFIG_ITEM_KEY:
        p_control = new KeySelectorControl( p_this, p_item, parent, l, line );
        break;
    case CONFIG_ITEM_BOOL:
        p_control = new BoolConfigControl( p_this, p_item, parent, l, line );
        break;
    case CONFIG_ITEM_FLOAT:
        if( p_item->min.f || p_item->max.f )
            p_control = new FloatRangeConfigControl( p_this, p_item, parent,
                                                     l, line );
        else
            p_control = new FloatConfigControl( p_this, p_item, parent,
                                                  l, line );
        break;
    default:
        break;
    }
    return p_control;
}

/*******************************************************
 * Simple widgets
 *******************************************************/
InterfacePreviewWidget::InterfacePreviewWidget ( QWidget *parent ) : QLabel( parent )
{
    setGeometry( 0, 0, 128, 100 );
    setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
}

void InterfacePreviewWidget::setNormalPreview( bool b_minimal )
{
    setPreview( ( b_minimal ) ? MINIMAL : COMPLETE );
}

void InterfacePreviewWidget::setPreview( enum_style e_style )
{
    QString pixmapLocationString(":/prefsmenu/");

    switch( e_style )
    {
    default:
    case COMPLETE:
        pixmapLocationString += "sample_complete";
        break;
    case MINIMAL:
        pixmapLocationString += "sample_minimal";
        break;
    case SKINS:
        pixmapLocationString += "sample_skins";
        break;
    }

    setPixmap( QPixmap( pixmapLocationString ) );
    update();
}


/**************************************************************************
 * String-based controls
 *************************************************************************/

void
VStringConfigControl::doApply()
{
    config_PutPsz( p_this, getName(), qtu( getValue() ) );
}

/*********** String **************/
StringConfigControl::StringConfigControl( vlc_object_t *_p_this,
                                          module_config_t *_p_item,
                                          QWidget *_parent, QGridLayout *l,
                                          int line, bool pwd ) :
                           VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    text = new QLineEdit( qfu(p_item->value.psz) );
    if( pwd ) text->setEchoMode( QLineEdit::Password );
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 ); layout->insertSpacing( 1, 10 );
        layout->addWidget( text, LAST_COLUMN );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->setColumnMinimumWidth( 1, 10 );
        l->addWidget( text, line, LAST_COLUMN );
    }
}

StringConfigControl::StringConfigControl( vlc_object_t *_p_this,
                                   module_config_t *_p_item,
                                   QLabel *_label, QLineEdit *_text, bool pwd ):
                           VStringConfigControl( _p_this, _p_item )
{
    text = _text;
    if( pwd ) text->setEchoMode( QLineEdit::Password );
    label = _label;
    finish( );
}

void StringConfigControl::finish()
{
    text->setText( qfu(p_item->value.psz) );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        text->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( text );
}

/*********** File **************/
FileConfigControl::FileConfigControl( vlc_object_t *_p_this,
                                          module_config_t *_p_item,
                                          QWidget *_parent, QGridLayout *l,
                                          int line ) :
                           VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    text = new QLineEdit( qfu(p_item->value.psz) );
    browse = new QPushButton( qtr( "Browse..." ) );
    QHBoxLayout *textAndButton = new QHBoxLayout();
    textAndButton->setMargin( 0 );
    textAndButton->addWidget( text, 2 );
    textAndButton->addWidget( browse, 0 );

    BUTTONACT( browse, updateField() );

    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 );
        layout->insertSpacing( 1, 10 );
        layout->addLayout( textAndButton, LAST_COLUMN );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->setColumnMinimumWidth( 1, 10 );
        l->addLayout( textAndButton, line, LAST_COLUMN );
    }
}


FileConfigControl::FileConfigControl( vlc_object_t *_p_this,
                                   module_config_t *_p_item,
                                   QLabel *_label, QLineEdit *_text,
                                   QPushButton *_button ):
                           VStringConfigControl( _p_this, _p_item )
{
    browse = _button;
    text = _text;
    label = _label;

    BUTTONACT( browse, updateField() );

    finish( );
}

void FileConfigControl::updateField()
{
    QString file;

    if (p_item->i_type == CONFIG_ITEM_SAVEFILE)
        file = QFileDialog::getSaveFileName( NULL, qtr( "Save File" ),
                                             QVLCUserDir( VLC_HOME_DIR ) );
    else
        file = QFileDialog::getOpenFileName( NULL, qtr( "Select File" ),
                                             QVLCUserDir( VLC_HOME_DIR ) );

    if( file.isNull() ) return;
    text->setText( toNativeSeparators( file ) );
}

void FileConfigControl::finish()
{
    text->setText( qfu(p_item->value.psz) );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        text->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( text );
}

/********* String / Directory **********/
DirectoryConfigControl::DirectoryConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QWidget *_p_widget,
                        QGridLayout *_p_layout, int _int ) :
     FileConfigControl( _p_this, _p_item, _p_widget, _p_layout, _int )
{}

DirectoryConfigControl::DirectoryConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QLabel *_p_label,
                        QLineEdit *_p_line, QPushButton *_p_button ):
     FileConfigControl( _p_this, _p_item, _p_label, _p_line, _p_button)
{}

void DirectoryConfigControl::updateField()
{
    QString dir = QFileDialog::getExistingDirectory( NULL,
                      qtr( I_OP_SEL_DIR ),
                      text->text().isEmpty() ?
                        QVLCUserDir( VLC_HOME_DIR ) : text->text(),
                  QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );

    if( dir.isNull() ) return;
    text->setText( toNativeSepNoSlash( dir ) );
}

/********* String / Font **********/
FontConfigControl::FontConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QWidget *_parent,
                        QGridLayout *_p_layout, int line) :
     VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    font = new QFontComboBox( _parent );
    font->setCurrentFont( QFont( qfu( p_item->value.psz) ) );
    if( !_p_layout )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 );
        layout->addWidget( font, 1 );
        widget->setLayout( layout );
    }
    else
    {
        _p_layout->addWidget( label, line, 0 );
        _p_layout->addWidget( font, line, 1, 1, -1 );
    }

    if( p_item->psz_longtext )
    {
        label->setToolTip( formatTooltip( qtr(p_item->psz_longtext) ) );
    }
}

FontConfigControl::FontConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QLabel *_p_label,
                        QFontComboBox *_p_font):
     VStringConfigControl( _p_this, _p_item)
{
    label = _p_label;
    font = _p_font;
    font->setCurrentFont( QFont( qfu( p_item->value.psz) ) );

    if( p_item->psz_longtext )
    {
        label->setToolTip( formatTooltip( qtr(p_item->psz_longtext) ) );
    }
}

/********* String / choice list **********/
StringListConfigControl::StringListConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent,
               QGridLayout *l, int line) :
               VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    combo = new QComboBox();
    combo->setMinimumWidth( MINWIDTH_BOX );
    combo->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Preferred );

    module_config_t *p_module_config = config_FindConfig( p_this, p_item->psz_name );

    finish( p_module_config );
    if( !l )
    {
        l = new QGridLayout();
        l->addWidget( label, 0, 0 ); l->addWidget( combo, 0, LAST_COLUMN );
        widget->setLayout( l );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( combo, line, LAST_COLUMN, Qt::AlignRight );
    }

    if( p_item->i_action )
    {
        QSignalMapper *signalMapper = new QSignalMapper(this);

        /* Some stringLists like Capture listings have action associated */
        for( int i = 0; i < p_item->i_action; i++ )
        {
            QPushButton *button =
                new QPushButton( qtr( p_item->ppsz_action_text[i] ));
            CONNECT( button, clicked(), signalMapper, map() );
            signalMapper->setMapping( button, i );
            l->addWidget( button, line, LAST_COLUMN - p_item->i_action + i,
                    Qt::AlignRight );
        }
        CONNECT( signalMapper, mapped( int ),
                this, actionRequested( int ) );
    }
}

void StringListConfigControl::actionRequested( int i_action )
{
    /* Supplementary check for boundaries */
    if( i_action < 0 || i_action >= p_item->i_action ) return;

    module_config_t *p_module_config = config_FindConfig( p_this, getName() );
    if(!p_module_config) return;

    vlc_value_t val;
    val.psz_string = const_cast<char *>
        qtu( (combo->itemData( combo->currentIndex() ).toString() ) );

    p_module_config->ppf_action[i_action]( p_this, getName(), val, val, 0 );

    if( p_module_config->b_dirty )
    {
        combo->clear();
        finish( p_module_config );
        p_module_config->b_dirty = false;
    }
}
StringListConfigControl::StringListConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool ) : VStringConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;

    module_config_t *p_module_config = config_FindConfig( p_this, getName() );

    finish( p_module_config );
}

void StringListConfigControl::finish(module_config_t *p_module_config )
{
    combo->setEditable( false );

    if(!p_module_config) return;

    if( p_module_config->pf_update_list )
    {
        vlc_value_t val;
        val.psz_string = strdup(p_module_config->value.psz);

        p_module_config->pf_update_list(p_this, p_item->psz_name, val, val, NULL);

        // assume in any case that dirty was set to true
        // because lazy programmes will use the same callback for
        // this, like the one behind the refresh push button?
        p_module_config->b_dirty = false;

        free( val.psz_string );
    }

    for( int i_index = 0; i_index < p_module_config->i_list; i_index++ )
    {
        if( !p_module_config->ppsz_list[i_index] )
        {
              combo->addItem( "", QVariant(""));
              if( !p_item->value.psz )
                 combo->setCurrentIndex( combo->count() - 1 );
              continue;
        }
        combo->addItem( qfu((p_module_config->ppsz_list_text &&
                            p_module_config->ppsz_list_text[i_index])?
                            _(p_module_config->ppsz_list_text[i_index]) :
                            p_module_config->ppsz_list[i_index] ),
                   QVariant( qfu(p_module_config->ppsz_list[i_index] )) );
        if( p_item->value.psz && !strcmp( p_module_config->value.psz,
                                          p_module_config->ppsz_list[i_index] ) )
            combo->setCurrentIndex( combo->count() - 1 );
    }

    if( p_module_config->psz_longtext  )
    {
        QString tipText = qtr(p_module_config->psz_longtext);
        combo->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( combo );
}

QString StringListConfigControl::getValue() const
{
    return combo->itemData( combo->currentIndex() ).toString();
}

void setfillVLCConfigCombo( const char *configname, intf_thread_t *p_intf,
                            QComboBox *combo )
{
    module_config_t *p_config =
                      config_FindConfig( VLC_OBJECT(p_intf), configname );
    if( p_config )
    {
        QVariant def;
        bool string = (p_config->i_type & 0xF0) == CONFIG_ITEM_STRING;

        if( string )
            def = QVariant( qfu(p_config->value.psz) );
        else
            def = QVariant( qlonglong( p_config->value.i ) );

        if(p_config->pf_update_list)
        {
            vlc_value_t val;
            val.i_int = p_config->value.i;
            p_config->pf_update_list(VLC_OBJECT(p_intf), configname, val, val, NULL);
            // assume in any case that dirty was set to true
            // because lazy programmes will use the same callback for
            // this, like the one behind the refresh push button?
            p_config->b_dirty = false;
        }

        for ( int i_index = 0; i_index < p_config->i_list; i_index++ )
        {
            QVariant value;

            if( string )
                value = QVariant( qfu(p_config->ppsz_list[i_index]) );
            else
                value =QVariant( p_config->pi_list[i_index] );
            combo->addItem( qtr(p_config->ppsz_list_text[i_index]), value );
            if( def == value )
                combo->setCurrentIndex( i_index );
        }

        if( p_config->psz_longtext )
            combo->setToolTip( qfu( p_config->psz_longtext ) );
    }
}

/********* Module **********/
ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent, bool bycat,
               QGridLayout *l, int line) :
               VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    combo = new QComboBox();
    combo->setMinimumWidth( MINWIDTH_BOX );
    finish( bycat );
    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label ); layout->addWidget( combo, LAST_COLUMN );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( combo, line, LAST_COLUMN, Qt::AlignRight );
    }
}

ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool bycat ) : VStringConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;
    finish( bycat );
}

void ModuleConfigControl::finish( bool bycat )
{
    module_t *p_parser;

    combo->setEditable( false );

    /* build a list of available modules */
    module_t **p_list = module_list_get( NULL );
    combo->addItem( qtr("Default") );
    for( size_t i = 0; (p_parser = p_list[i]) != NULL; i++ )
    {
        if( bycat )
        {
            if( !strcmp( module_get_object( p_parser ), "main" ) ) continue;

            unsigned confsize;
            module_config_t *p_config;

            p_config = module_config_get (p_parser, &confsize);
            for (size_t i = 0; i < confsize; i++)
            {
                /* Hack: required subcategory is stored in i_min */
                const module_config_t *p_cfg = p_config + i;
                if( p_cfg->i_type == CONFIG_SUBCATEGORY &&
                    p_cfg->value.i == p_item->min.i )
                    combo->addItem( qtr( module_GetLongName( p_parser )),
                                    QVariant( module_get_object( p_parser ) ) );
                if( p_item->value.psz && !strcmp( p_item->value.psz,
                                                  module_get_object( p_parser ) ) )
                    combo->setCurrentIndex( combo->count() - 1 );
            }
            module_config_free (p_config);
        }
        else if( module_provides( p_parser, p_item->psz_type ) )
        {
            combo->addItem( qtr(module_GetLongName( p_parser ) ),
                            QVariant( module_get_object( p_parser ) ) );
            if( p_item->value.psz && !strcmp( p_item->value.psz,
                                              module_get_object( p_parser ) ) )
                combo->setCurrentIndex( combo->count() - 1 );
        }
    }
    module_list_free( p_list );

    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        combo->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( combo );
}

QString ModuleConfigControl::getValue() const
{
    return combo->itemData( combo->currentIndex() ).toString();
}

/********* Module list **********/
ModuleListConfigControl::ModuleListConfigControl( vlc_object_t *_p_this,
        module_config_t *_p_item, QWidget *_parent, bool bycat,
        QGridLayout *l, int line) :
    VStringConfigControl( _p_this, _p_item, _parent )
{
    groupBox = NULL;

    /* Special Hack */
    if( !p_item->psz_text ) return;

    groupBox = new QGroupBox ( qtr(p_item->psz_text), _parent );
    text = new QLineEdit;
    QGridLayout *layoutGroupBox = new QGridLayout( groupBox );

    finish( bycat );

    int boxline = 0;
    foreach ( checkBoxListItem *it, modules )
    {
        layoutGroupBox->addWidget( it->checkBox, boxline / 2, boxline % 2 );
        boxline++;
    }

    layoutGroupBox->addWidget( text, boxline, 0, 1, 2 );

    if( !l )
    {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget( groupBox, line, 0 );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( groupBox, line, 0, 1, -1 );
    }

    if( p_item->psz_longtext )
        text->setToolTip( formatTooltip( qtr( p_item->psz_longtext) ) );
}

ModuleListConfigControl::~ModuleListConfigControl()
{
    qDeleteAll( modules );
    modules.clear();
    delete groupBox;
}

void ModuleListConfigControl::checkbox_lists( module_t *p_parser )
{
    const char *help = module_get_help( p_parser );
    checkbox_lists( qtr( module_GetLongName( p_parser ) ),
                    help != NULL ? qtr( help ): "",
                    module_get_object( p_parser ) );
}

void ModuleListConfigControl::checkbox_lists( QString label, QString help, const char* psz_module )
{
    QCheckBox *cb = new QCheckBox( label );
    checkBoxListItem *cbl = new checkBoxListItem;

    CONNECT( cb, stateChanged( int ), this, onUpdate() );
    if( !help.isEmpty() )
        cb->setToolTip( formatTooltip( help ) );
    cbl->checkBox = cb;

    cbl->psz_module = strdup( psz_module );
    modules.append( cbl );

    if( p_item->value.psz && strstr( p_item->value.psz, cbl->psz_module ) )
        cbl->checkBox->setChecked( true );
}

void ModuleListConfigControl::finish( bool bycat )
{
    module_t *p_parser;

    /* build a list of available modules */
    module_t **p_list = module_list_get( NULL );
    for( size_t i = 0; (p_parser = p_list[i]) != NULL; i++ )
    {
        if( bycat )
        {
            if( !strcmp( module_get_object( p_parser ), "main" ) ) continue;

            unsigned confsize;
            module_config_t *p_config = module_config_get (p_parser, &confsize);

            for (size_t i = 0; i < confsize; i++)
            {
                module_config_t *p_cfg = p_config + i;
                /* Hack: required subcategory is stored in i_min */
                if( p_cfg->i_type == CONFIG_SUBCATEGORY &&
                        p_cfg->value.i == p_item->min.i )
                {
                    checkbox_lists( p_parser );
                }

                /* Parental Advisory HACK:
                 * Selecting HTTP, RC and Telnet interfaces is difficult now
                 * since they are just the lua interface module */
                if( p_cfg->i_type == CONFIG_SUBCATEGORY &&
                    !strcmp( module_get_object( p_parser ), "lua" ) &&
                    !strcmp( p_item->psz_name, "extraintf" ) &&
                    p_cfg->value.i == p_item->min.i )
                {
                    checkbox_lists( "Web", "Lua HTTP", "http" );
                    checkbox_lists( "Telnet", "Lua Telnet", "telnet" );
#ifndef WIN32
                    checkbox_lists( "Console", "Lua CLI", "cli" );
#endif
                }
            }
            module_config_free (p_config);
        }
        else if( module_provides( p_parser, p_item->psz_type ) )
        {
            checkbox_lists(p_parser);
        }
    }
    module_list_free( p_list );

    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);

        text->setToolTip( formatTooltip(tipText) );
        assert( groupBox );
        groupBox->setToolTip( formatTooltip(tipText) );
   }
}

QString ModuleListConfigControl::getValue() const
{
    assert( text );
    return text->text();
}

void ModuleListConfigControl::hide()
{
    foreach ( checkBoxListItem *it, modules )
        it->checkBox->hide();
    groupBox->hide();
}

void ModuleListConfigControl::show()
{
    foreach ( checkBoxListItem *it, modules )
        it->checkBox->show();
    groupBox->show();
}


void ModuleListConfigControl::onUpdate()
{
    text->clear();
    bool first = true;

    foreach ( checkBoxListItem *it, modules )
    {
        if( it->checkBox->isChecked() )
        {
            if( first )
            {
                text->setText( text->text() + it->psz_module );
                first = false;
            }
            else
            {
                text->setText( text->text() + ":" + it->psz_module );
            }
        }
    }
}

/**************************************************************************
 * Integer-based controls
 *************************************************************************/

void
VIntConfigControl::doApply()
{
    config_PutInt( p_this, getName(), getValue() );
}

/*********** Integer **************/
IntegerConfigControl::IntegerConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QWidget *_parent, QGridLayout *l,
                                            int line ) :
                           VIntConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    spin = new QSpinBox; spin->setMinimumWidth( MINWIDTH_BOX );
    spin->setAlignment( Qt::AlignRight );
    spin->setMaximumWidth( MINWIDTH_BOX );
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 ); layout->addWidget( spin, LAST_COLUMN );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( spin, line, LAST_COLUMN, Qt::AlignRight );
    }
}
IntegerConfigControl::IntegerConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QSpinBox *_spin ) :
                                      VIntConfigControl( _p_this, _p_item )
{
    spin = _spin;
    label = _label;
    finish();
}

void IntegerConfigControl::finish()
{
    spin->setMaximum( 2000000000 );
    spin->setMinimum( -2000000000 );
    spin->setValue( p_item->value.i );

    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        spin->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( spin );
}

int IntegerConfigControl::getValue() const
{
    return spin->value();
}

/********* Integer range **********/
IntegerRangeConfigControl::IntegerRangeConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QWidget *_parent, QGridLayout *l,
                                            int line ) :
            IntegerConfigControl( _p_this, _p_item, _parent, l, line )
{
    finish();
}

IntegerRangeConfigControl::IntegerRangeConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QSpinBox *_spin ) :
            IntegerConfigControl( _p_this, _p_item, _label, _spin )
{
    finish();
}

void IntegerRangeConfigControl::finish()
{
    spin->setMaximum( p_item->max.i > INT_MAX ? INT_MAX : p_item->max.i );
    spin->setMinimum( p_item->min.i < INT_MIN ? INT_MIN : p_item->min.i );
}

IntegerRangeSliderConfigControl::IntegerRangeSliderConfigControl(
                                            vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QSlider *_slider ):
                    VIntConfigControl( _p_this, _p_item )
{
    slider = _slider;
    label = _label;
    slider->setMaximum( p_item->max.i > INT_MAX ? INT_MAX : p_item->max.i );
    slider->setMinimum( p_item->min.i < INT_MIN ? INT_MIN : p_item->min.i );
    slider->setValue( p_item->value.i );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        slider->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( slider );
}

int IntegerRangeSliderConfigControl::getValue() const
{
        return slider->value();
}


/********* Integer / choice list **********/
IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent, bool,
               QGridLayout *l, int line) :
               VIntConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    combo = new QComboBox();
    combo->setMinimumWidth( MINWIDTH_BOX );

    module_config_t *p_module_config = config_FindConfig( p_this, p_item->psz_name );

    finish( p_module_config );
    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label ); layout->addWidget( combo, LAST_COLUMN );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( combo, line, LAST_COLUMN, Qt::AlignRight );
    }

    if( p_item->i_action )
    {
        QSignalMapper *signalMapper = new QSignalMapper(this);

        /* Some stringLists like Capture listings have action associated */
        for( int i = 0; i < p_item->i_action; i++ )
        {
            QPushButton *button =
                new QPushButton( qfu( p_item->ppsz_action_text[i] ));
            CONNECT( button, clicked(), signalMapper, map() );
            signalMapper->setMapping( button, i );
            l->addWidget( button, line, LAST_COLUMN - p_item->i_action + i,
                    Qt::AlignRight );
        }
        CONNECT( signalMapper, mapped( int ),
                this, actionRequested( int ) );
    }

}
IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool ) : VIntConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;

    module_config_t *p_module_config = config_FindConfig( p_this, getName() );

    finish( p_module_config );
}

void IntegerListConfigControl::finish(module_config_t *p_module_config )
{
    combo->setEditable( false );

    if(!p_module_config) return;

    if( p_module_config->pf_update_list )
    {
       vlc_value_t val;
       val.i_int = p_module_config->value.i;

       p_module_config->pf_update_list(p_this, p_item->psz_name, val, val, NULL);

       // assume in any case that dirty was set to true
       // because lazy programmes will use the same callback for
       // this, like the one behind the refresh push button?
       p_module_config->b_dirty = false;
    }

    for( int i_index = 0; i_index < p_module_config->i_list; i_index++ )
    {
        combo->addItem( qtr(p_module_config->ppsz_list_text[i_index] ),
                        QVariant( p_module_config->pi_list[i_index] ) );
        if( p_module_config->value.i == p_module_config->pi_list[i_index] )
            combo->setCurrentIndex( combo->count() - 1 );
    }
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext );
        combo->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( combo );
}

void IntegerListConfigControl::actionRequested( int i_action )
{
    /* Supplementary check for boundaries */
    if( i_action < 0 || i_action >= p_item->i_action ) return;

    module_config_t *p_module_config = config_FindConfig( p_this, getName() );
    if(!p_module_config) return;


    vlc_value_t val;
    val.i_int = combo->itemData( combo->currentIndex() ).toInt();

    p_module_config->ppf_action[i_action]( p_this, getName(), val, val, 0 );

    if( p_module_config->b_dirty )
    {
        combo->clear();
        finish( p_module_config );
        p_module_config->b_dirty = false;
    }
}

int IntegerListConfigControl::getValue() const
{
    return combo->itemData( combo->currentIndex() ).toInt();
}

/*********** Boolean **************/
BoolConfigControl::BoolConfigControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item,
                                      QWidget *_parent, QGridLayout *l,
                                      int line ) :
                    VIntConfigControl( _p_this, _p_item, _parent )
{
    checkbox = new QCheckBox( qtr(p_item->psz_text) );
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( checkbox, 0 );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( checkbox, line, 0 );
    }
}

BoolConfigControl::BoolConfigControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item,
                                      QLabel *_label,
                                      QAbstractButton *_checkbox ) :
                   VIntConfigControl( _p_this, _p_item )
{
    checkbox = _checkbox;
    VLC_UNUSED( _label );
    finish();
}

void BoolConfigControl::finish()
{
    checkbox->setChecked( p_item->value.i );
    if( p_item->psz_longtext )
        checkbox->setToolTip( formatTooltip(qtr(p_item->psz_longtext)) );
}

int BoolConfigControl::getValue() const
{
    return checkbox->isChecked();
}

/************* Color *************/
ColorConfigControl::ColorConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QWidget *_parent, QGridLayout *l,
                                            int line ) :
                           VIntConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel;
    color_but = new QToolButton;
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 ); layout->addWidget( color_but, LAST_COLUMN );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( color_but, line, LAST_COLUMN, Qt::AlignRight );
    }
}

ColorConfigControl::ColorConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QAbstractButton *_color ):
                                      VIntConfigControl( _p_this, _p_item )
{
    label = _label;
    color_but = _color;
    finish();
}

void ColorConfigControl::finish()
{
    i_color = p_item->value.i;

    color_px = new QPixmap( 34, 20 );
    color_px->fill( QColor( i_color ) );
    color_but->setIcon( QIcon( *color_px ) );
    color_but->setMinimumWidth( 40 );

    label->setText( qtr(p_item->psz_text) );
    if( p_item->psz_longtext )
    {
        label->setToolTip( formatTooltip(qtr(p_item->psz_longtext)) );
        color_but->setToolTip( formatTooltip(qtr(p_item->psz_longtext)) );
    }

    BUTTONACT( color_but, selectColor() );
}

int ColorConfigControl::getValue() const
{
    return i_color;
}

void ColorConfigControl::selectColor()
{
    QColor color = QColorDialog::getColor( QColor( i_color ) );
    if( color.isValid() )
    {
        i_color = (color.red() << 16) + (color.green() << 8) + color.blue();

        color_px->fill( QColor( i_color ) );
        color_but->setIcon( QIcon( *color_px ) );
    }
}


/**************************************************************************
 * Float-based controls
 *************************************************************************/

void
VFloatConfigControl::doApply()
{
    config_PutFloat( p_this, getName(), getValue() );
}

/*********** Float **************/
FloatConfigControl::FloatConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QWidget *_parent, QGridLayout *l,
                                        int line ) :
                    VFloatConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qtr(p_item->psz_text) );
    spin = new QDoubleSpinBox;
    spin->setMinimumWidth( MINWIDTH_BOX );
    spin->setMaximumWidth( MINWIDTH_BOX );
    spin->setAlignment( Qt::AlignRight );
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 ); layout->addWidget( spin, LAST_COLUMN );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( spin, line, LAST_COLUMN, Qt::AlignRight );
    }
}

FloatConfigControl::FloatConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QLabel *_label,
                                        QDoubleSpinBox *_spin ) :
                    VFloatConfigControl( _p_this, _p_item )
{
    spin = _spin;
    label = _label;
    finish();
}

void FloatConfigControl::finish()
{
    spin->setMaximum( 2000000000. );
    spin->setMinimum( -2000000000. );
    spin->setSingleStep( 0.1 );
    spin->setValue( (double)p_item->value.f );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        spin->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( spin );
}

float FloatConfigControl::getValue() const
{
    return (float)spin->value();
}

/*********** Float with range **************/
FloatRangeConfigControl::FloatRangeConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QWidget *_parent, QGridLayout *l,
                                        int line ) :
                FloatConfigControl( _p_this, _p_item, _parent, l, line )
{
    finish();
}

FloatRangeConfigControl::FloatRangeConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QLabel *_label,
                                        QDoubleSpinBox *_spin ) :
                FloatConfigControl( _p_this, _p_item, _label, _spin )
{
    finish();
}

void FloatRangeConfigControl::finish()
{
    spin->setMaximum( (double)p_item->max.f );
    spin->setMinimum( (double)p_item->min.f );
}


/**********************************************************************
 * Key selector widget
 **********************************************************************/
KeySelectorControl::KeySelectorControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item,
                                      QWidget *_parent, QGridLayout *l,
                                      int line ) :
                                ConfigControl( _p_this, _p_item, _parent )

{
    QWidget *keyContainer = new QWidget;
    QGridLayout *gLayout = new QGridLayout( keyContainer );

    label = new QLabel(
            qtr( "Select an action to change the associated hotkey") );

    QLabel *searchLabel = new QLabel( qtr( "Search" ) );
    SearchLineEdit *actionSearch = new SearchLineEdit( keyContainer );

    table = new QTreeWidget;
    table->setColumnCount(3);
    table->headerItem()->setText( 0, qtr( "Action" ) );
    table->headerItem()->setText( 1, qtr( "Hotkey" ) );
    table->headerItem()->setText( 2, qtr( "Global" ) );
    table->setAlternatingRowColors( true );
    table->setSelectionBehavior( QAbstractItemView::SelectItems );

    table->installEventFilter( this );

    finish();

    gLayout->addWidget( label, 0, 0, 1, 4 );
    gLayout->addWidget( searchLabel, 1, 0, 1, 2 );
    gLayout->addWidget( actionSearch, 1, 2, 1, 2 );
    gLayout->addWidget( table, 2, 0, 1, 4 );

    l->addWidget( keyContainer, line, 0, 1, -1 );

    CONNECT( actionSearch, textChanged( const QString& ),
             this, filter( const QString& ) );
}

void KeySelectorControl::finish()
{
    if( label && p_item->psz_longtext )
        label->setToolTip( formatTooltip( qtr( p_item->psz_longtext ) ) );

    /* Fill the table */

    /* Get the main Module */
    module_t *p_main = module_get_main();
    assert( p_main );

    /* Access to the module_config_t */
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    QMap<QString, QString> global_keys;
    for (size_t i = 0; i < confsize; i++)
    {
        module_config_t *p_item = p_config + i;

        /* If we are a (non-global) key option not empty */
        if( CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
         && !strncmp( p_item->psz_name , "key-", 4 )
         && !EMPTY_STR( p_item->psz_text ) )
        {
            /*
               Each tree item has:
                - QString text in column 0
                - QString name in data of column 0
                - KeyValue in String in column 1
             */
            QTreeWidgetItem *treeItem = new QTreeWidgetItem();
            treeItem->setText( 0, qtr( p_item->psz_text ) );
            treeItem->setData( 0, Qt::UserRole,
                               QVariant( qfu( p_item->psz_name ) ) );

            QString keys = qfu( p_item->value.psz );
            treeItem->setText( 1, keys );
            treeItem->setData( 1, Qt::UserRole, QVariant( keys ) );
            table->addTopLevelItem( treeItem );
            continue;
        }

        if( CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
         && !strncmp( p_item->psz_name , "global-key", 10 )
         && !EMPTY_STR( p_item->psz_text )
         && !EMPTY_STR( p_item->value.psz ) )
        {
            global_keys.insertMulti( qtr( p_item->psz_text ), qfu( p_item->value.psz ) );
        }
    }

    QMap<QString, QString>::const_iterator i = global_keys.constBegin();
    while (i != global_keys.constEnd())
    {
        QList<QTreeWidgetItem *> list = table->findItems( i.key(), Qt::MatchExactly|Qt::MatchWrap, 0 );
        if( list.count() >= 1 )
        {
            QString keys = i.value();
            list[0]->setText( 2, keys );
            list[0]->setData( 2, Qt::UserRole, keys );
        }
        if( list.count() >= 2 )
            msg_Dbg( p_this, "This is probably wrong, %s", qtu(i.key()) );

        i++;
    }

    module_config_free (p_config);

    table->resizeColumnToContents( 0 );

    CONNECT( table, itemActivated( QTreeWidgetItem *, int ),
             this, selectKey( QTreeWidgetItem *, int ) );
}

void KeySelectorControl::filter( const QString &qs_search )
{
    QList<QTreeWidgetItem *> resultList =
            table->findItems( qs_search, Qt::MatchContains, 0 );
    for( int i = 0; i < table->topLevelItemCount(); i++ )
    {
        table->topLevelItem( i )->setHidden(
                !resultList.contains( table->topLevelItem( i ) ) );
    }
}

void KeySelectorControl::selectKey( QTreeWidgetItem *keyItem, int column )
{
    /* This happens when triggered by ClickEater */
    if( keyItem == NULL ) keyItem = table->currentItem();

    /* This can happen when nothing is selected on the treeView
       and the shortcutValue is clicked */
    if( !keyItem ) return;

    /* If clicked on the first column, assuming user wants the normal hotkey */
    if( column == 0 ) column = 1;

    bool b_global = ( column == 2 );

    /* Launch a small dialog to ask for a new key */
    KeyInputDialog *d = new KeyInputDialog( table, keyItem->text( 0 ), widget, b_global );
    d->exec();

    if( d->result() == QDialog::Accepted )
    {
        QString newKey = VLCKeyToString( d->keyValue );

        /* In case of conflict, reset other keys*/
        if( d->conflicts )
        {
            QTreeWidgetItem *it;
            for( int i = 0; i < table->topLevelItemCount() ; i++ )
            {
                it = table->topLevelItem(i);
                if( ( keyItem != it ) &&
                    ( it->data( 1 + b_global, Qt::UserRole ).toString() == newKey ) )
                {
                    it->setData( 1 + b_global, Qt::UserRole,
                                 QVariant( qfu( "Unset" ) ) );
                    it->setText( 1 + b_global, qtr( "Unset" ) );
                }
            }
        }

        keyItem->setText( column, newKey );
        keyItem->setData( column, Qt::UserRole, newKey );
    }
    delete d;
}

void KeySelectorControl::doApply()
{
    QTreeWidgetItem *it;
    for( int i = 0; i < table->topLevelItemCount() ; i++ )
    {
        it = table->topLevelItem(i);
        if( it->data( 1, Qt::UserRole ).toInt() >= 0 )
            config_PutPsz( p_this,
                           qtu( it->data( 0, Qt::UserRole ).toString() ),
                           qtu( it->data( 1, Qt::UserRole ).toString() ) );
        if( !it->data( 2, Qt::UserRole ).toString().isEmpty() )
        {
            config_PutPsz( p_this,
                           qtu( "global-" + it->data( 0, Qt::UserRole ).toString() ),
                           qtu( it->data( 2, Qt::UserRole ).toString() ) );
        }

    }
}

bool KeySelectorControl::eventFilter( QObject *obj, QEvent *e )
{
    if( obj != table || e->type() != QEvent::KeyPress )
        return ConfigControl::eventFilter(obj, e);

    QKeyEvent *keyEv = static_cast<QKeyEvent*>(e);
    QTreeWidget *aTable = static_cast<QTreeWidget *>(obj);
    if( keyEv->key() == Qt::Key_Escape )
    {
        aTable->clearFocus();
        return true;
    }
    else if( keyEv->key() == Qt::Key_Return ||
             keyEv->key() == Qt::Key_Enter )
    {
        selectKey( aTable->currentItem(), aTable->currentColumn() );
        return true;
    }
    else if( keyEv->key() == Qt::Key_Delete )
    {
        if( aTable->currentColumn() != 0 )
        {
            aTable->currentItem()->setText( aTable->currentColumn(), NULL );
            aTable->currentItem()->setData( aTable->currentColumn(), Qt::UserRole, QVariant() );
        }
        return true;
    }
    else
        return false;
}


/**
 * Class KeyInputDialog
 **/
KeyInputDialog::KeyInputDialog( QTreeWidget *_table,
                                const QString& keyToChange,
                                QWidget *_parent,
                                bool _b_global ) :
                                QDialog( _parent ), keyValue(0), b_global( _b_global )
{
    setModal( true );
    conflicts = false;

    table = _table;
    setWindowTitle( b_global ? qtr( "Global" ): ""
                    + qtr( "Hotkey for " ) + keyToChange );
    setWindowRole( "vlc-key-input" );

    QVBoxLayout *vLayout = new QVBoxLayout( this );
    selected = new QLabel( qtr( "Press the new keys for " ) + keyToChange );
    vLayout->addWidget( selected , Qt::AlignCenter );

    warning = new QLabel;
    warning->hide();
    vLayout->insertWidget( 1, warning );

    buttonBox = new QDialogButtonBox;
    QPushButton *ok = new QPushButton( qtr("OK") );
    QPushButton *cancel = new QPushButton( qtr("Cancel") );
    buttonBox->addButton( ok, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( cancel, QDialogButtonBox::RejectRole );
    ok->setDefault( true );

    vLayout->addWidget( buttonBox );
    buttonBox->hide();

    CONNECT( buttonBox, accepted(), this, accept() );
    CONNECT( buttonBox, rejected(), this, reject() );
}

void KeyInputDialog::checkForConflicts( int i_vlckey )
{
    QList<QTreeWidgetItem *> conflictList =
        table->findItems( VLCKeyToString( i_vlckey ), Qt::MatchExactly,
                          b_global ? 2 : 1 );

    if( conflictList.count() &&
        !conflictList[0]->data( b_global ? 2 : 1, Qt::UserRole ).toString().isEmpty() &&
         conflictList[0]->data( b_global ? 2 : 1, Qt::UserRole ).toString() != "Unset" )
    {
        warning->setText( qtr("Warning: the key is already assigned to \"") +
                conflictList[0]->text( 0 ) + "\"" );
        warning->show();
        buttonBox->show();

        conflicts = true;
    }
    else accept();
}

void KeyInputDialog::keyPressEvent( QKeyEvent *e )
{
    if( e->key() == Qt::Key_Tab ||
        e->key() == Qt::Key_Shift ||
        e->key() == Qt::Key_Control ||
        e->key() == Qt::Key_Meta ||
        e->key() == Qt::Key_Alt ||
        e->key() == Qt::Key_AltGr )
        return;
    int i_vlck = qtEventToVLCKey( e );
    selected->setText( qtr( "Key: " ) + VLCKeyToString( i_vlck ) );
    checkForConflicts( i_vlck );
    keyValue = i_vlck;
}

void KeyInputDialog::wheelEvent( QWheelEvent *e )
{
    int i_vlck = qtWheelEventToVLCKey( e );
    selected->setText( qtr( "Key: " ) + VLCKeyToString( i_vlck ) );
    checkForConflicts( i_vlck );
    keyValue = i_vlck;
}

