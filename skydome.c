#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <memory.h>

#include "ArHosekSkyModel.h"

typedef struct RawImage_ {
  double r, g, b;
} RawImage;

void conv(RawImage* pixel, unsigned char* out)  {
  float d;
  int e;
  d = pixel->r > pixel->g ? pixel->r : pixel->g;
  if (pixel->b > d) d = pixel->b;
  
  if (d <= 1e-32) {
    out[0] = out[1] = out[2] = 0;
    out[3] = 0;
    return;
  }
  
  float m = frexp(d, &e); // d = m * 2^e
  d = m * 256.0 / d;
  
  out[0] = pixel->r * d;
  out[1] = pixel->g * d;
  out[2] = pixel->b * d;
  out[3] = (e + 128);
}


void saveHDRfile(char *filename, RawImage *imgdata, int width, int height) {
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    printf("Error: %s", filename);
    return;
  }

  unsigned char ret = 0x0a;
  // Header
  fprintf(fp, "#?RADIANCE%c", (unsigned char)ret);
  fprintf(fp, "# Made with 100%% pure HDR Shop%c", ret);
  fprintf(fp, "FORMAT=32-bit_rle_rgbe%c", ret);
  fprintf(fp, "EXPOSURE=          1.0000000000000%c%c", ret, ret);

  // Data
  fprintf(fp, "-Y %d +X %d%c", height, width, ret);
  for (int i = height - 1; i >= 0; i --) {

    unsigned char buf[width * 4];
    for (int j = 0; j < width; j ++) {
      conv(&imgdata[i * width + j], &buf[j * 4]);
    }

    fprintf(fp, "%c%c", 0x02, 0x02);
    fprintf(fp, "%c%c", (width >> 8) & 0xFF, width & 0xFF);

    for (int j = 0; j < 4; j ++) {
      int cursor = 0;
      for (;;) {
	int w = width - cursor;
	if (w >= 128)
	  w = 127;
	fprintf(fp, "%c", w);
	for (int idx = cursor; idx < cursor + w; idx ++) 
	    fprintf(fp, "%c", buf[idx * 4 + j]); 
	cursor += w;
	if (cursor >= width)
	  break;
      }
    }
  }
	
  fclose(fp);
}

// tekito converter
void xyz2rgb(double* xyz, double* rgb) {
  double X = xyz[0];
  double Y = xyz[1];
  double Z = xyz[2];
  rgb[0] = 3.2410*X - 1.5374*Y - 0.4986*Z;
  rgb[1] = -0.9692*X + 1.8760*Y + 0.0416*Z;
  rgb[2] = 0.0556*X - 0.2040*Y + 1.5070*Z;
}

// 
// skydome filename width height turbidity albedo solarElevation
//
int main(int argc, char **argv) {  
  ArHosekXYZSkyModelState *skymodel_state;
  if (argc < 7)
    return 0;

  int width = atof(argv[2]);
  int height = atof(argv[3]);
  double turbidity = atof(argv[4]);
  double albedo = atof(argv[5]);
  double solarElevation = atof(argv[6]);
  RawImage *img = (RawImage*)malloc(sizeof(RawImage) * width * height);
  skymodel_state = arhosek_xyz_skymodelstate_alloc_init(turbidity, albedo, solarElevation);


  for (int y = 0; y < height; y ++) {
    for (int x = 0; x < width; x ++) {
      double PI = 3.14159265359;

      int cx = width / 2;
      int cy = height / 2;
      int rx = (x - cx);
      int ry = (y - cy);

      double nr = sqrt(rx*rx + ry*ry) / (width / 2.0);
      double th = nr * 0.5 * PI;
      double ph = atan2(rx, ry);
      
      double gamma = acos(cos(solarElevation) * sin(th) * sin(ph) + sin(solarElevation) * cos(th));
      double theta = th;

      if (nr < width / 2.0) {
	double xyz[3];
	double rgb[3];

	for (int i = 0; i < 3; i ++) {
	  xyz[i] = arhosek_xyz_skymodel_radiance(skymodel_state, theta, gamma, i);
	}
	xyz2rgb(xyz, rgb);

	img[y * width + x].r = rgb[0];
	img[y * width + x].g = rgb[1];
	img[y * width + x].b = rgb[2];       
      } else {
	img[y * width + x].r = 0;
	img[y * width + x].g = 0;
	img[y * width + x].b = 0;
      }
    }
  }

  arhosek_xyz_skymodelstate_free(skymodel_state);
  

  // save
  saveHDRfile(argv[1], img, width, height);
  free(img);

  return 0;
}
