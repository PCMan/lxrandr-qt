/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2013  <copyright holder> <email>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "randr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>



static double mode_refresh (XRRModeInfo *mode_info)
{
    double rate;
    unsigned int vTotal = mode_info->vTotal;

    if (mode_info->modeFlags & RR_DoubleScan) {
	/* doublescan doubles the number of lines */
	vTotal *= 2;
    }

    if (mode_info->modeFlags & RR_Interlace) {
	/* interlace splits the frame into two fields */
	/* the field rate is what is typically reported by monitors */
	vTotal /= 2;
    }
    
    if (mode_info->hTotal && vTotal)
	rate = ((double) mode_info->dotClock /
		((double) mode_info->hTotal * (double) vTotal));
    else
    	rate = 0;
    return rate;
}

/* Returns the index of the last value in an array < 0xffff */
static int find_last_non_clamped(unsigned short array[], int size) {
    int i;
    for (i = size - 1; i > 0; i--) {
        if (array[i] < 0xffff)
	    return i;
    }
    return 0;
}


static void get_gamma_info(Display *dpy, XRRScreenResources *res, RRCrtc crtc, float *brightness, float *red, float *blue, float *green)
{
    XRRCrtcGamma *crtc_gamma;
    double i1, v1, i2, v2;
    int size, middle, last_best, last_red, last_green, last_blue;
    unsigned short *best_array;

    size = XRRGetCrtcGammaSize(dpy, crtc);
    if (!size) {
      printf("Failed to get size of gamma for output\n");
      return;
    }

    crtc_gamma = XRRGetCrtcGamma(dpy, crtc);
    if (!crtc_gamma) {
      printf("Failed to get gamma for output\n");
      return;
    }

    /*
     * Here is a bit tricky because gamma is a whole curve for each
     * color.  So, typically, we need to represent 3 * 256 values as 3 + 1
     * values.  Therefore, we approximate the gamma curve (v) by supposing
     * it always follows the way we set it: a power function (i^g)
     * multiplied by a brightness (b).
     * v = i^g * b
     * so g = (ln(v) - ln(b))/ln(i)
     * and b can be found using two points (v1,i1) and (v2, i2):
     * b = e^((ln(v2)*ln(i1) - ln(v1)*ln(i2))/ln(i1/i2))
     * For the best resolution, we select i2 at the highest place not
     * clamped and i1 at i2/2. Note that if i2 = 1 (as in most normal
     * cases), then b = v2.
     */
    last_red = find_last_non_clamped(crtc_gamma->red, size);
    last_green = find_last_non_clamped(crtc_gamma->green, size);
    last_blue = find_last_non_clamped(crtc_gamma->blue, size);
    best_array = crtc_gamma->red;
    last_best = last_red;
    if (last_green > last_best) {
      last_best = last_green;
      best_array = crtc_gamma->green;
    }
    if (last_blue > last_best) {
      last_best = last_blue;
      best_array = crtc_gamma->blue;
    }
    if (last_best == 0)
      last_best = 1;

    middle = last_best / 2;
    i1 = (double)(middle + 1) / size;
    v1 = (double)(best_array[middle]) / 65535;
    i2 = (double)(last_best + 1) / size;
    v2 = (double)(best_array[last_best]) / 65535;
    if (v2 < 0.0001) { /* The screen is black */
      *brightness = 0;
      *red = 1;
      *green = 1;
      *blue = 1;
    } else {
    if ((last_best + 1) == size)
        *brightness = v2;
    else
        *brightness = exp((log(v2)*log(i1) - log(v1)*log(i2))/log(i1/i2));
        *red = log((double)(crtc_gamma->red[last_red / 2]) / *brightness
              / 65535) / log((double)((last_red / 2) + 1) / size);
        *green = log((double)(crtc_gamma->green[last_green / 2]) / *brightness
                / 65535) / log((double)((last_green / 2) + 1) / size);
        *blue = log((double)(crtc_gamma->blue[last_blue / 2]) / *brightness
               / 65535) / log((double)((last_blue / 2) + 1) / size);
    }

    XRRFreeGamma(crtc_gamma);
}

/** Sort modeLines using pixels size.
 */
static void sort_modes(QList<QString> &modes)
{
  int max=0;
  int pixels=0;
  QRegExp re("([0-9]*)x([0-9]*)");
  for(int i=0; i<modes.size() ; i++)
  {
    max=0;
    for(int j=i; j<modes.size(); j++)
    {
      if( re.indexIn(modes[j]) != -1 )
      {
        pixels = re.cap(1).toInt() * re.cap(2).toInt();
        
        if(pixels > max)
        {
          modes.swap(i,j);
          max = pixels;
        }
      }
    }
  }
}

/** Add virtual mode lines. Returns number of virtual modelines added.
 */
static int add_virtual_modes(QList<QString> &modes)
{
  if(modes.size()==0)
    return 0;
  QRegExp re("([0-9]*)x([0-9]*)");
  if( re.indexIn(modes[0]) != -1 )
  {
    int x = re.cap(1).toInt();
    int y = re.cap(2).toInt();
    for(int i=1;i<=4; i++)
    {
      float scale=1.0+0.25*(float)i;
      QString mode = QString("%1x%2 *").arg(round(scale*(float)x)).arg(round(scale*(float)y));
      modes.prepend(mode);
    }
    return 4;
  }
  return 0;
}

