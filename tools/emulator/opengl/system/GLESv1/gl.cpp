#include "EGLClientIface.h"
#include "HostConnection.h"
#include "GLEncoder.h"
#include "GLES/gl.h"
#include "GLES/glext.h"
#include "ErrorLog.h"
#include <private/ui/android_natives_priv.h>
#include "gralloc_cb.h"
#include "ThreadInfo.h"


//XXX: fix this macro to get the context from fast tls path
#define GET_CONTEXT gl_client_context_t * ctx = getEGLThreadInfo()->hostConn->glEncoder();

#include "gl_entry.cpp"

//The functions table
#include "gl_ftable.h"

static EGLClient_eglInterface * s_egl = NULL;
static EGLClient_glesInterface * s_gl = NULL;

#define DEFINE_AND_VALIDATE_HOST_CONNECTION(ret) \
    HostConnection *hostCon = HostConnection::get(); \
    if (!hostCon) { \
        LOGE("egl: Failed to get host connection\n"); \
        return ret; \
    } \
    renderControl_encoder_context_t *rcEnc = hostCon->rcEncoder(); \
    if (!rcEnc) { \
        LOGE("egl: Failed to get renderControl encoder context\n"); \
        return ret; \
    }

//GL extensions
void glEGLImageTargetTexture2DOES(void * self, GLenum target, GLeglImageOES image)
{
    DBG("glEGLImageTargetTexture2DOES v1 image=0x%x", image);
    //TODO: check error - we don't have a way to set gl error
    android_native_buffer_t* native_buffer = (android_native_buffer_t*)image;

    if (native_buffer->common.magic != ANDROID_NATIVE_BUFFER_MAGIC) {
        return;
    }

    if (native_buffer->common.version != sizeof(android_native_buffer_t)) {
        return;
    }

    DEFINE_AND_VALIDATE_HOST_CONNECTION();
    rcEnc->rcBindTexture(rcEnc, ((cb_handle_t *)(native_buffer->handle))->hostHandle);

    return;
}

void glEGLImageTargetRenderbufferStorageOES(void *self, GLenum target, GLeglImageOES image)
{
    DBG("glEGLImageTargetRenderbufferStorageOES v1 image=0x%x", image);
    //TODO: check error - we don't have a way to set gl error
    android_native_buffer_t* native_buffer = (android_native_buffer_t*)image;

    if (native_buffer->common.magic != ANDROID_NATIVE_BUFFER_MAGIC) {
        return;
    }

    if (native_buffer->common.version != sizeof(android_native_buffer_t)) {
        return;
    }

    DEFINE_AND_VALIDATE_HOST_CONNECTION();
    rcEnc->rcBindRenderbuffer(rcEnc, ((cb_handle_t *)(native_buffer->handle))->hostHandle);

    return;
}

void * getProcAddress(const char * procname)
{
    // search in GL function table
    for (int i=0; i<gl_num_funcs; i++) {
        if (!strcmp(gl_funcs_by_name[i].name, procname)) {
            return gl_funcs_by_name[i].proc;
        }
    }
    return NULL;
}

void finish()
{
    glFinish();
}

const GLubyte *my_glGetString (void *self, GLenum name)
{
    if (s_egl) {
        return (const GLubyte*)s_egl->getGLString(name);
    }
    return NULL;
}

void init()
{
    GET_CONTEXT;
    ctx->set_glEGLImageTargetTexture2DOES(glEGLImageTargetTexture2DOES);
    ctx->set_glEGLImageTargetRenderbufferStorageOES(glEGLImageTargetRenderbufferStorageOES);
    ctx->set_glGetString(my_glGetString);
}

extern "C" {
EGLClient_glesInterface * init_emul_gles(EGLClient_eglInterface *eglIface)
{
    s_egl = eglIface;

    if (!s_gl) {
        s_gl = new EGLClient_glesInterface();
        s_gl->getProcAddress = getProcAddress;
        s_gl->finish = finish;
        s_gl->init = init;
    }

    return s_gl;
}
} //extern


