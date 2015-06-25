/*
 * getjpeg.c, sample tool for grabbing images from video devices
 *
 * Copyright (c) 2003 Joerg Heckenbach <joerg@heckenbach-aw.de>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <libgen.h>

#define GETJPEG_MMAP
#define DEF_WIDTH 640
#define DEF_HEIGHT 480

void
usage (char *pname)
{
	fprintf (stderr,
	"Usage: %s <options>\n"
	" -d <device>          video device (default: /dev/video)\n"
	" -o <file>            write output to file instead of stdout\n"
	" -s NxN               define size of the output image (default:"
		" %dx%d)\n"
	" -n <num>             number of frames to grab\n"
	"Example: %s -d /dev/video0 -o imagefile -s 640x480 -n 10\n",
	(char*)basename(pname), DEF_WIDTH, DEF_HEIGHT, (char*)basename(pname));
	exit (1);
}

int main(int argc, char **argv)
{
	FILE *ofid;
	int ifid;
	ssize_t count;
	int num_frames=1, frame=0;
#ifndef GETJPEG_MMAP
	unsigned char buffer[524288];
#endif
	char default_dev[] = "/dev/video";
	char *device = default_dev;
	char default_file_templ[] = "image";
	char *file_templ = default_file_templ;
	char filename[255];

	char *vmbuffer = NULL;
	unsigned short *vmpointer = NULL;
	struct video_mbuf vmbuf;
	struct video_mmap vmmap;
	int c;
	int width = DEF_WIDTH, height = DEF_HEIGHT;

	while ((c = getopt (argc, argv, "d:o:s:n:")) != EOF) {
		switch (c) {
			case 'd': /* change default device */
				device = optarg;
				break;
			case 'o':
				file_templ = optarg;
				break;
			case 'n':
				sscanf (optarg, "%d", &num_frames);
				break;
			case 's':
				sscanf (optarg, "%dx%d", &width, &height);
				break;
			default:
				usage (argv[0]);
				break;
		}
	}

//	if ((ifid = open(device, O_RDWR)) < 0) {
//		printf("Could not open device %s\n", device);
//		return -1;
//	}
//	sleep(1);
//	count = read(ifid, buffer, sizeof(buffer));
//	close(ifid);
//sleep(1);

	if ((ifid = open(device, O_RDWR)) < 0) {
		printf("Could not open device %s\n", device);
		return -1;
	}

#ifdef GETJPEG_MMAP
	/* setup everything */
	ioctl(ifid, VIDIOCGMBUF, &vmbuf);
	//printf("VMBUF: size=%d, frames=%d, offset[0]=%d, offset[1]=%d\n",
	//		vmbuf.size, vmbuf.frames, vmbuf.offsets[0], vmbuf.offsets[1]);

	vmbuffer = mmap(0, vmbuf.size, PROT_READ, MAP_SHARED, ifid, 0);
	if (!(vmbuffer))
		perror("mmap failed");

	vmmap.format = VIDEO_PALETTE_YUV420P;
	vmmap.frame = 0;
	vmmap.width = width;
	vmmap.height = height;

	if (ioctl (ifid, VIDIOCMCAPTURE, &vmmap) == -1) {
		perror ("VIDIOCMCAPTURE(0)");
	}
	vmmap.frame = 1;

	for (frame = 0; frame < num_frames; frame++) {

		if (ioctl (ifid, VIDIOCMCAPTURE, &vmmap) == -1) {
			perror ("VIDIOCMCAPTURE()");
		}
		vmmap.frame = (vmmap.frame + 1) % vmbuf.frames;
		if (ioctl (ifid, VIDIOCSYNC, &vmmap.frame) == -1) {
			perror ("VIDIOCSYNC()");
		}

		/* process frame while the hardware captures next frame */
		sprintf(filename, "%s-%02d.jpg", file_templ, frame);

		if ((ofid = fopen(filename, "wb")) == NULL) {
			printf("Could not open file %s for writing\n", filename);
			return -1;
		}
		// The length / 8 of the JPEG is stored in the first 2 bytes of vmbuffer
		vmpointer = (unsigned short *)(vmbuffer+vmbuf.offsets[vmmap.frame]);
		count = (ssize_t)((unsigned int)(vmpointer[0])<<3);

		if (fwrite(vmbuffer+vmbuf.offsets[vmmap.frame] + 2, sizeof(unsigned char), (size_t)count, ofid) == count) {
			printf("Frame %d, %d bytes... success\n", frame, (int)count);
		}
		fclose(ofid);

	}

	munmap (vmbuffer, vmbuf.size);

#else // v4l_read

	for (frame = 0; frame < num_frames; frame++) {
		count = read(ifid, buffer, sizeof(buffer)) - 2; // Delete the 2 bytes of length information

		sprintf(filename, "%s-%02d.jpg", file_templ, frame);

		if ((ofid = fopen(filename, "wb")) == NULL) {
			printf("Could not open file %s for writing\n", filename);
			return -1;
		}
		if (fwrite(buffer + 2, sizeof(unsigned char), (size_t)count, ofid) == count) {
			printf("Frame %d, %d bytes... success\n", frame, (int)count);
		}
		fclose(ofid);
	}

#endif
	close(ifid);

	return 0;
}
