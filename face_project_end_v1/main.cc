extern "C"
{
#include <stdio.h>
#include <errno.h>
#include <linux/fb.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "DRMwrap.h"
#include <pthread.h>
#include "touch.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <arpa/inet.h>
}

#include "rockx.h"

rockx_handle_t face_det_handle;
rockx_handle_t face_5landmarks_handle;
rockx_handle_t face_recognize_handle;

// DRM设备操作结构体
struct drmHandle drm;

//LCD 显示器
int lcd;

int show_bmp(const char *pathname, struct drmHandle *drm, int lcd, int px, int py)
{

    //指向缓存
    unsigned int *lcd_p = (unsigned int *)drm->vaddr;

    //打开bmp图片
    int bmp_fd = open(pathname, O_RDWR);
    if (bmp_fd < 0)
    {
        printf("open fail %s\n", pathname);
        return 0;
    }

    //读取 54个自己头数据
    char head[54];
    read(bmp_fd, head, 54);

    //获取图片的宽度与高度
    int k = *((int *)&head[18]);
    int h = *((int *)&head[22]);

    unsigned int color[h][k];
    unsigned char buf[k * h * 3];
    read(bmp_fd, buf, sizeof(buf));

    unsigned char *p = buf;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < k; x++)
        {
            unsigned char b = *p++;
            unsigned char g = *p++;
            unsigned char r = *p++;
            unsigned char a = 0;

            color[y][x] = a << 24 | r << 16 | g << 8 | b;
        }
    }

    //显示图像
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < k; x++)
        {
            if ((y + py < 600) && (x + px < 1020)) //限制显示的范围
            {
                *(lcd_p + (y + py) * drm->width + (x + px)) = color[y][x];
            }
        }
    }

    // 更新 DRM 设备画面
    DRMshowUp(lcd, drm);
    close(bmp_fd);
}

int  init_video(); 
int  get_bmp(int fd);


//初始化RK 人面识别模块  
int  init_face()
{
	rockx_ret_t ret;
    struct timeval tv;

    /*************** Creat Handle ***************/
    // create a face detection handle
    ret = rockx_create(&face_det_handle, ROCKX_MODULE_FACE_DETECTION, nullptr, 0);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("init rockx module ROCKX_MODULE_FACE_DETECTION error %d\n", ret);
        return -1;
    }
    
    // create a face landmark handle
    ret = rockx_create(&face_5landmarks_handle, ROCKX_MODULE_FACE_LANDMARK_5, nullptr, 0);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("init rockx module ROCKX_MODULE_FACE_LANDMARK_68 error %d\n", ret);
        return -1;
    }

    // create a face recognize handle
    ret = rockx_create(&face_recognize_handle, ROCKX_MODULE_FACE_RECOGNIZE, nullptr, 0);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("init rockx module ROCKX_MODULE_FACE_LANDMARK_68 error %d\n", ret);
        return -1;
    }

}

//获取最大的人面图片
rockx_object_t *get_max_face(rockx_object_array_t *face_array) {
    if (face_array->count == 0) {
        return NULL;
    }
    rockx_object_t *max_face = NULL;
    int i;
    for (i = 0; i < face_array->count; i++) {
        rockx_object_t *cur_face = &(face_array->object[i]);
        if (max_face == NULL) {
            max_face = cur_face;
            continue;
        }
        int cur_face_box_area = (cur_face->box.right - cur_face->box.left) * (cur_face->box.bottom - cur_face->box.top);
        int max_face_box_area = (max_face->box.right - max_face->box.left) * (max_face->box.bottom - max_face->box.top);
        if (cur_face_box_area > max_face_box_area) {
            max_face = cur_face;
        }
    }
   // printf("get_max_face %d\n", i-1);
    return max_face;
}

