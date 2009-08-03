/*****************************************************************************
 * url.c: Test for url encoding/decoding stuff
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id: f68a0018106104c58b055c4cec769c2f05f56a5c $
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

#include <vlc_common.h>
#include "vlc_url.h"
#include "vlc_strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef char * (*conv_t) (const char *);

static void test (conv_t f, const char *in, const char *out)
{
    char *res;

    printf ("\"%s\" -> \"%s\" ?\n", in, out);
    res = f (in);
    if (res == NULL)
        exit (1);

    if (strcmp (res, out))
    {
        printf (" ERROR: got \"%s\"\n", res);
        exit (2);
    }

    free (res);
}

static inline void test_decode (const char *in, const char *out)
{
    test (decode_URI_duplicate, in, out);
}

static inline void test_b64 (const char *in, const char *out)
{
    test (vlc_b64_encode, in, out);
}

static inline void test_path (const char *in, const char *out)
{
    test (make_URI, in, out);
}

int main (void)
{
    int val;

    (void)setvbuf (stdout, NULL, _IONBF, 0);
    test_decode ("this_should_not_be_modified_1234",
                 "this_should_not_be_modified_1234");

    test_decode ("This+should+be+modified+1234!",
                 "This should be modified 1234!");

    test_decode ("This%20should%20be%20modified%201234!",
                 "This should be modified 1234!");

    test_decode ("%7E", "~");

    /* tests with invalid input */
    test_decode ("%", "%");
    test_decode ("%2", "%2");
    test_decode ("%0000", "");

    /* UTF-8 tests */
    test_decode ("T%C3%a9l%c3%A9vision+%e2%82%Ac", "Télévision €");
    test_decode ("T%E9l%E9vision", "T?l?vision");
    test_decode ("%C1%94%C3%a9l%c3%A9vision", "??élévision"); /* overlong */

    /* Base 64 tests */
    test_b64 ("", "");
    test_b64 ("f", "Zg==");
    test_b64 ("fo", "Zm8=");
    test_b64 ("foo", "Zm9v");
    test_b64 ("foob", "Zm9vYg==");
    test_b64 ("fooba", "Zm9vYmE=");
    test_b64 ("foobar", "Zm9vYmFy");

    /* Path test */
    test_path ("file:///", "file:///");
    test_path ("http://www.example.com/%7Ejohn/",
               "http://www.example.com/%7Ejohn/");
    test_path ("/", "file:///");
    test_path ("/home/john/", "file:///home/john/");
    test_path ("/home/john/music.ogg", "file:///home/john/music.ogg");
    test_path ("\\\\server/pub/music.ogg", "smb://server/pub/music.ogg");
    test_path ("\\\\server\\pub\\music.ogg", "smb://server/pub/music.ogg");

    /*int fd = open (".", O_RDONLY);
    assert (fd != -1);*/
    val = chdir ("/tmp");
    assert (val != -1);
    test_path ("movie.ogg", "file:///tmp/movie.ogg");
    test_path (".", "file:///tmp/.");
    test_path ("", "file:///tmp/");
    /*val = fchdir (fd);
    assert (val != -1);*/

    return 0;
}
