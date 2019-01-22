//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
//#include "turbojpeg.h"

#include <rapp/rapp.h>

#ifdef DEBUG
#define D(x)    x
#else
#define D(x)
#endif

#define LOGINFO(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOGERR(fmt, args...)     { syslog(LOG_CRIT, fmt, ## args); fprintf(stderr, fmt, ## args); }

void cropYUV420(char* source, int width, int height, 
                char* dest, int cropX, int cropY, int cropWidth, int cropHeight)
{
  int size = width * height;
  int cx = cropX - cropX % 2;
  int cy = cropY - cropY % 2;
  int cw = cropWidth + cropWidth % 2;
  int ch = cropHeight + cropHeight % 2;
  char* sc = source + cy * width + cx;
  for(int i = 0; i < ch; i++) {
    memcpy(dest+i*cw, sc+i*width, cw);
  }

  int cux = cx / 2;
  int cuy = cy / 2;
  int cuw = cw / 2;
  int cuh = ch / 2;
  char* su = source + width*height + cuy*width/2 + cux;
  char* du = dest + cw*ch;
  for(int i = 0; i < cuh; i++) {
    memcpy(du+i*cuw, su+i*width/2, cuw);
  }
  char* sv = source + width*height*5/4 + cuy*width/2 + cux;
  du = dest + cw*ch*5/4;
  for(int i = 0; i < cuh; i++) {
    memcpy(du+i*cuw, sv+i*width/2, cuw);
  }
}

int
main(int argc, char** argv)
{
  struct timeval     tv_start, tv_end;
  int                msec;
  int                i          = 1;
  int                numframes  = 100;
  unsigned long long totalbytes = 0;
  FILE *file;
  int cropX = 0, cropY = 0;
  int cropWidth, cropHeight;
  unsigned char *rapp_buffer = NULL;

  if (argc < 3) {

    /* The host version */
    fprintf(stderr,
            "Usage: %s original_image result_image\n", argv[0]);
    fprintf(stderr,
            "Example: %s %s %s\n",
            argv[0], "test.yuv", "result.yuv");

    return 1;
  }

  openlog("vidcap", LOG_PID | LOG_CONS, LOG_USER);

  gettimeofday(&tv_start, NULL);

  /* print intital information */
  file = fopen(argv[1], "rb");
  sleep(1);
  int width = 1280;
  int height = 720;
  int size = width * height * 2;
  unsigned char* data = (unsigned char*)malloc(size);
 // unsigned char* data = (unsigned char*)malloc(1843200);
  fread(data, 1, size, file);
  LOGINFO("resolution: %dx%d framesize: %d\n",
          width,
          height,
          size);
  fclose(file);
  file = fopen(argv[2], "wb");
  if(file == NULL) {
    printf("file open failed!\n");
    return -1;
  }
  rapp_initialize();
  unsigned char* dest = (unsigned char*)rapp_malloc(size,0);
  unsigned char* dest1 = (unsigned char*)rapp_malloc(size,0);
  unsigned char* dest2 = (unsigned char*)rapp_malloc(size,0);
  //rapp_buffer = (unsigned char*)rapp_malloc(width * height, 0);
  rapp_buffer = (unsigned char*)rapp_malloc(size, 0);
  memcpy(rapp_buffer, data, size);
  memcpy(dest, data, size);
  memcpy(dest1, data, size);
  memcpy(dest2, data, size);
  //cropYUV420(rapp_buffer, width, height, dest, cropX, cropY, cropWidth, cropHeight);
  /*
  tjhandle tjh = tjInitDecompress();
  if(tjh == NULL) {
    printf("tjInitCompress error '%s'\n", tjGetErrorStr());
    return -1;
  }

  unsigned char* jpegBuf = NULL;
  unsigned long jpegSize = 0;
  printf("Decompress begin!\n");
  if(tjDecompress2(tjh, data, size, dest, cropWidth, 0, cropHeight, TJPF_GRAY, 0)) {
    printf("tjCompressFromYUV error '%s' \n", tjGetErrorStr());
    return -1;
  }
  printf("Decompress successed! %d\n", rapp_alignment);
  */
  int ret = 0;
  char* contour = (char*)malloc(1000);
  ret = rapp_thresh_gt_u8(dest, width, rapp_buffer, width, width, height, 180);
  ret = rapp_pad_const_bin(dest,width,0,width,height,1,0);
for(int i = 0;i < 10;i++) {//## number of contours: fewer than 2948
  unsigned int origin[2];
  unsigned int box[4];
  ret = rapp_contour_8conn_bin(origin,contour,1000,dest,width,width,height);
  ret = rapp_fill_8conn_bin(dest1,width,dest,width,width,height,origin[0],origin[1]);
  //printf("id:%d\n",i);
  ret = rapp_crop_box_bin(dest1,width,width,height,box);
  //printf("x:%d,y:%d,width:%d,height:%d\n",box[0],box[1],box[2],box[3]);
  if(box[2]*box[3]<600) {
    fprintf(stderr,"wrong contour!\n");
    ret = rapp_bitblt_xor_bin(dest,width,0,dest1,width,0,width,height);
    memcpy(dest1,dest,rapp_align(size));
  } 
/*else {
   fprintf(stderr,"******right contour!*******\nid:%d\npos:x:%d,y:%d\n",i,box[0],box[1]);
   ret = rapp_bitblt_xor_bin(dest,width,0,dest1,width,0,width,height);
   memcpy(dest1,dest,rapp_align(size));
  }*/
  else {
   FILE* output = NULL;
   char file_name[15];
   fprintf(stderr,"******right!*******\nid:%d\npos:x:%d,y:%d\n",i,box[0],box[1]);
   sprintf(file_name,"contour%02d.yuv",i);
   output = fopen(file_name,"wb");
   rapp_type_bin_to_u8(dest2,width,dest1,width,width,height);
   fwrite(dest2,1,size,output);
   fclose(output);
   rapp_bitblt_xor_bin(dest1,width,0,dest,width,0,width,height);
   memcpy(dest,dest1,size);
  }
}
  ret = rapp_type_bin_to_u8(dest2,width,dest,width,width,height);
  if(ret < 0) {
    printf("thresh error %s\n", rapp_error(ret));
    printf("rapp_alignment : %d\n", rapp_alignment);
  }
  fwrite(dest2, 1, size, file);
  fclose(file);
 // printf("file free succeed\n");
  free(data);
 // printf("data free succeed\n");
 // rapp_free(dest);
 // printf("dest free succeed\n");
  rapp_free(dest1);
//  printf("dest1 free succeed\n");
  rapp_free(dest2);
 // printf("dest2 free succeed\n");
  free(contour);
 // printf("contour free succeed\n");
  rapp_free(rapp_buffer);
 // printf("rapp_buffer free succeed\n");
  //tjFree(jpegBuf);

  gettimeofday(&tv_end, NULL);

  /* calculate fps */
  msec  = tv_end.tv_sec * 1000 + tv_end.tv_usec / 1000;
  msec -= tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000;

  LOGINFO("Fetched %d images in %d milliseconds, fps:%0.3f MBytes/sec:%0.3f\n",
          i,
          msec,
          (float)(i / (msec / 1000.0f)),
          (float)(totalbytes / (msec * 1000.0f)));

  closelog();
  //rapp_terminate();
  return EXIT_SUCCESS;
}