//提取人面特征
int run_face_recognize(rockx_image_t *in_image,rockx_image_t *small_image,rockx_face_feature_t *out_feature) {
    rockx_ret_t ret;

    /*************** FACE Detect ***************/
    // create rockx_face_array_t for store result
    rockx_object_array_t face_array; //创建一个人面检查数组
    memset(&face_array, 0, sizeof(rockx_object_array_t));

    // detect face  把检测到的人面存放在数组中
    ret = rockx_face_detect(face_det_handle, in_image, &face_array, nullptr);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("rockx_face_detect error %d\n", ret);
        return -1;
    }


    rockx_image_t *tmp;
    
     //拷贝图片
         tmp = rockx_image_clone(in_image);

    // process result  输出每个人面的位置信息
    for (int i = 0; i < face_array.count; i++) {
        int left = face_array.object[i].box.left; //0 box=(80 63 168 195) 
        int top = face_array.object[i].box.top;
        int right = face_array.object[i].box.right;
        int bottom = face_array.object[i].box.bottom;
        float score = face_array.object[i].score;
        //printf("%d box=(%d %d %d %d) score=%f\n", i, left, top, right, bottom, score);
       //绘制人面图片  
       rockx_image_draw_rect(tmp, {left, top}, {right, bottom}, {255, 0, 0}, 3); 
    }

    //保存到本地  
    rockx_image_write("draw_face.bmp",tmp);
    
	//显示人面框图 
	 show_bmp("draw_face.bmp", &drm, lcd, 380, 100);

     //tmp 暂未释放
    rockx_image_release(tmp);

    // Get max face  获取最大的人面图片
    rockx_object_t* max_face = get_max_face(&face_array);
    if (max_face == NULL) {
        //printf("error no face detected\n");
        return -1;
    }

    // Face Align  提取人面图片
   // rockx_image_t out_img;
    memset(small_image, 0, sizeof(rockx_image_t));
    ret = rockx_face_align(face_5landmarks_handle, in_image, &(max_face->box), nullptr,small_image);
    if (ret != ROCKX_RET_SUCCESS) {
        return -1;
    }

    // Face Recognition 进行人面识别提取人面特征 
    rockx_face_recognize(face_recognize_handle,small_image, out_feature);

    //保存取出的人面图片  **
    //rockx_image_write("1.bmp",&out_img);

    // Release Aligned Image
    //rockx_image_release(&out_img);

   
    return 0;
}

//人面识别标志位 
int take_face=0;

//人面照片保存标志位
int get_face=0;


//保存所有的人面图片
char   face_path[100][100]={0};
int    people=0;


//触摸屏线程
void *touch_task(void *arg)
{

    //初始化触摸屏
    init_touch();
    int x, y;

    printf("touch_task star !!\n");

    while (1)
    {
        get_touch(&x, &y);

        if (x > 0 && x < 100 && y > 100 && y < 200)
        {
            printf("下班打卡\n");
            take_face = 1;
        }

        if (x > 0 && x < 100 && y > 200 && y < 300)
        {
            printf("上班打卡\n");
             take_face = 2;
        }

        if (x > 0 && x < 100 && y > 300 && y < 400)
        {
            printf("录入人面\n");

            get_face = 1;
        }
    }
}

//显示界面
void show_win()
{
    show_bmp("/sucai/welcome.bmp", &drm, lcd, 0, 0);
    show_bmp("/sucai/xbdk.bmp", &drm, lcd, 0, 100);
    show_bmp("/sucai/sbdk.bmp", &drm, lcd, 0, 200);
    show_bmp("/sucai/lrrm.bmp", &drm, lcd, 0, 300);
}


//进行人面识别  返回 人物照片名 识别成功  返回 NULL 识别失败
char *run_Face_Recognition(rockx_face_feature_t *in_feature)
{

    printf("run_Face_Recognition\n");
    //人面特征
    rockx_face_feature_t out_feature;

    rockx_image_t small_img;
    rockx_image_t out_img;

    float similarity = 0;

    for (int i = 0; i < people; i++)
    {
        printf("%s\n",face_path[i]);
        rockx_image_read(face_path[i], &out_img, 0);
        printf("x=%d y=%d\n",out_img.width,out_img.height);
        memset(&out_feature, 0, sizeof(rockx_face_feature_t));

        run_face_recognize(&out_img, &small_img, &out_feature);

        //特征比对
        rockx_face_feature_similarity(in_feature, &out_feature, &similarity);


        printf("face:%s,similarity=%f\n", face_path[i], similarity);
        if (similarity < 1)
        {
            //释放资源
            rockx_image_release(&out_img);
            rockx_image_release(&small_img);
        
            return face_path[i];
        }

        //释放资源
        rockx_image_release(&out_img);
        rockx_image_release(&small_img);
    }

    return NULL;
}



