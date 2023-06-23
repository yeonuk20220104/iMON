/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <gbm.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <assert.h>
#include <errno.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include<sys/ioctl.h>

#include <drm_fourcc.h>
#include <libdrm/xf86drm.h>
#include <pthread.h>


#define ALIGN(_v, _d) (((_v) + ((_d) - 1)) & ~((_d) - 1))

/*
#define DEBUG_MSG(fmt, ...) \
		do { \
			printf(fmt " (%s:%d)\n", \
					##__VA_ARGS__, __FUNCTION__, __LINE__); \
		} while (0)
#define ERROR_MSG(fmt, ...) \
		do { printf("ERROR: " fmt " (%s:%d)\n", \
				##__VA_ARGS__, __FUNCTION__, __LINE__); } while (0)


#define ECHK(x) do { \
		EGLImageKHR status; \
		DEBUG_MSG(">>> %s", #x); \
		status = (EGLImageKHR)(x); \
		if (!status) { \
			EGLint err = eglGetError(); \
			ERROR_MSG("<<< %s: failed: 0x%04x (    )", #x, err); \
			exit(-1); \
		} \
		DEBUG_MSG("<<< %s: succeeded", #x); \
	} while (0)

#define GCHK(x) do { \
		GLenum err; \
		DEBUG_MSG(">>> %s", #x); \
		x; \
		err = glGetError(); \
		if (err != GL_NO_ERROR) { \
			ERROR_MSG("<<< %s: failed: 0x%04x ( )", #x, err ); \
			exit(-1); \
		} \
		DEBUG_MSG("<<< %s: succeeded", #x); \
	} while (0)

*/
#define ECHK(x) x=x;
#define GCHK(x) x;


unsigned int crc_table[256];
static unsigned int crc32(unsigned int crc,unsigned char *buffer, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++) {
		crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
	}
	return crc;
}

static void init_crc_table(void)
{
	unsigned int c;
	unsigned int i, j;

	for (i = 0; i < 256; i++) {
		c = (unsigned int)i;
		for (j = 0; j < 8; j++) {
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
			    c = c >> 1;
		}
		crc_table[i] = c;
	}
}

static void printGLString(const char *name, GLenum s) {
    // fprintf(stderr, "printGLString %s, %d\n", name, s);
    const char *v = (const char *) glGetString(s);
    // int error = glGetError();
    // fprintf(stderr, "glGetError() = %d, result of glGetString = %x\n", error,
    //        (unsigned int) v);
    // if ((v < (const char*) 0) || (v > (const char*) 0x10000))
    //    fprintf(stderr, "GL %s = %s\n", name, v);
    // else
    //    fprintf(stderr, "GL %s = (null) 0x%08x\n", name, (unsigned int) v);
    fprintf(stderr, "GL %s = %s\n", name, v);
}


static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        fprintf(stderr, "after %s() glError (0x%x)\n", op, error);
    }
}

static const char gVertexShader[] =
    "#version 310 es \n"
    "in vec4 vPosition;\n"
    "in vec2 texCoords;\n"
    "out vec2 outTexCoords;\n"
    "void main() {\n"
    "    outTexCoords = vec2(texCoords.x,1.0 + texCoords.y*(-1.0));\n"
    "    gl_Position = vPosition;\n"
    "}\n";

//    "attribute vec4 vPosition;\n"
//    "attribute vec2 texCoords;"
//    "varying vec2 outTexCoords;"

//    "void main() {\n"
//    "    outTexCoords = texCoords;\n"
//    "    gl_Position = vPosition;\n"
//    "}\n";

static const char gFragmentShader[] =
    "#version 310 es \n"
    "#extension GL_OES_EGL_image_external : require \n"
    "#extension GL_EXT_YUV_target : require \n"
    "precision mediump float;\n"
    "uniform __samplerExternal2DY2YEXT outTexture;\n"
    "in vec2 outTexCoords;\n"
     "out vec4 FragColor;\n"
    "void main() {\n"
    "   FragColor=texture(outTexture, outTexCoords);\n"
    "}\n";

//    "#extension GL_OES_EGL_image_external : require\n"
//    "precision mediump float;\n"
//    "uniform samplerExternalOES outTexture;"
//    "varying vec2 outTexCoords;"
//    "void main() {\n"
//    //"  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
//    "    gl_FragColor = texture2D(outTexture, outTexCoords);"
//    "}\n";


GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    fprintf(stderr, "Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    fprintf(stderr, "Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

GLuint gProgram;
GLuint gvPositionHandle;
GLuint gvTextureTexCoordsHandle;
GLuint gvTextureSamplerHandle;


bool setupGraphics(int w, int h) {
    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        return false;
    }
    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    checkGlError("glGetAttribLocation");
    fprintf(stderr, "glGetAttribLocation(\"vPosition\") = %d\n",
            gvPositionHandle);

    gvTextureTexCoordsHandle = glGetAttribLocation(gProgram, "texCoords");
    checkGlError("glGetAttribLocation");
    gvTextureSamplerHandle = glGetUniformLocation(gProgram, "outTexture");
    checkGlError("glGetAttribLocation");

    glActiveTexture(GL_TEXTURE0);


    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    return true;
}



const GLfloat defaultTriangleVertices[] =
{
    -1.0f, 1.0f,
    -1.0f, -1.0f,
    1.0f, -1.0f,
    1.0f, -1.0f,
    1.0f, 1.0f,
    -1.0f, 1.0f,
};
const GLfloat defaulttexVertices[] =
{ 0.0f, 1.0f,
  0.0f, 0.0f,
  1.0f, 0.0f,
  1.0f, 0.0f,
  1.0f, 1.0f,
  0.0f, 1.0f
};


GLfloat * gTriangleVertices = NULL;

GLfloat * gtexVertices = NULL;

#define MESH_SIZE 1

