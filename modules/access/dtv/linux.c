/**
 * @file linux.c
 * @brief Linux DVB API version 5
 */
/*****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h>

#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "dtv/dtv.h"
#ifdef HAVE_DVBPSI
# include "dtv/en50221.h"
#endif

#ifndef O_SEARCH
# define O_SEARCH O_RDONLY
#endif

#define DVBv5(minor) \
        (DVB_API_VERSION > 5 \
     || (DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= (minor)))
#if !DVBv5(0)
# error Linux DVB kernel headers version 2.6.28 or later required.
#endif

typedef struct
{
    int vlc;
    int linux_;
} dvb_int_map_t;

static int icmp (const void *a, const void *b)
{
    int key = (intptr_t)a;
    const dvb_int_map_t *entry = b;
    return key - entry->vlc;
}

/** Maps a VLC config integer to a Linux DVB enum value */
static int dvb_parse_int (int i, const dvb_int_map_t *map, size_t n, int def)
{
    const void *k = (const void *)(intptr_t)i;
    const dvb_int_map_t *p = bsearch (k, map, n, sizeof (*map), icmp);
    return (p != NULL) ? p->linux_ : def;
}

typedef const struct
{
    char vlc[8];
    int linux_;
} dvb_str_map_t;

static int scmp (const void *a, const void *b)
{
    const char *key = a;
    dvb_str_map_t *entry = b;
    return strcmp (key, entry->vlc);
}

/** Maps a VLC config string to a Linux DVB enum value */
static int dvb_parse_str (const char *str, const dvb_str_map_t *map, size_t n,
                          int def)
{
    if (str != NULL)
    {
        const dvb_str_map_t *p = bsearch (str, map, n, sizeof (*map), scmp);
        if (p != NULL)
            def = p->linux_;
    }
    return def;
}

/*** Modulations ***/
static int dvb_parse_modulation (const char *str, int def)
{
    static const dvb_str_map_t mods[] =
    {
        { "128QAM",  QAM_128  },
        { "16APSK", APSK_16   },
        { "16QAM",   QAM_16   },
        { "16VSB",   VSB_16   },
        { "256QAM",  QAM_256  },
        { "32APSK", APSK_32   },
        { "32QAM",   QAM_32   },
        { "64QAM",   QAM_64   },
        { "8PSK",    PSK_8    }, 
        { "8VSB",    VSB_8    },
        { "DQPSK", DQPSK      },
        { "QAM",     QAM_AUTO },
        { "QPSK",   QPSK      },
    };
    return dvb_parse_str (str, mods, sizeof (mods) / sizeof (*mods), def);
}

static int dvb_parse_fec (uint32_t fec)
{
    static const dvb_int_map_t rates[] =
    {
        { 0,             FEC_NONE },
        { VLC_FEC(1,2),  FEC_1_2  },
        // TODO: 1/3
        // TODO: 1/4
        { VLC_FEC(2,3),  FEC_2_3  },
        { VLC_FEC(3,4),  FEC_3_4  },
        { VLC_FEC(3,5),  FEC_3_5  },
        { VLC_FEC(4,5),  FEC_4_5  },
        { VLC_FEC(5,6),  FEC_5_6  },
        { VLC_FEC(6,7),  FEC_6_7  },
        { VLC_FEC(7,8),  FEC_7_8  },
        { VLC_FEC(8,9),  FEC_8_9  },
        { VLC_FEC(9,10), FEC_9_10 },
        { VLC_FEC_AUTO,  FEC_AUTO },
    };
    return dvb_parse_int (fec, rates, sizeof (rates) / sizeof (*rates),
                          FEC_AUTO);
}


struct dvb_device
{
    vlc_object_t *obj;
    int dir;
    int demux;
    int frontend;
#ifndef USE_DMX
# define MAX_PIDS 256
    struct
    {
        int fd;
        uint16_t pid;
    } pids[MAX_PIDS];
#endif
#ifdef HAVE_DVBPSI
    cam_t *cam;
#endif
    uint8_t device;
    bool budget;
    //size_t buffer_size;
};

/** Opens the device directory for the specified DVB adapter */
static int dvb_open_adapter (uint8_t adapter)
{
    char dir[20];

    snprintf (dir, sizeof (dir), "/dev/dvb/adapter%"PRIu8, adapter);
    return vlc_open (dir, O_SEARCH|O_DIRECTORY);
}

