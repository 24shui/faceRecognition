#include <stdio.h>


//定义BMP 的头数据
typedef struct                       /**** BMP file header structure ****/  
{  
    unsigned int   bfSize;           /* Size of file */  
    unsigned short bfReserved1;      /* Reserved */  
    unsigned short bfReserved2;      /* ... */  
    unsigned int   bfOffBits;        /* Offset to bitmap data */  
} MyBITMAPFILEHEADER;

typedef struct                       /**** BMP file info structure ****/  
{  
    unsigned int   biSize;           /* Size of info header */  
    int            biWidth;          /* Width of image */  
    int            biHeight;         /* Height of image */  
    unsigned short biPlanes;         /* Number of color planes */  
    unsigned short biBitCount;       /* Number of bits per pixel */  
    unsigned int   biCompression;    /* Type of compression to use */  
    unsigned int   biSizeImage;      /* Size of image data */  
    int            biXPelsPerMeter;  /* X pixels per meter */  
    int            biYPelsPerMeter;  /* Y pixels per meter */  
    unsigned int   biClrUsed;        /* Number of colors used */  
    unsigned int   biClrImportant;   /* Number of important colors */  
} MyBITMAPINFOHEADER;

//rgb to BMP 
int creat_bmp(unsigned char *rgb,const char *bmp_file)
{
	char buf[480][640][3]={0};
	int width=640;
	int height=480;
	MyBITMAPFILEHEADER bfh;  
    MyBITMAPINFOHEADER bih;  
   
    unsigned short bfType=0x4d42;             
    bfh.bfReserved1 = 0;  
    bfh.bfReserved2 = 0;  
    bfh.bfSize = 2+sizeof(MyBITMAPFILEHEADER) + sizeof(MyBITMAPINFOHEADER)+width*height*3;  
    bfh.bfOffBits = 0x36;  
  
    bih.biSize = sizeof(MyBITMAPINFOHEADER);  
    bih.biWidth = width;  
    bih.biHeight = height;  
    bih.biPlanes = 1;  
    bih.biBitCount = 24;  
    bih.biCompression = 0;  
    bih.biSizeImage = 0;  
    bih.biXPelsPerMeter = 5000;  
    bih.biYPelsPerMeter = 5000;  
    bih.biClrUsed = 0;  
    bih.biClrImportant = 0;  
    FILE * file = fopen( bmp_file,"wb" );  
    if (!file)  
    {  
        printf("Could not write file\n");  
        return 0;  
    }  
    int i,j,k;
    for(i=479;i>=0;i--)
    {
    	for(j=0;j<640;j++)
    	{
    		for(k=2;k>=0;k--)
    		{
    			buf[i][j][k]=*rgb;
    			rgb++;
    		}
    	}
    }
   
    fwrite(&bfType,sizeof(bfType),1,file);  
    fwrite(&bfh,sizeof(bfh),1, file);  
    fwrite(&bih,sizeof(bih),1, file);  
  
    fwrite(buf,width*height*3,1,file);  
    fclose(file);  
}



int yuyv2rgb(int y, int u, int v)
{
     unsigned int pixel24 = 0;
     unsigned char *pixel = (unsigned char *)&pixel24;
     int r, g, b;
     static int  ruv, guv, buv;

	 // 色度
     ruv = 1596*(v-128);
	 guv = 391*(u-128) + 813*(v-128);
     buv = 2018*(u-128);
     
	// RGB
     r = (1164*(y-16) + ruv) / 1000;
     g = (1164*(y-16) - guv) / 1000;
     b = (1164*(y-16) + buv) / 1000;

     if(r > 255) r = 255;
     if(g > 255) g = 255;
     if(b > 255) b = 255;
     if(r < 0) r = 0;
     if(g < 0) g = 0;
     if(b < 0) b = 0;

     pixel[0] = r;
     pixel[1] = g;
     pixel[2] = b;

     return pixel24;
}



//YUYV to  RGB 
int yuyv2rgb0(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
     unsigned int in, out;
     int y0, u, y1, v;
     unsigned int pixel24;
     unsigned char *pixel = (unsigned char *)&pixel24;
     unsigned int size = width*height*2;

     for(in = 0, out = 0; in < size; in += 4, out += 6)
     {
		 // YUYV
          y0 = yuv[in+0];
          u  = yuv[in+1];
          y1 = yuv[in+2];
          v  = yuv[in+3];

          pixel24 = yuyv2rgb(y0, u, v); // RGB
          rgb[out+0] = pixel[0];    
          rgb[out+1] = pixel[1];
          rgb[out+2] = pixel[2];

          pixel24 = yuyv2rgb(y1, u, v);// RGB
          rgb[out+3] = pixel[0];
          rgb[out+4] = pixel[1];
          rgb[out+5] = pixel[2];

     }
     return 0;
}

