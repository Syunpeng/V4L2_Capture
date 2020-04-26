#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h> 
#include <getopt.h>
#include <fcntl.h>            
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
 
#include <asm/types.h>        
#include <linux/videodev2.h>
 
#include <jpeglib.h>
#include <jerror.h>
 
 
#define CLEAR(x) memset (&(x), 0, sizeof (x))
 
/* 分辨率600 * 480(VGA格式) */
#define  VIDEO_WIDTH    640
#define  VIDEO_HEIGHT   480 
#define  PIXEL_DEPTH    3 
#define  CAPTURE_FILE     "../test.yuv"  //"test.jpg"     // 笔记本摄像头选择"test.jpg" 
#define  JPEG_TEST_FILE   "../testjpeg.jpg"
#define  RGB_TO_BMP_FILE  "../testrgb2bmp.bmp"
//extern JSAMPLE * image_buffer;	/* Points to large array of R,G,B-order data */
int image_height = 480;	/* Number of rows in image */
int image_width  = 640;		/* Number of columns in image */ 
 
/* 录制格式,笔记本摄像头可以选择V4L2_PIX_FMT_MJPEG,这样可以直接保存成jpg,直观 */
#define VIDEO_FORMAT V4L2_PIX_FMT_YUYV  //V4L2_PIX_FMT_MJPEG  
//V4L2_PIX_FMT_YUYV 
//V4L2_PIX_FMT_MJPEG
//V4L2_PIX_FMT_YUYV
//V4L2_PIX_FMT_YVU420
//V4L2_PIX_FMT_RGB32


/* BMP 图像格式相关*/
#if 1
typedef int LONG;
typedef unsigned int DWORD;
typedef unsigned short WORD;

typedef struct {
        WORD    bfType;
        DWORD   bfSize;
        DWORD   bfReserved;
        DWORD   bfOffBits;
} BMPFILEHEADER_T;


typedef struct{
        DWORD      biSize;
        LONG       biWidth;
        LONG       biHeight;
        WORD       biPlanes;
        WORD       biBitCount;
        DWORD      biCompression;
        DWORD      biSizeImage;
        LONG       biXPelsPerMeter;
        LONG       biYPelsPerMeter;
        DWORD      biClrUsed;
        DWORD      biClrImportant;
} BMPINFOHEADER_T;
#endif


struct buffer {
    void *  start;
    size_t  length;
};
 
static char *dev_name = "/dev/video0";//摄像头设备名
static int fd = -1;
struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;
static FILE *file_fd;
static unsigned long file_length;

//900KB--暂时没有用到
unsigned char rgb24_buffer[VIDEO_WIDTH*VIDEO_HEIGHT*PIXEL_DEPTH];

/* YUV422 TO  RGB24  BUFF */
unsigned char   RGB24_buffer[VIDEO_WIDTH*VIDEO_HEIGHT*PIXEL_DEPTH]; 
 
