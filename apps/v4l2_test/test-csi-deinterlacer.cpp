#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <malloc.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <mach/nxp-deinterlacer.h>

#include <ion.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <linux/videodev2_nxp_media.h>

#include <nxp-v4l2.h>

#define V4L2_CID_CAMERA_MODE_CHANGE     (V4L2_CTRL_CLASS_CAMERA | 0x1003)

#include "common.cpp"

#define SRC_WIDTH     768
#define SRC_HEIGHT   	240

#define SRC_Y_STRIDE  768
#define SRC_C_STRIDE  384

#define DST_Y_STRIDE  768
#define DST_C_STRIDE  384

#define LOC_SRC_BUFFER_COUNT	4
//#define LOC_DST_BUFFER_COUNT	2

static char device_name[] = "/dev/deinterlacer";
static int g_fd = 0;
static int g_idx = 0;
static long g_frame_cnt = 0;
//static long g_frame_num = 0;

frame_data_info g_frame_info;

int start_deinterlacer(frame_data_info **frame_info, struct nxp_vid_buffer **frame_first, struct nxp_vid_buffer **frame_second);

int save_file(const char *file_name, unsigned char *buf, long file_size)
{
  FILE *hSaveFile = NULL;
  
  if((hSaveFile = fopen(file_name, "wb")) == NULL)
  {
    printf("Error Opening File.\n");
    return -1; 
  }

  fwrite(buf, 1, file_size, hSaveFile); 

  if( hSaveFile ) fclose(hSaveFile);

  return 0;
}

int alloc_dst_data(unsigned char **dst, unsigned char **src, int dst_size, int src_size, int dst_stride_val, int src_stride_val)
{
	int i=0;
	int src_data_count = 0;
	int dst_data_size = 0;

	unsigned char *dst_real_data = NULL;
	unsigned char **src_data = NULL;
	unsigned char **dst_data = NULL;

	int dst_stride = 0;
	int src_stride = 0;

	int src_data_size = 0;

	src_data = src;
	src_data_size = src_size;
	dst_data = dst;

	dst_data_size = dst_size;
	dst_stride = dst_stride_val;
	src_stride = src_stride_val;

	src_data_count = (src_data_size/dst_stride);
	dst_real_data = (unsigned char *)malloc(dst_data_size);

	if( !dst_real_data )
	{
		printf("Real data zalloc allocation error!!\n");
		return -1;
	} 

	for(i=0; i<src_data_count; i++)
		memcpy(dst_real_data+(i * src_stride), *src_data+(i * dst_stride), src_stride);
	
	*dst_data = dst_real_data;

	return 0;
}

void dealloc_dst_data(unsigned char **dst)
{
	unsigned char **dst_data;

	dst_data = dst;

	if( *dst_data )
	{
		free(*dst_data);
		*dst_data = NULL;
	}
}

unsigned int get_deinterlacing_size(int format, int plane_type, int y_cb_cr, int width, int height, int align_factor, int plane_kind)
{
  int stride=0;
  int size=0;

  switch (format) {
    case FMT_YUV420M:
      if(plane_type == FRAME_DST)
      {
        if(y_cb_cr == Y_FRAME)
        {
          stride = ALIGN(width, align_factor);
          size = stride * height * 2;
        }
        else
        {
          if(plane_kind == PLANE3)
          {
						width = (width/2);
            stride = ALIGN(width, align_factor);
            size = stride * (height/2) * 2;
          }
          else
            size = (width * height);
        }
      }
      else
      {
        if(y_cb_cr == Y_FRAME)
            size = width * height;
        else
            if(plane_kind == PLANE3)
              size = (width * height) >> 2;
            else
              size = (width * height);
      }
      break;

    default:
      break;
  }

  return size;
}