void caculateVertices(int mesh_w,int mesh_h, GLfloat * gVertexPoint,  GLfloat * gTexturePoint)
{
    float step_x = 1.0f / mesh_w;
    float step_y = 1.0f / mesh_h;
    int indexVertex  = 0;
    int indexTexture = 0;


    printf("rk-debug[%s %d] mesh_w:%d mesh_h:%d step_x:%f step_y:%f\n",__FUNCTION__,__LINE__,mesh_w,mesh_h,step_x,step_y);


    for(int y = 0; y < mesh_h ; y++)
    {
        for(int x = 0; x < mesh_w; x++)
        {
            gVertexPoint[indexVertex+0] = step_x*(x+0.0)*2.0-1.0;
            gVertexPoint[indexVertex+1] = step_y*(y+0.0)*2.0-1.0;
            gVertexPoint[indexVertex+2] = step_x*(x+1.0)*2.0-1.0;
            gVertexPoint[indexVertex+3] = step_y*(y+0.0)*2.0-1.0;
            gVertexPoint[indexVertex+4] = step_x*(x+1.0)*2.0-1.0;
            gVertexPoint[indexVertex+5] = step_y*(y+1.0)*2.0-1.0;
            gVertexPoint[indexVertex+6] = step_x*(x+0.0)*2.0-1.0;
            gVertexPoint[indexVertex+7] = step_y*(y+0.0)*2.0-1.0;
            gVertexPoint[indexVertex+8] = step_x*(x+1.0)*2.0-1.0;
            gVertexPoint[indexVertex+9] = step_y*(y+1.0)*2.0-1.0;
            gVertexPoint[indexVertex+10] = step_x*(x+0.0)*2.0-1.0;
            gVertexPoint[indexVertex+11] = step_y*(y+1.0)*2.0-1.0;

            indexVertex += 12;

        }
    }

    for(int y = 0; y < mesh_h ; y++)
    {
        for(int x = 0; x < mesh_w; x++)
        {
            gTexturePoint[indexTexture+0] = step_x*(x+0.0);
            gTexturePoint[indexTexture+1] = step_y*(y+0.0);
            gTexturePoint[indexTexture+2] = step_x*(x+1.0);
            gTexturePoint[indexTexture+3] = step_y*(y+0.0);
            gTexturePoint[indexTexture+4] = step_x*(x+1.0);
            gTexturePoint[indexTexture+5] = step_y*(y+1.0);
            gTexturePoint[indexTexture+6] = step_x*(x+0.0);
            gTexturePoint[indexTexture+7] = step_y*(y+0.0);
            gTexturePoint[indexTexture+8] = step_x*(x+1.0);
            gTexturePoint[indexTexture+9] = step_y*(y+1.0);
            gTexturePoint[indexTexture+10] = step_x*(x+0.0);
            gTexturePoint[indexTexture+11] = step_y*(y+1.0);

            indexTexture += 12;

        }
    }

}

void caculateVertices_strip(int mesh_w,int mesh_h, GLfloat * gVertexPoint,  GLfloat * gTexturePoint)
{
    float step_x = 1.0f / mesh_w;
    float step_y = 1.0f / mesh_h;
    int indexVertex  = 0;
//    int indexTexture = 0;

    printf("rk-debug[%s %d] mesh_w:%d mesh_h:%d step_x:%f step_y:%f\n",__FUNCTION__,__LINE__,mesh_w,mesh_h,step_x,step_y);


    gVertexPoint[indexVertex+0] = step_x*(0.0)*2.0-1.0;
    gVertexPoint[indexVertex+1] = step_y*(0.0)*2.0-1.0;
    indexVertex += 2;

    for(int y = 0; y < mesh_h ; y++)
    {
        if(y%2==0)
        {
            gVertexPoint[indexVertex+0] = 0.0*2.0-1.0;
            gVertexPoint[indexVertex+1] = step_y*(y+1.0)*2.0-1.0;
            indexVertex += 2;
        }else
        {
            gVertexPoint[indexVertex+0] = 1.0*2.0-1.0;
            gVertexPoint[indexVertex+1] = step_y*(y+1.0)*2.0-1.0;
            indexVertex += 2;
        }

        for(int x = 0; x < mesh_w; x++)
        {
            if(y%2==0)
            {
                gVertexPoint[indexVertex+0] = step_x*(x+1.0)*2.0-1.0;
                gVertexPoint[indexVertex+1] = step_y*(y+0.0)*2.0-1.0;
                gVertexPoint[indexVertex+2] = step_x*(x+1.0)*2.0-1.0;
                gVertexPoint[indexVertex+3] = step_y*(y+1.0)*2.0-1.0;
            }else{
                gVertexPoint[indexVertex+0] = step_x*(mesh_w-x-1.0)*2.0-1.0;
                gVertexPoint[indexVertex+1] = step_y*(y+0.0)*2.0-1.0;
                gVertexPoint[indexVertex+2] = step_x*(mesh_w-x-1.0)*2.0-1.0;
                gVertexPoint[indexVertex+3] = step_y*(y+1.0)*2.0-1.0;
            }
            indexVertex += 4;
        }

    }


    indexVertex=0;

    gTexturePoint[indexVertex+0] = step_x*(0.0);
    gTexturePoint[indexVertex+1] = step_y*(0.0);
    indexVertex += 2;

    for(int y = 0; y < mesh_h ; y++)
    {
        if(y%2==0)
        {
            gTexturePoint[indexVertex+0] = 0.0;
            gTexturePoint[indexVertex+1] = step_y*(y+1.0);
            indexVertex += 2;
        }else
        {
            gTexturePoint[indexVertex+0] = 1.0;
            gTexturePoint[indexVertex+1] = step_y*(y+1.0);
            indexVertex += 2;
        }

        for(int x = 0; x < mesh_w; x++)
        {
            if(y%2==0)
            {
                gTexturePoint[indexVertex+0] = step_x*(x+1.0);
                gTexturePoint[indexVertex+1] = step_y*(y+0.0);
                gTexturePoint[indexVertex+2] = step_x*(x+1.0);
                gTexturePoint[indexVertex+3] = step_y*(y+1.0);
            }else{
                gTexturePoint[indexVertex+0] = step_x*(mesh_w-x-1.0);
                gTexturePoint[indexVertex+1] = step_y*(y+0.0);
                gTexturePoint[indexVertex+2] = step_x*(mesh_w-x-1.0);
                gTexturePoint[indexVertex+3] = step_y*(y+1.0);
            }
            indexVertex += 4;
        }

    }


}