/** Opens the DVB device node of the specified type */
static int dvb_open_node (dvb_device_t *d, const char *type, int flags)
{
    int fd;
    char path[strlen (type) + 4];

    snprintf (path, sizeof (path), "%s%u", type, d->device);
    fd = vlc_openat (d->dir, path, flags);
    if (fd != -1)
        fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) | O_NONBLOCK);
    return fd;
}

/**
 * Opens the DVB tuner
 */
dvb_device_t *dvb_open (vlc_object_t *obj)
{
    dvb_device_t *d = malloc (sizeof (*d));
    if (unlikely(d == NULL))
        return NULL;

    d->obj = obj;

    uint8_t adapter = var_InheritInteger (obj, "dvb-adapter");
    d->device = var_InheritInteger (obj, "dvb-device");

    d->dir = dvb_open_adapter (adapter);
    if (d->dir == -1)
    {
        msg_Err (obj, "cannot access adapter %"PRIu8": %m", adapter);
        free (d);
        return NULL;
    }
    d->frontend = -1;
#ifdef HAVE_DVBPSI
    d->cam = NULL;
#endif
    d->budget = var_InheritBool (obj, "dvb-budget-mode");

#ifndef USE_DMX
    if (d->budget)
#endif
    {
       d->demux = dvb_open_node (d, "demux", O_RDONLY);
       if (d->demux == -1)
       {
           msg_Err (obj, "cannot access demultiplexer: %m");
           close (d->dir);
           free (d);
           return NULL;
       }

       if (ioctl (d->demux, DMX_SET_BUFFER_SIZE, 1 << 20) < 0)
           msg_Warn (obj, "cannot expand demultiplexing buffer: %m");

       /* We need to filter at least one PID. The tap for TS demultiplexing
        * cannot be configured otherwise. So add the PAT. */
        struct dmx_pes_filter_params param;

        param.pid = d->budget ? 0x2000 : 0x000;
        param.input = DMX_IN_FRONTEND;
        param.output = DMX_OUT_TSDEMUX_TAP;
        param.pes_type = DMX_PES_OTHER;
        param.flags = DMX_IMMEDIATE_START;
        if (ioctl (d->demux, DMX_SET_PES_FILTER, &param) < 0)
        {
            msg_Err (obj, "cannot setup TS demultiplexer: %m");
            goto error;
        }
#ifndef USE_DMX
    }
    else
    {
        for (size_t i = 0; i < MAX_PIDS; i++)
            d->pids[i].pid = d->pids[i].fd = -1;
        d->demux = dvb_open_node (d, "dvr", O_RDONLY);
        if (d->demux == -1)
        {
            msg_Err (obj, "cannot access DVR: %m");
            close (d->dir);
            free (d);
            return NULL;
        }
#endif
    }

#ifdef HAVE_DVBPSI
    int ca = dvb_open_node (d, "ca", O_RDWR);
    if (ca != -1)
    {
        d->cam = en50221_Init (obj, ca);
        if (d->cam == NULL)
            close (ca);
    }
    else
        msg_Dbg (obj, "conditional access module not available (%m)");
#endif
    return d;

error:
    dvb_close (d);
    return NULL;
}

void dvb_close (dvb_device_t *d)
{
#ifndef USE_DMX
    if (!d->budget)
    {
        for (size_t i = 0; i < MAX_PIDS; i++)
            if (d->pids[i].fd != -1)
                close (d->pids[i].fd);
    }
#endif
#ifdef HAVE_DVBPSI
    if (d->cam != NULL)
        en50221_End (d->cam);
#endif
    if (d->frontend != -1)
        close (d->frontend);
    close (d->demux);
    close (d->dir);
    free (d);
}

/**
 * Reads TS data from the tuner.
 * @return number of bytes read, 0 on EOF, -1 if no data (yet).
 */