int alloc_deinterlacing_buffers(int ion_fd, int count, frame_data *bufs, int width, int height, int format, int plane_kind)
{
	int ret;
	int i, j;
	frame_data *buffer;
	int plane_num;
	
	plane_num = bufs->plane_num;

	for (i = 0; i < count; i++) {
		buffer = &bufs[i];
		for (j = 0; j < plane_num; j++) {
			switch( plane_kind )
			{
			case PLANE3: 
				buffer->plane3.sizes[j] = get_deinterlacing_size(format, buffer->frame_type, j, width, height, buffer->frame_factor, plane_kind);

				ret = ion_alloc_fd(ion_fd, buffer->plane3.sizes[j], 0, ION_HEAP_NXP_CONTIG_MASK, 0, &buffer->plane3.fds[j]);
				if (ret < 0) {
					fprintf(stderr, "failed to ion_alloc_fd()\n");
					return ret;
				}

				buffer->plane3.virt[j] = (unsigned char *)mmap(NULL, buffer->plane3.sizes[j], PROT_READ | PROT_WRITE, MAP_SHARED, buffer->plane3.fds[j], 0); 
				if (!buffer->plane3.virt[j]) {
					fprintf(stderr, "failed to mmap\n");
					return ret;
				}

				ret = ion_get_phys(ion_fd, buffer->plane3.fds[j], &buffer->plane3.phys[j]);
				if (ret < 0) {
					fprintf(stderr, "failed to get phys\n");
					return ret;
				}
				break;
			case PLANE2:
				buffer->plane2.sizes[j] = get_deinterlacing_size(format, buffer->frame_type, j, width, height, buffer->frame_factor, plane_kind);
				ret = ion_alloc_fd(ion_fd, buffer->plane2.sizes[j], 0, ION_HEAP_NXP_CONTIG_MASK, 0, &buffer->plane2.fds[j]);
				if (ret < 0) {
					fprintf(stderr, "failed to ion_alloc_fd()\n");
					return ret;
				}

				buffer->plane2.virt[j] = (unsigned char *)mmap(NULL, buffer->plane2.sizes[j], PROT_READ | PROT_WRITE, MAP_SHARED, buffer->plane2.fds[j], 0); 
				if (!buffer->plane2.virt[j]) {
					fprintf(stderr, "failed to mmap\n");
					return ret;
				}

				ret = ion_get_phys(ion_fd, buffer->plane2.fds[j], &buffer->plane2.phys[j]);
				if (ret < 0) {
					fprintf(stderr, "failed to get phys\n");
					return ret;
				}
				break;
			}
		}
	}

	return 0;
}

int open_deinterlacer(void)
{
	int fd=0;

	if( fd<=0 )
	{
		fd = open(device_name, O_RDWR);
		if( fd == -1)
		{
			perror("device open error!");
			return -1;
		}	
	}

	return fd;
}

int close_deinterlacer(void)
{
	if( g_fd > 0 )
	{
		close(g_fd);
		g_fd = 0;

		return 1;
	}

	return 0;	
}

int set_frame_context(int ion_fd, frame_data_info *frame_info)
{
	int i=0;
	int j=0;
	int src_buf_count=0;
	int dst_buf_count=0;
	int format;

	//1. set value
	format 									= FMT_YUV420M;
	frame_info->command			=	ACT_DIRECT;
	frame_info->width				=	SRC_WIDTH;
	frame_info->height			=	SRC_HEIGHT;
	frame_info->plane_mode	= PLANE3;

	src_buf_count = LOC_SRC_BUFFER_COUNT;
	dst_buf_count = DST_BUFFER_COUNT;

	// 1-1 src frame set
	for(i=0; i<src_buf_count; i++)
	{
		frame_info->src_bufs[i].frame_num			=	0;
		frame_info->src_bufs[i].plane_num			=	frame_info->plane_mode;
		frame_info->src_bufs[i].frame_type		=	FRAME_SRC;
		frame_info->src_bufs[i].frame_factor	=	1;

		for(j=0; j<frame_info->plane_mode; j++)
		{
   		if(j == 0)
      {
				switch( frame_info->plane_mode )
				{
					case PLANE3:
						frame_info->src_bufs[i].plane3.src_stride[j] = SRC_Y_STRIDE;
						break;
					case PLANE2:
						frame_info->src_bufs[i].plane2.src_stride[j] = SRC_Y_STRIDE;
						break;
				}
      }
      else
      {
				switch( frame_info->plane_mode )
				{
					case PLANE3:
						frame_info->src_bufs[i].plane3.src_stride[j] = SRC_C_STRIDE;
						break;
					case PLANE2:
						frame_info->src_bufs[i].plane2.src_stride[j] = SRC_C_STRIDE;
						break;
				}
			}
		}
	}
	
	//1-2. dst frame set
	for(i=0 ; i<dst_buf_count; i++) 
  { 
    frame_info->dst_bufs[i].frame_num     = 0; 
    frame_info->dst_bufs[i].plane_num     = frame_info->plane_mode; 
    frame_info->dst_bufs[i].frame_type    = FRAME_DST; 
    frame_info->dst_bufs[i].frame_factor  = YUV_STRIDE_ALIGN_FACTOR; 
 
    for(j=0 ; j<frame_info->plane_mode; j++) 
    { 
      if(j == 0) 
      { 
				switch( frame_info->plane_mode )
				{
					case PLANE3:
						frame_info->dst_bufs[i].plane3.dst_stride[j] = DST_Y_STRIDE;
						break;
					case PLANE2:
						frame_info->dst_bufs[i].plane2.dst_stride[j] = DST_Y_STRIDE;
						break;
				}
			}
      else 
      { 
				switch( frame_info->plane_mode )
				{
					case PLANE3:
						frame_info->dst_bufs[i].plane3.dst_stride[j] = DST_C_STRIDE;
						break;
					case PLANE2:
						frame_info->dst_bufs[i].plane2.dst_stride[j] = DST_C_STRIDE; 
						break;
				}
      } 
    } 
  } 

	//2. alloc_deinterlacing_buffers
	alloc_deinterlacing_buffers(ion_fd, dst_buf_count, frame_info->dst_bufs, frame_info->width, frame_info->height, format, frame_info->plane_mode); 

	return 0;
}

