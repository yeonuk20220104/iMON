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
#include "gl_code_yuvinput.h"

int main()
{

    //for (int i = 0; i<10; i++) {
        slt_gpu_light_init();
        slt_gpu_light_run();
        slt_gpu_light_deinit();
        //sleep(5);
    //}

    return 0;
}