ssize_t dvb_read (dvb_device_t *d, void *buf, size_t len)
{
    struct pollfd ufd[2];
    int n;

#ifdef HAVE_DVBPSI
    if (d->cam != NULL)
        en50221_Poll (d->cam);
#endif

    ufd[0].fd = d->demux;
    ufd[0].events = POLLIN;
    if (d->frontend != -1)
    {
        ufd[1].fd = d->frontend;
        ufd[1].events = POLLIN;
        n = 2;
    }
    else
        n = 1;

    if (poll (ufd, n, 500 /* FIXME */) < 0)
        return -1;

    if (d->frontend != -1 && ufd[1].revents)
    {
        struct dvb_frontend_event ev;

        if (ioctl (d->frontend, FE_GET_EVENT, &ev) < 0)
        {
            if (errno == EOVERFLOW)
            {
                msg_Err (d->obj, "cannot dequeue events fast enough!");
                return -1;
            }
            msg_Err (d->obj, "cannot dequeue frontend event: %m");
            return 0;
        }

        msg_Dbg (d->obj, "frontend status: 0x%02X", (unsigned)ev.status);
    }

    if (ufd[0].revents)
    {
        ssize_t val = read (d->demux, buf, len);
        if (val == -1 && (errno != EAGAIN && errno != EINTR))
        {
            if (errno == EOVERFLOW)
            {
                msg_Err (d->obj, "cannot demux data fast enough!");
                return -1;
            }
            msg_Err (d->obj, "cannot demux: %m");
            return 0;
        }
        return val;
    }

    return -1;
}

int dvb_add_pid (dvb_device_t *d, uint16_t pid)
{
    if (d->budget)
        return 0;
#ifdef USE_DMX
    if (pid == 0 || ioctl (d->demux, DMX_ADD_PID, &pid) >= 0)
        return 0;
#else
    for (size_t i = 0; i < MAX_PIDS; i++)
    {
        if (d->pids[i].pid == pid)
            return 0;
        if (d->pids[i].fd != -1)
            continue;

        int fd = dvb_open_node (d, "demux", O_RDONLY);
        if (fd == -1)
            goto error;

       /* We need to filter at least one PID. The tap for TS demultiplexing
        * cannot be configured otherwise. So add the PAT. */
        struct dmx_pes_filter_params param;

        param.pid = pid;
        param.input = DMX_IN_FRONTEND;
        param.output = DMX_OUT_TS_TAP;
        param.pes_type = DMX_PES_OTHER;
        param.flags = DMX_IMMEDIATE_START;
        if (ioctl (fd, DMX_SET_PES_FILTER, &param) < 0)
        {
            close (fd);
            goto error;
        }
        d->pids[i].fd = fd;
        d->pids[i].pid = pid;
        return 0;
    }
    errno = EMFILE;
error:
#endif
    msg_Err (d->obj, "cannot add PID 0x%04"PRIu16": %m", pid);
    return -1;
}

void dvb_remove_pid (dvb_device_t *d, uint16_t pid)
{
    if (d->budget)
        return;
#ifdef USE_DMX
    if (pid != 0)
        ioctl (d->demux, DMX_REMOVE_PID, &pid);
#else
    for (size_t i = 0; i < MAX_PIDS; i++)
    {
        if (d->pids[i].pid == pid)
        {
            close (d->pids[i].fd);
            d->pids[i].pid = d->pids[i].fd = -1;
            return;
        }
    }
#endif
}

/** Finds a frontend of the correct type */
static int dvb_open_frontend (dvb_device_t *d)
{
    if (d->frontend != -1)
        return 0;
    int fd = dvb_open_node (d, "frontend", O_RDWR);
    if (fd == -1)
    {
        msg_Err (d->obj, "cannot access frontend: %m");
        return -1;
    }

    d->frontend = fd;
    return 0;
}
#define dvb_find_frontend(d, sys) (dvb_open_frontend(d))

/**
 * Detects supported delivery systems.
 * @return a bit mask of supported systems (zero on failure)
 */
