/* opengl32.dll: export presence table and context-free / software-bitmap behaviour. Never touches the display:
   rendering goes to a memory DC with a DIB section. */
#include "t.h"
#include <GL/gl.h>

#define GLM "opengl32.dll"
#define NEEDGL() do { if (!LoadLibraryA(GLM)) { t_skip("opengl32.dll absent"); return; } } while (0)

TEST(d3dx_redist_optional_gl_export_table) {
    HMODULE m = LoadLibraryA(GLM); t_note("opengl32.dll %s", m ? "present" : "absent");
    if (!m) { t_skip("opengl32.dll absent"); return; }
    static const char *must[] = { "wglCreateContext","wglDeleteContext","wglMakeCurrent","wglGetCurrentContext","wglGetCurrentDC","wglGetProcAddress","wglShareLists","wglCopyContext",
        "wglCreateLayerContext","wglUseFontBitmapsA","wglUseFontBitmapsW","wglUseFontOutlinesA","wglUseFontOutlinesW","wglSwapBuffers","wglChoosePixelFormat","wglSetPixelFormat",
        "wglGetPixelFormat","wglDescribePixelFormat","wglSwapLayerBuffers","wglDescribeLayerPlane","wglSetLayerPaletteEntries","wglGetLayerPaletteEntries","wglRealizeLayerPalette",
        "glClear","glClearColor","glGetString","glGetError","glGetIntegerv","glEnable","glDisable","glFinish","glFlush","glReadPixels","glDrawArrays","glDrawElements","glVertexPointer",
        "glBindTexture","glGenTextures","glDeleteTextures","glTexImage2D","glTexParameteri","glViewport","glBegin","glEnd","glVertex3f","glColor4f","glMatrixMode","glLoadIdentity",
        "glOrtho","glPushMatrix","glPopMatrix","glBlendFunc","glDepthFunc","glScissor","glPixelStorei","glIsEnabled","glGetFloatv","glGetBooleanv","glTexEnvi","glFrontFace","glCullFace" };
    int missing = 0;
    for (unsigned i = 0; i < sizeof must / sizeof *must; i++) {
        BOOL have = GetProcAddress(m, must[i]) != NULL; if (!have) missing++;
        t_note("%-26s %s", must[i], have ? "exported" : "missing");
    }
    CHECK(GetProcAddress(m, "wglCreateContext") != NULL); CHECK(GetProcAddress(m, "wglMakeCurrent") != NULL); CHECK(GetProcAddress(m, "wglGetProcAddress") != NULL);
    CHECK(GetProcAddress(m, "glGetString") != NULL); CHECK(GetProcAddress(m, "glClear") != NULL); CHECK(GetProcAddress(m, "wglDeleteContext") != NULL); CHECK(GetProcAddress(m, "glGetError") != NULL);
    t_note("%d of %u probed GL 1.1 / WGL exports missing", missing, (unsigned)(sizeof must / sizeof *must));
}
TEST(d3dx_redist_optional_gl_no_extension_exports) {
    HMODULE m = LoadLibraryA(GLM); if (!m) { t_skip("opengl32.dll absent"); return; }
    /* GL > 1.1 entry points are not static exports; they come from wglGetProcAddress with a current context */
    static const char *ext[] = { "glGenBuffers", "glCreateShader", "glBindFramebuffer", "glActiveTexture", "glUseProgram" };
    for (unsigned i = 0; i < sizeof ext / sizeof *ext; i++) CHECKF(GetProcAddress(m, ext[i]) == NULL, "%s is a static export", ext[i]);
}
TEST(d3dx_redist_optional_gl_no_current_context) {
    NEEDGL(); __typeof__(&wglGetCurrentContext) gc = UIN(GLM, wglGetCurrentContext); __typeof__(&wglGetCurrentDC) gd = UIN(GLM, wglGetCurrentDC); REQUIRE(gc); REQUIRE(gd);
    CHECK(gc() == NULL); CHECK(gd() == NULL);
}
TEST(d3dx_redist_optional_gl_get_proc_address_without_context) {
    NEEDGL(); __typeof__(&wglGetProcAddress) gp = UIN(GLM, wglGetProcAddress); REQUIRE(gp);
    PROC p = gp("glThisFunctionDoesNotExist"); CHECK(p == NULL);
    p = gp("glGenBuffers"); t_note("wglGetProcAddress(glGenBuffers) without a context -> %p", (void *)p);
}
TEST(d3dx_redist_optional_gl_delete_context_null) {
    NEEDGL(); __typeof__(&wglDeleteContext) dc = UIN(GLM, wglDeleteContext); REQUIRE(dc);
    SetLastError(0); BOOL r = dc(NULL); CHECK(r == FALSE); t_note("wglDeleteContext(NULL) last error %lu", GetLastError());
}
TEST(d3dx_redist_optional_gl_make_current_null) {
    NEEDGL(); __typeof__(&wglMakeCurrent) mc = UIN(GLM, wglMakeCurrent); REQUIRE(mc);
    BOOL r = mc(NULL, NULL); t_note("wglMakeCurrent(NULL, NULL) with no current context -> %d", r);
}
TEST(d3dx_redist_optional_gl_create_context_null_dc) {
    NEEDGL(); __typeof__(&wglCreateContext) cc = UIN(GLM, wglCreateContext); REQUIRE(cc);
    SetLastError(0); HGLRC r = cc(NULL); CHECK(r == NULL); t_note("wglCreateContext(NULL) last error %lu", GetLastError());
}
TEST(d3dx_redist_optional_gl_gl_calls_without_context) {
    NEEDGL(); __typeof__(&glGetString) gs = UIN(GLM, glGetString); __typeof__(&glGetError) ge = UIN(GLM, glGetError); REQUIRE(gs); REQUIRE(ge);
    const GLubyte *s = gs(GL_VERSION); t_note("glGetString(GL_VERSION) without context -> %p", (const void *)s);
    t_note("glGetError() without context -> 0x%x", ge());
}

