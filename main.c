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
    {0xFF0000FF, -1.02f, 1.30f, -2.51f}, {0xFF0000FF,  1.02f, 1.30f, -2.51f}, {0xFF0000CC,  1.02f, 1.30f,  2.51f},
    {0xFF0000FF, -1.02f, 1.30f, -2.51f}, {0xFF0000CC,  1.02f, 1.30f,  2.51f}, {0xFF0000CC, -1.02f, 1.30f,  2.51f},
    {0xFF00FFFF, -1.02f, 0.00f, -2.51f}, {0xFF00FFFF,  1.02f, 0.00f, -2.51f}, {0xFF0000FF,  1.02f, 1.30f, -2.51f},
    {0xFF00FFFF, -1.02f, 0.00f, -2.51f}, {0xFF0000FF,  1.02f, 1.30f, -2.51f}, {0xFF0000FF, -1.02f, 1.30f, -2.51f},
    {0xFF222222, -1.02f, 0.00f,  2.51f}, {0xFF222222,  1.02f, 0.00f,  2.51f}, {0xFF000099,  1.02f, 1.30f,  2.51f},
    {0xFF222222, -1.02f, 0.00f,  2.51f}, {0xFF000099,  1.02f, 1.30f,  2.51f}, {0xFF000099, -1.02f, 1.30f,  2.51f},
    {0xFF0000AA, -1.02f, 0.00f, -2.51f}, {0xFF0000AA, -1.02f, 0.00f,  2.51f}, {0xFF0000DD, -1.02f, 1.30f,  2.51f},
    {0xFF0000AA, -1.02f, 0.00f, -2.51f}, {0xFF0000DD, -1.02f, 1.30f,  2.51f}, {0xFF0000DD, -1.02f, 1.30f, -2.51f},
    {0xFF0000AA,  1.02f, 0.00f, -2.51f}, {0xFF0000AA,  1.02f, 0.00f,  2.51f}, {0xFF0000DD,  1.02f, 1.30f,  2.51f},
    {0xFF0000AA,  1.02f, 0.00f, -2.51f}, {0xFF0000DD,  1.02f, 1.30f,  2.51f}, {0xFF0000DD,  1.02f, 1.30f, -2.51f}
};

Vertex __attribute__((aligned(16))) fallbackTrack15m[6] = {
    {0xFF444444, -7.5f, 0.0f, -200.0f}, {0xFF444444,  7.5f, 0.0f, -200.0f}, {0xFF2A2A2A,  7.5f, 0.0f,  200.0f},
    {0xFF444444, -7.5f, 0.0f, -200.0f}, {0xFF2A2A2A,  7.5f, 0.0f,  200.0f}, {0xFF2A2A2A, -7.5f, 0.0f,  200.0f}
};

// Pembaca .obj yang mendukung Triangle maupun Quad (4 titik) secara otomatis
Model3D loadObjModel(const char* filename, int maxVertices, unsigned int baseColor) {
    Model3D model = {NULL, 0};
    FILE* file = fopen(filename, "r");
    if (!file) return model;

    float* vx = (float*)malloc(sizeof(float) * maxVertices);
    float* vy = (float*)malloc(sizeof(float) * maxVertices);
    float* vz = (float*)malloc(sizeof(float) * maxVertices);
    int vCount = 0;

    model.vertices = (Vertex*)memalign(16, sizeof(Vertex) * maxVertices * 6);

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            if (vCount < maxVertices) {
                sscanf(line, "v %f %f %f", &vx[vCount], &vy[vCount], &vz[vCount]);
                vCount++;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            char s1[32], s2[32], s3[32], s4[32];
            int count = sscanf(line, "f %31s %31s %31s %31s", s1, s2, s3, s4);
            if (count >= 3) {
                int i1 = atoi(s1) - 1;
                int i2 = atoi(s2) - 1;
                int i3 = atoi(s3) - 1;
                if (i1 >= 0 && i2 >= 0 && i3 >= 0 && i1 < vCount && i2 < vCount && i3 < vCount) {
                    model.vertices[model.vertexCount++] = (Vertex){baseColor, vx[i1], vy[i1], vz[i1]};
                    model.vertices[model.vertexCount++] = (Vertex){baseColor - 0x00151515, vx[i2], vy[i2], vz[i2]};
                    model.vertices[model.vertexCount++] = (Vertex){baseColor - 0x002A2A2A, vx[i3], vy[i3], vz[i3]};

                    // Jika wajah berupa kotak (4 titik), buat segitiga kedua otomatis
                    if (count == 4) {
                        int i4 = atoi(s4) - 1;
                        if (i4 >= 0 && i4 < vCount) {
                            model.vertices[model.vertexCount++] = (Vertex){baseColor, vx[i1], vy[i1], vz[i1]};
                            model.vertices[model.vertexCount++] = (Vertex){baseColor - 0x002A2A2A, vx[i3], vy[i3], vz[i3]};
                            model.vertices[model.vertexCount++] = (Vertex){baseColor - 0x00151515, vx[i4], vy[i4], vz[i4]};
                        }
                    }
                }
            }
        }
    }
    free(vx); free(vy); free(vz);
    fclose(file);
    sceKernelDcacheWritebackAll();
    return model;
}