unsigned dvb_enum_systems (dvb_device_t *d)
{
    if (dvb_open_frontend (d))
        return 0;

    struct dvb_frontend_info info;
    if (ioctl (d->frontend, FE_GET_INFO, &info) < 0)
    {
        msg_Err (d->obj, "cannot get frontend info: %m");
        return 0;
    }

    msg_Dbg (d->obj, "probing frontend: %s", info.name);
    msg_Dbg (d->obj, " type %u, capabilities 0x%08X", info.type, info.caps);
    msg_Dbg (d->obj, " frequencies %10"PRIu32" to %10"PRIu32,
             info.frequency_min, info.frequency_max);
    msg_Dbg (d->obj, " (%"PRIu32" tolerance, %"PRIu32" per step)",
             info.frequency_tolerance, info.frequency_stepsize);
    msg_Dbg (d->obj, " bauds rates %10"PRIu32" to %10"PRIu32,
             info.symbol_rate_min, info.symbol_rate_max);
    msg_Dbg (d->obj, " (%"PRIu32" tolerance)", info.symbol_rate_tolerance);

    unsigned systems;

    /* DVB first generation and ATSC */
    switch (info.type)
    {
        case FE_QPSK: systems = DVB_S; break;
        case FE_QAM:  systems = DVB_C; break;
        case FE_OFDM: systems = DVB_T; break;
        case FE_ATSC: systems = ATSC;  break;
        default:
            systems = 0;
            msg_Err (d->obj, "unknown frontend type %u", info.type);
    }

    /* DVB 2nd generation */
    switch (info.type)
    {
        case FE_QPSK:
        case FE_QAM:
        case FE_OFDM:
            if (info.caps & FE_CAN_2G_MODULATION)
                systems |= systems << 1; /* DVB_foo -> DVB_foo|DVB_foo2 */
        default:
            break;
    }

    /* ISDB (only terrestrial before DVBv5.5)  */
    if (info.type == FE_OFDM)
        systems |= ISDB_T;

    return systems;
}

float dvb_get_signal_strength (dvb_device_t *d)
{
    uint16_t strength;

    if (d->frontend == -1
     || ioctl (d->frontend, FE_READ_SIGNAL_STRENGTH, &strength) < 0)
        return 0.;
    return strength / 65535.;
}

float dvb_get_snr (dvb_device_t *d)
{
    uint16_t snr;

    if (d->frontend == -1 || ioctl (d->frontend, FE_READ_SNR, &snr) < 0)
        return 0.;
    return snr / 65535.;
}

#ifdef HAVE_DVBPSI
void dvb_set_ca_pmt (dvb_device_t *d, struct dvbpsi_pmt_s *pmt)
{
    if (d->cam != NULL)
        en50221_SetCAPMT (d->cam, pmt);
}
#endif

static int dvb_vset_props (dvb_device_t *d, size_t n, va_list ap)
{
    struct dtv_property buf[n], *prop = buf;
    struct dtv_properties props = { .num = n, .props = buf };

    memset (buf, 0, sizeof (buf));

    while (n > 0)
    {
        prop->cmd = va_arg (ap, uint32_t);
        prop->u.data = va_arg (ap, uint32_t);
        msg_Dbg (d->obj, "setting property %2"PRIu32" to %"PRIu32,
                 prop->cmd, prop->u.data);
        prop++;
        n--;
    }

    if (ioctl (d->frontend, FE_SET_PROPERTY, &props) < 0)
    {
        msg_Err (d->obj, "cannot set frontend tuning parameters: %m");
        return -1;
    }
    return 0;
}

static int dvb_set_props (dvb_device_t *d, size_t n, ...)
{
    va_list ap;
    int ret;

    va_start (ap, n);
    ret = dvb_vset_props (d, n, ap);
    va_end (ap);
    return ret;
}

static int dvb_set_prop (dvb_device_t *d, uint32_t prop, uint32_t val)
{
    return dvb_set_props (d, 1, prop, val);
}

int dvb_set_inversion (dvb_device_t *d, int v)
{
    switch (v)
    {
        case 0:  v = INVERSION_OFF;  break;
        case 1:  v = INVERSION_ON;   break;
        default: v = INVERSION_AUTO; break;
    }
    return dvb_set_prop (d, DTV_INVERSION, v);
}

int dvb_tune (dvb_device_t *d)
{
    return dvb_set_prop (d, DTV_TUNE, 0 /* dummy */);
}


/*** DVB-C ***/
int dvb_set_dvbc (dvb_device_t *d, uint32_t freq, const char *modstr,
                  uint32_t srate, uint32_t fec)
{
    unsigned mod = dvb_parse_modulation (modstr, QAM_AUTO);
    fec = dvb_parse_fec (fec);

    if (dvb_find_frontend (d, DVB_C))
        return -1;
    return dvb_set_props (d, 6, DTV_CLEAR, 0,
#if DVBv5(5)
                          DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_A,
#else
                          DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_AC,
#endif
                          DTV_FREQUENCY, freq, DTV_MODULATION, mod,
                          DTV_SYMBOL_RATE, srate, DTV_INNER_FEC, fec);
}


