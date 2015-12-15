// Example low level rendering Unity plugin
#include "RenderingPlugin.h"
#include "Unity/IUnityGraphics.h"

#include <math.h>
#include <stdio.h>
//#include <vector>
//#include <string>
#include <jni.h>
#include<android/log.h>
// --------------------------------------------------------------------------
// Include headers for the graphics APIs we support

#include <GLES2/gl2.h>

#define LOG_TAG "debug log"
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args)
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
#define LOGW(fmt, args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##args)
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args)
// --------------------------------------------------------------------------
// Helper utilities

// Prints a string
static void DebugLog(const char* str) {
	printf("%s", str);
}

// COM-like Release macro
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(a) if (a) { a->Release(); a = NULL; }
#endif

// --------------------------------------------------------------------------
// SetTimeFromUnity, an example function we export which is called by one of the scripts.

static float g_Time;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity(
		float t) {
	g_Time = t;
}

// --------------------------------------------------------------------------
// SetTextureFromUnity, an example function we export which is called by one of the scripts.

static void* g_TexturePointer = NULL;
static int g_TexWidth = 0;
static int g_TexHeight = 0;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(
		void* texturePtr, int w, int h)

		{
	// A script calls this at initialization time; just remember the texture pointer here.
	// Will update texture pixels each frame from the plugin rendering event (texture update
	// needs to happen on the rendering thread).
	g_TexturePointer = texturePtr;

	g_TexWidth = w;
	g_TexHeight = h;
}

// --------------------------------------------------------------------------
// SetUnityStreamingAssetsPath, an example function we export which is called by one of the scripts.

