/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Henry Sobotka <sobotka@axess.com>
 * Implementation of nsIFile for OS/2. Freely adapted from the Win32 and
 * UNIX versions, as well as the outgoing nsFileSpecOS2.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/23/2000  IBM Corp.       Updated original version, which was prior to nsIFile drop, to the  latest 
 *                                     Win/Unix versions.
 * 06/09/2000  IBM Corp.       Make more like Windows. (from Doug Turner's windows code).
 *
 */


#include "nsCOMPtr.h"
#include "nsMemory.h"

#include "nsLocalFileOS2.h"

#include "nsISimpleEnumerator.h"
#include "nsIComponentManager.h"
#include "prtypes.h"
#include "prio.h"

#include "prproces.h"

#include <ctype.h>    // needed for toupper
#include <string.h>

static unsigned char* PR_CALLBACK
_mbschr( const unsigned char* stringToSearch, int charToSearchFor);
static unsigned char* PR_CALLBACK
_mbsrchr( const unsigned char* stringToSearch, int charToSearchFor);

#ifdef XP_OS2_VACPP
#include <direct.h>
#include "dirent.h"
#else
#include <unistd.h>
#endif
#include <io.h>


#ifdef XP_OS2
static nsresult ConvertOS2Error(int err)
#else
static nsresult ConvertWinError(DWORD winErr)
#endif
{
    nsresult rv;
    
    switch (err)
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
            rv = NS_ERROR_FILE_NOT_FOUND;
            break;
        case ERROR_ACCESS_DENIED:
        case ERROR_NOT_SAME_DEVICE:
            rv = NS_ERROR_FILE_ACCESS_DENIED;
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_INVALID_BLOCK:
        case ERROR_INVALID_HANDLE:
        case ERROR_ARENA_TRASHED:
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
        case ERROR_CURRENT_DIRECTORY:
            rv = NS_ERROR_FILE_DIR_NOT_EMPTY;
            break;
        case ERROR_WRITE_PROTECT:
            rv = NS_ERROR_FILE_READ_ONLY;
            break;
        case ERROR_HANDLE_DISK_FULL:
            rv = NS_ERROR_FILE_TOO_BIG;
            break;
        case 0:
            rv = NS_OK;
        default:    
            rv = NS_ERROR_FAILURE;
    }
    return rv;
}


static void
myLL_II2L(PRInt32 hi, PRInt32 lo, PRInt64 *result)
{
    PRInt64 a64, b64;  // probably could have been done with 
                       // only one PRInt64, but these are macros, 
                       // and I am a wimp.

    // put hi in the low bits of a64.
    LL_I2L(a64, hi);
    // now shift it to the upperbit and place it the result in result
    LL_SHL(b64, a64, 32);
    // now put the low bits on by adding them to the result.
    LL_ADD(*result, b64, lo);
}


static void
myLL_L2II(PRInt64 result, PRInt32 *hi, PRInt32 *lo )
{
    PRInt64 a64, b64;  // probably could have been done with 
                       // only one PRInt64, but these are macros, 
                       // and I am a wimp.
    
    // shift the hi word to the low word, then push it into a long.
    LL_SHR(a64, result, 32);
    LL_L2I(*hi, a64);

    // shift the low word to the hi word first, then shift it back.
    LL_SHL(b64, result, 32);
    LL_SHR(a64, b64, 32);
    LL_L2I(*lo, a64);
}


class nsDirEnumerator : public nsISimpleEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsDirEnumerator() : mDir(nsnull) 
        {
            NS_INIT_REFCNT();
        }

        nsresult Init(nsILocalFile* parent) 
        {
            char* filepath;
            parent->GetTarget(&filepath);
        
            if (filepath == nsnull)
            {
                parent->GetPath(&filepath);
            }
            
            if (filepath == nsnull)
            {
                return NS_ERROR_OUT_OF_MEMORY;
            }

            mDir = PR_OpenDir(filepath);
            if (mDir == nsnull)    // not a directory?
                return NS_ERROR_FAILURE;
			
			nsMemory::Free(filepath);

            mParent          = parent;    
            return NS_OK;
        }

        NS_IMETHOD HasMoreElements(PRBool *result) 
        {
            nsresult rv;
            if (mNext == nsnull && mDir) 
            {
                PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH);
                if (entry == nsnull) 
                {
                    // end of dir entries

                    PRStatus status = PR_CloseDir(mDir);
                    if (status != PR_SUCCESS)
                        return NS_ERROR_FAILURE;
                    mDir = nsnull;

                    *result = PR_FALSE;
                    return NS_OK;
                }

                nsCOMPtr<nsIFile> file;
                rv = mParent->Clone(getter_AddRefs(file));
                if (NS_FAILED(rv)) 
                    return rv;
                
                rv = file->Append(entry->name);
                if (NS_FAILED(rv)) 
                    return rv;
                
                // make sure the thing exists.  If it does, try the next one.
                PRBool exists;
                    rv = file->Exists(&exists);
                if (NS_FAILED(rv) || !exists) 
                {
                    return HasMoreElements(result); 
                }
                
                mNext = do_QueryInterface(file);
            }
            *result = mNext != nsnull;
            return NS_OK;
        }

        NS_IMETHOD GetNext(nsISupports **result) 
        {
            nsresult rv;
            PRBool hasMore;
            rv = HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            *result = mNext;        // might return nsnull
            NS_IF_ADDREF(*result);
            
            mNext = null_nsCOMPtr();
            return NS_OK;
        }

        virtual ~nsDirEnumerator() 
        {
            if (mDir) 
            {
                PRStatus status = PR_CloseDir(mDir);
                NS_ASSERTION(status == PR_SUCCESS, "close failed");
            }
        }

    protected:
        PRDir*                  mDir;
        nsCOMPtr<nsILocalFile>  mParent;
        nsCOMPtr<nsILocalFile>  mNext;
};

NS_IMPL_ISUPPORTS(nsDirEnumerator, NS_GET_IID(nsISimpleEnumerator));


