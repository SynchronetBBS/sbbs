#ifndef JXLFUNCS_H
#define JXLFUNCS_H

#ifdef WITH_JPEG_XL

#include <stdbool.h>

#include <jxl/decode.h>
#include <jxl/encode.h>

#ifdef WITH_JPEG_XL_THREADS
#include <jxl/resizable_parallel_runner.h>
#endif

enum JxlStatus {
	JXL_STATUS_UNINITIALIZED,
	JXL_STATUS_FAILED,
	JXL_STATUS_NOTHREADS,
	JXL_STATUS_OK
};

struct jxlfuncs {
	void (*DecoderCloseInput)(JxlDecoder*);
	JxlDecoder *(*DecoderCreate)(const JxlMemoryManager*);
	void (*DecoderDestroy)(JxlDecoder*);
	JxlDecoderStatus (*DecoderGetBasicInfo)(const JxlDecoder*, JxlBasicInfo* info);
	JxlDecoderStatus (*DecoderImageOutBufferSize)(const JxlDecoder*, const JxlPixelFormat*, size_t*);
	JxlDecoderStatus (*DecoderProcessInput)(JxlDecoder*);
	size_t (*DecoderReleaseInput)(JxlDecoder*);
	JxlDecoderStatus (*DecoderSetImageOutBuffer)(JxlDecoder*, const JxlPixelFormat*, void*, size_t);
	JxlDecoderStatus (*DecoderSetInput)(JxlDecoder*, const uint8_t*, size_t);
	JxlDecoderStatus (*DecoderSetParallelRunner)(JxlDecoder*, JxlParallelRunner, void*);
	JxlDecoderStatus (*DecoderSetPreferredColorProfile)(JxlDecoder*, const JxlColorEncoding*);
	JxlDecoderStatus (*DecoderSubscribeEvents)(JxlDecoder*, int);
	JxlParallelRetCode (*ResizableParallelRunner)(void*, void*, JxlParallelRunInit, JxlParallelRunFunction, uint32_t, uint32_t);
	void *(*ResizableParallelRunnerCreate)(const JxlMemoryManager*);
	void (*ResizableParallelRunnerDestroy)(void*);
	void (*ResizableParallelRunnerSetThreads)(void*, size_t);
	uint32_t (*ResizableParallelRunnerSuggestThreads)(uint64_t, uint64_t);
	int status;
};

extern struct jxlfuncs Jxl;

bool load_jxl_funcs(void);

#endif

#endif