//发送考勤信息到服务器
int send_msg(char *ip,short port,char *msg)
{

  int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
     struct sockaddr_in addr;
     addr.sin_family  = AF_INET; 
     addr.sin_port  = htons(port); 
     addr.sin_addr.s_addr = inet_addr(ip);
    int ret=connect(tcp_socket,(struct sockaddr *)&addr,sizeof(addr));
        if(ret < 0)
        {
            perror("connet fail\n");
            return -1;
        }

    //发送数据给服务器 
  int size =  write(tcp_socket,msg,strlen(msg));

    close(tcp_socket);

    return  size;
}




//可执行文件./main   服务器ip 
int main(int argc,char *argv[])
{

    if(argc != 2)
    {
        printf("input server ip  demo: ./main  ip\n");
        return -1;
    }


    system("mkdir face");

    // 打开 DRM 设备
    lcd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

       //开启触摸屏线程
    pthread_t pid;
    pthread_create(&pid, NULL, touch_task, NULL);

    // 初始化 DRM 设备
    DRMinit(lcd);

    // 为显示屏添加一个FrameBuffer显存，并获取显示屏的分辨率、显存入口和色深等信息
    DRMcreateFB(lcd, &drm);
    int bpp = drm.pitch / drm.width * 8;
    printf("显示器尺寸: %d×%d\n", drm.width, drm.height);
    printf("色深: %u字节\n", bpp/8);

 
   //初始化摄像头 
   int  fd=init_video();
   
   //初始化rockx_face  
   init_face();
   
    //显示主界面
    show_win();
   
    //人面图片
	rockx_image_t input_image1;
	
    //人面小图
	rockx_image_t small_image;

	//人面特征
	rockx_face_feature_t out_feature1;
    memset(&out_feature1, 0, sizeof(rockx_face_feature_t));
	
    int ret=0;

	while(1)
	{
	   //获取bmp图片
	   get_bmp(fd);

	   //show_bmp("0.bmp",&drm,lcd);
	     // read image  读取人面照片 
        rockx_image_read("0.bmp", &input_image1, 1);
		
		//提取人面特征
        ret=run_face_recognize(&input_image1, &small_image, &out_feature1);

        //开始控制 
        if(get_face == 1)
        {
            get_face = 0;

            //把人面保存起来
            printf("Please Enter Names\n");
            char name[100] = {0};
            scanf("%s", name);
            char path[200] = {0};
            sprintf(path, "face/%s.bmp", name);

            //保存人面图片
            rockx_image_write(path, &small_image);


            show_bmp(path, &drm, lcd, 150, 450);   

        
            stpcpy(face_path[people],path);
            people++;
        }

    
        //下班打卡
        if(take_face == 1)
        {
            take_face = 0;

            //与目录下的人面进行匹配
            char *name = run_Face_Recognition(&out_feature1);
            if (name != NULL)
            {
                char msg[1024] = {0};
                sprintf(msg, "PunchOut%s", name);
                //发送考勤信息给服务器
                if(send_msg(argv[1], 6666, msg) != -1)
                {
                    printf("打卡成功\n");
                     show_bmp("/sucai/xbok.bmp", &drm, lcd, 0, 0);
                }else
                {
                     show_bmp("/sucai/fail.bmp", &drm, lcd, 0, 0);
                }
            }
        }

        //上班打卡
        if(take_face == 2)
        {
            take_face = 0;

            //与目录下的人面进行匹配
            char *name = run_Face_Recognition(&out_feature1);
            if (name != NULL)
            {
                char msg[1024] = {0};
                sprintf(msg, "PunchIn%s", name);
                 //发送考勤信息给服务器
                //发送考勤信息给服务器
                if(send_msg(argv[1], 6666, msg) != -1)
                {
                    printf("打卡成功\n");
                     show_bmp("/sucai/sbok.bmp", &drm, lcd, 0, 0);
                }else
                {
                     show_bmp("/sucai/fail.bmp", &drm, lcd, 0, 0);
                }
            }
        }



		
		//释放图片
	    rockx_image_release(&input_image1);
        if(ret == 0)
        {
          rockx_image_release(&small_image);
        }
	}


    // 释放 DRM 设备相关资源
    DRMfreeResources(lcd, &drm);

    return 0;
}
