#ifndef __GMON_UBOOT_H__
#define __GMON_UBOOT_H__

#define GMON_CONF_PART "gmon_conf"

#define GMON_MMC_PARTS "name=uboot,start=16M,size=8192;"\
                    "name=trust,start=24M,size=8192;"\
                    "name=misc,start=32M,size=8192;"\
                    "name=boot,start=40M,size=64M;"\
                    "name=recovery,start=104M,size=64M;"\
                    "name=backup,start=168M,size=64M;"\
                    "name=oem,start=232M,size=128M;"\
                    "name=rootfs,start=360M,size=12288M;"\
                    "name=userdata,start=12648M,size=-"



enum IMG{
	IMG_UBOOT,    
	IMG_DTB,
    IMG_KERNEL,
    IMG_ROOT,
    IMG_APP,
    IMG_DATA,
    IMG_MINI,
    IMG_PARA,      
    IMG_TRUST,
    IMG_MISC,
    IMG_RECOVERY,
    IMG_BACKUP,
    IMG_OEM,
    IMG_ROOTFS,
    IMG_USERDATA,
    IMG_UPDATE,
};

typedef struct gmon_img{
    char fname[128];
    char pname[64];
    int index;
    char start_offset[32];
    char size_count[32];
}gmon_img_t;

void gmon_init(void);

//int get_off_by_name(char *part, unsigned long long *off, unsigned long long *len, unsigned long long chipsize);
//int get_off_by_name(char *part, loff_t *off, loff_t *len, uint64_t chipsize);
//static int mmc_write_part(const char *pname);
int mmc_write_part(const char *pname, long p_fsize, char * p_offset, char * p_count);
//static int mmc_write_part(int num);

#endif
