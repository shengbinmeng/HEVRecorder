#ifndef __LENTHEVCENC_H__
#define __LENTHEVCENC_H__

#define LENT_BUILD 2.0.2.5
typedef struct LENT_t* LENT_HANDLE;

#define MAX_QUALITY_LAYER 1
#define MAX_SPATIAL_LAYER 1
#define MAX_TEMPORAL_LAYER 1


enum nal_unit_type_e
{
	NAL_UNIT_CODED_SLICE_TRAIL_N = 0,   // 0
	NAL_UNIT_CODED_SLICE_TRAIL_R,   // 1

	NAL_UNIT_CODED_SLICE_TSA_N,     // 2
	NAL_UNIT_CODED_SLICE_TLA,       // 3   // Current name in the spec: TSA_R

	NAL_UNIT_CODED_SLICE_STSA_N,    // 4
	NAL_UNIT_CODED_SLICE_STSA_R,    // 5

	NAL_UNIT_CODED_SLICE_RADL_N,    // 6
	NAL_UNIT_CODED_SLICE_DLP,       // 7 // Current name in the spec: RADL_R

	NAL_UNIT_CODED_SLICE_RASL_N,    // 8
	NAL_UNIT_CODED_SLICE_TFD,       // 9 // Current name in the spec: RASL_R

	NAL_UNIT_RESERVED_10,
	NAL_UNIT_RESERVED_11,
	NAL_UNIT_RESERVED_12,
	NAL_UNIT_RESERVED_13,
	NAL_UNIT_RESERVED_14,
	NAL_UNIT_RESERVED_15,

	NAL_UNIT_CODED_SLICE_BLA,       // 16   // Current name in the spec: BLA_W_LP
	NAL_UNIT_CODED_SLICE_BLANT,     // 17   // Current name in the spec: BLA_W_DLP
	NAL_UNIT_CODED_SLICE_BLA_N_LP,  // 18
	NAL_UNIT_CODED_SLICE_IDR,       // 19  // Current name in the spec: IDR_W_DLP
	NAL_UNIT_CODED_SLICE_IDR_N_LP,  // 20
	NAL_UNIT_CODED_SLICE_CRA,       // 21
	NAL_UNIT_RESERVED_22,
	NAL_UNIT_RESERVED_23,

	NAL_UNIT_RESERVED_24,
	NAL_UNIT_RESERVED_25,
	NAL_UNIT_RESERVED_26,
	NAL_UNIT_RESERVED_27,
	NAL_UNIT_RESERVED_28,
	NAL_UNIT_RESERVED_29,
	NAL_UNIT_RESERVED_30,
	NAL_UNIT_RESERVED_31,

	NAL_UNIT_VPS,                   // 32
	NAL_UNIT_SPS,                   // 33
	NAL_UNIT_PPS,                   // 34
	NAL_UNIT_ACCESS_UNIT_DELIMITER, // 35
	NAL_UNIT_EOS,                   // 36
	NAL_UNIT_EOB,                   // 37
	NAL_UNIT_FILLER_DATA,           // 38
	NAL_UNIT_SEI,                   // 39 Prefix SEI
	NAL_UNIT_SEI_SUFFIX,            // 40 Suffix SEI

	NAL_UNIT_RESERVED_41,
	NAL_UNIT_RESERVED_42,
	NAL_UNIT_RESERVED_43,
	NAL_UNIT_RESERVED_44,
	NAL_UNIT_RESERVED_45,
	NAL_UNIT_RESERVED_46,
	NAL_UNIT_RESERVED_47,
	NAL_UNIT_UNSPECIFIED_48,
	NAL_UNIT_UNSPECIFIED_49,
	NAL_UNIT_UNSPECIFIED_50,
	NAL_UNIT_UNSPECIFIED_51,
	NAL_UNIT_UNSPECIFIED_52,
	NAL_UNIT_UNSPECIFIED_53,
	NAL_UNIT_UNSPECIFIED_54,
	NAL_UNIT_UNSPECIFIED_55,
	NAL_UNIT_UNSPECIFIED_56,
	NAL_UNIT_UNSPECIFIED_57,
	NAL_UNIT_UNSPECIFIED_58,
	NAL_UNIT_UNSPECIFIED_59,
	NAL_UNIT_UNSPECIFIED_60,
	NAL_UNIT_UNSPECIFIED_61,
	NAL_UNIT_UNSPECIFIED_62,
	NAL_UNIT_UNSPECIFIED_63,
	NAL_UNIT_INVALID,
};

enum LENT_pic_type_e
{
	LENT_PIC_PENDING = 0,
	LENT_PIC_IDR,
	LENT_PIC_CDR,  //not support currently
	LENT_PIC_I,
	LENT_PIC_P,
	LENT_PIC_BREF,
	LENT_PIC_B,
	LENT_PIC_TYPE_COUNT
};