void renderFrame() {
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        checkGlError("glClearColor");
        glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        checkGlError("glClear");

        glUseProgram(gProgram);
        checkGlError("glUseProgram");

    {
        glVertexAttribPointer(gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
        checkGlError("glVertexAttribPointer");
        glEnableVertexAttribArray(gvPositionHandle);
        checkGlError("glEnableVertexAttribArray");
        glVertexAttribPointer(gvTextureTexCoordsHandle, 2, GL_FLOAT, GL_FALSE,0, gtexVertices);
        checkGlError("glVertexAttribPointer");
        glEnableVertexAttribArray(gvTextureTexCoordsHandle);
        checkGlError("glEnableVertexAttribArray");
        printf("rk-debug[%s %d]  \n",__FUNCTION__,__LINE__);
        //glDrawArrays(GL_TRIANGLES, 0, 6*MESH_SIZE*MESH_SIZE);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, (2*MESH_SIZE+1)*MESH_SIZE+1);
        checkGlError("glDrawArrays");

    }
    printf("rk-debug[%s %d] MESH_SIZE:%d \n",__FUNCTION__,__LINE__,MESH_SIZE);

}



static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE) {
    if (returnVal != EGL_TRUE) {
        fprintf(stderr, "%s() returned %d\n", op, returnVal);
    }

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error
            = eglGetError()) {
        fprintf(stderr, "after %s() eglError (0x%x)\n", op,error);
    }
}

int dumpPixels_new(int index,int inWindowWidth,int inWindowHeight,void * pPixelDataFront,const char * format,int size){
    char file_name[100];
    sprintf(file_name,"/data/dump/dumplayer_%d_%dx%d_%s.bin",index,inWindowWidth,inWindowHeight,format);
    if(1)
    {
        FILE *file = fopen(file_name, "wb");
        if (!file)
        {
            printf("Could not open /%s \n",file_name);
            return -1;
        } else {
            printf("open %s and write ok\n",file_name);
        }
        fwrite(pPixelDataFront, size, 1, file);
        fclose(file);
    }

    return 0;
}

struct dma_buf_sync
{
        __u64 flags;
};
#define DMA_BUF_SYNC_READ (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_RW (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END (1 << 2)

#define DMA_BUF_BASE 'b'
#define DMA_BUF_IOCTL_SYNC _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

void dma_buf_raw_sync(int fd, unsigned long flags){
    struct dma_buf_sync args = { flags };
    int err;

    err = ioctl(fd, DMA_BUF_IOCTL_SYNC, &args);
    if (err)
    {
        printf("rk-debug Failed to sync dma-buf fd %d: %d \n", fd, errno);
    }

}
void dma_buf_raw_sync_start(int fd)
{
        dma_buf_raw_sync(fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW);
}

void dma_buf_raw_sync_end(int fd)
{
        dma_buf_raw_sync(fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW);
}
int drm_fd = -1;

void *alloc_drm_buf(int *fd,int in_w, int in_h, int in_bpp, int need_cpu_cached,unsigned int * handle)
{
    printf("rk-debug[%s %d] *fd:%d w:%d h:%d in_vpp:%d \n",__FUNCTION__,__LINE__,*fd,in_w,in_h,in_bpp);

    static const char* card = "/dev/dri/card0";

    int flag = O_RDWR;
    int ret;
    void *map = NULL;

    void *vir_addr = NULL;
    struct drm_prime_handle fd_args;
    struct drm_mode_map_dumb mmap_arg;
    struct drm_mode_create_dumb alloc_arg;

    struct drm_mode_destroy_dumb destory_arg;
    if(drm_fd < 0)
    {
        drm_fd = open(card, flag);
        if(drm_fd < 0)
        {
            printf("failed to open %s\n", card);
            return NULL;
        }
    }

    memset(&alloc_arg, 0, sizeof(alloc_arg));
    alloc_arg.bpp = in_bpp;
    alloc_arg.width = in_w;
    alloc_arg.height = in_h;
    unsigned int ROCKCHIP_BO_CACHABLE    = 1 << 1;
    if(need_cpu_cached)
        alloc_arg.flags = ROCKCHIP_BO_CACHABLE;

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &alloc_arg);
    if (ret) {
        printf("failed to create dumb buffer: %s\n", strerror(errno));
        return NULL;
    }
    printf("rk-debug[%s %d] alloc_arg.handle:0x%x \n",__FUNCTION__,__LINE__,alloc_arg.handle);

    memset(&fd_args, 0, sizeof(fd_args));
	fd_args.fd = -1;
	fd_args.handle = alloc_arg.handle;
	fd_args.flags = 0;
	ret = drmIoctl(drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &fd_args);
	if (ret)
	{
	    printf("rk-debug handle_to_fd failed ret=%d,err=%s, handle=%x \n",ret,strerror(errno),fd_args.handle);
		return NULL;
    }
    printf("Dump fd = %d alloc_arg.handle:0x%x\n",fd_args.fd,alloc_arg.handle);
    *fd = fd_args.fd;
    *handle = alloc_arg.handle;

  //handle to Virtual address
    memset(&mmap_arg, 0, sizeof(mmap_arg));
    mmap_arg.handle = alloc_arg.handle;

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mmap_arg);
    if (ret) {
        printf("failed to create map dumb: %s\n", strerror(errno));
        vir_addr = NULL;
        goto destory_dumb;
    }
    vir_addr = map = mmap64(0, alloc_arg.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, mmap_arg.offset);
    if (map == MAP_FAILED) {
        printf("failed to mmap buffer: %s\n", strerror(errno));
        vir_addr = NULL;
        goto destory_dumb;
    }
    printf("alloc map=%p \n",map);

    return vir_addr;
