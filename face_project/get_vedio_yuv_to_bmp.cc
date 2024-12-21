#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>




//用户自定义的图像缓存BUF
struct buffer{  
	void *start;  
	unsigned int length;  
}*buffers; 




#define VIDEO_PATH "/dev/video6"

int creat_bmp(unsigned char *rgb,const char *bmp_file); 
int yuyv2rgb0(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height);



//初始化摄像头
int  init_video()
{
	//1.open device.打开摄像头设备 
	int fd = open(VIDEO_PATH,O_RDWR,0);//弄了好久 以阻塞模式打开摄像头  | O_NONBLOCK 非堵塞
	if(fd<0){
		printf("open device failed.\n");
	}
	printf("open device success.->fd=%d\n",fd);  
 
   

	
	//2.show all supported format.显示所有支持的格式
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index = 0; //form number
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//frame type  
	while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc) != -1){  
            printf("VIDIOC_ENUM_FMT success.->fmt.fmt.pix.pixelformat:%s\n",fmtdesc.description);
        fmtdesc.index ++;
    }

    //3.set or gain current frame.设置或查看当前格式
    struct v4l2_format fmt;
	memset ( &fmt, 0, sizeof(fmt) );
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //V4L2_PIX_FMT_RGB32   V4L2_PIX_FMT_YUYV   V4L2_STD_CAMERA_VGA  V4L2_PIX_FMT_JPEG
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;	
	fmt.fmt.pix.width   = 640; 
	fmt.fmt.pix.height  = 480;
	if (ioctl(fd,VIDIOC_S_FMT,&fmt) == -1) {
	   printf("VIDIOC_S_FMT failed.\n");
	   return -1;
    }
	printf("VIDIOC_S_FMT sucess.\n");
	
	//查看格式
	if (ioctl(fd,VIDIOC_G_FMT,&fmt) == -1) {
	   printf("VIDIOC_G_FMT failed.\n");
	   return -1;
    }
  	printf("VIDIOC_G_FMT sucess.->fmt.fmt.width is %ld\nfmt.fmt.pix.height is %ld\n\
fmt.fmt.pix.colorspace is %ld\n",fmt.fmt.pix.width,fmt.fmt.pix.height,fmt.fmt.pix.colorspace);


	
	//6.1 request buffers.申请缓冲区
	struct v4l2_requestbuffers req;  
	req.count = 1;//frame count.帧的个数 
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;//automation or user define．自动分配还是自定义
	if ( ioctl(fd,VIDIOC_REQBUFS,&req)==-1){  
		printf("VIDIOC_REQBUFS map failed.\n");  
		close(fd);  
		exit(-1);  
	} 
	printf("VIDIOC_REQBUFS map success.\n");
 
	 
	//为用户自定义的映射地址分配空间
	buffers = (struct buffer*)calloc (req.count, sizeof(*buffers));  //分配2块缓存

	  unsigned int n_buffers = 0; 
	  struct v4l2_buffer buf;   
	for(n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{  
	  
	    //查询序号为n_buffers 的缓冲区，得到其起始物理地址和大小
		memset(&buf,0,sizeof(buf)); 
		buf.index = n_buffers; 
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		buf.memory = V4L2_MEMORY_MMAP;  
		if(ioctl(fd,VIDIOC_QUERYBUF,&buf) == -1)
		{  
			printf("VIDIOC_QUERYBUF failed.\n");
			close(fd);  
			exit(-1);  
		} 
        printf("VIDIOC_QUERYBUF success.\n");

		
  		//memory map   把2块缓存地址映射到用户空间
		buffers[n_buffers].length = buf.length;	
		buffers[n_buffers].start = mmap(NULL,buf.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,buf.m.offset);  
		if(MAP_FAILED == buffers[n_buffers].start){  
			printf("memory map failed.\n");
			close(fd);  
			exit(-1);  
		} 
		printf("memory map success.\n"); 
		
		//Queen buffer.将缓冲帧放入队列 
		if (ioctl(fd , VIDIOC_QBUF, &buf) ==-1) {
		    printf("VIDIOC_QBUF failed.->n_buffers=%d\n", n_buffers);
		    return -1;
		}
		printf("VIDIOC_QBUF.->Frame buffer %d: address=0x%p, length=%ld\n",\
n_buffers,buffers[n_buffers].start, buffers[n_buffers].length);
	} 
	
	
	//7.使能视频设备输出视频流
	enum v4l2_buf_type type; 
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
	if (ioctl(fd,VIDIOC_STREAMON,&type) == -1) {
		printf("VIDIOC_STREAMON failed.\n");
		return -1;
	}
	printf("VIDIOC_STREAMON success.\n"); 
	
	
	return fd;
}


int  get_bmp(int fd)
{

    struct v4l2_buffer buf;
	//8.DQBUF.取出一帧
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        printf("VIDIOC_DQBUF failed.->fd=%d\n",fd);
        return -1;
    }  
	//printf("VIDIOC_DQBUF success.\n");
	
	//RGB  BUF 
	unsigned char rgb_buf[640*480*3]={0};
	yuyv2rgb0((unsigned char *)buffers[buf.index].start,rgb_buf,640,480);  //把YUV 转成 RGB  
	
	//RGB to  bmp  
	 creat_bmp(rgb_buf,"0.bmp");
	
	//10.QBUF.把一帧放回去
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        printf("VIDIOC_QBUF failed.->fd=%d\n",fd);
        return -1;
    } 
	//printf("VIDIOC_QBUF success.\n");
	
}

//释放摄像头
void free_video(int fd)
{
	//11.Release the resource．释放资源
    int i;
	for (i=0; i< 2; i++) {
        munmap(buffers[i].start, buffers[i].length);
		printf("munmap success.\n");
    }
   
	close(fd); 
	printf("Camera test Done.\n"); 
	
}