static int read_frame (void);
static int open_device(void);
static int init_device(void);
static int start_capture(void);
static int stop_capture(void);
static int close_device(void);
static int YUV422TORGB24(unsigned char *outRGB24,void * start);
void Rgb2Bmp(unsigned char * pdata, char * bmp_file, int width, int height ,int pixel_depth);
void write_JPEG_file (unsigned char *start,char * filename, int quality, int width, int height ,int pixel_depth,int in_color_space);

 
static int init_device()
{
	//获取驱动信息//获取摄像头参数//查询驱动功能并打印
    struct v4l2_capability cap;
 
    if(ioctl (fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        printf("get vidieo capability error,error code: %d \n", errno);
        exit(1);
    }
	// Print capability infomations
	printf("\nCapability Informations:\n");
	printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\nCapabilities: X\n",
	        cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0XFF, (cap.version>>8)&0XFF,cap.version&0XFF,cap.capabilities );
	
	
	//获取设备支持的视频格式
    struct v4l2_fmtdesc fmtdesc;     
    CLEAR (fmtdesc);
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("\nSupport format:\n");
	
	while ((ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0)
    {
        printf("/t%d.\n{\npixelformat = '%c%c%c%c',\ndescription = '%s'\n }\n",
            fmtdesc.index+1,
            fmtdesc.pixelformat & 0xFF,
            (fmtdesc.pixelformat >> 8) & 0xFF,
            (fmtdesc.pixelformat >> 16) & 0xFF,
            (fmtdesc.pixelformat >> 24) & 0xFF,
            fmtdesc.description);  
			fmtdesc.index++;
    }
	
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        fprintf (stderr, "%s is no video capture device\n", dev_name);
        exit (EXIT_FAILURE);
    }
	
	//检查是否支持某种帧格式   
    struct v4l2_format fmt2;
    fmt2.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;   
    fmt2.fmt.pix.pixelformat = VIDEO_FORMAT;
 
    if(ioctl(fd,VIDIOC_TRY_FMT,&fmt2)==-1)
	{		
        if(errno==EINVAL)
		{
            printf("not support format %s!\n","VIDEO_FORMAT");
		}			
	}
	
	//设置视频捕获格式
    struct v4l2_format fmt;
    CLEAR (fmt);
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = VIDEO_WIDTH;
    fmt.fmt.pix.height      = VIDEO_HEIGHT;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    fmt.fmt.pix.pixelformat = VIDEO_FORMAT;
    
    //设置图像格式
    if(ioctl (fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        printf("failture VIDIOC_S_FMT\n");
        exit(1);
    }
	
	
    // 显示当前帧的相关信息
    struct v4l2_format fmt3;
    fmt3.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd,VIDIOC_G_FMT,&fmt3); 
    printf("\nCurrent data format information:\n twidth:%d\n theight:%d\n",
	    fmt3.fmt.pix.width,fmt3.fmt.pix.height);
 
    struct v4l2_fmtdesc fmtdes;
    fmtdes.index=0;
    fmtdes.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdes)!=-1)
    {
        if(fmtdes.pixelformat & fmt3.fmt.pix.pixelformat)
        {
            printf(" tformat:%s\n",fmtdes.description);
            break;
        }
        fmtdes.index++;
    }
	
	
	 //视频分配捕获内存
    struct v4l2_requestbuffers req;
    CLEAR (req);
    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;
 
    //申请缓冲，count是申请的数量
    if(ioctl (fd, VIDIOC_REQBUFS, &req) < 0)
    {
        printf("failture VIDIOC_REQBUFS\n");
        exit(1);
    }
    
    if (req.count < 2)
	{
	    printf("Insufficient buffer memory\n");
	}
	
	//内存中建立对应空间
    //获取缓冲帧的地址、长度
    buffers = calloc (req.count, sizeof (*buffers));//在内存的动态存储区中分配n个长度为size的连续空间，函数返回一个指向分配起始地址的指针
    if (!buffers)
    {
        fprintf (stderr, "Out of memory/n");
        exit (EXIT_FAILURE);
    }
	
	
	for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        struct v4l2_buffer buf;   //驱动中的一帧
        CLEAR (buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;// 要获取内核视频缓冲区的信息编号
 
        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buf)) //映射用户空间
        {
            printf ("VIDIOC_QUERYBUF error\n");
            exit(-1);
        }
        buffers[n_buffers].length = buf.length; 
 
        // 把内核空间缓冲区映射到用户空间缓冲区
        buffers[n_buffers].start = mmap (NULL ,    //通过mmap建立映射关系
            buf.length,
            PROT_READ | PROT_WRITE ,
            MAP_SHARED ,
            fd,
            buf.m.offset);
 
        if (MAP_FAILED == buffers[n_buffers].start)
        {
            printf ("mmap failed\n");
            exit(1);
        }
    }	
	
	
	//投放一个空的视频缓冲区到视频缓冲区输入队列中
    //把四个缓冲帧放入队列，并启动数据流
    unsigned int i;
 
    // 将缓冲帧放入队列
    enum v4l2_buf_type type;
    for (i = 0; i < n_buffers; ++i)
    {
        struct v4l2_buffer buf;
        CLEAR (buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i; //指定要投放到视频输入队列中的内核空间视频缓冲区的编号;
 
        if (-1 == ioctl (fd, VIDIOC_QBUF, &buf))//申请到的缓冲进入列队
            printf ("VIDIOC_QBUF failed\n");
    }
 
    //开始捕捉图像数据  
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl (fd, VIDIOC_STREAMON, &type))
    {
        printf ("VIDIOC_STREAMON failed\n");
        exit(1);
    }
	
	
} 

static int open_device()
{
	    //打开设备
    fd = open (dev_name, O_RDWR | O_NONBLOCK, 0);
    if(fd < 0)
    {
        printf("open %s failed\n",dev_name);
        exit(1);
    }
	
}