destory_dumb:
  memset(&destory_arg, 0, sizeof(destory_arg));
  destory_arg.handle = alloc_arg.handle;

  ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory_arg);
  if (ret)
    printf("failed to destory dumb %d\n", ret);
  return vir_addr;
}

void free_drm_buf(int * fd, unsigned int * handle)
{
    int ret = 0;
    struct drm_mode_destroy_dumb destory_arg;
    memset(&destory_arg, 0, sizeof(destory_arg));
    destory_arg.handle = *handle;

    printf("rk-debug[%s %d] drm_fd:%d *handle:0x%x\n",__FUNCTION__,__LINE__,drm_fd,*handle);
    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory_arg);
    if (ret)
        printf("failed to destory dumb %d\n", ret);

    close(*fd);
}


EGLContext context;
EGLSurface surface;
EGLDisplay display;

EGLDisplay initEGLContex()
{
    struct timeval tpend_begin, tpend_end;
    float usec_init = 0;
    gettimeofday(&tpend_begin, NULL);

    EGLBoolean returnValue;
    EGLConfig myConfig = {0};

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLint s_configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE };

    EGLint majorVersion;
    EGLint minorVersion;
    EGLint w, h;

    EGLDisplay dpy;

    int drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    struct gbm_device *gbm_dev = gbm_create_device(drm_fd);

    static PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;

    get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

    dpy = get_platform_display(EGL_PLATFORM_GBM_KHR, (void*) gbm_dev, NULL);

    checkEglError("<init>");
    //dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    checkEglError("eglGetDisplay");
    if (dpy == EGL_NO_DISPLAY) {
        printf("eglGetDisplay returned EGL_NO_DISPLAY.\n");
        return 0;
    }
    display = dpy;

    returnValue = eglInitialize(dpy, NULL, NULL);//&majorVersion, &minorVersion);
    checkEglError("eglInitialize", returnValue);
    //fprintf(stderr, "EGL version %d.%d\n", majorVersion, minorVersion);
    if (returnValue != EGL_TRUE) {
        printf("eglInitialize failed\n");
        return 0;
    }

    gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] eglInitialize init use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);

    EGLint numConfig = 0;
    eglChooseConfig(dpy, s_configAttribs, 0, 0, &numConfig);
    int num = numConfig;
    if(num != 0){
       EGLConfig configs[num];
       //获取所有满足attributes的configs
       eglChooseConfig(dpy, s_configAttribs, configs, num, &numConfig);
       myConfig = configs[0]; //以某种规则选择一个config，这里使用了最简单的规则。
    }

    int sw = 100;
    int sh = 100;
    EGLint attribs[] = { EGL_WIDTH, sw, EGL_HEIGHT, sh, EGL_LARGEST_PBUFFER, EGL_TRUE, EGL_NONE, EGL_NONE };
    surface = eglCreatePbufferSurface(dpy, myConfig, attribs);

    checkEglError("eglCreateWindowSurface");
    if (surface == EGL_NO_SURFACE) {
        printf("eglCreateWindowSurface failed.\n");
        return 0;
    }
    gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] eglCreatePbufferSurface init use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);

    context = eglCreateContext(dpy, myConfig, EGL_NO_CONTEXT, context_attribs);
    checkEglError("eglCreateContext");
    if (context == EGL_NO_CONTEXT) {
        printf("eglCreateContext failed\n");
        return 0;
    }
        gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] eglCreateContext init use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);
    returnValue = eglMakeCurrent(dpy, surface, surface, context);
    checkEglError("eglMakeCurrent", returnValue);
    if (returnValue != EGL_TRUE) {
        return 0;
    }
        gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] eglMakeCurrent init use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);

//    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
//    checkEglError("eglQuerySurface");
//    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
//    checkEglError("eglQuerySurface");

//    fprintf(stderr, "Window dimensions: %d x %d\n", w, h);

//    printGLString("Version", GL_VERSION);
//    printGLString("Vendor", GL_VENDOR);
//    printGLString("Renderer", GL_RENDERER);
//    printGLString("Extensions", GL_EXTENSIONS);

    return dpy;

}

void deinitEGLContex()
{
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
    }
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
    }

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(display);
    eglReleaseThread();
}

typedef struct rk_texture_s{
    int w;
    int h;
    int drm_format;
    int is_afbc;
    int texture_id;
    int need_fbo;
    int fbo_id;
    int need_cpu_cached;
    int drm_fd;
    unsigned int drm_handle;
    void * drm_viraddr;
} rk_texture_t;


#ifndef DRM_FORMAT_YUV420_8BIT
#define DRM_FORMAT_YUV420_8BIT  fourcc_code('Y', 'U', '0', '8')
#endif

#ifndef DRM_FORMAT_YUV420_10BIT
#define DRM_FORMAT_YUV420_10BIT fourcc_code('Y', 'U', '1', '0')
#endif

#ifndef DRM_FORMAT_YUYV   //YUV422 1plane
#define DRM_FORMAT_YUYV         fourcc_code('Y', 'U', 'Y', 'V')
#endif

#ifndef DRM_FORMAT_Y210  //YUV422 10bit 1plane
#define DRM_FORMAT_Y210         fourcc_code('Y', '2', '1', '0')
#endif


