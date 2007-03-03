#ifndef _WIN32
 #include <dlfcn.h>
#endif
#include <stdio.h>	/* NULL */

#include "st_crypt.h"

struct crypt_funcs cl;
int crypt_loaded=0;

int init_crypt(void)
{
#ifdef STATIC_LINK
	cl.PopData=cryptPopData;
	cl.PushData=cryptPushData;
	cl.FlushData=cryptFlushData;
	cl.Init=cryptInit;
	cl.End=cryptEnd;
	cl.CreateSession=cryptCreateSession;
	cl.GetAttribute=cryptGetAttribute;
	cl.GetAttributeString=cryptGetAttributeString;
	cl.SetAttribute=cryptSetAttribute;
	cl.SetAttributeString=cryptSetAttributeString;
	cl.DestroySession=cryptDestroySession;
	cl.AddRandom=cryptAddRandom;
#else
#ifdef _WIN32
	HMODULE cryptlib;

	if(crypt_loaded)
		return(0);

	cryptlib=LoadLibrary("cl32.dll");
	if(cryptlib==NULL)
		return(-1);

	if((cl.PopData=GetProcAddress(cryptlib,"cryptPopData"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.PushData=GetProcAddress(cryptlib,"cryptPushData"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.FlushData=GetProcAddress(cryptlib,"cryptFlushData"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.Init=GetProcAddress(cryptlib,"cryptInit"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.End=GetProcAddress(cryptlib,"cryptEnd"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.CreateSession=GetProcAddress(cryptlib,"cryptCreateSession"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.GetAttribute=GetProcAddress(cryptlib,"cryptGetAttribute"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.GetAttributeString=GetProcAddress(cryptlib,"cryptGetAttributeString"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.SetAttribute=GetProcAddress(cryptlib,"cryptSetAttribute"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.SetAttributeString=GetProcAddress(cryptlib,"cryptSetAttributeString"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.DestroySession=GetProcAddress(cryptlib,"cryptDestroySession"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
	if((cl.AddRandom=GetProcAddress(cryptlib,"cryptAddRandom"))==NULL) {
		FreeLibrary(cryptlib);
		return(-1);
	}
#else
	void *cryptlib;

	if(crypt_loaded)
		return(0);

	cryptlib=dlopen("libcl.so",RTLD_LAZY);
	if(cryptlib==NULL)
		return(-1);
	if((cl.PopData=dlsym(cryptlib,"cryptPopData"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.PushData=dlsym(cryptlib,"cryptPushData"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.FlushData=dlsym(cryptlib,"cryptFlushData"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.Init=dlsym(cryptlib,"cryptInit"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.End=dlsym(cryptlib,"cryptEnd"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.CreateSession=dlsym(cryptlib,"cryptCreateSession"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.GetAttribute=dlsym(cryptlib,"cryptGetAttribute"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.GetAttributeString=dlsym(cryptlib,"cryptGetAttributeString"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.SetAttribute=dlsym(cryptlib,"cryptSetAttribute"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.SetAttributeString=dlsym(cryptlib,"cryptSetAttributeString"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.DestroySession=dlsym(cryptlib,"cryptDestroySession"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
	if((cl.AddRandom=dlsym(cryptlib,"cryptAddRandom"))==NULL) {
		dlclose(cryptlib);
		return(-1);
	}
#endif
#endif	/* !STATIC_LINK */
	if(cryptStatusOK(cl.Init())) {
		if(cryptStatusOK(cl.AddRandom(NULL, CRYPT_RANDOM_SLOWPOLL))) {
			crypt_loaded=1;
			return(0);
		}
	}
	return(-1);
}

void exit_crypt(void)
{
	if(crypt_loaded)
		cl.End();
}
