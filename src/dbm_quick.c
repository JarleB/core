/* 

   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/*****************************************************************************/
/*                                                                           */
/* File: dbm_quick.c                                                         */
/*                                                                           */
/*****************************************************************************/

/* 
 * Implementation of the Cfengine DBM API using Quick Database Manager.
 * TODO: System mutex on malloc/free.
 */

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef QDB

int QDB_OpenDB(char *filename, CF_QDB **qdbp)
{
  ThreadLock(cft_system);

  *qdbp = malloc(sizeof(CF_QDB));

  if(*qdbp == NULL)
    {
      FatalError("Memory allocation in QDB_OpenDB()");
    }
  
  (*qdbp)->depot = dpopen(filename, DP_OWRITER | DP_OCREAT, -1);

  if((*qdbp)->depot == NULL)
    {
      CfOut(cf_error, "dpopen", "!! Opening database \"%s\" failed: %s", filename, dperrmsg(dpecode));
      free(*qdbp);
      ThreadUnlock(cft_system);
      return false;
    }

  (*qdbp)->valmemp = NULL;

  ThreadUnlock(cft_system);
  return true;
}

/*****************************************************************************/

int QDB_CloseDB(CF_QDB *qdbp)
{
  int res;

  if(qdbp->depot == NULL)
    {
      return false;
    }

  ThreadLock(cft_system);

  if(qdbp->valmemp != NULL)
    {
      free(qdbp->valmemp);
      qdbp->valmemp = NULL;
    }

  res = dpclose(qdbp->depot);

  free(qdbp);

  ThreadUnlock(cft_system);

  return res;
}

/*****************************************************************************/

int QDB_ValueSizeDB(CF_QDB *qdbp, char *key)

{
  if(qdbp->depot == NULL)
    {
      return -1;
    }

  return dpvsiz(qdbp->depot, key, -1);
}

/*****************************************************************************/

int QDB_ReadComplexKeyDB(CF_QDB *qdbp, char *key, int keySz,void *dest, int destSz)
{
  int bytesRead;

  if(qdbp->depot == NULL)
    {
      return false;
    }
  
  bytesRead = dpgetwb(qdbp->depot, key, keySz, 0, destSz, dest);

  if(bytesRead == -1)
    {
      Debug("QDB_ReadComplexKeyDB(%s): Could not read: %s\n", key, dperrmsg(dpecode));
      return false;
    }

  return true;
}

/*****************************************************************************/

int QDB_ReadDB(CF_QDB *qdbp, char *key, void *dest, int destSz)
{
  return QDB_ReadComplexKeyDB(qdbp, key, -1, dest, destSz);
}

/*****************************************************************************/

int QDB_WriteComplexKeyDB(CF_QDB *qdbp, char *key, int keySz, void *src, int srcSz)
{
  if(qdbp->depot == NULL)
    {
      return false;
    }

  if(!dpput(qdbp->depot, key, keySz, src, srcSz, DP_DOVER))
    {
      CfOut(cf_error, "dpput", "!! Could not write key: %s", dperrmsg(dpecode));
      return false;
    }
  
  return true;
}

/*****************************************************************************/

int QDB_WriteDB(CF_QDB *qdbp, char *key, void *src, int srcSz)
{
  return QDB_WriteComplexKeyDB(qdbp, key, -1, src, srcSz);
}

/*****************************************************************************/

int QDB_DeleteComplexKeyDB(CF_QDB *qdbp, char *key, int size)
{
  if(qdbp->depot == NULL)
    {
      return false;
    }
  
  if(!dpout(qdbp->depot, key, size))
    {
      return false;
    }
  
  return true;
}

/*****************************************************************************/

int QDB_DeleteDB(CF_QDB *qdbp, char *key)
{
  return QDB_DeleteComplexKeyDB(qdbp, key, -1);
}

/*****************************************************************************/

int QDB_RevealDB(CF_QDB *qdbp, char *key, void **result, int *rsize)
{
  if(qdbp->depot == NULL)
    {
      return false;
    }

  ThreadLock(cft_system);

  if(qdbp->valmemp != NULL)
    {
      free(qdbp->valmemp);
      qdbp->valmemp = NULL;
    }

  ThreadUnlock(cft_system);

  *result = dpget(qdbp->depot, key, -1, 0, -1, rsize);

  if(*result == NULL)
    {
      Debug("QDB_RevealDB(%s): Could not read: %s\n", key, dperrmsg(dpecode));
      return false;
    }
  
  qdbp->valmemp = *result;  // save mem-pointer for later free

  return true;
}

/*****************************************************************************/

int QDB_NewDBCursor(CF_QDB *qdbp,CF_QDBC **qdbcp)
{
  Debug("Entering QDB_NewDBCursor()\n");

  if(qdbp->depot == NULL)
    {
      return false;
    }

  if(!dpiterinit(qdbp->depot))
    {
      CfOut(cf_error, "dpiterinit", "!! Could not initialize iterator: %s", dperrmsg(dpecode));
      return false;
    }

  ThreadLock(cft_system);

  *qdbcp = malloc(sizeof(CF_QDBC));

  ThreadUnlock(cft_system);

  if(*qdbcp == NULL)
    {
      FatalError("Memory allocation in QDB_NewDBCursor");
    }
  
  (*qdbcp)->curkey = NULL;
  (*qdbcp)->curval = NULL;

  return true;
}

/*****************************************************************************/

int QDB_DeleteDBCursor(CF_QDB *qdbp,CF_QDBC *qdbcp)
{
  Debug("Entering QDB_DeleteDBCursor()\n");
  
  ThreadLock(cft_system);
  
  if(qdbcp->curkey != NULL)
    {
      free(qdbcp->curkey);
      qdbcp->curkey = NULL;
    }

  if(qdbcp->curval != NULL)
    {
      free(qdbcp->curval);
      qdbcp->curval = NULL;
    }

  free(qdbcp);

  ThreadUnlock(cft_system);
  
  return true;
}

/*****************************************************************************/

int QDB_NextDB(CF_QDB *qdbp,CF_QDBC *qdbcp,char **key,int *ksize,void **value,int *vsize)
{
  ThreadLock(cft_system);
  
  if(qdbcp->curkey != NULL)
    {
      free(qdbcp->curkey);
      qdbcp->curkey = NULL;
    }

  if(qdbcp->curval != NULL)
    {
      free(qdbcp->curval);
      qdbcp->curval = NULL;
    }
  
  *key = dpiternext(qdbp->depot, ksize);

  if(*key == NULL)
    {
      ThreadUnlock(cft_system);
      Debug("Got NULL-key in QDB_NextDB()\n");
      return false;
    }
  
  *value = dpget(qdbp->depot, *key, -1, 0, -1, vsize);
  
  if(*value == NULL)
    {
      free(*key);
      *key = NULL;
      ThreadUnlock(cft_system);
      Debug("Got NULL-value in QDB_NextDB()\n");
      return false;
    }
  
  // keep pointers for later free
  qdbcp->curkey = *key;
  qdbcp->curval = *value;

  ThreadUnlock(cft_system);

  return true;
}

#endif