/*
	获取一帧数据
	从视频缓冲区的输出队列中取得一个已经保存有一帧视频数据的视频缓冲区
*/
static int read_frame (void)
{
    struct v4l2_buffer buf;
    CLEAR (buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if(ioctl (fd, VIDIOC_DQBUF, &buf) == -1)
    {
        printf("VIDIOC_DQBUF failture\n"); //出列采集的帧缓冲
        exit(1);
    }
 
    assert (buf.index < n_buffers);
    printf ("buf.index dq is %d, n_buffers = %d\n",buf.index,n_buffers);
 
    //写入文件中
    fwrite(buffers[buf.index].start, buffers[buf.index].length, 1, file_fd);
    printf("Capture one frame saved in %s\n", CAPTURE_FILE);
	
	YUV422TORGB24(rgb24_buffer,buffers[buf.index].start);
	
    Rgb2Bmp(rgb24_buffer,RGB_TO_BMP_FILE,VIDEO_WIDTH,VIDEO_HEIGHT,PIXEL_DEPTH);
	
	write_JPEG_file (rgb24_buffer,JPEG_TEST_FILE, 80,VIDEO_WIDTH,VIDEO_HEIGHT,3,2);	
	
    //再入列
    if(ioctl (fd, VIDIOC_QBUF, &buf)<0)
	{
	    printf("failture VIDIOC_QBUF\n");
		return -1;
		
	}
    
    return 1;
}
#if 1
/***********************
Bmp  fwrite to file
************************/
void Rgb2Bmp(unsigned char * pdata, char * bmp_file, int width, int height ,int pixel_depth)
{      //分别为rgb数据，要保存的bmp文件名，图片长宽
       BMPFILEHEADER_T bfh;
       BMPINFOHEADER_T bih;
       FILE * fp = NULL;
       
       bih.biSize = 40;
       bih.biWidth = width;
       bih.biHeight = height;//BMP图片从最后一个点开始扫描，显示时图片是倒着的，所以用-height，这样图片就正了
       bih.biPlanes = 1;//为1，不用改
       bih.biBitCount = 24;
       bih.biCompression = 0;//不压缩
       bih.biSizeImage = width*height*pixel_depth;
       bih.biXPelsPerMeter = 0 ;//像素每米
       bih.biYPelsPerMeter = 0 ;
       bih.biClrUsed = 0;//已用过的颜色，24位的为0
       bih.biClrImportant = 0;//每个像素都重要
       
       bfh.bfType = 0x4d42;  //bm
       bfh.bfSize = 54 + width*height*pixel_depth;       
       bfh.bfReserved = 0; 
	   bfh.bfOffBits = 54;
       
       fp = fopen( bmp_file,"wb" );
       if(!fp) 
       {
       		printf("open %s failed in %s,line number is %d",bmp_file,__FILE__,__LINE__);
       		perror("");
       		return;
	   }
		/*
		不能用 fwrite(&bfh,14,1,fp); 代替以下四行，否则会出错
		因为linux上是4字节对齐，14不是4的倍数，所以分若干次写入
		*/		
       fwrite(&bfh.bfType,2,1,fp);
       fwrite(&bfh.bfSize,4,1,fp);
       fwrite(&bfh.bfReserved,4,1,fp); 
       fwrite(&bfh.bfOffBits ,4,1,fp);
       
       fwrite(&bih, 40,1,fp);
       fwrite(pdata,bih.biSizeImage,1,fp);
       fclose(fp);
       
}
 
#endif


#if 1

/***********************
将YUV转换为RGB888
************************/
static int YUV422TORGB24(unsigned char *outRGB24,void * start)
{
	int           	i,j;
    unsigned char 	y1,y2,u,v;
    int 			r1,g1,b1,r2,g2,b2;
    char * 			pointer;
	
	pointer = start;
	
	/*有写图像的坐标是左下角有些是右下角*/
    for(i=0;i<VIDEO_HEIGHT;i++)
	//for(i=VIDEO_HEIGHT-1;i>0;i--)
    {
    	for(j=0;j<(VIDEO_WIDTH/2);j++)
    	{
    		y1 = *( pointer + (i*(VIDEO_WIDTH/2)+j)*4);
    		u  = *( pointer + (i*(VIDEO_WIDTH/2)+j)*4 + 1);
    		y2 = *( pointer + (i*(VIDEO_WIDTH/2)+j)*4 + 2);
    		v  = *( pointer + (i*(VIDEO_WIDTH/2)+j)*4 + 3);
    		
    		r1 = y1 + 1.042*(v-128);
    		g1 = y1 - 0.34414*(u-128) - 0.71414*(v-128);
    		b1 = y1 + 1.772*(u-128);
    		
    		r2 = y2 + 1.042*(v-128);
    		g2 = y2 - 0.34414*(u-128) - 0.71414*(v-128);
    		b2 = y2 + 1.772*(u-128);
    		
    		if(r1>255)    r1 = 255;
    		else if(r1<0) r1 = 0;
    		
    		if(b1>255)    b1 = 255;
    		else if(b1<0) b1 = 0;	
    		
    		if(g1>255)    g1 = 255;
    		else if(g1<0) g1 = 0;	
    			
    		if(r2>255)    r2 = 255;
    		else if(r2<0) r2 = 0;
    		
    		if(b2>255)	  b2 = 255;
    		else if(b2<0) b2 = 0;	
    		
    		if(g2>255)	  g2 = 255;
    		else if(g2<0) g2 = 0;

			/*垂直镜像 RGB*/
    		#if 1
    		*(RGB24_buffer + ((i+1)*(VIDEO_WIDTH/2)+j)*6 + 0) = (unsigned char)r1;
    		*(RGB24_buffer + ((i+1)*(VIDEO_WIDTH/2)+j)*6 + 1) = (unsigned char)g1;
    		*(RGB24_buffer + ((i+1)*(VIDEO_WIDTH/2)+j)*6 + 2) = (unsigned char)b1;
			
    		*(RGB24_buffer + ((1+i)*(VIDEO_WIDTH/2)+j)*6 + 3) = (unsigned char)r2;
    		*(RGB24_buffer + ((1+i)*(VIDEO_WIDTH/2)+j)*6 + 4) = (unsigned char)g2;
    		*(RGB24_buffer + ((1+i)*(VIDEO_WIDTH/2)+j)*6 + 5) = (unsigned char)b2;
			#endif

			/*垂直镜像 RGB*/
    		#if 0
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 0) = (unsigned char)r1;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 1) = (unsigned char)g1;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 2) = (unsigned char)b1;
			
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 3) = (unsigned char)r2;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 4) = (unsigned char)g2;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 5) = (unsigned char)b2;
			#endif
			
			/*BGR*/
			#if 0
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6    ) = (unsigned char)b1;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 1) = (unsigned char)g1;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 2) = (unsigned char)r1;
			
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 3) = (unsigned char)b2;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 4) = (unsigned char)g2;
    		*(RGB24_buffer + ((VIDEO_HEIGHT-1-i)*(VIDEO_WIDTH/2)+j)*6 + 5) = (unsigned char)r2;
			#endif
			
    	}
    }	
	
	memcpy(outRGB24,RGB24_buffer,VIDEO_HEIGHT*VIDEO_WIDTH*PIXEL_DEPTH);
	
	return 0;
}

