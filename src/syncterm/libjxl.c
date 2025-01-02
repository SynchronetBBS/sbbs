#include "libjxl.h"
#include "xp_dl.h"

#ifdef WITH_JPEG_XL

struct jxlfuncs Jxl;

#ifdef WITH_STATIC_JXL
bool load_jxl_funcs(void)
{
	Jxl.DecoderCloseInput = JxlDecoderCloseInput;
	Jxl.DecoderCreate = JxlDecoderCreate;
	Jxl.DecoderDestroy = JxlDecoderDestroy;
	Jxl.DecoderGetBasicInfo = JxlDecoderGetBasicInfo;
	Jxl.DecoderImageOutBufferSize = JxlDecoderImageOutBufferSize;
	Jxl.DecoderProcessInput = JxlDecoderProcessInput;
	Jxl.DecoderReleaseInput	= JxlDecoderReleaseInput;
	Jxl.DecoderSetImageOutBuffer = JxlDecoderSetImageOutBuffer;
	Jxl.DecoderSetInput = JxlDecoderSetInput;
#ifdef WITH_JPEG_XL_THREADS
	Jxl.DecoderSetParallelRunner = JxlDecoderSetParallelRunner;
#endif
	Jxl.DecoderSetPreferredColorProfile = JxlDecoderSetPreferredColorProfile;
	Jxl.DecoderSubscribeEvents = JxlDecoderSubscribeEvents;
#ifdef WITH_JPEG_XL_THREADS
	Jxl.ResizableParallelRunner = JxlResizableParallelRunner;
	Jxl.ResizableParallelRunnerCreate = JxlResizableParallelRunnerCreate;
	Jxl.ResizableParallelRunnerDestroy = JxlResizableParallelRunnerDestroy;
	Jxl.ResizableParallelRunnerSetThreads = JxlResizableParallelRunnerSetThreads;
	Jxl.ResizableParallelRunnerSuggestThreads = JxlResizableParallelRunnerSuggestThreads;
	Jxl.status = JXL_STATUS_OK;
#else
	Jxl.status = JXL_STATUS_NOTHREADS;
#endif

	return true;
}
#else
static dll_handle jxl_dll;
#ifdef WITH_JPEG_XL_THREADS
static dll_handle jxlt_dll;
#endif
bool load_jxl_funcs(void)
{
	const char *libnames[] = {"jxl", NULL};

	if (Jxl.status == JXL_STATUS_OK)
		return true;
	if (Jxl.status == JXL_STATUS_NOTHREADS)
		return true;
	if (Jxl.status == JXL_STATUS_FAILED)
		return false;

	Jxl.status = JXL_STATUS_FAILED;
	if (jxl_dll == NULL && (jxl_dll = xp_dlopen(libnames, RTLD_LAZY | RTLD_GLOBAL, JPEGXL_MAJOR_VERSION)) == NULL)
		return false;

	if ((Jxl.DecoderCloseInput = xp_dlsym(jxl_dll, JxlDecoderCloseInput)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderCreate = xp_dlsym(jxl_dll, JxlDecoderCreate)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderDestroy = xp_dlsym(jxl_dll, JxlDecoderDestroy)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderGetBasicInfo = xp_dlsym(jxl_dll, JxlDecoderGetBasicInfo)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderImageOutBufferSize = xp_dlsym(jxl_dll, JxlDecoderImageOutBufferSize)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderProcessInput = xp_dlsym(jxl_dll, JxlDecoderProcessInput)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderReleaseInput = xp_dlsym(jxl_dll, JxlDecoderReleaseInput)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderSetImageOutBuffer = xp_dlsym(jxl_dll, JxlDecoderSetImageOutBuffer)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderSetInput = xp_dlsym(jxl_dll, JxlDecoderSetInput)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
#ifdef WITH_JPEG_XL_THREADS
	if ((Jxl.DecoderSetParallelRunner = xp_dlsym(jxl_dll, JxlDecoderSetParallelRunner)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
#endif
	if ((Jxl.DecoderSetPreferredColorProfile = xp_dlsym(jxl_dll, JxlDecoderSetPreferredColorProfile)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}
	if ((Jxl.DecoderSubscribeEvents = xp_dlsym(jxl_dll, JxlDecoderSubscribeEvents)) == NULL) {
		xp_dlclose(jxl_dll);
		return false;
	}

	Jxl.status = JXL_STATUS_NOTHREADS;
#ifdef WITH_JPEG_XL_THREADS
	dll_handle jxlt_dll;
	const char *tlibnames[] = {"jxl_threads", NULL};

	Jxl.status = JXL_STATUS_FAILED;
	if (jxlt_dll == NULL && (jxlt_dll = xp_dlopen(tlibnames, RTLD_LAZY | RTLD_GLOBAL, JPEGXL_MAJOR_VERSION)) == NULL)
		return true;

	if ((Jxl.ResizableParallelRunner = xp_dlsym(jxlt_dll, JxlResizableParallelRunner)) == NULL) {
		xp_dlclose(jxl_dll);
		return true;
	}
	if ((Jxl.ResizableParallelRunnerCreate = xp_dlsym(jxlt_dll, JxlResizableParallelRunnerCreate)) == NULL) {
		xp_dlclose(jxl_dll);
		return true;
	}
	if ((Jxl.ResizableParallelRunnerDestroy = xp_dlsym(jxlt_dll, JxlResizableParallelRunnerDestroy)) == NULL) {
		xp_dlclose(jxl_dll);
		return true;
	}
	if ((Jxl.ResizableParallelRunnerSetThreads = xp_dlsym(jxlt_dll, JxlResizableParallelRunnerSetThreads)) == NULL) {
		xp_dlclose(jxl_dll);
		return true;
	}
	if ((Jxl.ResizableParallelRunnerSuggestThreads = xp_dlsym(jxlt_dll, JxlResizableParallelRunnerSuggestThreads)) == NULL) {
		xp_dlclose(jxl_dll);
		return true;
	}
	Jxl.status = JXL_STATUS_OK;
#endif
	return true;
}
#endif

#endif