/*** DVB-S ***/
static unsigned dvb_parse_polarization (char pol)
{
    static const dvb_int_map_t tab[5] = {
        { 0,   SEC_VOLTAGE_OFF },
        { 'H', SEC_VOLTAGE_18  },
        { 'L', SEC_VOLTAGE_18  },
        { 'R', SEC_VOLTAGE_13  },
        { 'V', SEC_VOLTAGE_13  },
    };
    return dvb_parse_int (pol, tab, 5, SEC_VOLTAGE_OFF);
}

int dvb_set_sec (dvb_device_t *d, uint64_t freq_Hz, char pol,
                 uint32_t lowf, uint32_t highf, uint32_t switchf)
{
    uint32_t freq = freq_Hz / 1000;

    /* Always try to configure high voltage, but only warn on enable failure */
    int val = var_InheritBool (d->obj, "dvb-high-voltage");
    if (ioctl (d->frontend, FE_ENABLE_HIGH_LNB_VOLTAGE, &val) < 0 && val)
        msg_Err (d->obj, "cannot enable high LNB voltage: %m");

    /* Windows BDA exposes a higher-level API covering LNB oscillators.
     * So lets pretend this is platform-specific stuff and do it here. */
    if (!lowf)
    {   /* Default oscillator frequencies */
        static const struct
        {
             uint16_t min, max, low, high;
        } tab[] =
        {    /*  min    max    low   high */
             { 10700, 13250,  9750, 10600 }, /* Ku band */
             {  4500,  4800,  5950,     0 }, /* C band (high) */
             {  3400,  4200,  5150,     0 }, /* C band (low) */
             {  2500,  2700,  3650,     0 }, /* S band */
             {   950,  2150,     0,     0 }, /* adjusted IF (L band) */
        };
        uint_fast16_t mHz = freq / 1000;

        for (size_t i = 0; i < sizeof (tab) / sizeof (tab[0]); i++)
             if (mHz >= tab[i].min && mHz <= tab[i].max)
             {
                 lowf = tab[i].low * 1000;
                 highf = tab[i].high * 1000;
                 goto known;
             }

        msg_Err (d->obj, "no known band for frequency %u kHz", freq);
known:
        msg_Dbg (d->obj, "selected LNB low: %u kHz, LNB high: %u kHz",
                 lowf, highf);
    }

    /* Use high oscillator frequency? */
    bool high = highf != 0 && freq > switchf;

    freq -= high ? highf : lowf;
    if ((int32_t)freq < 0)
        freq *= -1;
    assert (freq < 0x7fffffff);

    int tone;
    switch (var_InheritInteger (d->obj, "dvb-tone"))
    {
        case 0:  tone = SEC_TONE_OFF; break;
        case 1:  tone = SEC_TONE_ON;  break;
        default: tone = high ? SEC_TONE_ON : SEC_TONE_OFF;
    }

    /*** LNB selection / DiSEqC ***/
    unsigned voltage = dvb_parse_polarization (pol);
    if (dvb_set_props (d, 2, DTV_TONE, SEC_TONE_OFF, DTV_VOLTAGE, voltage))
        return -1;

    unsigned satno = var_InheritInteger (d->obj, "dvb-satno");
    if (satno > 0)
    {
        /* DiSEqC 1.0 */
#undef msleep /* we know what we are doing! */
        struct dvb_diseqc_master_cmd cmd;

        satno = (satno - 1) & 3;
        cmd.msg[0] = 0xE0; /* framing: master, no reply, 1st TX */
        cmd.msg[1] = 0x10; /* address: all LNB/switch */
        cmd.msg[2] = 0x38; /* command: Write Port Group 0 */
        cmd.msg[3] = 0xF0  /* data[0]: clear all bits */
                   | (satno << 2) /* LNB (A, B, C or D) */
                   | ((voltage == SEC_VOLTAGE_18) << 1) /* polarization */
                   | (tone == SEC_TONE_ON); /* option */
        cmd.msg[4] = cmd.msg[5] = 0; /* unused */
        cmd.msg_len = 4; /* length*/
        msleep (15000); /* wait 15 ms before DiSEqC command */
        if (ioctl (d->frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd) < 0)
        {
            msg_Err (d->obj, "cannot send DiSEqC command: %m");
            return -1;
        }
        msleep (54000 + 15000);

        /* Mini-DiSEqC */
        satno &= 1;
        if (ioctl (d->frontend, FE_DISEQC_SEND_BURST,
                   satno ? SEC_MINI_B : SEC_MINI_A) < 0)
        {
            msg_Err (d->obj, "cannot send Mini-DiSEqC tone burst: %m");
            return -1;
        }
        msleep (15000);
    }

    /* Continuous tone (to select high oscillator frequency) */
    return dvb_set_props (d, 2, DTV_FREQUENCY, freq, DTV_TONE, tone);
}