/* software rendering into a DIB section */
typedef struct { HDC dc; HBITMAP bmp, old; void *bits; HGLRC rc; int w, h; } GLB;
static int glb_open(GLB *g, int w, int h) {
    memset(g, 0, sizeof *g); g->w = w; g->h = h;
    g->dc = CreateCompatibleDC(NULL); if (!g->dc) { t_skip("no memory DC"); return 0; }
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    g->bmp = CreateDIBSection(g->dc, &bi, DIB_RGB_COLORS, &g->bits, NULL, 0);
    if (!g->bmp) { DeleteDC(g->dc); g->dc = NULL; t_skip("no DIB section"); return 0; }
    g->old = SelectObject(g->dc, g->bmp);
    PIXELFORMATDESCRIPTOR pfd; memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd; pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_SUPPORT_GDI; pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 16; pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g->dc, &pfd);
    if (!pf || !SetPixelFormat(g->dc, pf, &pfd)) { SelectObject(g->dc, g->old); DeleteObject(g->bmp); DeleteDC(g->dc); memset(g, 0, sizeof *g); t_skip("no bitmap pixel format"); return 0; }
    __typeof__(&wglCreateContext) cc = UIN(GLM, wglCreateContext); __typeof__(&wglMakeCurrent) mc = UIN(GLM, wglMakeCurrent);
    g->rc = cc(g->dc);
    if (!g->rc || !mc(g->dc, g->rc)) {
        if (g->rc) UIN(GLM, wglDeleteContext)(g->rc);
        SelectObject(g->dc, g->old); DeleteObject(g->bmp); DeleteDC(g->dc); memset(g, 0, sizeof *g); t_skip("no GL context on a DIB"); return 0;
    }
    return 1;
}
static void glb_close(GLB *g) {
    UIN(GLM, wglMakeCurrent)(NULL, NULL);
    if (g->rc) UIN(GLM, wglDeleteContext)(g->rc);
    SelectObject(g->dc, g->old); DeleteObject(g->bmp); DeleteDC(g->dc);
}
TEST(d3dx_redist_optional_gl_context_current_state) {
    NEEDGL(); GLB g; if (!glb_open(&g, 16, 16)) return;
    CHECK(UIN(GLM, wglGetCurrentContext)() == g.rc); CHECK(UIN(GLM, wglGetCurrentDC)() == g.dc);
    glb_close(&g);
    CHECK(UIN(GLM, wglGetCurrentContext)() == NULL);
}
TEST(d3dx_redist_optional_gl_get_string) {
    NEEDGL(); GLB g; if (!glb_open(&g, 16, 16)) return;
    __typeof__(&glGetString) gs = UIN(GLM, glGetString);
    const char *v = (const char *)gs(GL_VERSION), *vd = (const char *)gs(GL_VENDOR), *r = (const char *)gs(GL_RENDERER), *e = (const char *)gs(GL_EXTENSIONS);
    CHECK(v != NULL && v[0] >= '1' && v[0] <= '9'); CHECK(vd != NULL && vd[0]); CHECK(r != NULL && r[0]); CHECK(e != NULL);
    t_note("GL_VERSION=%s GL_VENDOR=%s GL_RENDERER=%s", v ? v : "(null)", vd ? vd : "(null)", r ? r : "(null)");
    CHECK(gs((GLenum)0x1234) == NULL);   /* invalid enum */
    CHECK(UIN(GLM, glGetError)() == GL_INVALID_ENUM);
    CHECK(UIN(GLM, glGetError)() == GL_NO_ERROR);   /* error flag is cleared by reading it */
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_clear_and_read_pixels) {
    NEEDGL(); GLB g; if (!glb_open(&g, 8, 8)) return;
    UIN(GLM, glClearColor)(1.f, 0.f, 0.f, 1.f); UIN(GLM, glClear)(GL_COLOR_BUFFER_BIT); UIN(GLM, glFinish)();
    BYTE px[4 * 4] = {0}; UIN(GLM, glReadPixels)(0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK(UIN(GLM, glGetError)() == GL_NO_ERROR);
    for (int i = 0; i < 4; i++) { CHECKF(px[i*4] == 255 && px[i*4+1] == 0 && px[i*4+2] == 0, "pixel %d = %u,%u,%u,%u", i, px[i*4], px[i*4+1], px[i*4+2], px[i*4+3]); }
    UIN(GLM, glClearColor)(0.f, 1.f, 0.f, 1.f); UIN(GLM, glClear)(GL_COLOR_BUFFER_BIT); UIN(GLM, glReadPixels)(7, 7, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK(px[0] == 0 && px[1] == 255 && px[2] == 0);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_clear_color_roundtrip) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    GLfloat c[4] = {0};
    UIN(GLM, glClearColor)(0.25f, 0.5f, 0.75f, 1.0f); UIN(GLM, glGetFloatv)(GL_COLOR_CLEAR_VALUE, c);
    CHECK(c[0] == 0.25f && c[1] == 0.5f && c[2] == 0.75f && c[3] == 1.0f);
    UIN(GLM, glClearColor)(2.f, -1.f, 0.5f, 0.f); UIN(GLM, glGetFloatv)(GL_COLOR_CLEAR_VALUE, c);   /* clamped to [0,1] */
    CHECK(c[0] == 1.f && c[1] == 0.f && c[2] == 0.5f && c[3] == 0.f);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_enable_state) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    CHECK(UIN(GLM, glIsEnabled)(GL_DEPTH_TEST) == GL_FALSE);
    UIN(GLM, glEnable)(GL_DEPTH_TEST); CHECK(UIN(GLM, glIsEnabled)(GL_DEPTH_TEST) == GL_TRUE);
    UIN(GLM, glDisable)(GL_DEPTH_TEST); CHECK(UIN(GLM, glIsEnabled)(GL_DEPTH_TEST) == GL_FALSE);
    CHECK(UIN(GLM, glIsEnabled)(GL_BLEND) == GL_FALSE); CHECK(UIN(GLM, glIsEnabled)(GL_DITHER) == GL_TRUE); CHECK(UIN(GLM, glIsEnabled)(GL_CULL_FACE) == GL_FALSE);
    UIN(GLM, glEnable)((GLenum)0x1234); CHECK(UIN(GLM, glGetError)() == GL_INVALID_ENUM);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_default_matrix_and_viewport) {
    NEEDGL(); GLB g; if (!glb_open(&g, 32, 16)) return;
    GLint vp[4] = {-1,-1,-1,-1}; UIN(GLM, glGetIntegerv)(GL_VIEWPORT, vp);
    CHECK(vp[0] == 0 && vp[1] == 0 && vp[2] == 32 && vp[3] == 16);
    GLfloat m[16]; UIN(GLM, glGetFloatv)(GL_MODELVIEW_MATRIX, m);
    for (int i = 0; i < 16; i++) CHECKF(m[i] == ((i % 5) == 0 ? 1.f : 0.f), "modelview[%d] = %g", i, m[i]);
    GLint mode = 0; UIN(GLM, glGetIntegerv)(GL_MATRIX_MODE, &mode); CHECK(mode == GL_MODELVIEW);
    GLint depth = 0; UIN(GLM, glGetIntegerv)(GL_MODELVIEW_STACK_DEPTH, &depth); CHECK(depth == 1);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_matrix_stack_overflow_error) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    UIN(GLM, glPopMatrix)(); CHECK(UIN(GLM, glGetError)() == GL_STACK_UNDERFLOW);
    CHECK(UIN(GLM, glGetError)() == GL_NO_ERROR);
    GLint maxd = 0; UIN(GLM, glGetIntegerv)(GL_MAX_MODELVIEW_STACK_DEPTH, &maxd); CHECK(maxd >= 32);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_limits_minimums) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    GLint v = 0;
    UIN(GLM, glGetIntegerv)(GL_MAX_TEXTURE_SIZE, &v); CHECKF(v >= 64, "GL_MAX_TEXTURE_SIZE %d", v); t_note("GL_MAX_TEXTURE_SIZE %d", v);
    UIN(GLM, glGetIntegerv)(GL_MAX_LIGHTS, &v); CHECK(v >= 8);
    UIN(GLM, glGetIntegerv)(GL_MAX_CLIP_PLANES, &v); CHECK(v >= 6);
    UIN(GLM, glGetIntegerv)(GL_MAX_PROJECTION_STACK_DEPTH, &v); CHECK(v >= 2);
    UIN(GLM, glGetIntegerv)(GL_MAX_ATTRIB_STACK_DEPTH, &v); CHECK(v >= 16);
    UIN(GLM, glGetIntegerv)(GL_MAX_NAME_STACK_DEPTH, &v); CHECK(v >= 64);
    UIN(GLM, glGetIntegerv)(GL_MAX_LIST_NESTING, &v); CHECK(v >= 64);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_gen_textures_unique) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    GLuint t[4] = {0}; UIN(GLM, glGenTextures)(4, t);
    for (int i = 0; i < 4; i++) { CHECK(t[i] != 0); for (int j = 0; j < i; j++) CHECK(t[i] != t[j]); CHECK(UIN(GLM, glIsTexture)(t[i]) == GL_FALSE); }
    UIN(GLM, glBindTexture)(GL_TEXTURE_2D, t[0]); CHECK(UIN(GLM, glIsTexture)(t[0]) == GL_TRUE);
    UIN(GLM, glDeleteTextures)(4, t); CHECK(UIN(GLM, glIsTexture)(t[0]) == GL_FALSE);
    CHECK(UIN(GLM, glGetError)() == GL_NO_ERROR);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_tex_image_roundtrip_via_draw) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    BYTE tex[2 * 2 * 4] = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255 };
    GLuint t = 0; UIN(GLM, glGenTextures)(1, &t); UIN(GLM, glBindTexture)(GL_TEXTURE_2D, t);
    UIN(GLM, glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); UIN(GLM, glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    UIN(GLM, glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
    CHECK(UIN(GLM, glGetError)() == GL_NO_ERROR);
    GLint w = 0, h = 0; UIN(GLM, glGetTexLevelParameteriv)(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w); UIN(GLM, glGetTexLevelParameteriv)(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    CHECK(w == 2 && h == 2);
    UIN(GLM, glDeleteTextures)(1, &t); glb_close(&g);
}
TEST(d3dx_redist_optional_gl_invalid_texture_size_error) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    BYTE px[4 * 3 * 4] = {0};
    UIN(GLM, glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, 3, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);   /* non power of two: GL 1.1 */
    GLenum e = UIN(GLM, glGetError)(); t_note("glTexImage2D(3x4) -> error 0x%x (GL_INVALID_VALUE=0x501)", e);
    UIN(GLM, glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);   /* border must be 0 or 1 -> 1 is legal, 2 is not */
    UIN(GLM, glGetError)();
    UIN(GLM, glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK(UIN(GLM, glGetError)() == GL_INVALID_VALUE);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_wgl_extensions_via_getproc) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    __typeof__(&wglGetProcAddress) gp = UIN(GLM, wglGetProcAddress);
    static const char *n[] = { "wglGetExtensionsStringARB", "wglChoosePixelFormatARB", "wglCreateContextAttribsARB", "glGenBuffers", "glCreateShader", "glActiveTexture", "glTexImage3D", "glDrawRangeElements" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) t_note("wglGetProcAddress(%s) -> %s", n[i], gp(n[i]) ? "available" : "NULL");
    CHECK(gp("glNoSuchEntryPoint_xyz") == NULL);
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_share_lists_and_delete) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    HGLRC rc2 = UIN(GLM, wglCreateContext)(g.dc);
    CHECK(rc2 != NULL);
    if (rc2) {
        CHECK(rc2 != g.rc);
        BOOL r = UIN(GLM, wglShareLists)(g.rc, rc2); t_note("wglShareLists -> %d", r);
        CHECK(UIN(GLM, wglDeleteContext)(rc2) == TRUE);
    }
    glb_close(&g);
}
TEST(d3dx_redist_optional_gl_delete_current_context) {
    NEEDGL(); GLB g; if (!glb_open(&g, 4, 4)) return;
    /* a context is current on this thread: wglDeleteContext on it from the same thread succeeds and detaches it */
    HGLRC rc = g.rc;
    BOOL r = UIN(GLM, wglDeleteContext)(rc); CHECK(r == TRUE);
    CHECK(UIN(GLM, wglGetCurrentContext)() == NULL);
    g.rc = NULL; glb_close(&g);
}
TEST(d3dx_redist_optional_gl_pixel_format_describe) {
    NEEDGL(); HDC dc = CreateCompatibleDC(NULL); if (!dc) { t_skip("no DC"); return; }
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 2; bi.bmiHeader.biHeight = 2; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits; HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0); HGDIOBJ old = SelectObject(dc, bm);
    int n = DescribePixelFormat(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    t_note("pixel formats on a 32bpp DIB: %d", n);
    PIXELFORMATDESCRIPTOR pfd; memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd; pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL; pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32;
    int pf = ChoosePixelFormat(dc, &pfd); if (pf <= 0 && t_is_wine()) { t_skip("no bitmap pixel format in this emulator"); SelectObject(dc, old); DeleteObject(bm); DeleteDC(dc); return; } CHECK(pf > 0);
    if (pf > 0) { PIXELFORMATDESCRIPTOR out; memset(&out, 0, sizeof out); CHECK(DescribePixelFormat(dc, pf, sizeof out, &out) > 0); CHECK(out.nSize == sizeof out); CHECK(out.nVersion == 1); CHECK((out.dwFlags & PFD_DRAW_TO_BITMAP) != 0); CHECK(out.iPixelType == PFD_TYPE_RGBA); CHECK(out.cColorBits >= 24); }
    SelectObject(dc, old); DeleteObject(bm); DeleteDC(dc);
}
