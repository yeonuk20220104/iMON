#include "gmon_uboot.h"
#include <common.h>
#include <asm/arch/gpio.h>
#include "spi.h"
#include <part.h>
#include <fat.h>
#include <usb.h>
#include <lcd.h>
#include <spi_flash.h>
#include <linux/mtd/mtd.h>
#include <mmc.h>
#include <part.h>
#include <memalign.h>
#include <fs.h>
#include <jffs2/jffs2.h>
#include <command.h>
#include <spl.h>
#include <android_cmds.h>
#include <linux/mtd/mtd.h>
#include "gmon_uboot.h"

#include <config.h>
#include <fdtdec.h>
#include <asm/processor.h>
#include <asm/gpio.h>
#include <fs.h>


#define BUFF_ADDFESS "0x22000000"
static int curr_device = -1;
struct mmc *xmmc;
static 	ulong fsize = 0;
static ulong ksize =0;
int is_usb_updated = 0;
gmon_img_t ximages[] = {
//        {"MiniLoaderAll.bin"    ,"miniloader"   ,IMG_MINI       ,"0x0" ,"0x0"},
//        {"parameter.txt"        ,"parameter"    ,IMG_PARA       ,"0x0" ,"0x0"},      
       	{"uboot.img"            ,"uboot"        ,IMG_UBOOT      ,"0x4000" ,"0x2000"},
		{"trust.img"            ,"trust"        ,IMG_TRUST      ,"0x6000" ,"0x2000"},
		{"misc.img"             ,"misc"         ,IMG_MISC       ,"0x8000" ,"0x2000"},
		{"boot.img"             ,"boot"         ,IMG_KERNEL     ,"0xa000" ,"0x10000"},
		{"recovery.img"         ,"recovery"     ,IMG_RECOVERY   ,"0x1a000" ,"0x10000"},
		{"backup.img"           ,"backup"       ,IMG_BACKUP     ,"0x2a000" ,"0x10000"},
		{"oem.img"              ,"oem"          ,IMG_OEM        ,"0x3a000" ,"0x20000"},
		{"rootfs.img"           ,"rootrs"       ,IMG_ROOTFS     ,"0x5a000" ,"0xc00000"},
		{"userdata.img"         ,"userdata"     ,IMG_USERDATA   ,"0xc5a000" ,"0x0"},
//		{"update.img"           ,"update"       ,IMG_UPDATE     ,"0x0" ,"0x0"},
};

extern int part_get_info_efi(struct blk_desc *dev_desc, int part,
		      disk_partition_t *info);
		      
					 
extern void do_usb_start(void);	     		
	
extern int do_load(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[],
		int fstype); 
extern int do_mmc_write(cmd_tbl_t *cmdtp, int flag,
			int argc, char * const argv[]);		
extern struct mmc *init_mmc_device(int dev, bool force_init);
static ulong getenv_hex(const char *varname, ulong default_val)
{
    const char *s;
    ulong value;
    char *endp;

    s = env_get(varname);

    if(s){
    	value = simple_strtoul(s, &endp, 16);
	}

	if(!s || endp == s)
    	return default_val;

    return value;
}