int dvb_set_dvbs (dvb_device_t *d, uint64_t freq_Hz,
                  uint32_t srate, uint32_t fec)
{
    uint32_t freq = freq_Hz / 1000;
    fec = dvb_parse_fec (fec);

    if (dvb_find_frontend (d, DVB_S))
        return -1;
    return dvb_set_props (d, 5, DTV_CLEAR, 0, DTV_DELIVERY_SYSTEM, SYS_DVBS,
                          DTV_FREQUENCY, freq, DTV_SYMBOL_RATE, srate,
                          DTV_INNER_FEC, fec);
}

int dvb_set_dvbs2 (dvb_device_t *d, uint64_t freq_Hz, const char *modstr,
                   uint32_t srate, uint32_t fec, int pilot, int rolloff)
{
    uint32_t freq = freq_Hz / 1000;
    unsigned mod = dvb_parse_modulation (modstr, QPSK);
    fec = dvb_parse_fec (fec);

    switch (pilot)
    {
        case 0:  pilot = PILOT_OFF;  break;
        case 1:  pilot = PILOT_ON;   break;
        default: pilot = PILOT_AUTO; break;
    }

    switch (rolloff)
    {
        case 20: rolloff = ROLLOFF_20;  break;
        case 25: rolloff = ROLLOFF_25;  break;
        case 35: rolloff = ROLLOFF_35;  break;
        default: rolloff = PILOT_AUTO; break;
    }

    if (dvb_find_frontend (d, DVB_S2))
        return -1;
    return dvb_set_props (d, 8, DTV_CLEAR, 0, DTV_DELIVERY_SYSTEM, SYS_DVBS2,
                          DTV_FREQUENCY, freq, DTV_MODULATION, mod,
                          DTV_SYMBOL_RATE, srate, DTV_INNER_FEC, fec,
                          DTV_PILOT, pilot, DTV_ROLLOFF, rolloff);
}


/*** DVB-T ***/
static uint32_t dvb_parse_bandwidth (uint32_t i)
{
    switch (i)
    {
      //case  0: return 0;
        case  2: return 1712000;
        default: return i * 1000000;
    }
}

static int dvb_parse_transmit_mode (int i)
{
    static const dvb_int_map_t tab[] = {
        { -1, TRANSMISSION_MODE_AUTO },
#if DVBv5(3)
        {  1, TRANSMISSION_MODE_1K   },
#endif
        {  2, TRANSMISSION_MODE_2K   },
#if DVBv5(1)
        {  4, TRANSMISSION_MODE_4K   },
#endif
        {  8, TRANSMISSION_MODE_8K   },
#if DVBv5(3)
        { 16, TRANSMISSION_MODE_16K  },
        { 32, TRANSMISSION_MODE_32K  },
#endif
    };
    return dvb_parse_int (i, tab, sizeof (tab) / sizeof (*tab),
                          TRANSMISSION_MODE_AUTO);
}

static int dvb_parse_guard (uint32_t guard)
{
    static const dvb_int_map_t tab[] = {
        { VLC_GUARD(1,4),    GUARD_INTERVAL_1_4 },
        { VLC_GUARD(1,8),    GUARD_INTERVAL_1_8 },
        { VLC_GUARD(1,16),   GUARD_INTERVAL_1_16 },
        { VLC_GUARD(1,32),   GUARD_INTERVAL_1_32 },
#if DVBv5(3)
        { VLC_GUARD(1,128),  GUARD_INTERVAL_1_128 },
        { VLC_GUARD(19,128), GUARD_INTERVAL_19_128 },
        { VLC_GUARD(19,256), GUARD_INTERVAL_19_256 },
#endif
        { VLC_GUARD_AUTO,    GUARD_INTERVAL_AUTO },
    };
    return dvb_parse_int (guard, tab, sizeof (tab) / sizeof (*tab),
                          GUARD_INTERVAL_AUTO);
}

