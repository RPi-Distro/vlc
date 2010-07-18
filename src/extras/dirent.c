/*
 * dirent.c
 *
 * Derived from DIRLIB.C by Matt J. Weinstein 
 * This note appears in the DIRLIB.H
 * DIRLIB.H by M. J. Weinstein   Released to public domain 1-Jan-89
 *
 * Updated by Jeremy Bettis <jeremy@hksys.com>
 * Significantly revised and rewinddir, seekdir and telldir added by Colin
 * Peters <colin@fu.is.saga-u.ac.jp>
 *      
 * $Revision: 1.6 $
 * $Author: sam $
 * $Date: 2002/11/13 20:51:04 $
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#else
    static int errno;
    /* FIXME: anything clever to put here? */
#   define EFAULT 12
#   define ENOTDIR 12
#   define ENOENT 12
#   define ENOMEM 12
#   define EINVAL 12
#endif
#include <string.h>
#ifndef UNDER_CE
#   include <io.h>
#   include <direct.h>
#else
#   define FILENAME_MAX (260)
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h> /* for GetFileAttributes */

#include <tchar.h>
#define SUFFIX  "*"
#define SLASH   "\\"

struct dirent
{
	long		d_ino;		/* Always zero. */
	unsigned short	d_reclen;	/* Always zero. */
	unsigned short	d_namlen;	/* Length of name in d_name. */
	char            d_name[FILENAME_MAX]; /* File name. */
};

typedef struct
{
	/* disk transfer area for this dir */
	WIN32_FIND_DATA		dd_dta;

	/* dirent struct to return from dir (NOTE: this makes this thread
	 * safe as long as only one thread uses a particular DIR struct at
	 * a time) */
	struct dirent		dd_dir;

	/* findnext handle */
	HANDLE			dd_handle;

	/*
         * Status of search:
	 *   0 = not started yet (next entry to read is first entry)
	 *  -1 = off the end
	 *   positive = 0 based index of next entry
	 */
	int			dd_stat;

	/* given path for dir with search pattern (struct is extended) */
	char			dd_name[1];
} DIR;

/*
 * opendir
 *
 * Returns a pointer to a DIR structure appropriately filled in to begin
 * searching a directory.
 */
DIR * 
vlc_opendir (const CHAR *szPath)
{
  DIR *nd;
  unsigned int rc;
  CHAR szFullPath[MAX_PATH];

  errno = 0;

  if (!szPath)
    {
      errno = EFAULT;
      return (DIR *) 0;
    }

  if (szPath[0] == '\0')
    {
      errno = ENOTDIR;
      return (DIR *) 0;
    }

  /* Attempt to determine if the given path really is a directory. */
#ifdef UNICODE
  {
    wchar_t szPathTmp[MAX_PATH];
    mbstowcs( szPathTmp, szPath, MAX_PATH );
    szPathTmp[MAX_PATH-1] = 0;
    rc = GetFileAttributes (szPathTmp);
  }
#else
  rc = GetFileAttributes (szPath);
#endif
  if (rc == (unsigned int)-1)
    {
      /* call GetLastError for more error info */
      errno = ENOENT;
      return (DIR *) 0;
    }
  if (!(rc & FILE_ATTRIBUTE_DIRECTORY))
    {
      /* Error, entry exists but not a directory. */
      errno = ENOTDIR;
      return (DIR *) 0;
    }

  /* Make an absolute pathname.  */
#if defined( UNDER_CE )
  if (szPath[0] == '\\' || szPath[0] == '/')
    {
      sprintf (szFullPath, "%s", szPath);
      szFullPath[0] = '\\';
    }
  else
    {
      wchar_t szFullPathTmp[MAX_PATH];
      if (GetModuleFileName( NULL, szFullPathTmp, MAX_PATH ) )
        {
          wcstombs( szFullPath, szFullPathTmp, MAX_PATH );
          szFullPath[MAX_PATH-1] = 0;
        }
      else
        {
          /* FIXME: if I wasn't lazy, I'd check for overflows here. */
          sprintf (szFullPath, "\\%s", szPath );
        }
    }
#else
  _fullpath (szFullPath, szPath, MAX_PATH);
#endif

  /* Allocate enough space to store DIR structure and the complete
   * directory path given. */
  nd = (DIR *) malloc (sizeof (DIR) + strlen (szFullPath) + sizeof (SLASH) +
                       sizeof (SUFFIX));

  if (!nd)
    {
      /* Error, out of memory. */
      errno = ENOMEM;
      return (DIR *) 0;
    }

  /* Create the search expression. */
  strcpy (nd->dd_name, szFullPath);

  /* Add on a slash if the path does not end with one. */
  if (nd->dd_name[0] != '\0' &&
      nd->dd_name[strlen (nd->dd_name) - 1] != '/' &&
      nd->dd_name[strlen (nd->dd_name) - 1] != '\\')
    {
      strcat (nd->dd_name, SLASH);
    }

  /* Add on the search pattern */
  strcat (nd->dd_name, SUFFIX);

  /* Initialize handle so that a premature closedir doesn't try
   * to call FindClose on it. */
  nd->dd_handle = INVALID_HANDLE_VALUE;

  /* Initialize the status. */
  nd->dd_stat = 0;

  /* Initialize the dirent structure. ino and reclen are invalid under
   * Win32, and name simply points at the appropriate part of the
   * findfirst_t structure. */
  nd->dd_dir.d_ino = 0;
  nd->dd_dir.d_reclen = 0;
  nd->dd_dir.d_namlen = 0;
  memset (nd->dd_dir.d_name, 0, FILENAME_MAX);

  return nd;
}


