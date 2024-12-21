//触摸屏设备坐标获取接口 

#ifndef  TOUCH
#define  TOUCH 

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
     
static int fd=0;

//初始化摄像头
void init_touch()
{
     //打开触摸屏设备
	    fd=open("/dev/input/event2",O_RDWR);
		if(fd < 0)
		{
			perror("open touch fail\n");
		}
}

//获取坐标点 
void get_touch(int *x,int *y)
{
    struct input_event  ts;	
	while(1)
	{
	
		read(fd,&ts,sizeof(ts));
		
		if(ts.type == EV_ABS )
		{
				//printf("touch EV\n");
			if(ts.code == ABS_X )
			{
				 printf("x=%d\n",ts.value);
                 *x = ts.value;
			}

			if(ts.code == ABS_Y)
			{
				// printf("y=%d\n",ts.value);
                  *y = ts.value;
			}
		}
		
      //#define EV_KEY                  0x01  
     //#define BTN_TOUCH               0x14a
       if(ts.type == EV_KEY && ts.code == BTN_TOUCH)
	   {
		  // printf("touch =%d\n",ts.value);
          if(ts.value == 0)
          {
              break;
          }
	   }
	}

    printf("x=%d,y=%d\n",*x,*y);

    return ;
}

void free_touch()
{
    close(fd);
}
#endif