int dump_rk_texture(rk_texture_t * rk_texture)
{
    if(!rk_texture){
        printf("dump error!!! rk_texture = NULL\n");
        return -1;
    }

    printf("rk_texture{ w:%d h:%d format:0x%x afbc:%d texture_id:%d need_fbo:%d fbo_id:%d drm_fd:%d drm_viraddr:%p } \n"
            ,rk_texture->w,rk_texture->h,rk_texture->drm_format,rk_texture->is_afbc,rk_texture->texture_id
            ,rk_texture->need_fbo,rk_texture->fbo_id,rk_texture->drm_fd,rk_texture->drm_viraddr);
    return 0;
}


float get_format_size(int in_format)
{
    //create img
    switch(in_format){
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_RGBA8888:
            return 4.0;
        break;
        case DRM_FORMAT_BGR888:
        case DRM_FORMAT_RGB888:
            return 3.0;
        break;
        case DRM_FORMAT_YUYV:
            return 2.0;
        break;
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_YUV420_8BIT:
            return 1.5;
        break;
        default :
            return 0;
    }

}


int create_drm_fd(rk_texture_t * rk_texture)
{
    int in_format = rk_texture->drm_format;
    int is_afbc = rk_texture->is_afbc;
    int textureW = rk_texture->w;
    int textureH = rk_texture->h;
    int need_cpu_cached = rk_texture->need_cpu_cached;

    //create img
    switch(in_format){
        case DRM_FORMAT_ABGR8888:
            rk_texture->drm_viraddr = alloc_drm_buf(&(rk_texture->drm_fd),textureW,textureH,is_afbc?64:32,need_cpu_cached,&(rk_texture->drm_handle)); //afbc bpp 先按照2倍申请
        break;
        case DRM_FORMAT_BGR888:
        case DRM_FORMAT_RGB888:
            rk_texture->drm_viraddr = alloc_drm_buf(&(rk_texture->drm_fd),textureW,textureH,is_afbc?48:24,need_cpu_cached,&(rk_texture->drm_handle)); //afbc bpp 先按照2倍申请
        break;
        case DRM_FORMAT_YUYV:
            rk_texture->drm_viraddr = alloc_drm_buf(&(rk_texture->drm_fd),textureW,textureH,is_afbc?32:16,need_cpu_cached,&(rk_texture->drm_handle)); //afbc bpp 先按照2倍申请
        break;
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_YUV420_8BIT:

            rk_texture->drm_viraddr = alloc_drm_buf(&(rk_texture->drm_fd),textureW,textureH,is_afbc?24:12,need_cpu_cached,&(rk_texture->drm_handle)); //afbc bpp 先按照2倍申请
        break;
    }

    return 0;
}



