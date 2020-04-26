# V4L2_Capture

gcc cam3_yuv_bmp_OK.c -ljpeg

https://blog.csdn.net/yuupengsun

output info:

Capability Informations:
Driver Name:uvcvideo
Card Name:Integrated Camera
Bus info:usb-0000:02:03.0-2
Driver Version:3.2.24
Capabilities: X

Support format:
/t1.
{
pixelformat = 'YUYV',
description = 'YUV 4:2:2 (YUYV)'
 }
/t2.
{
pixelformat = 'MJPG',
description = 'MJPEG'
 }

Current data format information:
 twidth:640
 theight:480
 tformat:YUV 4:2:2 (YUYV)
循环等待,fd = 5 
buf.index dq is 0, n_buffers = 4
Capture one frame saved in ../test.yuv
睡眠等待
循环等待,fd = 5 
buf.index dq is 1, n_buffers = 4
Capture one frame saved in ../test.yuv
睡眠等待
循环等待,fd = 5 
buf.index dq is 2, n_buffers = 4
Capture one frame saved in ../test.yuv
睡眠等待
循环等待,fd = 5 
buf.index dq is 3, n_buffers = 4
Capture one frame saved in ../test.yuv
睡眠等待
循环等待,fd = 5 
buf.index dq is 0, n_buffers = 4
Capture one frame saved in ../test.yuv
睡眠等待
Camera test Done.