static QList<Monitor*> get_outputs (Display *dpy, XRRScreenResources *res)
{
    QList<Monitor*> monitors;
    int o;
    int jj = 0;
    QString currentMode;
    QString currentRate;
    QString preferredMode;
    QString preferredRate;
    for (o = 0; o < res->noutput; o++)
    {
        // Get output name
        XRROutputInfo *output_info = XRRGetOutputInfo (dpy, res, res->outputs[o]);
        Monitor *monitor = new Monitor();
        monitor->name = output_info->name;
        //printf("Nombre Output: %s\n", output_info->name);
        switch (output_info->connection) {
        case RR_Disconnected:
            //printf("  Desconectado\n");
            continue;
            break;
        case RR_Connected:
            //printf("  Conectado\n");
            if (!output_info->crtc) {
                //output->changes |= changes_automatic;
                //output->automatic = True;
            }
        }
            {
              // Get output modes
              QHash<QString, QList<QString> > modes_hash;
              for (int j = jj; j < output_info->nmode; j++)
              {
                  // Get mode rates
                  XID mode_xid = output_info->modes[j];
                  for(int k=0; k < res->nmode; k++)
                  {
                    XRRModeInfo *mode = &res->modes[k];
                    if(mode->id==mode_xid)
                    {
                      //printf("  Modo %d: %s\n", j, mode->name);
                      //printf ("     %s (0x%x) %6.1fMHz\n", mode->name, (int)mode->id, (double)mode->dotClock / 1000000.0);
                      //printf("      %6.1fMHz\n", mode_refresh (mode));
                      char buffer[10];
                      sprintf(buffer, "%6.1f", mode_refresh (mode));
                      QString rate(buffer);
                      if(!modes_hash.contains(QString(mode->name)))
                      {
                        QList<QString> rates;
                        modes_hash[QString(mode->name)] = rates;
                      }
                      modes_hash[QString(mode->name)].append(rate);
                      if (j < output_info->npreferred)
                      {
                        //  printf ("       +preferred\n");
                        preferredMode = mode->name;
                        preferredRate = rate;
                      }
                      // Is this mode current mode?
                      for (int c = 0; c < res->ncrtc; c++)
                      {
                        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo (dpy, res, res->crtcs[c]);
                        if(crtc_info->mode==mode_xid)
                        {
                          float brightness, red, blue, green;
                          get_gamma_info(dpy, res, res->crtcs[c], &brightness, &red, &blue, &green);
                          //printf ("       +current\n");
                          //printf ("       Brightness: %6.1f\n", brightness);
                          //printf ("       Gamma:\n");
                          //printf ("         Red: %6.1f\n", red);
                          //printf ("         Blue: %6.1f\n", blue);
                          //printf ("         Green: %6.1f\n", green);
                          currentMode = mode->name;
                          currentRate = rate;
                          monitor->brightness = brightness;
                          monitor->red = red;
                          monitor->blue = blue;
                          monitor->green = green;
                        }
                        XRRFreeCrtcInfo (crtc_info);
                      }
                      //printf("\n");
                    }
                  }
                //jj++;
              }
              QList<ModeLine> modeLines;
              QList<QString> keys = modes_hash.uniqueKeys();
              sort_modes(keys);
              int n_virtual_modelines = add_virtual_modes(keys);
              for(int i=0; i<keys.size(); i++)
              {
                ModeLine modeline;
                modeline.modeline = keys[i];
                modeline.rates = modes_hash[keys[i]];
                if(modeline.modeline==currentMode)
                  monitor->currentMode = modeLines.size();
                if(modeline.modeline==preferredMode)
                  monitor->preferredMode = modeLines.size();
                for(int rates_index=0; rates_index<modeline.rates.size(); rates_index++) {
                  if(modeline.rates[rates_index]==currentRate)
                    monitor->currentRate=rates_index;
                  if(modeline.rates[rates_index]==preferredRate)
                    monitor->preferredRate=rates_index;
                }
                if(i<n_virtual_modelines)
                {
                  modeline.scale=QString("%1x%1").arg(2.0-1.0/(float)n_virtual_modelines*(float)i);
                }
                else
                  modeline.scale=QString("1x1");
                modeLines.append(modeline);
              }
              monitor->modeLines = modeLines;
              monitor->modes_hash = modes_hash;
            }
        monitors.append(monitor);
    }
    printf("[get_outputs]: Finalizado\n");
    return monitors;
}


QList<Monitor*> get_monitors_info(){
  int event_base, error_base;
  int major, minor;
  Display                 *dpy;
  Window                  root;
  
  QList<Monitor*> monitors;

  //     CONNECT TO X-SERVER, GET ROOT WINDOW ID
  dpy    = XOpenDisplay(NULL);
  root   = RootWindow(dpy, 0);
  XRRScreenResources  *res;
  int screenNo = ScreenCount(dpy);

  if (!XRRQueryExtension (dpy, &event_base, &error_base) ||
      !XRRQueryVersion (dpy, &major, &minor))
  {
      fprintf (stderr, "RandR extension missing\n");
      return monitors;
  }

  res = XRRGetScreenResources (dpy, root);
  if (!res) 
  {
    fprintf (stderr, "Could not get screen resources\n");
    return monitors;
  }

  monitors = get_outputs(dpy, res);
  XCloseDisplay(dpy);
  return monitors;
}
//
//      gcc -o Xrandr Xrandr.cc -lX11 -lXrandr -lstdc++
//