int create_texture_fbo_img(EGLDisplay dpy,rk_texture_t * rk_texture)
{

    EGLImageKHR img = NULL;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
    PFNEGLCREATEIMAGEKHRPROC create_image;
    PFNEGLDESTROYIMAGEKHRPROC destroy_image;
    create_image = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    image_target_texture_2d = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
    destroy_image = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");

    int in_format = rk_texture->drm_format;
    int is_afbc = rk_texture->is_afbc;
    GLuint * p_texture_id =(GLuint *) &(rk_texture->texture_id);
    GLuint * p_fbo_id = (GLuint *)&(rk_texture->fbo_id);
    int fd = rk_texture->drm_fd;
    int textureW = rk_texture->w;
    int textureH = rk_texture->h;
    int is_need_fbo = rk_texture->need_fbo;

    printf("rk-debug[%s %d] rk_texture:%p t:%p p_texture_id:%p \n",__FUNCTION__,__LINE__,rk_texture,&(rk_texture->texture_id),p_texture_id);


    if(dump_rk_texture(rk_texture))
    {
        printf("rk-debug[%s %d] rk_texture == NULL \n",__FUNCTION__,__LINE__);
    }

    //create img
    switch(in_format){
        case DRM_FORMAT_ABGR8888:
            {
            int stride = ALIGN(textureW, 32) * 4;

            EGLint attr[] = {
                    EGL_WIDTH, textureW,
                    EGL_HEIGHT, textureH,
                    EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ABGR8888,
                    EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT:EGL_NONE, is_afbc?((AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_BLOCK_SIZE_16x16)&0xffffffff):EGL_NONE, //rk支持afbc默认格式
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT:EGL_NONE, is_afbc?(0x08<<24):EGL_NONE,  //ARM平台标志位
                    EGL_NONE
            };
            img = create_image(dpy, EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
            ECHK(img);
            if(img == EGL_NO_IMAGE_KHR)
            {
                printf("rk-debug eglCreateImageKHR NULL \n ");
                return -1;
            }
            }
            break;
        case DRM_FORMAT_RGBA8888:
            {
            int stride = ALIGN(textureW, 32) * 4;

            EGLint attr[] = {
                    EGL_WIDTH, textureW,
                    EGL_HEIGHT, textureH,
                    EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_RGBA8888,
                    EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT:EGL_NONE, is_afbc?((AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_BLOCK_SIZE_16x16)&0xffffffff):EGL_NONE, //rk支持afbc默认格式
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT:EGL_NONE, is_afbc?(0x08<<24):EGL_NONE,  //ARM平台标志位
                    EGL_NONE
            };
            img = create_image(dpy, EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
            ECHK(img);
            if(img == EGL_NO_IMAGE_KHR)
            {
                printf("rk-debug eglCreateImageKHR NULL \n ");
                return -1;
            }
            }
            break;

        case DRM_FORMAT_BGR888:
        case DRM_FORMAT_RGB888:
            {
            int stride = ALIGN(textureW, 32) * 3;

            EGLint attr[] = {
                    EGL_WIDTH, textureW,
                    EGL_HEIGHT, textureH,
                    EGL_LINUX_DRM_FOURCC_EXT, in_format,
                    EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT:EGL_NONE, is_afbc?((AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_BLOCK_SIZE_16x16)&0xffffffff):EGL_NONE, //rk支持afbc默认格式
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT:EGL_NONE, is_afbc?(0x08<<24):EGL_NONE,  //ARM平台标志位
                    EGL_NONE
            };
            img = create_image(dpy, EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
            ECHK(img);
            if(img == EGL_NO_IMAGE_KHR)
            {
                printf("rk-debug eglCreateImageKHR NULL \n ");
                return -1;
            }
            }
            break;
        case DRM_FORMAT_YUYV:
            {
            int stride = ALIGN(textureW, 32) * 2;

            EGLint attr[] = {
                    EGL_WIDTH, textureW,
                    EGL_HEIGHT, textureH,
                    EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUYV,
                    EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT:EGL_NONE, is_afbc?((AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_BLOCK_SIZE_16x16)&0xffffffff):EGL_NONE, //rk支持afbc默认格式
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT:EGL_NONE, is_afbc?(0x08<<24):EGL_NONE,  //ARM平台标志位
                    EGL_NONE
            };
            img = create_image(dpy, EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
            ECHK(img);
            if(img == EGL_NO_IMAGE_KHR)
            {
                printf("rk-debug eglCreateImageKHR NULL \n ");
                return -1;
            }
            }
            break;
        case DRM_FORMAT_YUV420_8BIT: //该格式仅支持afbc，不支持linear
            {
            int stride = ALIGN(textureW, 32) * 1;

            EGLint attr[] = {
                    EGL_WIDTH, textureW,
                    EGL_HEIGHT, textureH,
                    EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUV420_8BIT,
                    EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT, stride, //该格式afbc 无所谓stride 为1还是2
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT:EGL_NONE, is_afbc?((AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_BLOCK_SIZE_16x16)&0xffffffff):EGL_NONE, //rk支持afbc默认格式
                    is_afbc?EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT:EGL_NONE, is_afbc?(0x08<<24):EGL_NONE,  //ARM平台标志位
                    EGL_NONE
            };
            img = create_image(dpy, EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
            ECHK(img);
            if(img == EGL_NO_IMAGE_KHR)
            {
                printf("rk-debug eglCreateImageKHR NULL \n ");
                return -1;
            }
            }
            break;

        case DRM_FORMAT_NV12:
            {
                int stride = ALIGN(textureW, 32) * 1;
                if(!is_afbc)
                {
                    EGLint attr[] = {
                            EGL_WIDTH, textureW,
                            EGL_HEIGHT, textureH,
                            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,
                            EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                            EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                            EGL_DMA_BUF_PLANE1_FD_EXT, fd,
                            EGL_DMA_BUF_PLANE1_OFFSET_EXT, stride*textureH,
                            EGL_DMA_BUF_PLANE1_PITCH_EXT, stride,
                            EGL_NONE
                    };
                    img = create_image(dpy, EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
                    ECHK(img);
                    if(img == EGL_NO_IMAGE_KHR)
                    {
                        printf("rk-debug eglCreateImageKHR NULL \n ");
                        return -1;
                    }


                }else {
                    EGLint attr[] = {
                            EGL_WIDTH, textureW,
                            EGL_HEIGHT, textureH,
                            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,

                            EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                            EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, ((AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_BLOCK_SIZE_32x8)&0xffffffff), //rk支持afbc默认格式
                            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (0x08<<24),  //ARM平台标志位

                            EGL_DMA_BUF_PLANE1_FD_EXT, fd,
                            EGL_DMA_BUF_PLANE1_OFFSET_EXT, stride*textureH,
                            EGL_DMA_BUF_PLANE1_PITCH_EXT, stride,
                            EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, ((AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_BLOCK_SIZE_64x4)&0xffffffff), //rk支持afbc默认格式
                            EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, (0x08<<24),  //ARM平台标志位

                            EGL_NONE
                    };
                    img = create_image(dpy, EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
                    ECHK(img);
                    if(img == EGL_NO_IMAGE_KHR)
                    {
                        printf("rk-debug eglCreateImageKHR NULL \n ");
                        return -1;
                    }
                    printf("rk-debug[%s %d] nv12 afbc is fault!\n",__FUNCTION__,__LINE__);
                }
            }
            break;
        default:
            printf("rk-debug[%s %d] error in_format unSupport:0x%x \n",__FUNCTION__,__LINE__,in_format);

    }


    if(!is_need_fbo)
    {
        GCHK(glActiveTexture(GL_TEXTURE0));
        GCHK(glGenTextures(1, p_texture_id));
        printf("rk-debug[%s %d] p_texture_id:%d \n",__FUNCTION__,__LINE__,*p_texture_id);
        GCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, *p_texture_id));
        GCHK(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GCHK(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GCHK(image_target_texture_2d(GL_TEXTURE_EXTERNAL_OES, img));

    }else{
        GCHK(glActiveTexture(GL_TEXTURE1));
        GCHK(glGenTextures(1, p_texture_id));
        printf("rk-debug[%s %d] p_texture_id:%d \n",__FUNCTION__,__LINE__,*p_texture_id);
        GCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, *p_texture_id));
        GCHK(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GCHK(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GCHK(image_target_texture_2d(GL_TEXTURE_EXTERNAL_OES, img));

        glGenFramebuffers(1, p_fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, *p_fbo_id);
        printf("rk-debug[%s %d] p_fbo_id:%d \n",__FUNCTION__,__LINE__,*p_fbo_id);
        GCHK(glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0, GL_TEXTURE_EXTERNAL_OES, *p_texture_id, 0));
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
            printf("rk_debug create fbo success!\n");
        else
            printf("rk_debug create fbo failed!\n");
    }

    destroy_image(dpy,img);

    if(dump_rk_texture(rk_texture))
    {
        printf("rk-debug[%s %d] rk_texture == NULL \n",__FUNCTION__,__LINE__);
    }


    return 0;
}

int destory_texture_fbo_img(EGLDisplay dpy,rk_texture_t * rk_texture)
{
    GLuint * p_texture_id = (GLuint *) &(rk_texture->texture_id);
    GLuint * p_fbo_id = (GLuint *) &(rk_texture->fbo_id);
    int is_need_fbo = rk_texture->need_fbo;

    glDeleteTextures(1,p_texture_id);

    if(is_need_fbo)
        glDeleteFramebuffers(1,p_fbo_id);

    printf("rk-debug[%s %d] delete tex:%d fbo:%d\n",__FUNCTION__,__LINE__,*p_texture_id,*p_fbo_id);

    return 0;
}
//unsigned int golden_crc_data1 =  0x7361a5c3; //1280x720
//unsigned int golden_crc_data2 =  0xd98258ad; //1280x720

unsigned int golden_crc_data1 =  0xB1C6475A; //64x64
unsigned int golden_crc_data2 =  0x39EF6F3E; //64x64

EGLDisplay dpy;
rk_texture_t src={0};
rk_texture_t win={0};
rk_texture_t src2={0};

int slt_gpu_light_init(void)
{
    struct timeval tpend_begin, tpend_end;
    float usec_init = 0;
    gettimeofday(&tpend_begin, NULL);
    printf("rk-debug[%s %d] slt_gpu_light v5\n",__FUNCTION__,__LINE__);

    //初始化egl
    dpy = initEGLContex();

    gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] egl init use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);

    init_crc_table();

    //init src param
    src.w = 64;
    src.h = 64;
    src.need_fbo = 0;
    src.drm_format = DRM_FORMAT_RGB888;
    src.is_afbc = 0;

    //init src param
    src2.w = 64;
    src2.h = 64;
    src2.need_fbo = 0;
    src2.drm_format = DRM_FORMAT_RGB888;
    src2.is_afbc = 0;

    //init win param
    win.w = 64;
    win.h = 64;
    win.need_fbo = 1;
    win.drm_format = DRM_FORMAT_RGB888;
    win.is_afbc = 0;
    win.need_cpu_cached = 1;

    create_drm_fd(&src);
    create_drm_fd(&src2);
    create_drm_fd(&win);

    gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] drm fd init use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);

    {//read source file
        FILE * pfile = NULL;
        char layername[100] ;

        //sprintf(layername,"/system/slt/gpu_data_light/golden_1280x720_nv12_p1.bin");
        sprintf(layername,"/usr/lib/slt/gpu_data_light/golden_1280x720_rgb_p1.bin");

        pfile = fopen(layername,"rb");
        if(pfile)
        {
            int size =src.w*src.h*(src.is_afbc?2:1)* get_format_size(src.drm_format);
            int fret= fread((void *)src.drm_viraddr,1,size,pfile);
            printf("rk-debug read %s Success fread_size=%d,asize=%d\n",layername,fret,size);
            fclose(pfile);
        }else{
            printf("rk-debug Could not open file:%s !\n",layername);
        }

    }
    {//read source file
        FILE * pfile = NULL;
        char layername[100] ;

        //sprintf(layername,"/system/slt/gpu_data_light/golden_1280x720_nv12_p2.bin");
        sprintf(layername,"/usr/lib/slt/gpu_data_light/golden_1280x720_rgb_p2.bin");

        pfile = fopen(layername,"rb");
        if(pfile)
        {
            int size =src2.w*src2.h*(src2.is_afbc?2:1)* get_format_size(src2.drm_format);
            int fret= fread((void *)src2.drm_viraddr,1,size,pfile);
            printf("rk-debug read %s Success fread_size=%d,asize=%d\n",layername,fret,size);
            fclose(pfile);
        }else{
            printf("rk-debug Could not open file:%s !\n",layername);
        }

    }

    gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] read file use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);


    create_texture_fbo_img(dpy, &src);
    create_texture_fbo_img(dpy, &src2);
    create_texture_fbo_img(dpy, &win);


    gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] create_texture use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);
    gettimeofday(&tpend_begin, NULL);

    if(!setupGraphics(win.w, win.h)) {
        fprintf(stderr, "Could not set up graphics.\n");
        return 0;
    }

    gTriangleVertices = (GLfloat *)malloc(((2*MESH_SIZE+1)*MESH_SIZE+1)*sizeof(GLfloat)*2);
    gtexVertices = (GLfloat *)malloc(((2*MESH_SIZE+1)*MESH_SIZE+1)*sizeof(GLfloat)*2);
    //caculateVertices(MESH_SIZE,MESH_SIZE, gTriangleVertices,  gtexVertices);
    caculateVertices_strip(MESH_SIZE,MESH_SIZE, gTriangleVertices,  gtexVertices);

    gettimeofday(&tpend_end, NULL);
    usec_init = 1000.0 * (tpend_end.tv_sec - tpend_begin.tv_sec) + (tpend_end.tv_usec - tpend_begin.tv_usec) / 1000.0;
    printf("rk-debug[%s %d] setupGraphics init use time=%f ms\n",__FUNCTION__,__LINE__,usec_init);

    return 0;
}
int slt_gpu_light_deinit(void)
{
    printf("rk-debug[%s %d]  \n",__FUNCTION__,__LINE__);
    free_drm_buf(&(src.drm_fd),&(src.drm_handle));
    free_drm_buf(&(src2.drm_fd),&(src2.drm_handle));
    free_drm_buf(&(win.drm_fd),&(win.drm_handle));
    deinitEGLContex();
//    close(drm_fd);
//    drm_fd = -1;
    return 0;
}

