#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>

PSP_MODULE_INFO("SimEndurance", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

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

    pspDebugScreenInit();
    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while (1) {
        pspDebugScreenSetXY(0, 2);
        pspDebugScreenPrintf("Sim Racing Endurance PSP - Build Sukses!\n");
        
        sceCtrlReadBufferPositive(&pad, 1);
        pspDebugScreenPrintf("Analog X: %d | Analog Y: %d\n", pad.Lx, pad.Ly);
        
        if (pad.Buttons & PSP_CTRL_CROSS) {
            pspDebugScreenPrintf("Status: Gas Ditekan!   \n");
        } else {
            pspDebugScreenPrintf("Status: Netral         \n");
        }
        sceKernelDelayThread(16666);
    }
    return 0;
}
