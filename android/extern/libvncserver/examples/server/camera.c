
/**
 * @example camera.c
 * Question: I need to display a live camera image via VNC. Until now I just 
 * grab an image, set the rect to modified and do a 0.1 s sleep to give the 
 * system time to transfer the data.
 * This is obviously a solution which doesn't scale very well to different 
 * connection speeds/cpu horsepowers, so I wonder if there is a way for the 
 * server application to determine if the updates have been sent. This would 
 * cause the live image update rate to always be the maximum the connection 
 * supports while avoiding excessive loads.
 *
 * Thanks in advance,
 *
 *
 * Christian Daschill 
 *
 *
 * Answer: Originally, I thought about using separate threads and using a
 * mutex to determine when the frame buffer was being accessed by any client
 * so we could determine a safe time to take a picture.  The probem is, we
 * are lock-stepping everything with framebuffer access.  Why not be a
 * single-thread application and in-between rfbProcessEvents perform a
 * camera snapshot.  And this is what I do here.  It guarantees that the
 * clients have been serviced before taking another picture.
 *
 * The downside to this approach is that the more clients you have, there is
 * less time available for you to service the camera equating to reduced
 * frame rate.  (or, your clients are on really slow links). Increasing your
 * systems ethernet transmit queues may help improve the overall performance
 * as the libvncserver should not stall on transmitting to any single
 * client.
 *
 * Another solution would be to provide a separate framebuffer for each
 * client and use mutexes to determine if any particular client is ready for
 * a snapshot.  This way, your not updating a framebuffer for a slow client
 * while it is being transferred.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rfb/rfb.h>
#ifdef LIBVNCSERVER_HAVE_GETTIMEOFDAY
/* if we have gettimeofday(), it is in this header */
#include <sys/time.h>
#endif
#if !defined LIBVNCSERVER_HAVE_GETTIMEOFDAY && defined WIN32
#include <fcntl.h>
#include <conio.h>
#include <sys/timeb.h>

static void gettimeofday(struct timeval* tv,char* dummy)
{
   SYSTEMTIME t;
   GetSystemTime(&t);
   tv->tv_sec=t.wHour*3600+t.wMinute*60+t.wSecond;
   tv->tv_usec=t.wMilliseconds*1000;
}
#endif


#define WIDTH  640
#define HEIGHT 480
#define BPP      4

/* 15 frames per second (if we can) */
#define PICTURE_TIMEOUT (1.0/15.0)


/*
 * throttle camera updates
*/
int TimeToTakePicture() {
    static struct timeval now={0,0}, then={0,0};
    double elapsed, dnow, dthen;

    gettimeofday(&now,NULL);

    dnow  = now.tv_sec  + (now.tv_usec /1000000.0);
    dthen = then.tv_sec + (then.tv_usec/1000000.0);
    elapsed = dnow - dthen;

    if (elapsed > PICTURE_TIMEOUT)
      memcpy((char *)&then, (char *)&now, sizeof(struct timeval));
    return elapsed > PICTURE_TIMEOUT;
}



/*
 * simulate grabbing a picture from some device
 */
int TakePicture(unsigned char *buffer)
{
  static int last_line=0, fps=0, fcount=0;
  int line=0;
  int i,j;
  struct timeval now;

  /*
   * simulate grabbing data from a device by updating the entire framebuffer
   */
  
  for(j=0;j<HEIGHT;++j) {
    for(i=0;i<WIDTH;++i) {
      buffer[(j*WIDTH+i)*BPP+0]=(i+j)*128/(WIDTH+HEIGHT); /* red */
      buffer[(j*WIDTH+i)*BPP+1]=i*128/WIDTH; /* green */
      buffer[(j*WIDTH+i)*BPP+2]=j*256/HEIGHT; /* blue */
    }
    buffer[j*WIDTH*BPP+0]=0xff;
    buffer[j*WIDTH*BPP+1]=0xff;
    buffer[j*WIDTH*BPP+2]=0xff;
  }

  /*
   * simulate the passage of time
   *
   * draw a simple black line that moves down the screen. The faster the
   * client, the more updates it will get, the smoother it will look!
   */
  gettimeofday(&now,NULL);
  line = now.tv_usec / (1000000/HEIGHT);
  if (line>=HEIGHT) line=HEIGHT-1;
  memset(&buffer[(WIDTH * BPP) * line], 0, (WIDTH * BPP));

  /* frames per second (informational only) */
  fcount++;
  if (last_line > line) {
    fps = fcount;
    fcount = 0;
  }
  last_line = line;
  fprintf(stderr,"%03d/%03d Picture (%03d fps)\r", line, HEIGHT, fps);

  /* success!   We have a new picture! */
  return (1==1);
}




/* 
 * Single-threaded application that interleaves client servicing with taking
 * pictures from the camera.  This way, we do not update the framebuffer
 * while an encoding is working on it too (banding, and image artifacts).
 */
int main(int argc,char** argv)
{                                       
  long usec;
  
  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,WIDTH,HEIGHT,8,3,BPP);
  if(!server)
    return 1;
  server->desktopName = "Live Video Feed Example";
  server->frameBuffer=(char*)malloc(WIDTH*HEIGHT*BPP);
  server->alwaysShared=(1==1);

  /* Initialize the server */
  rfbInitServer(server);           

  /* Loop, processing clients and taking pictures */
  while (rfbIsActive(server)) {
    if (TimeToTakePicture())
      if (TakePicture((unsigned char *)server->frameBuffer))
        rfbMarkRectAsModified(server,0,0,WIDTH,HEIGHT);
          
    usec = server->deferUpdateTime*1000;
    rfbProcessEvents(server,usec);
  }
  return(0);
}
