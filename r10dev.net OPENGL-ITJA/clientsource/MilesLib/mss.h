#ifndef __MSS_H__
#define __MSS_H__

// Android / OpenAL stub for Miles Sound System
#ifdef __ANDROID__
typedef unsigned int U32;
typedef int S32;
typedef unsigned short U16;
typedef short S16;
typedef unsigned char U8;
typedef signed char S8;
typedef float F32;
typedef int HPROVIDER;
typedef int H3DPOBJECT;
#define AILCALLBACK
#define FILE_READ_WITH_SIZE 1
typedef int HSAMPLE;
typedef int HSTREAM;
typedef int H3DSAMPLE;
typedef int HDIGDRIVER;
#define AIL_shutdown() {}
#define AIL_set_redist_directory(x) {}
#define AIL_startup() {}
#define AIL_file_read(x,y) 0
#define AIL_file_type(x,y) 0
#define AIL_WAV_info(x,y) {}
#define AIL_decompress_ADPCM(x,y,z) 0
#define AIL_mem_free_lock(x) {}
#define AIL_decompress_ASI(a,b,c,d,e,f) 0
#define AIL_set_file_callbacks(a,b,c,d) {}
#define AIL_release_sample_handle(x) {}
#define AIL_allocate_sample_handle(x) 0
#define AIL_init_sample(x) {}
#define AIL_set_sample_file(x,y,z) 0
#define AIL_last_error() "No Error"
#define AIL_sample_status(x) 0
#define AIL_set_sample_loop_count(x,y) {}
#define AIL_start_sample(x) {}
#define AIL_stop_sample(x) {}
#define AIL_resume_sample(x) {}
#define AIL_end_sample(x) {}
#define AIL_sample_volume_pan(x,y,z) {}
#define AIL_set_sample_volume_pan(x,y,z) {}

#define AIL_release_3D_sample_handle(x) {}
#define AIL_allocate_3D_sample_handle(x) 0
#define AIL_set_3D_sample_file(x,y) 0
#define AIL_set_3D_position(x,y,z,w) {}
#define AIL_auto_update_3D_position(x,y) {}
#define AIL_3D_sample_status(x) 0
#define AIL_set_3D_sample_loop_count(x,y) {}
#define AIL_start_3D_sample(x) {}
#define AIL_stop_3D_sample(x) {}
#define AIL_resume_3D_sample(x) {}
#define AIL_end_3D_sample(x) {}
#define AIL_3D_sample_volume(x) 0
#define AIL_set_3D_sample_volume(x,y) {}
#define AIL_set_3D_orientation(a,b,c,d,e,f,g) {}
#define AIL_set_3D_velocity(x,y,z,w,a) {}
#define AIL_update_3D_position(x,y) {}

#define AIL_close_stream(x) {}
#define AIL_stream_status(x) 0
#define AIL_set_stream_loop_count(x,y) {}
#define AIL_start_stream(x) {}
#define AIL_pause_stream(x,y) {}
#define AIL_stream_volume_levels(x,y,z) {}
#define AIL_set_stream_volume_levels(x,y,z) {}

#define AIL_open_digital_driver(x,y,z,w) 0
#define AIL_close_digital_driver(x) {}
#define AIL_enumerate_3D_providers(x,y,z) 0
#define AIL_open_3D_provider(x) 0
#define AIL_open_3D_listener(x) 0
#define AIL_close_3D_listener(x) {}
#define AIL_close_3D_provider(x) {}
#define AIL_open_stream(a,b,c) 0

#define SMP_DONE 0
#define SMP_PLAYING 1
#define SMP_STOPPED 2
#define SMP_FREE 3

#define SMP_PLAYMULTI 1
#define AIL_FILE_ERROR 0

#define AILFILETYPE_PCM_WAV 1
#define AILFILETYPE_ADPCM_WAV 2
#define AILFILETYPE_MPEG_L3_AUDIO 5
#define AILFILETYPE_MPEG_L2_AUDIO 6
#define AILFILETYPE_MPEG_L1_AUDIO 7
#define AILFILETYPE_RAW_L16 9
#define AILFILETYPE_VOC 12
#define AILFILETYPE_VOX 13
#define AILFILETYPE_AIFF 14
#define AILFILETYPE_XM 15
#define AILFILETYPE_MOD 16
#define AILFILETYPE_S3M 17
#define AILFILETYPE_IT 18
#define AILFILETYPE_SPX 19
#define AILFILETYPE_UNKNOWN 0

typedef struct _AILSOUNDINFO {
    S32 format;
    void* data_ptr;
    U32 data_len;
    U32 rate;
    S32 bits;
    S32 channels;
    U32 samples;
    U32 block_size;
    void* initial_ptr;
} AILSOUNDINFO;

typedef int HPROENUM;
#define HPROENUM_FIRST 0
#define HPROENUM_NEXT 1
#define M3D_NOERR 0

#else
// Windows MSS includes
// #include <mss.h>
#endif

#endif