#endif


#if 1

void write_JPEG_file (unsigned char *start,char * filename, int quality, int width, int height ,int pixel_depth ,int in_color_space)
{
  char *  pointer = start;

  struct jpeg_compress_struct cinfo;

  struct jpeg_error_mgr jerr;
  FILE * outfile;		
  JSAMPROW row_pointer[1];	
  int row_stride;	

  /* Step 1: allocate and initialize JPEG compression object */

  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
 
  if ((outfile = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */
  cinfo.image_width = width; 	
  cinfo.image_height = height;
  cinfo.input_components = pixel_depth;	 
  cinfo.in_color_space = in_color_space; 

  jpeg_set_defaults(&cinfo);

  jpeg_set_quality(&cinfo, quality, TRUE ); 
  

  /* Step 4: Start compressor */

  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */

  row_stride = image_width * 3;	

  while (cinfo.next_scanline < cinfo.image_height) {

    row_pointer[0] = & pointer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  fclose(outfile);

  /* Step 7: release JPEG compression object */
  
  jpeg_destroy_compress(&cinfo);
  
}
#endif


static int start_capture()
{
	int i = 0 ;
	for (i=0;i<5;i++) //这一段涉及到异步IO
    {
        fd_set fds;
        struct timeval tv;
        int r;
 
        FD_ZERO (&fds);//将指定的文件描述符集清空
        FD_SET (fd, &fds);//在文件描述符集合中增加一个新的文件描述符
 
        tv.tv_sec = 2;
        tv.tv_usec = 0;
		printf("循环等待,fd = %d \n",fd);
        r = select (fd + 1, &fds, NULL, NULL, &tv);//判断是否可读（即摄像头是否准备好），tv是定时
 
        if (-1 == r){
            if (EINTR == errno)
            continue;
            printf ("select err\n");
        }
 
        if (0 == r){
            fprintf (stderr, "select timeout\n");
            exit (EXIT_FAILURE);
        }
 
        if (read_frame())//如果可读，执行read_frame ()函数，并跳出循环
		{
			printf("睡眠等待\n");
			sleep(1);
            //break;			
		}
    }
 
}

static int stop_capture()
{	
	/*释放资源*/
	unsigned int ii;
    for (ii = 0; ii < n_buffers; ++ii)
	{
        if (-1 == munmap (buffers[ii].start, buffers[ii].length))
		{			
            free (buffers);
		}
	}
}

static int close_device()
{
	close (fd);
}


//主函数
int main (int argc,char ** argv)
{
    file_fd = fopen(CAPTURE_FILE, "w");//图片文件名 	
	
	open_device(); 
	init_device();
	start_capture();
	stop_capture();
	close_device();
 
    
    printf("Camera test Done.\n");
    fclose (file_fd);
    return 0;
}