nsLocalFile::nsLocalFile()
{
    NS_INIT_REFCNT();
      
#ifndef XP_OS2  
    mPersistFile = nsnull;
    mShellLink   = nsnull;
#endif
    mLastResolution = PR_FALSE;
    MakeDirty();
}

nsLocalFile::~nsLocalFile()
{
#ifndef XP_OS2  
    PRBool uninitCOM = PR_FALSE;
    if (mPersistFile || mShellLink)
    {
        uninitCOM = PR_TRUE;
    }
    // Release the pointer to the IPersistFile interface. 
    if (mPersistFile)
        mPersistFile->Release(); 
    
    // Release the pointer to the IShellLink interface. 
    if(mShellLink)
        mShellLink->Release();

    if (uninitCOM)
        CoUninitialize();
#endif
}

/* nsISupports interface implementation. */
NS_IMPL_THREADSAFE_ISUPPORTS2(nsLocalFile, nsILocalFile, nsIFile)

NS_METHOD
nsLocalFile::nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_NO_AGGREGATION(outer);

    nsLocalFile* inst = new nsLocalFile();
    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = inst->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv))
    {
        delete inst;
        return rv;
    }
    return NS_OK;
}

// This function resets any cached information about the file.
void
nsLocalFile::MakeDirty()
{
    mDirty       = PR_TRUE;
}


//----------------------------------------------------------------------------------------
//
// ResolvePath
//  this function will walk the native path of |this| resolving any symbolic
//  links found.  The new resulting path will be placed into mResolvedPath.
//----------------------------------------------------------------------------------------

nsresult 
nsLocalFile::ResolvePath(const char* workingPath, PRBool resolveTerminal, char** resolvedPath)
{
    nsresult rv = NS_OK;
    
#ifndef XP_OS2
    if (mPersistFile == nsnull || mShellLink == nsnull)
    {
        CoInitialize(NULL);  // FIX: we should probably move somewhere higher up during startup

        HRESULT hres; 

        // FIX.  This should be in a service.
        // Get a pointer to the IShellLink interface. 
        hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&mShellLink); 
        if (SUCCEEDED(hres)) 
        { 
            // Get a pointer to the IPersistFile interface. 
            hres = mShellLink->QueryInterface(IID_IPersistFile, (void**)&mPersistFile); 
        }
        
        if (mPersistFile == nsnull || mShellLink == nsnull)
        {
            return NS_ERROR_FILE_INVALID_PATH;
        }
    }
#endif
        
    
    // Get the native path for |this|
    char* filePath = (char*) nsMemory::Clone( workingPath, strlen(workingPath)+1 );

    if (filePath == nsnull)
        return NS_ERROR_NULL_POINTER;
    
    // We are going to walk the native file path
    // and stop at each slash.  For each partial
    // path (the string to the left of the slash)
    // we will check to see if it is a shortcut.
    // if it is, we will resolve it and continue
    // with that resolved path.
    
    // Get the first slash.          
    char* slash = strchr(filePath, '\\');
            
    if (slash == nsnull)
    {
        if (filePath[0] != nsnull && filePath[1] == ':' && filePath[2] == '\0')
        {
            // we have a drive letter and a colon (eg 'c:'
            // this is resolve already
            
            *resolvedPath = (char*) nsMemory::Clone( filePath, strlen(filePath)+2 );
            strcat(*resolvedPath, "\\");

            nsMemory::Free(filePath);
            return NS_OK;
        }
        else
        {
            nsMemory::Free(filePath);
            return NS_ERROR_FILE_INVALID_PATH;
        }
    }
        

    // We really cant have just a drive letter as
    // a shortcut, so we will skip the first '\\'
    slash = strchr(++slash, '\\');
    
    while (slash || resolveTerminal)
    {
        // Change the slash into a null so that
        // we can use the partial path. It is is 
        // null, we know it is the terminal node.
        
        if (slash)
        {
            *slash = '\0';
        }
        else
        {
            if (resolveTerminal)
            {
                // this is our last time in this loop.
                // set loop condition to false

                resolveTerminal = PR_FALSE;
            }
            else
            {
                // something is wrong.  we should not have
                // both slash being null and resolveTerminal 
                // not set!
                nsMemory::Free(filePath);
                return NS_ERROR_NULL_POINTER;
            }
        }
#ifndef XP_OS2
      
        WORD wsz[MAX_PATH];     // TODO, Make this dynamically allocated.

        // check to see the file is a shortcut by the magic .lnk extension.
        size_t offset = strlen(filePath) - 4;
        if ((offset > 0) && (strncmp( (filePath + offset), ".lnk", 4) == 0))
        {
            MultiByteToWideChar(CP_ACP, 0, filePath, -1, wsz, MAX_PATH); 
        }        
        else
        {
            char linkStr[MAX_PATH];
            strcpy(linkStr, filePath);
            strcat(linkStr, ".lnk");

            // Ensure that the string is Unicode. 
            MultiByteToWideChar(CP_ACP, 0, linkStr, -1, wsz, MAX_PATH); 
        }        

        HRESULT hres; 
        
        // see if we can Load the path.
        hres = mPersistFile->Load(wsz, STGM_READ); 

        if (SUCCEEDED(hres)) 
        {
            // Resolve the link. 
            hres = mShellLink->Resolve(nsnull, SLR_NO_UI ); 
            if (SUCCEEDED(hres)) 
            { 
                WIN32_FIND_DATA wfd; 
                
                char *temp = (char*) nsMemory::Alloc( MAX_PATH );
                if (temp == nsnull)
                    return NS_ERROR_NULL_POINTER;
                
                // Get the path to the link target. 
                hres = mShellLink->GetPath( temp, MAX_PATH, &wfd, SLGP_UNCPRIORITY ); 

                if (SUCCEEDED(hres))
                {
                    // found a new path.
                    
                    // addend a slash on it since it does not come out of GetPath()
                    // with one only if it is a directory.  If it is not a directory
                    // and there is more to append, than we have a problem.
                    
                    struct stat st;
                    int statrv = stat(temp, &st);
                    
                    if (0 == statrv && (_S_IFDIR & st.st_mode))
                    {
                        strcat(temp, "\\");
                    }
                                       
                    if (slash)
                    {
                        // save where we left off.
                        char *carot= (temp + strlen(temp) -1 );

                        // append all the stuff that we have not done.
                        strcat(temp, ++slash);
                        
                        slash = carot;
                    }

                    nsMemory::Free(filePath);
                    filePath = temp;
            
                }
                else
                {
                    nsMemory::Free(temp);
                }
            }
            else
            {
                // could not resolve shortcut.  Return error;
                nsMemory::Free(filePath);
                return NS_ERROR_FILE_INVALID_PATH;
            }
        }
#endif
    
        if (slash)
        {
            *slash = '\\';
            ++slash;
            slash = strchr(slash, '\\');
        }
    }

    // kill any trailing seperator
    char* temp = filePath;
    int len = strlen(temp) - 1;
    if(temp[len] == '\\')
        temp[len] = '\0';
    
    *resolvedPath = filePath;
    return rv;
}

