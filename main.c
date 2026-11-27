#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

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

typedef struct {
    Vertex* vertices;
    int vertexCount;
} Model3D;

// Cadangan Balok Skala 1:1 BMW M4 GT3 Evo (Lebar 2.04m, Tinggi 1.30m, Panjang 5.02m)
Vertex __attribute__((aligned(16))) fallbackM4GT3[30] = {
    // Atap (Merah terang)
    {0xFF0000FF, -1.02f, 1.30f, -2.51f}, {0xFF0000FF,  1.02f, 1.30f, -2.51f}, {0xFF0000CC,  1.02f, 1.30f,  2.51f},
    {0xFF0000FF, -1.02f, 1.30f, -2.51f}, {0xFF0000CC,  1.02f, 1.30f,  2.51f}, {0xFF0000CC, -1.02f, 1.30f,  2.51f},
    // Depan (Putih/Kuning lampu)
    {0xFF00FFFF, -1.02f, 0.00f, -2.51f}, {0xFF00FFFF,  1.02f, 0.00f, -2.51f}, {0xFF0000FF,  1.02f, 1.30f, -2.51f},
    {0xFF00FFFF, -1.02f, 0.00f, -2.51f}, {0xFF0000FF,  1.02f, 1.30f, -2.51f}, {0xFF0000FF, -1.02f, 1.30f, -2.51f},
    // Belakang (Abu-abu gelap diffuser)
    {0xFF222222, -1.02f, 0.00f,  2.51f}, {0xFF222222,  1.02f, 0.00f,  2.51f}, {0xFF000099,  1.02f, 1.30f,  2.51f},
    {0xFF222222, -1.02f, 0.00f,  2.51f}, {0xFF000099,  1.02f, 1.30f,  2.51f}, {0xFF000099, -1.02f, 1.30f,  2.51f},
    // Sisi Kiri (Merah sedang)
    {0xFF0000AA, -1.02f, 0.00f, -2.51f}, {0xFF0000AA, -1.02f, 0.00f,  2.51f}, {0xFF0000DD, -1.02f, 1.30f,  2.51f},
    {0xFF0000AA, -1.02f, 0.00f, -2.51f}, {0xFF0000DD, -1.02f, 1.30f,  2.51f}, {0xFF0000DD, -1.02f, 1.30f, -2.51f},
    // Sisi Kanan (Merah sedang)
    {0xFF0000AA,  1.02f, 0.00f, -2.51f}, {0xFF0000AA,  1.02f, 0.00f,  2.51f}, {0xFF0000DD,  1.02f, 1.30f,  2.51f},
    {0xFF0000AA,  1.02f, 0.00f, -2.51f}, {0xFF0000DD,  1.02f, 1.30f,  2.51f}, {0xFF0000DD,  1.02f, 1.30f, -2.51f}
};

// Cadangan Trek Uji Coba Lebar 15 Meter (-7.5m sampai +7.5m) Skala 1:1
Vertex __attribute__((aligned(16))) fallbackTrack15m[6] = {
    {0xFF444444, -7.5f, 0.0f, -200.0f}, {0xFF444444,  7.5f, 0.0f, -200.0f}, {0xFF2A2A2A,  7.5f, 0.0f,  200.0f},
    {0xFF444444, -7.5f, 0.0f, -200.0f}, {0xFF2A2A2A,  7.5f, 0.0f,  200.0f}, {0xFF2A2A2A, -7.5f, 0.0f,  200.0f}
};

