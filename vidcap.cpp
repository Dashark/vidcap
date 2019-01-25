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
#include <capture.h>
#include "rapp.h"

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
  media_stream *     stream = NULL;
  media_frame *      frame = NULL;
  struct timeval     tv_start, tv_end;
  int                msec;
  int                i          = 1;
  int                numframes  = 100;
  unsigned long long totalbytes = 0;
  FILE *file;

  /* is numframes specified ? */
  if (argc >= 4) {
    numframes = atoi(argv[3]);

    /* Need at least 2 frames for the achived fps calculation */
    if (numframes < 2) {
      numframes = 2;
    }
  }

  openlog("vidcap", LOG_PID | LOG_CONS, LOG_USER);
  int width = 1280;
  int height = 720;
  int stride = 1280;
  int size = 1280 * 720;
  uint8_t* data = new uint8_t[size];
  if(argc == 2) {
    file = fopen("test.yuv", "rb");
    fread(data, 1, size, file);
    fclose(file);
  }
  else {
    delete[] data;
    stream = capture_open_stream(argv[1], argv[2]);
    if (stream == NULL) {
      LOGERR("Failed to open stream\n");
      closelog();
      return EXIT_FAILURE;
    }

    gettimeofday(&tv_start, NULL);

    /* print intital information */
    frame = capture_get_frame(stream);
    sleep(1);
    width = (int)capture_frame_width(frame);
    height = (int)capture_frame_height(frame);
    stride = (int)capture_frame_stride(frame);
    size = (int)capture_frame_size(frame);
    data = (uint8_t*)capture_frame_data(frame);
  }
  LOGINFO("etting %d frames. resolution: %dx%d framesize: %d\n",
          numframes,
          width,
          height,
          size);

  rapp_initialize();
  Filter *filt = new Filter(10, 2650, 102000, 15);
  Filter *dig_filt = new Filter(20, 100, 300, 5);
  Contour *cont = new Contour(data, stride, width, height);
  unsigned thresh = 150;
  cont->thresh_gt(thresh);
  cont->save("bin.yuv");
  /*
  for(int i = 0; i < 3; ++i) {
    Contour *dig_cont = cont->search(filt);
    //dig_cont->save("2nd.yuv");
    delete dig_cont;
  }
  */
  cont->save_bin("2nd.yuv");
  Contour *dig_cont = cont->search(filt);
  dig_cont->thresh_lt(thresh);
  dig_cont->save("last.yuv");
  dig_cont->save_bin("last_bin.yuv");
  Contour *digital = dig_cont->search(dig_filt);
  digital->save("dig.yuv");
  digital->save_bin("dig_bin.yuv");
  delete digital;
  digital = dig_cont->search(dig_filt);
  digital->save("o1.yuv");
  delete dig_cont;

  //Contour *digital = dig_cont->search(dig_filt);

  cont->save("1st.yuv");
  //dig_cont->save("2nd.yuv");
  //digital->save("3rd.yuv");
  //delete digital;
  //delete dig_cont;
  delete cont;
  delete dig_filt;
  delete filt;
  //rapp_free(rapp_buffer);
  if(frame != NULL)
    capture_frame_free(frame);

  gettimeofday(&tv_end, NULL);

  /* calculate fps */
  msec  = tv_end.tv_sec * 1000 + tv_end.tv_usec / 1000;
  msec -= tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000;

  LOGINFO("Fetched %d images in %d milliseconds, fps:%0.3f MBytes/sec:%0.3f\n",
          i,
          msec,
          (float)(i / (msec / 1000.0f)),
          (float)(totalbytes / (msec * 1000.0f)));
  if(stream != NULL)
    capture_close_stream(stream);
  closelog();

  return EXIT_SUCCESS;
}
