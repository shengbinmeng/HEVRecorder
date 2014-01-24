#ifndef __LENTHEVCENC_H__
#define __LENTHEVCENC_H__


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef LENTAPI
	#if defined(_WIN32) || defined(WIN32)
		#define LENTAPI __stdcall
	#else
		#define LENTAPI
	#endif
#endif

	/* encoder context */
	typedef void* lenthevcenc_ctx;

	/* parameters for create encoder */
	enum lenthevcenc_preset {
		LENTHEVCENC_Preset_UltraFast = 0,
		LENTHEVCENC_Preset_Fast,
		LENTHEVCENC_Preset_Medium,
		LENTHEVCENC_Preset_Slow,
		LENTHEVCENC_Preset_UltraSlow
	};
	enum lenthevcenc_rate_control_mode {
		LENTHEVCENC_RCMODE_CQP = 0,
		LENTHEVCENC_RCMODE_ABR
	};
	typedef struct lenthevcenc_create_param {
		int32_t size; /* size in byte of this struct, initialize by caller for expand */
		int32_t width; /* width of video picture */
		int32_t height; /* height of video picture */
		int32_t compatibility; /* 91=HM9.1 100=HM10 120=HM12 */
		int32_t preset; /* enum lenthevcenc_preset */
		int32_t frame_threads; /* >=1 */
		int32_t wpp_threads; /* >=1 */
		int32_t idr_period_max; /* >=1 */
		int32_t idr_period_min; /* >=1 */
		int32_t rc_mode; /* enum lenthevcenc_rate_control_mode */
		/* for CQP mode */
		int32_t qp;	/* 0~51 */
		/* for ABR mode */
		int32_t bitrate; /* kbps */
		int32_t fps_num; /* double FPS = (double)fps_num / (double)fps_den */
		int32_t fps_den;
		/* miscellaneous */
		int32_t log_level; /* 0: no log output; 1: output PSNR */
		int32_t sei_flag; /* 0: no SEI output; 1: output SEI before VPS in bitstream of key frame */
	} lenthevcenc_create_param;

	/* parameters for encode frame */
	enum lenthevcenc_frame_type {
		LENTHEVCENC_Frame_Auto = 0,
		LENTHEVCENC_Frame_IDR = 1,
		LENTHEVCENC_Frame_CDR = 2,
		LENTHEVCENC_Frame_I = 3,
		LENTHEVCENC_Frame_P = 4,
		LENTHEVCENC_Frame_BREF = 5,
		LENTHEVCENC_Frame_B = 6
	};
	typedef struct lenthevcenc_encode_param {
		int32_t size; /* size in byte of this struct, initialize by caller for expand */
		int32_t frame_type; /* enum lenthevcenc_frame_type */
	} lenthevcenc_encode_param;

	/* Get version of this library 
	 * e.g. return 0x02010013 for version 2.1.0.19
	 */
	uint32_t        LENTAPI lenthevcenc_version(void);

	/* Get default parameters 
	 * param->size must initialize to the real size of the struct of param before call this function
	 */
	int				LENTAPI lenthevcenc_default_param(lenthevcenc_create_param* param);

	/* Create encoder 
	 * return the context of the created encoder, otherwise return NULL if failed
	 */
	lenthevcenc_ctx LENTAPI lenthevcenc_create(lenthevcenc_create_param* param);

	/* Get header data ( VPS & SPS & PPS ) from current encoder
	 * 'buf' is the memory buffer for return header data, 'buf_size' is the size of buffer 'buf' in byte
	 * if the buf size is letter than the header data, function return negative number, the absolute value of the return is the size of the header data
	 */
	int				LENTAPI lenthevcenc_get_header(lenthevcenc_ctx ctx, uint8_t* buf, int buf_size);

	/* Close encoder and release any resource of the encoder
	 */
	void            LENTAPI lenthevcenc_destroy(lenthevcenc_ctx ctx);

	/* return 1 if in process of encoding, otherwise return 0 */
	int             LENTAPI lenthevcenc_is_encoding(lenthevcenc_ctx ctx);

	/* Input one frame's pixel data to encoder, and get one frame's bitstream from encoder if any frame has been encoded
	 * in_pic_stride[3]: the line stride of the input buffer
	 * in_pic_plane[3]: the pixel data of three 8-bit plane ( YUV420 )
	 * in_pts: the play timestamp of the input frame
	 * param: parameters of frame encoding, NULL for default parameters
	 * out_bs_ptr: the memory address for the output bitstream of output frame
	 * out_pts: the play timestamp of the output frame
	 * out_pic_stride[3]: the line stride of the reconstructed picture of the output frame
	 * out_pic_plane[3]: the pixel data of three 8-bit plane of the reconstructed picture of the output frame ( YUV420 )
	 * return the length in byte of output bitstream; if no frame output, return 0; if call failed, return negative number
	 */
	int             LENTAPI lenthevcenc_encode_frame(lenthevcenc_ctx ctx,
							int in_pic_stride[3], void* in_pic_plane[3], int64_t in_pts,
							lenthevcenc_encode_param* param,
							void** out_bs_ptr, int64_t* out_pts,
							int out_pic_stride[3], void* out_pic_plane[3]);


#ifdef __cplusplus
}
#endif

#endif/*__LENTHEVCENC_H__*/