// Fungsi menghitung ketinggian permukaan sirkuit (Y) tepat di bawah posisi mobil (X, Z)
float getTrackHeightAt(Model3D* track, float x, float z, float currentY) {
    if (!track || !track->vertices || track->vertexCount < 3) return 0.0f;

    float bestY = currentY;
    float minHeightDiff = 9999.0f;
    int found = 0;

    for (int i = 0; i < track->vertexCount; i += 3) {
        Vertex v0 = track->vertices[i];
        Vertex v1 = track->vertices[i + 1];
        Vertex v2 = track->vertices[i + 2];

        // Cek cepat bounding box segitiga pada sumbu X dan Z
        float minX = fminf(v0.x, fminf(v1.x, v2.x)) - 0.1f;
        float maxX = fmaxf(v0.x, fmaxf(v1.x, v2.x)) + 0.1f;
        if (x < minX || x > maxX) continue;

        float minZ = fminf(v0.z, fminf(v1.z, v2.z)) - 0.1f;
        float maxZ = fmaxf(v0.z, fmaxf(v1.z, v2.z)) + 0.1f;
        if (z < minZ || z > maxZ) continue;

        // Hitung koordinat Barycentric 2D (X, Z)
        float denom = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
        if (fabsf(denom) < 0.0001f) continue;

        float w0 = ((v1.z - v2.z) * (x - v2.x) + (v2.x - v1.x) * (z - v2.z)) / denom;
        float w1 = ((v2.z - v0.z) * (x - v2.x) + (v0.x - v2.x) * (z - v2.z)) / denom;
        float w2 = 1.0f - w0 - w1;

        if (w0 >= -0.02f && w1 >= -0.02f && w2 >= -0.02f) {
            float hitY = w0 * v0.y + w1 * v1.y + w2 * v2.y;
            float diff = fabsf(hitY - currentY);
            // Pilih permukaan yang paling dekat dengan ketinggian mobil saat ini (mengantisipasi jembatan/terowongan)
            if (diff < 3.0f && diff < minHeightDiff) {
                minHeightDiff = diff;
                bestY = hitY;
                found = 1;
            }
        }
    }
    return found ? bestY : currentY;
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

    float carX = 0.0f, carY = 0.0f, carZ = 0.0f, carAngle = 0.0f;
    float speed = 0.0f;
    float smoothSteer = 0.0f;

    // Setel ketinggian awal mobil agar langsung menapak di trek
    carY = getTrackHeightAt(&trackModel, carX, carZ, 0.0f);

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        // 1. Handling Analog Kiri TANPA Deadzone
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

        // Perbarui ketinggian Y mobil mengikuti elevasi permukaan trek
        float targetY = getTrackHeightAt(&trackModel, carX, carZ, carY);
        carY += (targetY - carY) * 0.5f; // Transisi elevasi mulus (seperti suspensi)

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFFEBCE87);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        // Near clip dinaikkan ke 1.2f & Far clip 450.0f agar Z-Buffer PSP tajam dan tidak bergerigi
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(58.0f, 16.0f / 9.0f, 1.2f, 450.0f);

        // 3. Kamera Luar Mobil Lebih Dekat & Rendah (Jarak 4.8m, Tinggi 1.55m)
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        ScePspFVector3 camPos, camLook;
        ScePspFVector3 camUp = {0.0f, 1.0f, 0.0f};

        if (pad.Buttons & PSP_CTRL_CIRCLE) {
            // Look Behind
            camPos.x = carX - sinf(carAngle) * 4.8f;
            camPos.y = carY + 1.55f;
            camPos.z = carZ - cosf(carAngle) * 4.8f;

            camLook.x = carX + sinf(carAngle) * 5.0f;
            camLook.y = carY + 0.85f;
            camLook.z = carZ + cosf(carAngle) * 5.0f;
        } else {
            // Chase Cam Normal
            camPos.x = carX + sinf(carAngle) * 4.8f;
            camPos.y = carY + 1.55f;
            camPos.z = carZ + cosf(carAngle) * 4.8f;

            camLook.x = carX - sinf(carAngle) * 6.0f;
            camLook.y = carY + 0.85f;
            camLook.z = carZ - cosf(carAngle) * 6.0f;
        }
        sceGumLookAt(&camPos, &camLook, &camUp);

        // Render Sirkuit
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        if (trackModel.vertices != NULL && trackModel.vertexCount > 0) {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, trackModel.vertexCount, 0, trackModel.vertices);
        } else {
            sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, fallbackTrack15m);
        }

        // Render Mobil Prototipe
        sceGumLoadIdentity();
        ScePspFVector3 pos = {carX, carY, carZ};
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