enum LENT_preset_e
{
	LENT_PRESET_ULTRAFAST = 0,
	LENT_PRESET_FAST,
	LENT_PRESET_MEDIUM,
	LENT_PRESET_SLOW,
	LENT_PRESET_ULTRASLOW,
	LENT_PRESET_COUNT
};

#define LENT_ME_HEX           0
#define LENT_ME_UMH           1
#define LENT_RC_CQP           0
#define LENT_RC_ABR           1
#define LENT_RC_CRF           2 //not support currently
#define LENT_RC_LQP           3 //not support currently
#define LENT_AQ_NONE          0
#define LENT_AQ_VARIANCE      1 //not support currently
#define LENT_AQ_AUTOVARIANCE  2 //not support currently

#define LENT_LOG_PSNR   1

typedef struct
{
	int i_type;		/* nal_unit_type_e */
	int b_long_startcode;

	int temporal_id;
	int output_flag;
	int reserved_one_4bits;

	int i_payload;
	uint8_t *p_payload;
} LENT_nal_t;

typedef struct
{
	/* CPU info */
	//total threads = i_threads + i_threads_wpp
	//threads_wpp cost less memory, while providing similar parallelism
	//assert( i_threads > 0 && i_threads_wpp >= 0 && (i_threads+i_threads_wpp) <= 127 );
	//assert( MAX(i_threads_wpp_pic) <= i_threads_wpp );
	unsigned int cpu;
	int			i_threads;
	int			i_threads_wpp;
	int			i_threads_wpp_pic[LENT_PIC_TYPE_COUNT]; //threads for different pic

	/* Encoder info */
	//for transcoder
	void		*h_file_handle;
	int			i_encoder_index;
	int			i_encoder_count;

	// compatibility: 91 for HM9.1; 100 for HM10.0; 120 for HM12.0
	int			b_SEI;
	int			i_compatibility;

	// log
	int			i_log_flag;

	/* Video info */
	//int			i_level_idc; //TODO: not defined yet
	int			i_idr_max;
	int			i_idr_min;
	int			i_bframe;
	int			i_hierach_bframe; //the hierach level

	/* All spatial layer encoding param */
	int			i_spatial_layer;
	int			i_frame_reference;

	// analyse
	int			i_rdo_level;
	int			i_me_method;
	int			b_temporal_mvp;
	int			b_rdoq;
	int			b_sao;
	int			b_sign_hiding;
	int			b_intra_smooth;
	int			i_bdir_refine;

	// rate control
	int			i_fps_num;
	int			i_fps_den;

	struct
	{
		int			i_aq_mode;
		float		f_aq_strength;

		int			i_rc_method;
		int			b_stat_read;
		char		*psz_stat_in;
		int			b_stat_write;
		char		*psz_stat_out;
		int			b_qu_tree;

		int			i_qp_max;
		int			i_qp_min;
		int			i_qp_step;
		int			i_lookahead;
		float		f_qcompress;

		float		f_ip_factor;
		float		f_pb_factor;
		float		f_rate_tolerance;
	} rc;

	/* Each spatial layer encoding param */
	struct
	{
		int			i_layer_num; //quality layer

		int			i_width;
		int			i_height;

		//analyse
		int			i_me_range;
		int			i_mv_range;

		//rc
		int			i_qp[MAX_QUALITY_LAYER];              //CQP
		int			i_bitrate[MAX_QUALITY_LAYER];         //ABR
		float		f_rf_constant[MAX_QUALITY_LAYER];     //CRF
	} spatial[MAX_SPATIAL_LAYER];
} LENT_param_t;

typedef struct
{
	int i_height[3];
	int i_width[3];
	int i_stride[3];
	uint8_t *plane[3];
} LENT_image_t;

typedef struct
{
	int i_type;
	int64_t i_pts;
	int64_t i_dts;
	LENT_image_t img;
} LENT_picture_t;


void LENT_param_default( LENT_param_t *, int i_preset );

int LENT_picture_alloc( LENT_picture_t *pic, int i_width, int i_height );

void LENT_picture_free( LENT_picture_t *pic );

LENT_HANDLE LENT_encoder_open( LENT_param_t * );

int LENT_encoder_header_get( LENT_HANDLE, uint8_t *p_buf, int i_max_size );

int LENT_encoder_encode( LENT_HANDLE , LENT_nal_t **pp_nal, int *pi_nal, LENT_picture_t *pic_in, LENT_picture_t *pic_out );

int LENT_encoder_encoding( LENT_HANDLE );

void LENT_encoder_close( LENT_HANDLE );

#endif /* __LENTHEVCENC_H__ */