static int dvb_parse_hierarchy (int i)
{
    static const dvb_int_map_t tab[] = {
        { HIERARCHY_AUTO, -1 },
        { HIERARCHY_NONE,  0 },
        { HIERARCHY_1,     1 },
        { HIERARCHY_2,     2 },
        { HIERARCHY_4,     4 },
    };
    return dvb_parse_int (i, tab, sizeof (tab) / sizeof (*tab),
                          HIERARCHY_AUTO);
}

int dvb_set_dvbt (dvb_device_t *d, uint32_t freq, const char *modstr,
                  uint32_t fec_hp, uint32_t fec_lp, uint32_t bandwidth,
                  int transmit_mode, uint32_t guard, int hierarchy)
{
    uint32_t mod = dvb_parse_modulation (modstr, QAM_AUTO);
    fec_hp = dvb_parse_fec (fec_hp);
    fec_lp = dvb_parse_fec (fec_lp);
    bandwidth = dvb_parse_bandwidth (bandwidth);
    transmit_mode = dvb_parse_transmit_mode (transmit_mode);
    guard = dvb_parse_guard (guard);
    hierarchy = dvb_parse_hierarchy (hierarchy);

    if (dvb_find_frontend (d, DVB_T))
        return -1;
    return dvb_set_props (d, 10, DTV_CLEAR, 0, DTV_DELIVERY_SYSTEM, SYS_DVBT,
                          DTV_FREQUENCY, freq, DTV_MODULATION, mod,
                          DTV_CODE_RATE_HP, fec_hp, DTV_CODE_RATE_LP, fec_lp,
                          DTV_BANDWIDTH_HZ, bandwidth,
                          DTV_TRANSMISSION_MODE, transmit_mode,
                          DTV_GUARD_INTERVAL, guard,
                          DTV_HIERARCHY, hierarchy);
}

int dvb_set_dvbt2 (dvb_device_t *d, uint32_t freq, const char *modstr,
                   uint32_t fec, uint32_t bandwidth,
                   int transmit_mode, uint32_t guard)
{
#if DVBv5(3)
    uint32_t mod = dvb_parse_modulation (modstr, QAM_AUTO);
    fec = dvb_parse_fec (fec);
    bandwidth = dvb_parse_bandwidth (bandwidth);
    transmit_mode = dvb_parse_transmit_mode (transmit_mode);
    guard = dvb_parse_guard (guard);

    if (dvb_find_frontend (d, DVB_T2))
        return -1;
    return dvb_set_props (d, 8, DTV_CLEAR, 0, DTV_DELIVERY_SYSTEM, SYS_DVBT2,
                          DTV_FREQUENCY, freq, DTV_MODULATION, mod,
                          DTV_INNER_FEC, fec, DTV_BANDWIDTH_HZ, bandwidth,
                          DTV_TRANSMISSION_MODE, transmit_mode,
                          DTV_GUARD_INTERVAL, guard);
#else
# warning DVB-T2 needs Linux DVB version 5.3 or later.
    msg_Err (d->obj, "DVB-T2 support not compiled-in");
    (void) freq; (void) modstr; (void) fec; (void) bandwidth;
    (void) transmit_mode; (void) guard;
    return -1;
#endif
}


/*** ISDB-C ***/
int dvb_set_isdbc (dvb_device_t *d, uint32_t freq, const char *modstr,
                   uint32_t srate, uint32_t fec)
{
    unsigned mod = dvb_parse_modulation (modstr, QAM_AUTO);
    fec = dvb_parse_fec (fec);

    if (dvb_find_frontend (d, ISDB_C))
        return -1;
    return dvb_set_props (d, 6, DTV_CLEAR, 0,
#if DVBv5(5)
                          DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_C,
#else
# warning ISDB-C might need Linux DVB version 5.5 or later.
                          DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_AC,
#endif
                          DTV_FREQUENCY, freq, DTV_MODULATION, mod,
                          DTV_SYMBOL_RATE, srate, DTV_INNER_FEC, fec);
}


