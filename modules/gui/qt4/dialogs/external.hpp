/*****************************************************************************
 * external.hpp : Dialogs from other LibVLC core and other plugins
 ****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#ifndef QVLC_DIALOGS_EXTERNAL_H_
#define QVLC_DIALOGS_EXTERNAL_H_ 1

#include <QObject>
#include <vlc_common.h>

class QVLCVariable : public QObject
{
    Q_OBJECT
private:
    static int callback (vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);
    vlc_object_t *object;
    QString name;

public:
    QVLCVariable (vlc_object_t *, const char *, int);
    virtual ~QVLCVariable (void);

signals:
    void pointerChanged (vlc_object_t *, void *);
};

struct intf_thread_t;
class QProgressDialog;

class DialogHandler : public QObject
{
    Q_OBJECT

    friend class QVLCProgressDialog;

public:
    DialogHandler (intf_thread_t *, QObject *parent);
    ~DialogHandler (void);

private:
    intf_thread_t *intf;
    static int error (vlc_object_t *, const char *, vlc_value_t, vlc_value_t,
                      void *);
    QVLCVariable critical;
    QVLCVariable login;
    QVLCVariable question;
    QVLCVariable progressBar;
signals:
    void progressBarDestroyed (QWidget *);
    void error (const QString&, const QString&);

private slots:
    void displayError (const QString&, const QString&);
    void displayCritical (vlc_object_t *, void *);
    void requestLogin (vlc_object_t *, void *);
    void requestAnswer (vlc_object_t *, void *);
    void startProgressBar (vlc_object_t *, void *);
    void stopProgressBar (QWidget *);
};

/* Put here instead of .cpp because of MOC */
#include <QProgressDialog>

class QVLCProgressDialog : public QProgressDialog
{
    Q_OBJECT
public:
    QVLCProgressDialog (DialogHandler *parent,
                        struct dialog_progress_bar_t *);
    virtual ~QVLCProgressDialog (void);

private:
    DialogHandler *handler;
    bool cancelled;

    static void update (void *, const char *, float);
    static bool check (void *);
    static void destroy (void *);
private slots:
    void saveCancel (void);

signals:
    void progressed (int);
    void described (const QString&);
    void destroyed (void);
};

#endif
