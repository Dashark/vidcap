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
#include <cassert>
#include <iostream>
#include <SDL/SDL.h>
#include "rapp.h"
#include "yamlServices.h"
#include "kdtree.h"

#ifdef DEBUG
#define D(x)    x
#else
#define D(x)
#endif

#define LOGINFO(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOGERR(fmt, args...)     { syslog(LOG_CRIT, fmt, ## args); fprintf(stderr, fmt, ## args); }
#define DEF_NUM_PTS 466

void cropYUV420(char* source, int width, int height, 
                char* dest, int cropX, int cropY, int cropWidth, int cropHeight)
{
  //int size = width * height;
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

#ifdef YUV_SHOW
static void YUVplayer(const char title[], const unsigned char *yuv,int w,int h) {
  SDL_Rect *rect = new SDL_Rect;

  SDL_Surface* screen = SDL_SetVideoMode(w, h, 0, 0);
  if (screen == NULL){
    fprintf(stderr, "create surface error!\n");
    exit(1);
  }

  SDL_Overlay* overlay = SDL_CreateYUVOverlay(w, h, SDL_YV12_OVERLAY, screen);
  if (overlay == NULL){
    fprintf(stderr, "create overlay error!\n");
    exit(1);
  }

  SDL_LockSurface(screen);
  SDL_LockYUVOverlay(overlay);

  memcpy(overlay->pixels[0], yuv, w*h);
  memset(overlay->pixels[1], 0x80, w*h/4);
  memset(overlay->pixels[2], 0x80, w*h/4);

  SDL_UnlockYUVOverlay(overlay);
  SDL_UnlockSurface(screen);

  rect->w = w;
  rect->h = h;
  rect->x = rect->y = 0;
  SDL_WM_SetCaption(title, 0);
  SDL_DisplayYUVOverlay(overlay, rect);

  SDL_Delay(1000);

  SDL_FreeYUVOverlay(overlay);
  SDL_FreeSurface(screen);
  delete rect;
}
#endif

static void savePacked(const char file[], int* buf);
static int data[DEF_NUM_PTS];
static int coords[DEF_NUM_PTS][100];
static kdtree* buildKDTree() {
  YamlData *labelData = new YamlData();
  labelData->parseYamlData("LabelData.yml");
  //labelData->printYamlData();
  YamlData *trainData = new YamlData();
  trainData->parseYamlData("TrainingData.yml");
  char tmp[100];
  unsigned char tmp1[100];
  kdtree *ptree = kd_create(100);

  for(int i=0; i<DEF_NUM_PTS; i++ ) {
    for(int j =0;j<100;j++)
      coords[i][j] = trainData->data(i,j);
    data[i] = labelData->data(0,i);
    //printf("hello world %d\n", i);
    if(data[i] < 0) continue;
    assert( 0 == kd_insert( ptree, coords[i], &data[i] ) );
    /*
    if(data[i] == 1 || data[i] == 6 || data[i] == 8) {
      sprintf(tmp, "%d-%d.sam", i, data[i]);
      savePacked(tmp, coords[i]);
      for(int j = 0; j < 100; ++j)
        tmp1[j] = static_cast<unsigned char>(coords[i][j]);
      YUVplayer(tmp, tmp1, 10, 10);
    }
    */
  }
  delete labelData;
  delete trainData;
  return ptree;
}

static void savePacked(const char file[], int* buf) {
  uint8_t *tmp = new uint8_t[100];
  FILE *pf = fopen(file, "w");
  for(int i = 0; i < 100; ++i) {
    tmp[i] = static_cast<uint8_t>(buf[i]);
    fprintf(pf, "%d ", tmp[i]);
    if((i+1)%10 == 0)
      fprintf(pf, "\n");
  }
  fclose(pf);
  delete[] tmp;
}

static int distance(int *pos1, int *pos2, int dim) {
  int ret = 0;
  for(int i = 0; i < dim; ++i) {
    ret += (pos1[i] - pos2[i])*(pos1[i] - pos2[i]);
  }
  return ret;
}

static int filter(int *data, int *dist, int num) {
  int count = 0;
  for(int i = 0; i < num - 1; ++i) {
    for(int j = i + 1; j < num; ++j) {
      if(data[i] == data[j]) {
        dist[i] += dist[j];
        count += 1;
        data[j] = -1;
        dist[j] = -1;
      }
    }
    if(count != 0) dist[i] /= count;
    count = 0;
  }
  int i = 0, min = dist[0], index = data[0];
  for(i = 0; i < num; ++i) {
    if(data[i] != -1 && dist[i] < min) {
      min = dist[i];
      index = data[i];
    }
  }
  if(min > 400000)
    index = -1;
  std::cout << "\tcorrect data: " << index << "  distance: " << min << std::endl;
  return index;
}

int
main(int argc, char** argv)
{
  media_stream *     stream = NULL;
  media_frame *      frame = NULL;
  struct timeval     tv_start, tv_end;
  int                msec;
  int                i          = 1;
  int                numframes  = 2;
  unsigned long long totalbytes = 0;
  FILE *file;

  openlog("vidcap", LOG_PID | LOG_CONS, LOG_USER);
  int width = 1280;
  int height = 720;
  int stride = 1280;
  size_t size = 1280 * 720;
  uint8_t* data1 = new uint8_t[size];
  if(argc == 2) {
    std::cout << "File name: " << argv[1] << std::endl;
    file = fopen(argv[1], "rb");
    size_t s = fread(data1, 1, size, file);
    assert(s == size);
    fclose(file);
  }
  else {
    delete[] data1;
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
    data1 = (uint8_t*)capture_frame_data(frame);
  }
  LOGINFO("etting %d frames. resolution: %dx%d framesize: %d\n",
          numframes,
          width,
          height,
          size);

  kdtree* ptree = buildKDTree();
  //return 0;

  if (SDL_Init(SDL_INIT_VIDEO) < 0){
    fprintf(stderr, "can not initialize SDL:%s\n", SDL_GetError());
    exit(1);
  }
  rapp_initialize();
  Filter *filt = new Filter(30, 650, 2500, 15);
  Filter *dig_filt = new Filter(10, 10, 300, 5);
  Contour *orgc = new Contour(data1, stride, width, height);
  unsigned thresh = 140, count = 0, count1 = 0;
  char fname[50];
  orgc->thresh_gt(thresh);
  orgc->showU8("orgc.yuv");
  if(argc == 5)
    orgc->save(argv[4]);
  orgc->save_bin("orgc_bin1.yuv");
  //uint8_t* orgbuf = orgc->getData();
  orgc->showBin("orgc.yuv");
  Contour *labelc = orgc->search(filt);
  while(labelc != nullptr) {
    labelc->thresh_lt(130);
    snprintf(fname, 50, "labelc%d.yuv", count);
    labelc->showU8(fname);
    labelc->showBin(fname);


    //labelc->save(fname);
    //snprintf(fname, 50, "labelc%d_bin.yuv", count);
    //labelc->save_bin(fname);
    bool isLabel = false;
    Contour *digc = labelc->search(dig_filt);
    while(digc != nullptr) {
      snprintf(fname, 50, "digc%d-%d.yuv", count, count1);
      //digc->save(fname);
      digc->showU8(fname);

      int* dig = digc->getPacked();
      savePacked(fname, dig);
      int *buf = new int[100];
      int buf1[100];
      std::uninitialized_copy_n(dig, 100, buf);
      delete[] dig;

      struct kdres *pres;
      pres = kd_nearest(ptree, buf);
      //pres = kd_nearest_range(ptree, buf, 800);
      std::cout << "find results: " << kd_res_size(pres) << std::endl;
      int data[500]={-1}, dist[500]={-1}, num = 0;
      while(!kd_res_end(pres)) {
        int *pch = static_cast<int*>(kd_res_item(pres, buf1));
        data[num] = *pch; dist[num] = distance(buf, buf1, 100);
        //std::cout << "node has data:" << data[num];
        //std::cout << "   distance: " << dist[num] << std::endl;
        num += 1;
        kd_res_next(pres);
      }
      if(num != 0 && (filter(data, dist, num) != -1))
        isLabel = true;//isLabel && (filter(data, dist, num) != -1);
      else
        isLabel = false;
      kd_res_free(pres);

      delete digc;
      digc = labelc->search(dig_filt);
      count1 += 1;
    }
    if(isLabel) {
      labelc->showU8("correct");
      isLabel = false;
      //continue;
    }
    delete labelc;
    labelc = orgc->search(filt);
    count += 1;
  }
  delete orgc;
  delete dig_filt;
  delete filt;
  kd_free(ptree);
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
  SDL_Quit();
  closelog();

  return EXIT_SUCCESS;
}