static const char* s_UnityStreamingAssetsPath;
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetUnityStreamingAssetsPath(
		const char* path) {
	s_UnityStreamingAssetsPath = path;
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(
		UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(
		IUnityInterfaces* unityInterfaces) {
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

static void DoEventGraphicsDeviceGLES(UnityGfxDeviceEventType eventType);

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(
		UnityGfxDeviceEventType eventType) {
	UnityGfxRenderer currentDeviceType = s_DeviceType;

	switch (eventType) {
	case kUnityGfxDeviceEventInitialize: {
		LOGI("OnGraphicsDeviceEvent(Initialize).\n");
		s_DeviceType = s_Graphics->GetRenderer();
		currentDeviceType = s_DeviceType;
		break;
	}

	case kUnityGfxDeviceEventShutdown: {
		LOGI("OnGraphicsDeviceEvent(Shutdown).\n");
		s_DeviceType = kUnityGfxRendererNull;
		g_TexturePointer = NULL;
		break;
	}

	case kUnityGfxDeviceEventBeforeReset: {
		LOGI("OnGraphicsDeviceEvent(BeforeReset).\n");
		break;
	}

	case kUnityGfxDeviceEventAfterReset: {
		LOGI("OnGraphicsDeviceEvent(AfterReset).\n");
		break;
	}
	};
	DoEventGraphicsDeviceGLES(eventType);

}

// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.

struct MyVertex {
	float x, y, z;
	unsigned int color;
};
static void SetDefaultGraphicsState();
static void DoRendering(const float* worldMatrix, const float* identityMatrix,
		float* projectionMatrix, const MyVertex* verts);

static void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
	// Unknown graphics device type? Do nothing.
//	if (s_DeviceType == kUnityGfxRendererNull)
//		return;
	//LOGI("22222222222222222222222222+++renderevent");
	// A colored triangle. Note that colors will come out differently
	// in D3D9/11 and OpenGL, for example, since they expect color bytes
	// in different ordering.
	MyVertex verts[3] = { { -0.5f, -0.25f, 0, 0xFFff0000 }, { 0.5f, -0.25f, 0,
			0xFF00ff00 }, { 0, 0.5f, 0, 0xFF0000ff }, };

	// Some transformation matrices: rotate around Z axis for world
	// matrix, identity view matrix, and identity projection matrix.

	float phi = g_Time;
	float cosPhi = cosf(phi);
	float sinPhi = sinf(phi);

	float worldMatrix[16] = { cosPhi, -sinPhi, 0, 0, sinPhi, cosPhi, 0, 0, 0, 0,
			1, 0, 0, 0, 0.7f, 1, };
	float identityMatrix[16] =
			{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, };
	float projectionMatrix[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,
			1, };

	// Actual functions defined below
	SetDefaultGraphicsState();
	DoRendering(worldMatrix, identityMatrix, projectionMatrix, verts);
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() {
	return OnRenderEvent;
}

// -------------------------------------------------------------------
// GLES setup/teardown code
#define VPROG_SRC(ver, attr, varying)								\
	ver																\
	attr " highp vec3 pos;\n"										\
	attr " lowp vec4 color;\n"										\
	"\n"															\
	varying " lowp vec4 ocolor;\n"									\
	"\n"															\
	"uniform highp mat4 worldMatrix;\n"								\
	"uniform highp mat4 projMatrix;\n"								\
	"\n"															\
	"void main()\n"													\
	"{\n"															\
	"	gl_Position = (projMatrix * worldMatrix) * vec4(pos,1);\n"	\
	"	ocolor = color;\n"											\
	"}\n"															\

static const char* kGlesVProgTextGLES2 = VPROG_SRC("\n", "attribute",
		"varying");
static const char* kGlesVProgTextGLES3 = VPROG_SRC("#version 300 es\n", "in",
		"out");

#undef VPROG_SRC

#define FSHADER_SRC(ver, varying, outDecl, outVar)	\
	ver												\
	outDecl											\
	varying " lowp vec4 ocolor;\n"					\
	"\n"											\
	"void main()\n"									\
	"{\n"											\
	"	" outVar " = ocolor;\n"						\
	"}\n"											\

static const char* kGlesFShaderTextGLES2 = FSHADER_SRC("\n", "varying", "\n",
		"gl_FragColor");
static const char* kGlesFShaderTextGLES3 = FSHADER_SRC("#version 300 es\n",
		"in", "out lowp vec4 fragColor;\n", "fragColor");

#undef FSHADER_SRC

static GLuint g_VProg;
static GLuint g_FShader;
static GLuint g_Program;
static int g_WorldMatrixUniformIndex;
static int g_ProjMatrixUniformIndex;

static GLuint CreateShader(GLenum type, const char* text) {
	GLuint ret = glCreateShader(type);
	glShaderSource(ret, 1, &text, NULL);
	glCompileShader(ret);

	return ret;
}

static void DoEventGraphicsDeviceGLES(UnityGfxDeviceEventType eventType) {
	LOGI("OpenGLES 2.0 device\n");
	g_VProg = CreateShader(GL_VERTEX_SHADER, kGlesVProgTextGLES2);
	g_FShader = CreateShader(GL_FRAGMENT_SHADER, kGlesFShaderTextGLES2);

	g_Program = glCreateProgram();
	glBindAttribLocation(g_Program, 1, "pos");
	glBindAttribLocation(g_Program, 2, "color");
	glAttachShader(g_Program, g_VProg);
	glAttachShader(g_Program, g_FShader);
	glLinkProgram(g_Program);

	g_WorldMatrixUniformIndex = glGetUniformLocation(g_Program, "worldMatrix");
	g_ProjMatrixUniformIndex = glGetUniformLocation(g_Program, "projMatrix");
}

// --------------------------------------------------------------------------
// SetDefaultGraphicsState
//
// Helper function to setup some "sane" graphics state. Rendering state
// upon call into our plugin can be almost completely arbitrary depending
// on what was rendered in Unity before.
// Before calling into the plugin, Unity will set shaders to null,
// and will unbind most of "current" objects (e.g. VBOs in OpenGL case).
//
// Here, we set culling off, lighting off, alpha blend & test off, Z
// comparison to less equal, and Z writes off.

static void SetDefaultGraphicsState() {
	// OpenGLES case
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

}

static void FillTextureFromCode(int width, int height, int stride,
		unsigned char* dst) {
	const float t = g_Time * 4.0f;

	for (int y = 0; y < height; ++y) {
		unsigned char* ptr = dst;
		for (int x = 0; x < width; ++x) {
			// Simple oldskool "plasma effect", a bunch of combined sine waves
			int vv = int(
					(127.0f + (127.0f * sinf(x / 7.0f + t)))
							+ (127.0f + (127.0f * sinf(y / 5.0f - t)))
							+ (127.0f + (127.0f * sinf((x + y) / 6.0f - t)))
							+ (127.0f
									+ (127.0f
											* sinf(
													sqrtf(float(x * x + y * y))
															/ 4.0f - t)))) / 4;

			// Write the texture pixel
			ptr[0] = vv;
			ptr[1] = vv;
			ptr[2] = vv;
			ptr[3] = vv;

			// To next pixel (our pixels are 4 bpp)
			ptr += 4;
		}

		// To next image row
		dst += stride;
	}
}

static void DoRendering(const float* worldMatrix, const float* identityMatrix,
		float* projectionMatrix, const MyVertex* verts) {
	// OpenGLES case

	// Tweak the projection matrix a bit to make it match what identity projection would do in D3D case.
	projectionMatrix[10] = 2.0f;
	projectionMatrix[14] = -1.0f;

	glUseProgram(g_Program);
	glUniformMatrix4fv(g_WorldMatrixUniformIndex, 1, GL_FALSE, worldMatrix);
	glUniformMatrix4fv(g_ProjMatrixUniformIndex, 1, GL_FALSE, projectionMatrix);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	const int stride = 3 * sizeof(float) + sizeof(unsigned int);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
			(const float*) verts);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_TRUE, stride,
			(const float*) verts + 3);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	// update native texture from code
	if (g_TexturePointer) {
		GLuint gltex = (GLuint) (size_t)(g_TexturePointer);
		glBindTexture(GL_TEXTURE_2D, gltex);

		unsigned char* data = new unsigned char[g_TexWidth * g_TexHeight * 4];
		FillTextureFromCode(g_TexWidth, g_TexHeight, g_TexHeight * 4, data);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_TexWidth, g_TexHeight,
		GL_RGBA, GL_UNSIGNED_BYTE, data);
		delete[] data;
	}
}