void send_cmd_to_gpu(const char* node_path,const char* cmd) {
    const char *route;
    int fd = -1;
    route = node_path;
    fd = open(route,O_WRONLY);
    printf("rk-debug[%s %d] node_path:%s cmd:%s fd:%d\n",__FUNCTION__,__LINE__,node_path,cmd,fd);
    if (fd > -1) {
//    if(1){
       struct timeval tpend1;
        gettimeofday(&tpend1, NULL);
        printf("SLT-GPU[%5ld.%06ld] fd %d, cmdc is %s\r\n",tpend1.tv_sec,tpend1.tv_usec, fd, cmd);
        if (write(fd, cmd, strlen(cmd)) < 0) {
            printf("rk-debug write %s failed\n", route);
            //fflush(stdout);tcdrain(1);
        }
        close(fd);
        return;
    }else
    {
        printf("rk-debug[%s %d] node_path:%s cmd:%s fd:%d  fail\n",__FUNCTION__,__LINE__,node_path,cmd,fd);
    }
}

#define GPU_RESET_NODE "/sys/devices/platform/ff400000.gpu/soft_reset"

int reset_gpu()
{
    printf("rk-debug[%s %d]  \n",__FUNCTION__,__LINE__);
    send_cmd_to_gpu(GPU_RESET_NODE,"1");
    usleep(50*1000);
    return 0;
}