int set_frame_data(frame_data_info *frame_info, struct nxp_vid_buffer *buf, struct nxp_vid_buffer *frame_first, struct nxp_vid_buffer *frame_second)
{
	int y_size=0;
	int cb_size=0;
	int cr_size=0;

	y_size	= buf->sizes[0]/2;
	cb_size = buf->sizes[1]/2;
	cr_size = buf->sizes[2]/2;

	switch( frame_info->plane_mode )
	{
		case PLANE3:
			if( g_idx>0 ) g_idx = 2;

			frame_info->src_bufs[g_idx].frame_num = 1;
			frame_info->src_bufs[g_idx].plane3.phys[0]	= buf->phys[0]; //Y
			frame_info->src_bufs[g_idx].plane3.sizes[0] = get_deinterlacing_size(FMT_YUV420M, FRAME_SRC, Y_FRAME, frame_info->width, frame_info->height, frame_info->src_bufs[g_idx].frame_factor, PLANE3);
			frame_info->src_bufs[g_idx].plane3.virt[0] 	= (unsigned char *)buf->virt[0];

			frame_info->src_bufs[g_idx].plane3.phys[1] 	= buf->phys[1]; //CB
			frame_info->src_bufs[g_idx].plane3.sizes[1] = get_deinterlacing_size(FMT_YUV420M, FRAME_SRC, CB_FRAME, frame_info->width, frame_info->height, frame_info->src_bufs[g_idx].frame_factor, PLANE3);
			frame_info->src_bufs[g_idx].plane3.virt[1] 	= (unsigned char *)buf->virt[1];

			frame_info->src_bufs[g_idx].plane3.phys[2] 	= buf->phys[2]; //CR
			frame_info->src_bufs[g_idx].plane3.sizes[2] = get_deinterlacing_size(FMT_YUV420M, FRAME_SRC, CR_FRAME, frame_info->width, frame_info->height, frame_info->src_bufs[g_idx].frame_factor, PLANE3);
			frame_info->src_bufs[g_idx].plane3.virt[2] 	= (unsigned char *)buf->virt[2];

			g_idx++;

			frame_info->src_bufs[g_idx].frame_num = 0;
			frame_info->src_bufs[g_idx].plane3.phys[0] 	= buf->phys[0] + y_size; //Y
			frame_info->src_bufs[g_idx].plane3.sizes[0] = get_deinterlacing_size(FMT_YUV420M, FRAME_SRC, Y_FRAME, frame_info->width, frame_info->height, frame_info->src_bufs[g_idx].frame_factor, PLANE3);
			frame_info->src_bufs[g_idx].plane3.virt[0] 	= (unsigned char *)buf->virt[0] + y_size;;

			frame_info->src_bufs[g_idx].plane3.phys[1] 	= buf->phys[1] + cb_size; //CB
			frame_info->src_bufs[g_idx].plane3.sizes[1] = get_deinterlacing_size(FMT_YUV420M, FRAME_SRC, CB_FRAME, frame_info->width, frame_info->height, frame_info->src_bufs[g_idx].frame_factor, PLANE3);
			frame_info->src_bufs[g_idx].plane3.virt[1] 	= (unsigned char *)buf->virt[1] + cb_size;

			frame_info->src_bufs[g_idx].plane3.phys[2] 	= buf->phys[2] + cr_size; //CR
			frame_info->src_bufs[g_idx].plane3.sizes[2] = get_deinterlacing_size(FMT_YUV420M, FRAME_SRC, CR_FRAME, frame_info->width, frame_info->height, frame_info->src_bufs[g_idx].frame_factor, PLANE3);
			frame_info->src_bufs[g_idx].plane3.virt[2] 	= (unsigned char *)buf->virt[2] + cr_size;

			g_frame_cnt++;
			break;
		case PLANE2:
			break;
	}


	if( g_idx >= 2 )
	{
		start_deinterlacer(&frame_info, &frame_first, &frame_second);
	}

	return 1;
}

