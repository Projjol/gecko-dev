/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsIComponentManager.h" 
#include "nsMimeObjectClassAccess.h"
#include "nsCOMPtr.h"
#include "nsMsgMimeCID.h"

static NS_DEFINE_CID(kMimeObjectClassAccessCID, NS_MIME_OBJECT_CLASS_ACCESS_CID); 

/*
 * These calls are necessary to expose the object class heirarchy 
 * to externally developed content type handlers.
 */
extern "C" void *
COM_GetmimeInlineTextClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeInlineTextClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeLeafClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeLeafClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeObjectClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeObjectClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeContainerClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeContainerClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeMultipartClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeMultipartClass(&ptr);

  return ptr;
}

extern "C" void *
COM_GetmimeMultipartSignedClass(void)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  void                                *ptr = NULL;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
    objAccess->GetmimeMultipartSignedClass(&ptr);

  return ptr;
}

extern "C" int  
COM_MimeObject_write(void *mimeObject, char *data, PRInt32 length, 
                     PRBool user_visible_p)
{
  nsCOMPtr<nsIMimeObjectClassAccess>  objAccess;
  PRInt32                             rc=-1;

  nsresult res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                                                    NULL, NS_GET_IID(nsIMimeObjectClassAccess), 
                                                    (void **) getter_AddRefs(objAccess)); 
  if (NS_SUCCEEDED(res) && objAccess)
  { 
    if (NS_SUCCEEDED(objAccess->MimeObjectWrite(mimeObject, data, length, user_visible_p)))
      rc = length;
    else
      rc = -1;
  } 

  return rc;
}