nsresult
nsLocalFile::ResolveAndStat(PRBool resolveTerminal)
{
    if (!mDirty && mLastResolution == resolveTerminal)
    {
        return NS_OK;
    }
    mLastResolution = resolveTerminal;

    // First we will see if the workingPath exists.  If it does, then we
    // can simply use that as the resolved path.  This simplification can
    // be done on windows cause its symlinks (shortcuts) use the .lnk
    // file extension.

    char temp[4];
    const char* workingFilePath = mWorkingPath.GetBuffer();
    const char* nsprPath = workingFilePath;

    if (mWorkingPath.Length() == 2 && mWorkingPath.CharAt(1) == ':') {
        temp[0] = workingFilePath[0];
        temp[1] = workingFilePath[1];
        temp[2] = '\\';
        temp[3] = '\0';
        nsprPath = temp;
    }

    PRStatus status = PR_GetFileInfo64(nsprPath, &mFileInfo64);
    if ( status == PR_SUCCESS )
    {
        mResolvedPath.Assign(workingFilePath);
		mDirty = PR_FALSE;
		return NS_OK;
    }
#ifdef XP_OS2
    else
    {
        mResolvedPath.Assign(workingFilePath);
        return(NS_ERROR_FILE_NOT_FOUND);
    } 
#else

    nsresult result;

    // okay, something is wrong with the working path.  We will try to resolve it.

    char *resolvePath;
    
	result = ResolvePath(workingFilePath, resolveTerminal, &resolvePath);
    if (NS_FAILED(result))
       return result;
    
	mResolvedPath.Assign(resolvePath);
    nsMemory::Free(resolvePath);

    // if we are not resolving the terminal node, we have to "fake" windows
    // out and append the ".lnk" file extension before getting any information
    // about the shortcut.  If resoveTerminal was TRUE, than it the shortcut was
    // resolved by the call to ResolvePath above.

    
    char linkStr[MAX_PATH];
    strcpy(linkStr, mResolvedPath.GetBuffer());
    strcat(linkStr, ".lnk");
    
    status = PR_GetFileInfo64(linkStr, &mFileInfo64);
    
    if ( status == PR_SUCCESS )
		mDirty = PR_FALSE;
    else
        result = NS_ERROR_FILE_NOT_FOUND;

	return result;
#endif
}