/*
 * readdir
 *
 * Return a pointer to a dirent structure filled with the information on the
 * next entry in the directory.
 */
struct dirent *
vlc_readdir (DIR * dirp)
{
  errno = 0;

  /* Check for valid DIR struct. */
  if (!dirp)
    {
      errno = EFAULT;
      return (struct dirent *) 0;
    }

  if (dirp->dd_stat < 0)
    {
      /* We have already returned all files in the directory
       * (or the structure has an invalid dd_stat). */
      return (struct dirent *) 0;
    }
  else if (dirp->dd_stat == 0)
    {
#ifdef UNICODE
        wchar_t dd_name[MAX_PATH];
        mbstowcs( dd_name, dirp->dd_name, MAX_PATH );
        dd_name[MAX_PATH-1] = 0;
#else
        char *dd_name = dirp->dd_name;
#endif
      /* We haven't started the search yet. */
      /* Start the search */
      dirp->dd_handle = FindFirstFile (dd_name, &(dirp->dd_dta));

          if (dirp->dd_handle == INVALID_HANDLE_VALUE)
        {
          /* Whoops! Seems there are no files in that
           * directory. */
          dirp->dd_stat = -1;
        }
      else
        {
          dirp->dd_stat = 1;
        }
    }
  else
    {
      /* Get the next search entry. */
      if (!FindNextFile ((HANDLE)dirp->dd_handle, &(dirp->dd_dta)))
        {
          /* We are off the end or otherwise error. */
          FindClose ((HANDLE)dirp->dd_handle);
          dirp->dd_handle = INVALID_HANDLE_VALUE;
          dirp->dd_stat = -1;
        }
      else
        {
          /* Update the status to indicate the correct
           * number. */
          dirp->dd_stat++;
        }
    }

  if (dirp->dd_stat > 0)
    {
      /* Successfully got an entry */

#ifdef UNICODE
      char d_name[MAX_PATH];
      wcstombs( d_name, dirp->dd_dta.cFileName, MAX_PATH );
      d_name[MAX_PATH-1] = 0;
#else
      char *d_name = dirp->dd_dta.cFileName;
#endif

      strcpy (dirp->dd_dir.d_name, d_name);
      dirp->dd_dir.d_namlen = strlen (dirp->dd_dir.d_name);
      return &dirp->dd_dir;
    }

  return (struct dirent *) 0;
}


/*
 * closedir
 *
 * Frees up resources allocated by opendir.
 */
int
vlc_closedir (DIR * dirp)
{
  int rc;

  errno = 0;
  rc = 0;

  if (!dirp)
    {
      errno = EFAULT;
      return -1;
    }

  if (dirp->dd_handle != INVALID_HANDLE_VALUE)
    {
      rc = FindClose ((HANDLE)dirp->dd_handle);
    }

  /* Delete the dir structure. */
  free (dirp);

  return rc;
}

/*
 * rewinddir
 *
 * Return to the beginning of the directory "stream". We simply call findclose
 * and then reset things like an opendir.
 */
void
vlc_rewinddir (DIR * dirp)
{
  errno = 0;

  if (!dirp)
    {
      errno = EFAULT;
      return;
    }

  if (dirp->dd_handle != INVALID_HANDLE_VALUE)
    {
      FindClose ((HANDLE)dirp->dd_handle);
    }

  dirp->dd_handle = INVALID_HANDLE_VALUE;
  dirp->dd_stat = 0;
}

/*
 * telldir
 *
 * Returns the "position" in the "directory stream" which can be used with
 * seekdir to go back to an old entry. We simply return the value in stat.
 */
long
vlc_telldir (DIR * dirp)
{
  errno = 0;

  if (!dirp)
    {
      errno = EFAULT;
      return -1;
    }
  return dirp->dd_stat;
}

/*
 * seekdir
 *
 * Seek to an entry previously returned by telldir. We rewind the directory
 * and call readdir repeatedly until either dd_stat is the position number
 * or -1 (off the end). This is not perfect, in that the directory may
 * have changed while we weren't looking. But that is probably the case with
 * any such system.
 */
void
vlc_seekdir (DIR * dirp, long lPos)
{
  errno = 0;

  if (!dirp)
    {
      errno = EFAULT;
      return;
    }

  if (lPos < -1)
    {
      /* Seeking to an invalid position. */
      errno = EINVAL;
      return;
    }
  else if (lPos == -1)
    {
      /* Seek past end. */
      if (dirp->dd_handle != INVALID_HANDLE_VALUE)
        {
          FindClose ((HANDLE)dirp->dd_handle);
        }
      dirp->dd_handle = INVALID_HANDLE_VALUE;
      dirp->dd_stat = -1;
    }
  else
    {
      /* Rewind and read forward to the appropriate index. */
      vlc_rewinddir (dirp);

      while ((dirp->dd_stat < lPos) && vlc_readdir (dirp))
        ;
    }
}
