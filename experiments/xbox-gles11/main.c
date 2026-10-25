// Separate, bounded graphics probe. It never modifies the game installation.
#define GL_GLEXT_PROTOTYPES 1
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>
#include <stdio.h>

int main(void) {
    FILE *log = fopen("D:\\gles11.log", "w");
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    glContextInit(640, 480);
    glSwapInterval(1);
    const GLfloat vertices[] = {-0.7f,-0.7f, 0.7f,-0.7f, -0.7f,0.7f, 0.7f,0.7f};
    const GLfloat uv[] = {0,0, 1,0, 0,1, 1,1};
    const unsigned char pixels[] = {255,0,0,255, 0,255,0,255,
                                   0,0,255,255, 255,255,0,255};
    GLuint texture, target, framebuffer;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffersOES(1, &framebuffer);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);
    glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES,
                            GL_TEXTURE_2D, target, 0);
    GLenum status = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
    if (log) {
        fprintf(log, "FBO status: 0x%x (complete 0x%x)\n", status, GL_FRAMEBUFFER_COMPLETE_OES);
        fflush(log);
    }
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, uv);
    DWORD start = GetTickCount();
    unsigned frames = 0;
    while (GetTickCount() - start < 45000) {
        glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);
        glViewport(0, 0, 256, 256);
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D, texture);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef((GetTickCount() - start) * 0.06f, 0, 0, 1);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
        glViewport(0, 0, 640, 480);
        glClearColor(0.05f, 0.15f, 0.25f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glLoadIdentity();
        glBindTexture(GL_TEXTURE_2D, target);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glFlipNV2A();
        ++frames;
    }
    if (log) {
        fprintf(log, "Frames: %u; elapsed ms: %lu; GL error: 0x%x\n",
                frames, (unsigned long)(GetTickCount() - start), glGetError());
        fclose(log);
    }
    XReboot();
    return 0;
}
