#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

PSP_MODULE_INFO("SimEndurance3D", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

// Model 3D cadangan (bentuk bodi mobil sederhana) jika mobil.obj belum dimasukkan
Vertex __attribute__((aligned(16))) fallbackCar[12] = {
    // Atap & kap depan (Merah balap)
    {0xFF0000FF,  0.0f,  0.6f, -1.5f}, {0xFF0000CC, -0.8f,  0.0f,  1.5f}, {0xFF0000CC,  0.8f,  0.0f,  1.5f},
    // Sisi kanan (Merah gelap)
    {0xFF0000FF,  0.0f,  0.6f, -1.5f}, {0xFF000088,  0.8f,  0.0f,  1.5f}, {0xFF000088,  0.8f,  0.0f, -1.5f},
    // Sisi kiri (Merah gelap)
    {0xFF0000FF,  0.0f,  0.6f, -1.5f}, {0xFF000088, -0.8f,  0.0f, -1.5f}, {0xFF000088, -0.8f,  0.0f,  1.5f},
    // Bagian belakang (Abu-abu gelap)
    {0xFF0000FF,  0.0f,  0.6f, -1.5f}, {0xFF333333,  0.8f,  0.0f, -1.5f}, {0xFF333333, -0.8f,  0.0f, -1.5f}
};

// Lintasan aspal sederhana di bawah mobil
Vertex __attribute__((aligned(16))) trackFloor[6] = {
    {0xFF444444, -6.0f, -0.05f, -20.0f}, {0xFF444444,  6.0f, -0.05f, -20.0f}, {0xFF222222,  6.0f, -0.05f,  20.0f},
    {0xFF444444, -6.0f, -0.05f, -20.0f}, {0xFF222222,  6.0f, -0.05f,  20.0f}, {0xFF222222, -6.0f, -0.05f,  20.0f}
};

Vertex* customModel = NULL;
int customVertexCount = 0;

// Fungsi pembaca file .obj (wajib export dengan opsi Triangulate Faces)
void loadObjModel(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;

    float* vx = (float*)malloc(sizeof(float) * 10000);
    float* vy = (float*)malloc(sizeof(float) * 10000);
    float* vz = (float*)malloc(sizeof(float) * 10000);
    int vCount = 0;

    customModel = (Vertex*)memalign(16, sizeof(Vertex) * 30000);
    customVertexCount = 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            sscanf(line, "v %f %f %f", &vx[vCount], &vy[vCount], &vz[vCount]);
            vCount++;
        } else if (line[0] == 'f' && line[1] == ' ') {
            int i1, i2, i3;
            char s1[32], s2[32], s3[32];
            if (sscanf(line, "f %31s %31s %31s", s1, s2, s3) == 3) {
                i1 = atoi(s1) - 1;
                i2 = atoi(s2) - 1;
                i3 = atoi(s3) - 1;
                if (i1 >= 0 && i2 >= 0 && i3 >= 0 && i1 < vCount && i2 < vCount && i3 < vCount) {
                    unsigned int shade = 0xFF888888 + ((int)(vy[i1] * 40) & 0x55);
                    customModel[customVertexCount++] = (Vertex){shade, vx[i1], vy[i1], vz[i1]};
                    customModel[customVertexCount++] = (Vertex){0xFFAAAAAA, vx[i2], vy[i2], vz[i2]};
                    customModel[customVertexCount++] = (Vertex){0xFF666666, vx[i3], vy[i3], vz[i3]};
                }
            }
        }
    }
    free(vx); free(vy); free(vz);
    fclose(file);
    sceKernelDcacheWritebackAll();
}

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int main(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);

    // Coba muat file mobil.obj dari folder yang sama dengan EBOOT.PBP
    loadObjModel("mobil.obj");

    // Inisialisasi Hardware 3D PSP GU
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    float carX = 0.0f, carZ = 0.0f, carAngle = 0.0f;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        // Kontrol setir dengan Analog X
        float steer = ((float)pad.Lx - 128.0f) / 128.0f;
        if (steer > 0.15f || steer < -0.15f) {
            carAngle -= steer * 0.04f;
        }

        // Gas (X) dan Mundur/Rem (Kotak)
        if (pad.Buttons & PSP_CTRL_CROSS) {
            carZ -= 0.08f;
        }
        if (pad.Buttons & PSP_CTRL_SQUARE) {
            carZ += 0.08f;
        }

        sceGuStart(GU_DIRECT, list);
        // Warna langit biru cerah (ABGR format)
        sceGuClearColor(0xFFEBCE87);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        // Atur Kamera Perspektif
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(60.0f, 16.0f / 9.0f, 0.5f, 100.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        ScePspFVector3 camPos = {0.0f, 2.2f, carZ + 4.5f};
        ScePspFVector3 camLook = {0.0f, 0.5f, carZ - 2.0f};
        ScePspFVector3 camUp = {0.0f, 1.0f, 0.0f};
        sceGumLookAt(&camPos, &camLook, &camUp);

        // Gambar Lintasan Aspal
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, trackFloor);

        // Gambar Model 3D Mobil
        sceGumLoadIdentity();
        ScePspFVector3 pos = {carX, 0.0f, carZ};
        sceGumTranslate(&pos);
        sceGumRotateY(carAngle);

        if (customModel != NULL && customVertexCount > 0) {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, customVertexCount, 0, customModel);
        } else {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 12, 0, fallbackCar);
        }

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