/*** ISDB-S ***/
int dvb_set_isdbs (dvb_device_t *d, uint64_t freq_Hz, uint16_t ts_id)
{
#if DVBv5(1)
    uint32_t freq = freq_Hz / 1000;

    if (dvb_find_frontend (d, ISDB_S))
        return -1;
    return dvb_set_props (d, 5, DTV_CLEAR, 0, DTV_DELIVERY_SYSTEM, SYS_ISDBS,
                          DTV_FREQUENCY, freq,
#if DVBv5(8)
                          DTV_STREAM_ID,
#else
                          DTV_ISDBS_TS_ID,
#endif
                          (uint32_t)ts_id);
#else
# warning ISDB-S needs Linux DVB version 5.1 or later.
    msg_Err (d->obj, "ISDB-S support not compiled-in");
    (void) freq_Hz; (void) ts_id;
    return -1;
#endif
}


/*** ISDB-T ***/
#if DVBv5(1)
static int dvb_set_isdbt_layer (dvb_device_t *d, unsigned num,
                                const isdbt_layer_t *l)
{
    uint32_t mod = dvb_parse_modulation (l->modulation, QAM_AUTO);
    uint32_t fec = dvb_parse_fec (l->code_rate);
    uint32_t count = l->segment_count;
    uint32_t ti = l->time_interleaving;

    num *= DTV_ISDBT_LAYERB_FEC - DTV_ISDBT_LAYERA_FEC;

    return dvb_set_props (d, 5, DTV_DELIVERY_SYSTEM, SYS_ISDBT,
                          DTV_ISDBT_LAYERA_FEC + num, fec,
                          DTV_ISDBT_LAYERA_MODULATION + num, mod,
                          DTV_ISDBT_LAYERA_SEGMENT_COUNT + num, count,
                          DTV_ISDBT_LAYERA_TIME_INTERLEAVING + num, ti);
}
#endif

int dvb_set_isdbt (dvb_device_t *d, uint32_t freq, uint32_t bandwidth,
                   int transmit_mode, uint32_t guard,
                   const isdbt_layer_t layers[3])
{
#if DVBv5(1)
    bandwidth = dvb_parse_bandwidth (bandwidth);
    transmit_mode = dvb_parse_transmit_mode (transmit_mode);
    guard = dvb_parse_guard (guard);

    if (dvb_find_frontend (d, ISDB_T))
        return -1;
    if (dvb_set_props (d, 5, DTV_CLEAR, 0, DTV_DELIVERY_SYSTEM, SYS_ISDBT,
                       DTV_FREQUENCY, freq, DTV_BANDWIDTH_HZ, bandwidth,
                       DTV_GUARD_INTERVAL, guard))
        return -1;
    for (unsigned i = 0; i < 3; i++)
        if (dvb_set_isdbt_layer (d, i, layers + i))
            return -1;
    return 0;
#else
# warning ISDB-T needs Linux DVB version 5.1 or later.
    msg_Err (d->obj, "ISDB-T support not compiled-in");
    (void) freq; (void) bandwidth; (void) transmit_mode; (void) guard;
    (void) layers;
    return -1;
#endif
}


/*** ATSC ***/
int dvb_set_atsc (dvb_device_t *d, uint32_t freq, const char *modstr)
{
    unsigned mod = dvb_parse_modulation (modstr, VSB_8);

    if (dvb_find_frontend (d, ATSC))
        return -1;
    return dvb_set_props (d, 4, DTV_CLEAR, 0, DTV_DELIVERY_SYSTEM, SYS_ATSC,
                          DTV_FREQUENCY, freq, DTV_MODULATION, mod);
}

int dvb_set_cqam (dvb_device_t *d, uint32_t freq, const char *modstr)
{
    unsigned mod = dvb_parse_modulation (modstr, QAM_AUTO);

    if (dvb_find_frontend (d, ATSC))
        return -1;
    return dvb_set_props (d, 4, DTV_CLEAR, 0,
                          DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_B,
                          DTV_FREQUENCY, freq, DTV_MODULATION, mod);
}