#if 1
static int xmmc_init(void)
{
	xmmc = find_mmc_device(0);
	
	if(!xmmc){
		printf("find_mmc_device fail\n");
		return -1;
	}

	if(mmc_init(xmmc) < 0){
		printf("mmc_init fail\n");
		return -1;
	}

	return 0;
}
#endif 
int mmc_write(cmd_tbl_t *cmdtp, int flag,
			int argc, char * const argv[])
{
	u32 blk, cnt, n;
	void *addr;


#if 0
    printf("flag[%d]\n",flag);
    printf("argc[%d]\n",argc);
    printf("argv[%s]\n",*(argv+0));
    printf("argv[%s]\n",argv[1]);
    printf("argv[%s]\n",argv[2]);
    printf("argv[%s]\n",argv[3]);
    printf("argv[%s]\n",argv[4]);
#endif

	addr = (void *)simple_strtoul(argv[1], NULL, 16);
	blk = simple_strtoul(argv[2], NULL, 16);
	cnt = simple_strtoul(argv[3], NULL, 16);

     if(xmmc_init() < 0)	
    {
        printf("xmmc_init Fail\n");
        return -1;
       
    }
    else
    {
           printf("xmmc_init Pass\n");
    }
    
	printf("\nMMC write: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);
    printf("DEBUG POINT [06] \n");
	if (mmc_getwp(xmmc) == 1) {
		printf("Error: card is write protected!\n");
		return CMD_RET_FAILURE;
	}
	n = blk_dwrite(mmc_get_blk_desc(xmmc), blk, cnt, addr);
	printf("%d blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");

	return (n == cnt) ? CMD_RET_SUCCESS : CMD_RET_FAILURE;
}
int mmc_write_part(const char *pname, long p_fsize, char * p_offset, char * p_count)
{

	ulong fsize, laddr;

    char str[17]={0,};
	

    printf("Standard SIZE[%lu]", ksize);
	fsize = getenv_hex("filesize", -1);
	laddr = getenv_hex("fileaddr", -1);

	if(fsize < 0 || laddr < 0)
	{
    	printf("File Address or Size Error\n");
	   	return -1;
	}


    int load_height=4, load_width=32;
    char **load_param;
    int i;
    load_param=(char**)malloc(sizeof(char*)*load_height);
    for(i=0;i<load_height; i++)
    {
        load_param[i] = (char*)malloc(sizeof(char)*load_width);
    }

  
    sprintf(str, "%lu", ksize);
    strcpy(load_param[0], "write");
    strcpy(load_param[1], BUFF_ADDFESS);
    strcpy(load_param[2], p_offset);
    strcpy(load_param[3], str);



#if 1
    printf("load_param 0 [%s]\n",*(load_param+0));
    printf("load_param 1 [%s]\n",load_param[1]);
    printf("load_param 2 [%s]\n",load_param[2]);
    printf("load_param 3 [%s]\n",load_param[3]);
#endif 


    //udelay(2000000);

    mmc_write(NULL, 0, 4, load_param);
  //  udelay(2000000);
    for(i=0; i < load_height ; i++)
    {
        free(load_param[i]);
    }
    free(load_param);
	return 0;
}

static void check_init(void)
{

	usb_stop();
	do_usb_start();		
	
}


static int usb_update_part(const char *fname, const char *part, char * s_offset, char * s_count)
{

	long chunkSize = 0x3200000;	// 50MB
	unsigned long laddr = 0x22000000;
	static loff_t pos = 0;;
	int ret;

	if(!fat_exists(fname))
	{
	   // printf("fat_exists Fail");
		return 0;
    }

	printf("Loading %s... ", part);
	ksize = env_get_hex("filesize", 0);
    printf("Standard SIZE[%lu]", ksize);

	while(1)
	{
		fsize = file_fat_read(fname,  (void *)laddr, chunkSize);
		
		if(fsize < 0){
			printf("file_fat_readFail\n");
			return 0;
		}
	    printf("File[%s]SIZE[%lu]chunkSize[%ld]",fname,fsize,chunkSize);
       
        if(fsize != ksize )
        {
            printf("Buffer Size Wrong\n");
            return -1;
        }
        ksize = 1 + (ksize/16384);
		if(pos == 0)
		{
			printf("Done\n");
        }
		//setenv_hex("filesize", fsize);
		//env_set("fileaddr", "0x22000000");

		if(pos == 0)
		{
			printf("Writing...");
        }

		char pname[128];
		int pnum = 1;

		for(int i = 1; i < pnum + 1; i++)
		{
			sprintf(pname, "%s%d", part, i);


			ret = mmc_write_part(pname, fsize,s_offset, s_count);
			is_usb_updated = 1;
		}

		if(fsize < chunkSize){
			printf("\nMMC Flash Update (%s) %s\n\n", part, ret > 0 ? "OK" : "FAIL");
			break;
		}
		else
		{
			pos += fsize;
			printf(".");

		}
		
	}

	return 1;
}


static void gmon_fw_update_usb(void)
{




	printf("Board USB UPGRADE ============================================================\n");


    int load_height=5, load_width=128;
    char **load_param;
    int i;
    load_param=(char**)malloc(sizeof(char*)*load_height);
    for(i=0;i<load_height; i++)
    {
        load_param[i] = (char*)malloc(sizeof(char)*load_width);
    }
    

    char fname[1024]={0,};


    strcpy(load_param[0], "fatload");
    strcpy(load_param[1], "usb");
    strcpy(load_param[2], "0:1");
    strcpy(load_param[3], BUFF_ADDFESS);
  
	

	for(int i = 0; i < ARRAY_SIZE(ximages); i++)
	{
		gmon_img_t *img = &ximages[i];


        memset(load_param[4], 0, sizeof(load_param[4]));
		snprintf(fname, sizeof(fname), "firmware_update/%s", img->fname);
        strcpy(load_param[4], fname);
    	do_load(NULL, 0, 5, load_param , FS_TYPE_FAT);
 
		usb_update_part(fname, img->pname,img->start_offset ,img->size_count );


	}

    for(i=0; i<load_height; i++)
    {
        free(load_param[i]);
        
    }
    free(load_param);
	printf("Board USB UPGRADE COMPLETE ====================================================\n");
}



void gmon_init(void)
{
	printf("Board ready (u-boot)===========================================================\n");



	check_init();

    gmon_fw_update_usb();




}