Model3D loadObjModel(const char* filename, int maxVertices, unsigned int baseColor) {
    Model3D model = {NULL, 0};
    FILE* file = fopen(filename, "r");
    if (!file) return model;

    float* vx = (float*)malloc(sizeof(float) * maxVertices);
    float* vy = (float*)malloc(sizeof(float) * maxVertices);
    float* vz = (float*)malloc(sizeof(float) * maxVertices);
    int vCount = 0;

    model.vertices = (Vertex*)memalign(16, sizeof(Vertex) * maxVertices * 3);

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            if (vCount < maxVertices) {
                sscanf(line, "v %f %f %f", &vx[vCount], &vy[vCount], &vz[vCount]);
                vCount++;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            char s1[32], s2[32], s3[32];
            if (sscanf(line, "f %31s %31s %31s", s1, s2, s3) == 3) {
                int i1 = atoi(s1) - 1;
                int i2 = atoi(s2) - 1;
                int i3 = atoi(s3) - 1;
                if (i1 >= 0 && i2 >= 0 && i3 >= 0 && i1 < vCount && i2 < vCount && i3 < vCount) {
                    model.vertices[model.vertexCount++] = (Vertex){baseColor, vx[i1], vy[i1], vz[i1]};
                    model.vertices[model.vertexCount++] = (Vertex){baseColor - 0x00181818, vx[i2], vy[i2], vz[i2]};
                    model.vertices[model.vertexCount++] = (Vertex){baseColor - 0x00303030, vx[i3], vy[i3], vz[i3]};
                }
            }
        }
    }
    free(vx); free(vy); free(vz);
    fclose(file);
    sceKernelDcacheWritebackAll();
    return model;
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

    // Muat model 3D skala 1:1 dari folder game
    Model3D carModel = loadObjModel("mobil.obj", 10000, 0xFF0000EE);
    Model3D trackModel = loadObjModel("circuit.obj", 30000, 0xFF555555);

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
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Variabel fisika skala 1:1 (satuan meter per frame @ 60fps)
    float carX = 0.0f, carZ = 0.0f, carAngle = 0.0f;
    float speed = 0.0f; // 1.0f = 60 m/s (~216 km/jam)
    float smoothSteer = 0.0f;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        // 1. Handling Analog Kiri TANPA Deadzone + Smoothing Halus
        float rawSteer = ((float)pad.Lx - 128.0f) / 128.0f;
        float curvedSteer = rawSteer * (0.65f + 0.35f * fabsf(rawSteer));
        smoothSteer += (curvedSteer - smoothSteer) * 0.25f;

        if (fabsf(speed) > 0.005f) {
            float dir = (speed >= 0.0f) ? 1.0f : -1.0f;
            float speedFactor = 1.0f / (1.0f + fabsf(speed) * 0.8f);
            carAngle -= smoothSteer * 0.032f * speedFactor * dir;
        }

        // 2. Kontrol Tombol: X = Gas, Kotak = Rem, Segitiga = Mundur
        if (pad.Buttons & PSP_CTRL_CROSS) {
            speed += 0.006f;
        } else if (pad.Buttons & PSP_CTRL_SQUARE) {
            if (speed > 0.0f) {
                speed -= 0.015f;
                if (speed < 0.0f) speed = 0.0f;
            } else if (speed < 0.0f) {
                speed += 0.015f;
                if (speed > 0.0f) speed = 0.0f;
            }
        } else if (pad.Buttons & PSP_CTRL_TRIANGLE) {
            if (speed <= 0.05f) {
                speed -= 0.005f;
            }
        } else {
            speed *= 0.992f;
            if (fabsf(speed) < 0.0005f) speed = 0.0f;
        }

        if (speed > 1.25f) speed = 1.25f;
        if (speed < -0.35f) speed = -0.35f;

        carX -= sinf(carAngle) * speed;
        carZ -= cosf(carAngle) * speed;

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFFEBCE87);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(60.0f, 16.0f / 9.0f, 0.5f, 1000.0f);

        // 3. Kamera Luar Mobil (Chase Cam) + Tombol Bulat untuk Look Behind
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        ScePspFVector3 camPos, camLook;
        ScePspFVector3 camUp = {0.0f, 1.0f, 0.0f};

        if (pad.Buttons & PSP_CTRL_CIRCLE) {
            camPos.x = carX - sinf(carAngle) * 6.5f;
            camPos.y = 2.2f;
            camPos.z = carZ - cosf(carAngle) * 6.5f;

            camLook.x = carX + sinf(carAngle) * 4.0f;
            camLook.y = 0.9f;
            camLook.z = carZ + cosf(carAngle) * 4.0f;
        } else {
            camPos.x = carX + sinf(carAngle) * 6.5f;
            camPos.y = 2.2f;
            camPos.z = carZ + cosf(carAngle) * 6.5f;

            camLook.x = carX - sinf(carAngle) * 5.0f;
            camLook.y = 0.9f;
            camLook.z = carZ - cosf(carAngle) * 5.0f;
        }
        sceGumLookAt(&camPos, &camLook, &camUp);

        // Render Trek Uji Coba 15 Meter
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        if (trackModel.vertices != NULL && trackModel.vertexCount > 0) {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, trackModel.vertexCount, 0, trackModel.vertices);
        } else {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, fallbackTrack15m);
        }

        // Render Mobil Prototipe Skala 1:1
        sceGumLoadIdentity();
        ScePspFVector3 pos = {carX, 0.0f, carZ};
        sceGumTranslate(&pos);
        sceGumRotateY(carAngle);

        if (carModel.vertices != NULL && carModel.vertexCount > 0) {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, carModel.vertexCount, 0, carModel.vertices);
        } else {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 30, 0, fallbackM4GT3);
        }

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