int start_deinterlacer(frame_data_info **frame_info, struct nxp_vid_buffer **frame_first, struct nxp_vid_buffer **frame_second)
{
	int fd;
	int i=0;

	unsigned char *dst_y_real_data = NULL;
	unsigned char *dst_cb_real_data = NULL;
	unsigned char *dst_cr_real_data = NULL;

	unsigned char *dst_y_data = NULL;
	unsigned char *dst_cb_data = NULL;
	unsigned char *dst_cr_data = NULL;

	int dst_y_data_size = 0;
	int dst_cb_data_size = 0;
	int dst_cr_data_size = 0;
	
	int dst_y_stride = 0;
	int dst_c_stride = 0;

	int dst_real_y_size = 0;
	int dst_real_c_size = 0;

	//1. device open
	if( g_fd <= 0 )
		g_fd = fd = open_deinterlacer();
	else
		fd = g_fd;

	(*frame_info)->dst_idx = 0;

	//2. first deinterlacing
	if(ioctl(fd, IOCTL_DEINTERLACE_SET_AND_RUN, *frame_info) == -1)
  {
    perror("[error]first deinterlacing ioctl set");
		return -1;
  }

	(*frame_first)->plane_num	= 3; 

	dst_y_data 	= (*frame_info)->dst_bufs[0].plane3.virt[0];
	dst_cb_data = (*frame_info)->dst_bufs[0].plane3.virt[1];
	dst_cr_data = (*frame_info)->dst_bufs[0].plane3.virt[2];

	dst_y_data_size = (int)(*frame_info)->dst_bufs[0].plane3.sizes[0]; 
	dst_cb_data_size = (int)(*frame_info)->dst_bufs[0].plane3.sizes[1]; 
	dst_cr_data_size = (int)(*frame_info)->dst_bufs[0].plane3.sizes[2]; 

	dst_y_stride = (int)(*frame_info)->dst_bufs[0].plane3.dst_stride[0];
	dst_c_stride = (int)(*frame_info)->dst_bufs[0].plane3.dst_stride[1];

	dst_real_y_size = ((*frame_info)->width * (*frame_info)->height)*2;
	dst_real_c_size = (((*frame_info)->width/2) * ((*frame_info)->height)/2)*2;

	alloc_dst_data(&dst_y_real_data, &dst_y_data, dst_real_y_size, dst_y_data_size, dst_y_stride, (*frame_info)->width);
	alloc_dst_data(&dst_cb_real_data, &dst_cb_data, dst_real_c_size, dst_cb_data_size, dst_c_stride, ((*frame_info)->width/2));
	alloc_dst_data(&dst_cr_real_data, &dst_cr_data, dst_real_c_size, dst_cr_data_size, dst_c_stride, ((*frame_info)->width/2));

	memcpy((*frame_info)->dst_bufs[0].plane3.virt[0],	(char *)dst_y_real_data, dst_real_y_size);
	(*frame_info)->dst_bufs[0].plane3.sizes[0] = dst_real_y_size;	

	memcpy((*frame_info)->dst_bufs[0].plane3.virt[1],	(char *)dst_cb_real_data, dst_real_c_size);
	(*frame_info)->dst_bufs[0].plane3.sizes[1] = dst_real_c_size;	

	memcpy((*frame_info)->dst_bufs[0].plane3.virt[2],	(char *)dst_cr_real_data, dst_real_c_size);
	(*frame_info)->dst_bufs[0].plane3.sizes[2] = dst_real_c_size;	

	dealloc_dst_data(&dst_y_real_data);
	dealloc_dst_data(&dst_cb_real_data);
	dealloc_dst_data(&dst_cr_real_data);

	for(i=0; i<MAX_BUFFER_PLANES ; i++)
	{
		(*frame_first)->fds[i] 	= (*frame_info)->dst_bufs[0].plane3.fds[i];
		(*frame_first)->phys[i]	= (*frame_info)->dst_bufs[0].plane3.phys[i];
		(*frame_first)->virt[i]	= (char *)(*frame_info)->dst_bufs[0].plane3.virt[i];
		(*frame_first)->sizes[i]	= (int)(*frame_info)->dst_bufs[0].plane3.sizes[i];
	}

#if 1
	//3. second data sorting
  (*frame_info)->src_bufs[0] = (*frame_info)->src_bufs[1];
	(*frame_info)->src_bufs[1] = (*frame_info)->src_bufs[2];
	(*frame_info)->src_bufs[2] = (*frame_info)->src_bufs[3];

	//memcpy(&g_frame_info, (*frame_info), sizeof(frame_data_info));
	g_frame_info = **frame_info;

#if 1
	//4. second deinterlacing
	//g_frame_info.dst_idx = 1;

	if(ioctl(fd, IOCTL_DEINTERLACE_SET_AND_RUN, &g_frame_info) == -1)
  {
    perror("[error]first deinterlacing ioctl set");
		return -1;
  }
#endif
	//6. 
	(*frame_info)->src_bufs[0] = (*frame_info)->src_bufs[1];
	(*frame_info)->src_bufs[1] = (*frame_info)->src_bufs[2];

#if 1
	alloc_dst_data(&dst_y_real_data, &dst_y_data, dst_real_y_size, dst_y_data_size, dst_y_stride, (*frame_info)->width);
	alloc_dst_data(&dst_cb_real_data, &dst_cb_data, dst_real_c_size, dst_cb_data_size, dst_c_stride, ((*frame_info)->width/2));
	alloc_dst_data(&dst_cr_real_data, &dst_cr_data, dst_real_c_size, dst_cr_data_size, dst_c_stride, ((*frame_info)->width/2));

	memcpy(g_frame_info.dst_bufs[0].plane3.virt[0],	(char *)dst_y_real_data, dst_real_y_size);
	g_frame_info.dst_bufs[0].plane3.sizes[0] = dst_real_y_size;	

	memcpy(g_frame_info.dst_bufs[0].plane3.virt[1],	(char *)dst_cb_real_data, dst_real_c_size);
	g_frame_info.dst_bufs[0].plane3.sizes[1] = dst_real_c_size;	

	memcpy(g_frame_info.dst_bufs[0].plane3.virt[2],	(char *)dst_cr_real_data, dst_real_c_size);
	g_frame_info.dst_bufs[0].plane3.sizes[2] = dst_real_c_size;	

	dealloc_dst_data(&dst_y_real_data);
	dealloc_dst_data(&dst_cb_real_data);
	dealloc_dst_data(&dst_cr_real_data);

	//7. set return data
	(*frame_second)->plane_num	= 3;
	for(i=0; i<MAX_BUFFER_PLANES ; i++)
	{
		(*frame_second)->fds[i] 	= g_frame_info.dst_bufs[0].plane3.fds[i];
		(*frame_second)->phys[i]	= g_frame_info.dst_bufs[0].plane3.phys[i];
		(*frame_second)->virt[i]	= (char *)g_frame_info.dst_bufs[0].plane3.virt[i];
		(*frame_second)->sizes[i]= (int)g_frame_info.dst_bufs[0].plane3.sizes[i];
	}
#endif
#endif

	return 0;
}