pthread_mutex_t gpu_reset_mutex = PTHREAD_MUTEX_INITIALIZER;
int need_gpu_reset = 0;
#define RENDER_TIMEOUT 60  //ms


void* wait_thread(void *arg)
{
    struct timeval tthread;
    gettimeofday(&tthread, NULL);
    printf("SLT-GPU[%5ld.%06ld] wait_thread \r\n",tthread.tv_sec,tthread.tv_usec);

    printf("thread's ID is %lu , sleep:%d ms\n",pthread_self(),RENDER_TIMEOUT);
    usleep(RENDER_TIMEOUT * 1000);

    pthread_mutex_lock(&gpu_reset_mutex);

    printf("Now need_gpu_reset = %d\n",need_gpu_reset);
    if(need_gpu_reset)
        reset_gpu();

    pthread_mutex_unlock(&gpu_reset_mutex);
    return NULL;
}


int slt_gpu_light_run(void)
{
//    char * pPixelDataFront = NULL;
//    pPixelDataFront = (char*)malloc(w*h*4);

    struct timeval tpend1, tpend2;
    float usec1 = 0;

    int w = win.w;
    int h = win.h;

    for (int i = 0; i < 20; i++) {
        gettimeofday(&tpend1, NULL);
        int render_texture_id;
        unsigned int  read_crcdata;
        if(i%2 == 0) {
            render_texture_id = src.texture_id;
            read_crcdata = golden_crc_data1;
        }else {
            render_texture_id = src2.texture_id;
            read_crcdata = golden_crc_data2;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, render_texture_id);
        renderFrame();

        //创建等待线程:
        struct timeval tthread;
        gettimeofday(&tthread, NULL);
        printf("SLT-GPU[%5ld.%06ld] create \r\n",tthread.tv_sec,tthread.tv_usec);

        pthread_t thread_id;
        printf("main thread's ID is %lu\n",pthread_self());
        need_gpu_reset = 1;
        printf("In main func, need_gpu_reset = %d\n",need_gpu_reset);
        if (!pthread_create(&thread_id, NULL, wait_thread, NULL)) {
            printf("Create thread success!\n");
        } else {
            printf("Create thread failed!\n");
        }

        //主线程做渲染
        glFinish();

        //渲染耗时正常,无需gpu reset
        pthread_mutex_lock(&gpu_reset_mutex);
        need_gpu_reset = 0;
        pthread_mutex_unlock(&gpu_reset_mutex);


        gettimeofday(&tpend2, NULL);
        usec1 = 1000.0 * (tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec - tpend1.tv_usec) / 1000.0;
        printf("rk-debug[%s %d]  glFinish i:%d use time=%f ms\n",__FUNCTION__,__LINE__,i,usec1);
        gettimeofday(&tpend1, NULL);

        dma_buf_raw_sync_start(win.drm_fd);

        //check crc
        unsigned int crcdata = crc32(0xffffffff, (unsigned char *)win.drm_viraddr, win.w * win.h * 3);
        //unsigned int crcdata = crc32(0xffffffff, (unsigned char *)pPixelDataFront, win.w * win.h * 4);
        printf("rk-debug[%s %d] renderFrame i:%d crc_result is:0x%X \n",__FUNCTION__,__LINE__,i,crcdata);

        if(crcdata != read_crcdata)
        {
            printf("================ SLT-GPU[%5ld.%06ld] crcdata-err index=%d crcdata:0x%X read:0x%X\r\n",tpend1.tv_sec,tpend1.tv_usec,i,crcdata,read_crcdata);
            pthread_join(thread_id, NULL);
        }else{

            printf("SLT-GPU[%5ld.%06ld] crcdata-success i=%d crcdata:0x%X read:0x%X\r\n",tpend1.tv_sec,tpend1.tv_usec,i,crcdata,read_crcdata);
            gettimeofday(&tpend2, NULL);
            usec1 = 1000.0 * (tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec - tpend1.tv_usec) / 1000.0;
            printf("rk-debug[%s %d]  checkcrc i:%d use time=%f ms\n",__FUNCTION__,__LINE__,i,usec1);

            printf("rk-debug[%s %d] slt_gpu pass and run exit(0); \n",__FUNCTION__,__LINE__);
            //exit(0);

            pthread_join(thread_id, NULL);
            return 0;
        }

        gettimeofday(&tpend2, NULL);
        usec1 = 1000.0 * (tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec - tpend1.tv_usec) / 1000.0;
        printf("rk-debug[%s %d]  checkcrc i:%d use time=%f ms\n",__FUNCTION__,__LINE__,i,usec1);

    }




#if 0 //read rgba pixel
    char * pPixelDataFront = NULL;
    pPixelDataFront = (char*)malloc(w*h*4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pPixelDataFront);
    dumpPixels(1,w,h,pPixelDataFront);
#endif
    //exit(-1);

    return -1;
}

#if 0
int is_first_call = 0;

int main()
{

    for(int i = 0 ;i<10; i++)
    {
        slt_gpu_light_init();
        slt_gpu_light_run();
        slt_gpu_light_deinit();
        sleep(5);
    }


    return 0;
}
#endif