NS_IMETHODIMP  
nsLocalFile::Clone(nsIFile **file)
{
    nsresult rv;
    char * aFilePath;
    GetPath(&aFilePath);

    nsCOMPtr<nsILocalFile> localFile;

    rv = NS_NewLocalFile(aFilePath, getter_AddRefs(localFile));
    nsMemory::Free(aFilePath);
    
    if (NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)file);
    }
            
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::InitWithPath(const char *filePath)
{
    MakeDirty();
    NS_ENSURE_ARG(filePath);
    
    char* nativeFilePath = nsnull;
    
    // just do a sanity check.  if it has any forward slashes, it is not a Native path
    // on windows.  Also, it must have a colon at after the first char.
    if (filePath[0] == 0)
        return NS_ERROR_FAILURE;
    
    if ( (filePath[2] == 0) && (filePath[1] == ':') )
    {
        nativeFilePath = (char*) nsMemory::Clone( filePath, 4 );
        // C : //
        nativeFilePath[2] = '\\';
    }

    if ( ( (filePath[1] == ':')  && (strchr(filePath, '/') == 0) ) ||  // normal windows path
         ( (filePath[0] == '\\') && (filePath[1] == '\\') ) )  // netwerk path
    {
        // This is a native path
        nativeFilePath = (char*) nsMemory::Clone( filePath, strlen(filePath)+1 );
    }
    
    if (nativeFilePath == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // kill any trailing seperator
    char* temp = nativeFilePath;
    int len = strlen(temp) - 1;
#ifdef XP_OS2
    if(temp[len] == '\\')    // OS2TODO - fix this for DBCS
        temp[len] = '\0';
#else
    // Is '\' second charactor of DBCS?
    if(temp[len] == '\\' && !::IsDBCSLeadByte(temp[len-1]))
        temp[len] = '\0';
#endif
    
    mWorkingPath.Assign(nativeFilePath);
    nsMemory::Free( nativeFilePath );
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv; 
   
    *_retval = PR_Open(mResolvedPath, flags, mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ErrorAccordingToNSPR();
}


NS_IMETHODIMP  
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv; 
   
    *_retval = fopen(mResolvedPath, mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}



NS_IMETHODIMP  
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{ 
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    nsresult rv = ResolveAndStat(PR_FALSE);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;  
    
   // create nested directories to target
    unsigned char* slash = _mbschr((const unsigned char*) mResolvedPath.GetBuffer(), '\\');
    // skip the first '\\'
    ++slash;
    slash = _mbschr(slash, '\\');

    while (slash)
    {
        *slash = '\0';

#ifdef XP_OS2
#if 0
        int ret;
        ret = mkdir(mResolvedPath, attributes); 
#endif

        DosCreateDir((PSZ) mResolvedPath, NULL);  // OS2TODO: pass back the result
#else
        CreateDirectoryA(mResolvedPath, NULL);// todo: pass back the result
#endif

        *slash = '\\';
        ++slash;
        slash = _mbschr(slash, '\\');
    }


    if (type == NORMAL_FILE_TYPE)
    {
        PRFileDesc* file = PR_Open(mResolvedPath, PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL, attributes);
        if (!file) return NS_ERROR_FILE_ALREADY_EXISTS;
          
        PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
#ifdef XP_OS2

        DosCreateDir((PSZ) mResolvedPath, NULL);  // OS2TODO: pass back the result
        return NS_OK;

#else
	    CreateDirectoryA(mResolvedPath, NULL); // todo: pass back the result
        return NS_OK;
#endif
    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}
    
NS_IMETHODIMP  
nsLocalFile::Append(const char *node)
{
    // Append only one component. Check for subdirs.
    if (!node || (_mbschr((const unsigned char*) node, '\\') != nsnull))
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    return AppendRelativePath(node);
}

NS_IMETHODIMP  
nsLocalFile::AppendRelativePath(const char *node)
{
    // Cannot start with a / or have .. or have / anywhere
    if (!node || (*node == '/') || (strstr(node, "..") != nsnull) ||
        (strchr(node, '/') != nsnull))
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    MakeDirty();
    mWorkingPath.Append("\\");
    mWorkingPath.Append(node);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLeafName(char * *aLeafName)
{
    NS_ENSURE_ARG_POINTER(aLeafName);

    const char* temp = mWorkingPath.GetBuffer();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp, '\\');
    
    // if the working path is just a node without any lashes.
    if (leaf == nsnull)
        leaf = temp;
    else
        leaf++;

    *aLeafName = (char*) nsMemory::Clone(leaf, strlen(leaf)+1);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLeafName(const char * aLeafName)
{
    MakeDirty();
    
    const unsigned char* temp = (const unsigned char*) mWorkingPath.GetBuffer();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = (PRInt32) (_mbsrchr(temp, '\\') - temp);
    if (offset)
    {
        mWorkingPath.Truncate(offset+1);
    }
    mWorkingPath.Append(aLeafName);

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetPath(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
#ifdef XP_OS2_VACPP
    *_retval = (char*) nsMemory::Clone((void *)mWorkingPath, strlen(mWorkingPath)+1);
#else
    *_retval = (char*) nsMemory::Clone(mWorkingPath, strlen(mWorkingPath)+1);
#endif
    return NS_OK;
}

nsresult
nsLocalFile::CopySingleFile(nsIFile *sourceFile, nsIFile *destParent, const char * newName, PRBool followSymlinks, PRBool move)
{
    nsresult rv;
    char* filePath;

    // get the path that we are going to copy to.
    // Since windows does not know how to auto
    // resolve shortcust, we must work with the
    // target.
    char* inFilePath;
    destParent->GetTarget(&inFilePath);  
    nsCString destPath = inFilePath;
    nsMemory::Free(inFilePath);

    destPath.Append("\\");

    if (newName == nsnull)
    {
        char *aFileName;
        sourceFile->GetLeafName(&aFileName);
        destPath.Append(aFileName);
        nsMemory::Free(aFileName);
    }
    else
    {
        destPath.Append(newName);
    }

           
    if (followSymlinks)
    {
        rv = sourceFile->GetTarget(&filePath);
        if (!filePath)
            rv = sourceFile->GetPath(&filePath);
    }
    else
    {
        rv = sourceFile->GetPath(&filePath);
    }

    if (NS_FAILED(rv))
        return rv;

    int copyOK;

    if (!move)
#ifdef XP_OS2
    {
        rv = DosCopy(filePath, (PSZ)destPath, DCPY_EXISTING);

        if (NS_FAILED(rv))
            copyOK = PR_FALSE;
        else
            copyOK = PR_TRUE;
    } 
#else  
        copyOK = CopyFile(filePath, destPath, PR_TRUE);
#endif
    else
#ifdef XP_OS2
   {
        rv = DosMove(filePath, (PSZ)destPath);
        if (NS_FAILED(rv))
            copyOK = PR_FALSE;
        else
            copyOK = PR_TRUE;
   }
#else  
        copyOK = MoveFile(filePath, destPath);
#endif
    
    if (!copyOK)  // CopyFile and MoveFile returns non-zero if succeeds (backward if you ask me).
#ifdef XP_OS2
        // rv already contains DosCopy return code (do we need to convert it?) OS2TODO
        rv = ConvertOS2Error(rv);
#else
        rv = ConvertWinError(GetLastError());
#endif
    
    nsMemory::Free(filePath);

    return rv;
}


nsresult
nsLocalFile::CopyMove(nsIFile *aParentDir, const char *newName, PRBool followSymlinks, PRBool move)
{
    nsCOMPtr<nsIFile> newParentDir = aParentDir;
    // check to see if this exists, otherwise return an error.
    // we will check this by resolving.  If the user wants us
    // to follow links, then we are talking about the target,
    // hence we can use the |followSymlinks| parameter.
    nsresult rv  = ResolveAndStat(followSymlinks);
    if (NS_FAILED(rv))
        return rv;

    if (!newParentDir)
    {
        // no parent was specified.  We must rename.
        
        if (!newName)
            return NS_ERROR_INVALID_ARG;

        move = PR_TRUE;
        
        rv = GetParent(getter_AddRefs(newParentDir));
        if (NS_FAILED(rv))
            return rv;
    }

    if (!newParentDir)
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
    
    // make sure it exists and is a directory.  Create it if not there.
    PRBool exists;
    newParentDir->Exists(&exists);
    if (exists == PR_FALSE)
    {
        rv = newParentDir->Create(DIRECTORY_TYPE, 0644);  // TODO, what permissions should we use
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        PRBool isDir;
        newParentDir->IsDirectory(&isDir);
        if (isDir == PR_FALSE)
        {
            if (followSymlinks)
            {
                PRBool isLink;
                newParentDir->IsSymlink(&isLink);
                if (isLink)
                {
                    char* target;
                    newParentDir->GetTarget(&target);

                    nsCOMPtr<nsILocalFile> realDest = new nsLocalFile();
                    if (realDest == nsnull)
                        return NS_ERROR_OUT_OF_MEMORY;

                    rv = realDest->InitWithPath(target);
                    
                    nsMemory::Free(target);

                    if (NS_FAILED(rv)) 
                        return rv;
                    
                    return CopyMove(realDest, newName, followSymlinks, move);
                }
            }
            else
            {                
                return NS_ERROR_FILE_DESTINATION_NOT_DIR;
            }
        }
    }

    // check to see if we are a directory, if so enumerate it.

    PRBool isDir;
    IsDirectory(&isDir);
    PRBool isSymlink;
    IsSymlink(&isSymlink);

    if (!isDir || (isSymlink && !followSymlinks))
    {
        rv = CopySingleFile(this, newParentDir, newName, followSymlinks, move);
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        // create a new target destination in the new parentDir;
        nsCOMPtr<nsIFile> target;
        rv = newParentDir->Clone(getter_AddRefs(target));
        
        if (NS_FAILED(rv)) 
            return rv;
        
        char *allocatedNewName;
        if (!newName)
        {
            PRBool isLink;
            IsSymlink(&isLink);
            if (isLink)
            {
                char* temp;
                GetTarget(&temp);
                const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp, '\\');
                if (leaf[0] == '\\')
                    leaf++;
                allocatedNewName = (char*) nsMemory::Clone( leaf, strlen(leaf)+1 );
            }
            else
            {
                GetLeafName(&allocatedNewName);// this should be the leaf name of the 
            }
        }
        else
        {
            allocatedNewName = (char*) nsMemory::Clone( newName, strlen(newName)+1 );
        }
        
        rv = target->Append(allocatedNewName);
        if (NS_FAILED(rv)) 
            return rv;

        nsMemory::Free(allocatedNewName);

        target->Create(DIRECTORY_TYPE, 0644);  // TODO, what permissions should we use
        if (NS_FAILED(rv))
            return rv;
        
        nsDirEnumerator* dirEnum = new nsDirEnumerator();
        if (!dirEnum)
            return NS_ERROR_OUT_OF_MEMORY;
        
        rv = dirEnum->Init(this);

        nsCOMPtr<nsISimpleEnumerator> iterator = do_QueryInterface(dirEnum);

        PRBool more;
        iterator->HasMoreElements(&more);
        while (more)
        {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIFile> file;
            iterator->GetNext(getter_AddRefs(item));
            file = do_QueryInterface(item);
            PRBool isDir, isLink;
            
            file->IsDirectory(&isDir);
            file->IsSymlink(&isLink);

            if (move)
            {
                if (followSymlinks)
                    rv = NS_ERROR_FAILURE;
                else
                    rv = file->MoveTo(target, nsnull);
            }
            else
            {   
                if (followSymlinks)
                    rv = file->CopyToFollowingLinks(target, nsnull);
                else
                    rv = file->CopyTo(target, nsnull);
            }
                    
            iterator->HasMoreElements(&more);
        }
    }
    

    // If we moved, we want to adjust this.
    if (move)
    {
        MakeDirty();
        
        char* newParentPath;
        newParentDir->GetPath(&newParentPath);
        
        if (newParentPath == nsnull)
            return NS_ERROR_FAILURE;

        if (newName == nsnull)
        {
            char *aFileName;
            GetLeafName(&aFileName);
            
            InitWithPath(newParentPath);
            Append(aFileName); 

            nsMemory::Free(aFileName);
        }
        else
        {
            InitWithPath(newParentPath);
            Append(newName);
        }
        
        nsMemory::Free(newParentPath);
    }
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::CopyTo(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::MoveTo(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_TRUE);
}

NS_IMETHODIMP  
nsLocalFile::Spawn(const char **args, PRUint32 count)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;

    // make sure that when we allocate we have 1 greater than the
    // count since we need to null terminate the list for the argv to
    // pass into PR_CreateProcessDetached
    char **my_argv = NULL;
    
    my_argv = (char **)malloc(sizeof(char *) * (count + 2) );
    if (!my_argv) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // copy the args
    PRUint32 i;
    for (i=0; i < count; i++) {
        my_argv[i+1] = (char *)args[i];
    }
    // we need to set argv[0] to the program name.
    my_argv[0] = mResolvedPath;
    
    // null terminate the array
    my_argv[count+1] = NULL;
    rv = PR_CreateProcessDetached(mResolvedPath, my_argv, NULL, NULL);

     // free up our argv
     nsMemory::Free(my_argv);

     if (PR_SUCCESS == rv)
         return NS_OK;
     else
         return NS_ERROR_FILE_EXECUTION_FAILED;
}

NS_IMETHODIMP  
nsLocalFile::Load(PRLibrary * *_retval)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;
    
    if (! isFile)
        return NS_ERROR_FILE_IS_DIRECTORY;

    *_retval =  PR_LoadLibrary(mResolvedPath);
    
    if (*_retval)
        return NS_OK;

    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP  
nsLocalFile::Delete(PRBool recursive)
{
    PRBool isDir;
    
    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;

    const char *filePath = mResolvedPath.GetBuffer();

    if (isDir)
    {
        if (recursive)
        {
            nsDirEnumerator* dirEnum = new nsDirEnumerator();
            if (dirEnum == nsnull)
                return NS_ERROR_OUT_OF_MEMORY;
        
            rv = dirEnum->Init(this);

            nsCOMPtr<nsISimpleEnumerator> iterator = do_QueryInterface(dirEnum);
        
            PRBool more;
            iterator->HasMoreElements(&more);
            while (more)
            {
                nsCOMPtr<nsISupports> item;
                nsCOMPtr<nsIFile> file;
                iterator->GetNext(getter_AddRefs(item));
                file = do_QueryInterface(item);
    
                file->Delete(recursive);
                
                iterator->HasMoreElements(&more);
            }
        }
#ifdef XP_OS2_VACPP
        rmdir((char *) filePath);  // todo: save return value?
#else
        rmdir(filePath);  // todo: save return value?
#endif
    }
    else
    {
        remove(filePath); // todo: save return value?
    }
    
    MakeDirty();
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDate(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    
    *aLastModificationDate = 0;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    // microseconds -> milliseconds
    *aLastModificationDate = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetLastModificationDateOfLink(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    
    *aLastModificationDate = 0;

    nsresult rv = ResolveAndStat(PR_FALSE);
    
    if (NS_FAILED(rv))
        return rv;
    
    // microseconds -> milliseconds
    *aLastModificationDate = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::SetLastModificationDate(PRInt64 aLastModificationDate)
{
    return nsLocalFile::SetModDate(aLastModificationDate, PR_TRUE);
}


NS_IMETHODIMP  
nsLocalFile::SetLastModificationDateOfLink(PRInt64 aLastModificationDate)
{
    return nsLocalFile::SetModDate(aLastModificationDate, PR_FALSE);
}

nsresult
nsLocalFile::SetModDate(PRInt64 aLastModificationDate, PRBool resolveTerminal)
{
    nsresult rv = ResolveAndStat(resolveTerminal);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.GetBuffer();
    
#ifndef XP_OS2
    HANDLE file = CreateFile(  filePath,          // pointer to name of the file
                               GENERIC_WRITE,     // access (write) mode
                               0,                 // share mode
                               NULL,              // pointer to security attributes
                               OPEN_EXISTING,     // how to create
                               0,                 // file attributes 
                               NULL);

    MakeDirty();

    if (!file)
    {
        return ConvertWinError(GetLastError());
    }
#endif

#ifdef XP_OS2
    PRExplodedTime pret;
    FILESTATUS3 pathInfo;

    rv = DosQueryPathInfo(filePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));

    if (NS_FAILED(rv))
    {
       rv = ConvertOS2Error(rv);
       return rv;
    }
    
    // PR_ExplodeTime expects usecs...
    PR_ExplodeTime(aLastModificationDate * PR_USEC_PER_MSEC, PR_LocalTimeParameters, &pret);
    pathInfo.fdateLastWrite.year      = pret.tm_year;    
    pathInfo.fdateLastWrite.month    = pret.tm_month + 1; // Convert start offset -- Win32: Jan=1; NSPR: Jan=0
// ???? OS2TODO    st.wDayOfWeek       = pret.tm_wday;    
    pathInfo.fdateLastWrite.day        = pret.tm_mday;    
    pathInfo.ftimeLastWrite.hours      = pret.tm_hour;
    pathInfo.ftimeLastWrite.minutes   = pret.tm_min;    
    pathInfo.ftimeLastWrite.twosecs   = pret.tm_sec / 2;   // adjust for twosecs?
// ??? OS2TODO    st.wMilliseconds    = pret.tm_usec/1000;

    rv = DosSetPathInfo(filePath, 
                                 FIL_STANDARD,             // Level 1 info
                                 &pathInfo, 
                                 sizeof(pathInfo),
                                 0UL);
                               
    if (NS_FAILED(rv))
       return rv;

    MakeDirty();
#else  

    FILETIME lft, ft;
    SYSTEMTIME st;
    PRExplodedTime pret;
    
    // PR_ExplodeTime expects usecs...
    PR_ExplodeTime(aLastModificationDate * PR_USEC_PER_MSEC, PR_LocalTimeParameters, &pret);
    st.wYear            = pret.tm_year;    
    st.wMonth           = pret.tm_month + 1; // Convert start offset -- Win32: Jan=1; NSPR: Jan=0
    st.wDayOfWeek       = pret.tm_wday;    
    st.wDay             = pret.tm_mday;    
    st.wHour            = pret.tm_hour;
    st.wMinute          = pret.tm_min;    
    st.wSecond          = pret.tm_sec;
    st.wMilliseconds    = pret.tm_usec/1000;

    if ( 0 == SystemTimeToFileTime(&st, &lft) )
    {
        rv = ConvertWinError(GetLastError());
    }
    else if ( 0 == LocalFileTimeToFileTime(&lft, &ft) )
    {
        rv = ConvertWinError(GetLastError());
    }
    else if ( 0 == SetFileTime(file, NULL, &ft, &ft) )
    {
        // could not set time
        rv = ConvertWinError(GetLastError());
    }

    CloseHandle( file );
#endif
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.GetBuffer();


    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP  
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.GetBuffer();
    if( chmod(filePath, aPermissions) == -1 )
        return NS_ERROR_FAILURE;
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    nsresult rv = ResolveAndStat(PR_FALSE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.GetBuffer();
    if( chmod(filePath, aPermissions) == -1 )
        return NS_ERROR_FAILURE;
        
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    
    *aFileSize = 0;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    

    *aFileSize = mFileInfo64.size;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{

#ifndef XP_OS2
    DWORD status;
    HANDLE hFile;
#endif

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.GetBuffer();


#ifdef XP_OS2   
    APIRET rc;
    HFILE hFile;
    ULONG actionTaken;

    rc = DosOpen(filePath,
                       &hFile,
                       &actionTaken,
                       0,                 
                       FILE_NORMAL,
                       OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                       OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
                       NULL);
                                 
    if (rc != NO_ERROR)
    {
        MakeDirty();
        return NS_ERROR_FAILURE;
    }
#else
    // Leave it to Microsoft to open an existing file with a function
    // named "CreateFile".
    hFile = CreateFile(filePath,
                       GENERIC_WRITE, 
                       FILE_SHARE_READ, 
                       NULL, 
                       OPEN_EXISTING, 
                       FILE_ATTRIBUTE_NORMAL, 
                       NULL); 

    if (hFile == INVALID_HANDLE_VALUE)
    {
        MakeDirty();
        return NS_ERROR_FAILURE;
    }
#endif   
    
    // Seek to new, desired end of file
    PRInt32 hi, lo;
    myLL_L2II(aFileSize, &hi, &lo );

#ifdef XP_OS2
    rc = DosSetFileSize(hFile, lo);
    if (rc == NO_ERROR) 
       DosClose(hFile);
    else
       goto error; 
#else   
    status = SetFilePointer(hFile, lo, NULL, FILE_BEGIN);
    if (status == 0xffffffff)
        goto error;

    // Truncate file at current cursor position
    if (!SetEndOfFile(hFile))
        goto error;

    if (!CloseHandle(hFile))
        return NS_ERROR_FAILURE;
#endif

    MakeDirty();
    return NS_OK;

 error:
    MakeDirty();
#ifdef XP_OS2
    DosClose(hFile);
#else   
    CloseHandle(hFile);
#endif
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP  
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    
    *aFileSize = 0;

    nsresult rv = ResolveAndStat(PR_FALSE);
    
    if (NS_FAILED(rv))
        return rv;
    
    *aFileSize = mFileInfo64.size;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG(aDiskSpaceAvailable);
    
    ResolveAndStat(PR_FALSE);
    
#ifdef XP_OS2
    ULONG ulDriveNo = toupper(mResolvedPath.CharAt(0)) + 1 - 'A';
    FSALLOCATE fsAllocate;
    APIRET rv = DosQueryFSInfo(ulDriveNo,
                                            FSIL_ALLOC,
                                            &fsAllocate,
                                            sizeof(fsAllocate));

    if (NS_FAILED(rv)) 
        return rv;

    *aDiskSpaceAvailable = (PRInt64)(fsAllocate.cUnitAvail  *
                                                    fsAllocate.cSectorUnit *
                                                    (ULONG)fsAllocate.cbSector);
#else   
    const char *filePath = mResolvedPath.GetBuffer();

    PRInt64 int64;
    
    LL_I2L(int64 , LONG_MAX);

    char aDrive[_MAX_DRIVE + 2];
	_splitpath( (const char*)filePath, aDrive, NULL, NULL, NULL);
	strcat(aDrive, "\\");

    // Check disk space
    DWORD dwSecPerClus, dwBytesPerSec, dwFreeClus, dwTotalClus;
    ULARGE_INTEGER liFreeBytesAvailableToCaller, liTotalNumberOfBytes, liTotalNumberOfFreeBytes;
    double nBytes = 0;

    BOOL (WINAPI* getDiskFreeSpaceExA)(LPCTSTR lpDirectoryName, 
                                       PULARGE_INTEGER lpFreeBytesAvailableToCaller,
                                       PULARGE_INTEGER lpTotalNumberOfBytes,    
                                       PULARGE_INTEGER lpTotalNumberOfFreeBytes) = NULL;

    HINSTANCE hInst = LoadLibrary("KERNEL32.DLL");
    NS_ASSERTION(hInst != NULL, "COULD NOT LOAD KERNEL32.DLL");
    if (hInst != NULL)
    {
        getDiskFreeSpaceExA =  (BOOL (WINAPI*)(LPCTSTR lpDirectoryName, 
                                               PULARGE_INTEGER lpFreeBytesAvailableToCaller,
                                               PULARGE_INTEGER lpTotalNumberOfBytes,    
                                               PULARGE_INTEGER lpTotalNumberOfFreeBytes)) 
        GetProcAddress(hInst, "GetDiskFreeSpaceExA");
        FreeLibrary(hInst);
    }

    if (getDiskFreeSpaceExA && (*getDiskFreeSpaceExA)(aDrive,
                                                      &liFreeBytesAvailableToCaller, 
                                                      &liTotalNumberOfBytes,  
                                                      &liTotalNumberOfFreeBytes))
    {
        nBytes = (double)(signed __int64)liFreeBytesAvailableToCaller.QuadPart;
    }
    else if ( GetDiskFreeSpace(aDrive, &dwSecPerClus, &dwBytesPerSec, &dwFreeClus, &dwTotalClus))
    {
        nBytes = (double)dwFreeClus*(double)dwSecPerClus*(double) dwBytesPerSec;
    }

    LL_D2L(*aDiskSpaceAvailable, nBytes);
#endif

    return NS_OK;

}

NS_IMETHODIMP  
nsLocalFile::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    nsCString parentPath = mWorkingPath;

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = (PRInt32) (_mbsrchr((const unsigned char *) parentPath.GetBuffer(), '\\')
                     - (const unsigned char *) parentPath.GetBuffer());
    if (offset == -1)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    parentPath.Truncate(offset);

    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv =  NS_NewLocalFile(parentPath.GetBuffer(), getter_AddRefs(localFile));
    
    if(NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)aParent);
    }
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::Exists(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    MakeDirty();
    nsresult rv = ResolveAndStat( PR_TRUE );
    
    if (NS_SUCCEEDED(rv))
        *_retval = PR_TRUE;
    else 
        *_retval = PR_FALSE;
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsWritable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
   
    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;  
    
    const char *workingFilePath = mWorkingPath.GetBuffer();

#ifdef XP_OS2
    APIRET rc;
    FILESTATUS3 pathInfo;

    rc = DosQueryPathInfo(workingFilePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));
                               
    if (rc != NO_ERROR) 
    {
       rc = ConvertOS2Error(rc);
       return rc;
    }

    *_retval =  !((pathInfo.attrFile & FILE_READONLY)  != 0); 
#else  
    DWORD word = GetFileAttributes(workingFilePath);

    *_retval = !((word & FILE_ATTRIBUTE_READONLY) != 0); 
#endif

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsReadable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat( PR_TRUE );
    if (NS_FAILED(rv))
        return rv;

    *_retval = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsExecutable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;


    nsresult rv = ResolveAndStat( PR_TRUE );
    if (NS_FAILED(rv))
        return rv;

    char* path = nsnull;
    PRBool symLink;
    
    rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;
    
    if (symLink)
        GetTarget(&path);
    else
        GetPath(&path);

    const char* leaf = (const char*) _mbsrchr((const unsigned char*) path, '\\');

    // XXX On Windows NT / 2000, it should use "PATHEXT" environment value
#ifdef XP_OS2
    if ( (strstr(leaf, ".bat") != nsnull) ||
         (strstr(leaf, ".exe") != nsnull) ||
         (strstr(leaf, ".cmd") != nsnull) ||
         (strstr(leaf, ".com") != nsnull) ) {
#else
    if ( (strstr(leaf, ".bat") != nsnull) ||
         (strstr(leaf, ".exe") != nsnull) ) {
#endif
        *_retval = PR_TRUE;
    } else {
        *_retval = PR_FALSE;
    }
    
    nsMemory::Free(path);
    
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsDirectory(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_DIRECTORY); 

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_FILE); 
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::IsHidden(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *workingFilePath = mWorkingPath.GetBuffer();

#ifdef XP_OS2
    APIRET rc;
    FILESTATUS3 pathInfo;

    rc = DosQueryPathInfo(workingFilePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));
                               
    if (rc != NO_ERROR) 
    {
       rc = ConvertOS2Error(rc);
       return rc;
    }

    *_retval =  ((pathInfo.attrFile & FILE_HIDDEN)  != 0); 
    
#else  
    DWORD word = GetFileAttributes(workingFilePath);

    *_retval =  ((word & FILE_ATTRIBUTE_HIDDEN)  != 0); 
#endif

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

#ifndef XP_OS2   // No Symlinks on OS/2
    char* path;
    int   pathLen;
    
    GetPath(&path);
    pathLen = strlen(path);
    
    const char* leaf = path + pathLen - 4;
    
    if ( (strcmp(leaf, ".lnk") == 0)) 
    {
        *_retval = PR_TRUE;
    }
    
    nsMemory::Free(path);
#endif
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsSpecial(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *workingFilePath = mWorkingPath.GetBuffer();

#ifdef XP_OS2
    APIRET rc;
    FILESTATUS3 pathInfo;

    rc = DosQueryPathInfo(workingFilePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));
                               
    if (rc != NO_ERROR) 
    {
       rc = ConvertOS2Error(rc);
       return rc;
    } 

    *_retval =  ((pathInfo.attrFile & FILE_SYSTEM)  != 0); 
#else  
    DWORD word = GetFileAttributes(workingFilePath);

    *_retval = ((word & FILE_ATTRIBUTE_SYSTEM)  != 0); 
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    char* inFilePath;
    inFile->GetPath(&inFilePath);
    
    char* filePath;
    GetPath(&filePath);

    if (strcmp(inFilePath, filePath) == 0)
        *_retval = PR_TRUE;
    
    nsMemory::Free(inFilePath);
    nsMemory::Free(filePath);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    *_retval = PR_FALSE;
       
    char* myFilePath;
    if ( NS_FAILED(GetTarget(&myFilePath)))
        GetPath(&myFilePath);
    
    PRInt32 myFilePathLen = strlen(myFilePath);
    
    char* inFilePath;
    if ( NS_FAILED(inFile->GetTarget(&inFilePath)))
        inFile->GetPath(&inFilePath);

    if ( strncmp( myFilePath, inFilePath, myFilePathLen) == 0)
    {
        // now make sure that the |inFile|'s path has a trailing
        // separator.

        if (inFilePath[myFilePathLen] == '\\')
        {
            *_retval = PR_TRUE;
        }

    }
        
    nsMemory::Free(inFilePath);
    nsMemory::Free(myFilePath);

    return NS_OK;
}



NS_IMETHODIMP
nsLocalFile::GetTarget(char **_retval)
{   
    NS_ENSURE_ARG(_retval);
    *_retval = nsnull;
#if STRICT_FAKE_SYMLINKS    
    PRBool symLink;
    
    nsresult rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;

    if (!symLink)
    {
        return NS_ERROR_FILE_INVALID_PATH;
    }
#endif
    ResolveAndStat(PR_TRUE);
 
#ifdef XP_OS2_VACPP
    *_retval = (char*) nsMemory::Clone( (void *)mResolvedPath, strlen(mResolvedPath)+1 );
#else       
    *_retval = (char*) nsMemory::Clone( mResolvedPath, strlen(mResolvedPath)+1 );
#endif
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator * *entries)
{
    nsresult rv;
    
    *entries = nsnull;

    PRBool isDir;
    rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) 
        return rv;
    if (!isDir)
        return NS_ERROR_FILE_NOT_DIRECTORY;

    nsDirEnumerator* dirEnum = new nsDirEnumerator();
    if (dirEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dirEnum);
    rv = dirEnum->Init(this);
    if (NS_FAILED(rv)) 
    {
        NS_RELEASE(dirEnum);
        return rv;
    }
    
    *entries = dirEnum;
    return NS_OK;
}


nsresult 
NS_NewLocalFile(const char* path, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    nsresult rv = file->InitWithPath(path);
    if (NS_FAILED(rv)) {
        NS_RELEASE(file);
        return rv;
    }
    *result = file;
    return NS_OK;
}




// OS2TODO - implement multibyte versions of string search

// Locates the first occurrence of charToSearchFor in the stringToSearch
static unsigned char* PR_CALLBACK
_mbschr( const unsigned char* stringToSearch, int charToSearchFor)
{
   return( (unsigned char*) strchr( (const char *) stringToSearch, charToSearchFor) );
}

// Locates last occurence of charToSearchFor in the stringToSearch
static unsigned char* PR_CALLBACK
_mbsrchr( const unsigned char* stringToSearch, int charToSearchFor)
{
   unsigned char * tempString = (unsigned char *) stringToSearch;
   unsigned char * tempPrev = nsnull;

   do {

      tempString = _mbschr( tempString, charToSearchFor );
 
      if (tempString != nsnull) 
      {
         tempPrev = tempString;
         tempString++;          // skip past character just found
      }
      else
         break;

   } while ( PR_TRUE); 

  
   return tempPrev;
}