int stop_deinterlacer(void)
{
	close_deinterlacer();

	return 0;
}

// # ./test module w h
int main(int argc, char *argv[])
{
    int ion_fd = ion_open();
    int width;
    int height;
    int clipper_id = nxp_v4l2_clipper0;
    int sensor_id = nxp_v4l2_sensor0;
    int video_id = nxp_v4l2_mlc0_video;
    int format = V4L2_PIX_FMT_YUV420M; // yuv420
    int module;
    // int format = V4L2_PIX_FMT_YUV422P; // yuv422
		frame_data_info frame_info;

    if (ion_fd < 0) {
        fprintf(stderr, "can't open ion!!!\n");
        return -EINVAL;
    }

    if (argc >= 4) {
        module = atoi(argv[1]);
        width = atoi(argv[2]);
        height = atoi(argv[3]);
    } else {
        printf("usage: ./test module width height\n");
        return 0;
    }

    struct V4l2UsageScheme s;
    memset(&s, 0, sizeof(s));

    if (module == 0) {
        s.useClipper0 = true;
        s.useDecimator0 = true;
        clipper_id = nxp_v4l2_clipper0;
        sensor_id = nxp_v4l2_sensor0;
    } else {
        s.useClipper1 = true;
        s.useDecimator1 = true;
        clipper_id = nxp_v4l2_clipper1;
        sensor_id = nxp_v4l2_sensor1;
    }
    s.useMlc0Video = true;

    printf("width %d, height %d, module %d\n", width, height, module);

    CHECK_COMMAND(v4l2_init(&s));

    CHECK_COMMAND(v4l2_set_format(clipper_id, width, height, format));
    CHECK_COMMAND(v4l2_set_crop(clipper_id, 0, 0, width, height));
    CHECK_COMMAND(v4l2_set_format(sensor_id, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mipicsi, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_set_format(video_id, width, height, format));

		// setting destination position
    CHECK_COMMAND(v4l2_set_crop(video_id, 0, 0, width, height));
    // setting source crop
    CHECK_COMMAND(v4l2_set_crop_with_pad(video_id, 2, 0, 0, width, height)); //psw 20150331

    CHECK_COMMAND(v4l2_set_ctrl(video_id, V4L2_CID_MLC_VID_PRIORITY, 0));
    CHECK_COMMAND(v4l2_set_ctrl(video_id, V4L2_CID_MLC_VID_COLORKEY, 0x0));

    CHECK_COMMAND(v4l2_reqbuf(clipper_id, MAX_BUFFER_COUNT));
    CHECK_COMMAND(v4l2_reqbuf(video_id, MAX_BUFFER_COUNT));

    struct nxp_vid_buffer bufs[MAX_BUFFER_COUNT];
    CHECK_COMMAND(alloc_buffers(ion_fd, MAX_BUFFER_COUNT, bufs, width, height, format));

    int i;
    for (i = 0; i < MAX_BUFFER_COUNT; i++) {
        struct nxp_vid_buffer *buf = &bufs[i];
        CHECK_COMMAND(v4l2_qbuf(clipper_id, buf->plane_num, i, buf, -1, NULL));
    }

    CHECK_COMMAND(v4l2_streamon(clipper_id));

    int out_index = 0;
    int out_dq_index = 0;
    int out_q_count = 0;
    bool started_out = false;
    //int j;
    int capture_index = 0;
    int count = 10000;
		struct nxp_vid_buffer frame_first;
		struct nxp_vid_buffer frame_second;
		int err=0;

		set_frame_context(ion_fd, &frame_info);
		set_frame_context(ion_fd, &g_frame_info);

		struct nxp_vid_buffer *buf = &bufs[0];
    CHECK_COMMAND(v4l2_dqbuf(clipper_id, buf->plane_num, &capture_index, NULL));
		buf = &bufs[capture_index];

		set_frame_data(&frame_info, buf, &frame_first, &frame_second);

   // while (count >= 0) {
    while (1) {
        CHECK_COMMAND(v4l2_dqbuf(clipper_id, buf->plane_num, &capture_index, NULL));
        buf = &bufs[capture_index];
				set_frame_data(&frame_info, buf, &frame_first, &frame_second);
				
        err = v4l2_qbuf(video_id, frame_first.plane_num, out_index, &frame_first, -1, NULL);
				//printf("error first: %d\n", err);
		    out_q_count++;
       	out_index++;
        out_index %= MAX_BUFFER_COUNT;

				if (out_q_count >= 2) {
            CHECK_COMMAND(v4l2_dqbuf(video_id, buf->plane_num, &out_dq_index, NULL));
            out_q_count--;
        }

			 if (!started_out) {
            CHECK_COMMAND(v4l2_streamon(video_id));
            started_out = true;
        }

#if 1
        err = v4l2_qbuf(video_id, frame_second.plane_num, out_index, &frame_second, -1, NULL);
				//printf("error second : %d\n", err);
        out_q_count++;
        out_index++;
        out_index %= MAX_BUFFER_COUNT;

       
//        if (out_q_count >= MAX_BUFFER_COUNT) {
        if (out_q_count >= 2) {
            CHECK_COMMAND(v4l2_dqbuf(video_id, buf->plane_num, &out_dq_index, NULL));
            out_q_count--;
        }
#endif
        CHECK_COMMAND(v4l2_qbuf(clipper_id, buf->plane_num, capture_index, buf, -1, NULL));

        count--;
    }

    CHECK_COMMAND(v4l2_streamoff(video_id));
    CHECK_COMMAND(v4l2_streamoff(clipper_id));
    v4l2_exit();
    close(ion_fd);
		close_deinterlacer();

    return 0;
}
