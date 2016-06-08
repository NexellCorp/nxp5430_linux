LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES += \
	common/ihevc_quant_tables.c			\
	common/ihevc_inter_pred_filters.c	\
	common/ihevc_weighted_pred.c		\
	common/ihevc_padding.c				\
	common/ihevc_deblk_edge_filter.c	\
	common/ihevc_deblk_tables.c			\
	common/ihevc_cabac_tables.c			\
	common/ihevc_common_tables.c		\
	common/ihevc_intra_pred_filters.c	\
	common/ihevc_chroma_intra_pred_filters.c\
	common/ihevc_mem_fns.c				\
	common/ihevc_sao.c					\
	common/ihevc_trans_tables.c			\
	common/ihevc_recon.c				\
	common/ihevc_itrans.c				\
	common/ihevc_itrans_recon.c			\
	common/ihevc_iquant_recon.c			\
	common/ihevc_iquant_itrans_recon.c	\
	common/ihevc_itrans_recon_32x32.c	\
	common/ihevc_itrans_recon_16x16.c	\
	common/ihevc_itrans_recon_8x8.c		\
	common/ihevc_chroma_itrans_recon.c	\
	common/ihevc_chroma_iquant_recon.c	\
	common/ihevc_chroma_iquant_itrans_recon.c\
	common/ihevc_chroma_recon.c			\
	common/ihevc_chroma_itrans_recon_16x16.c\
	common/ihevc_chroma_itrans_recon_8x8.c	\
	common/ihevc_buf_mgr.c				\
	common/ihevc_disp_mgr.c				\
	common/ihevc_dpb_mgr.c				\
	common/ithread.c

LOCAL_SRC_FILES += \
	decoder/ihevcd_version.c	\
	decoder/ihevcd_api.c	\
	decoder/ihevcd_decode.c	\
	decoder/ihevcd_nal.c	\
	decoder/ihevcd_bitstream.c	\
	decoder/ihevcd_parse_headers.c	\
	decoder/ihevcd_parse_slice_header.c	\
	decoder/ihevcd_parse_slice.c	\
	decoder/ihevcd_parse_residual.c	\
	decoder/ihevcd_cabac.c	\
	decoder/ihevcd_intra_pred_mode_prediction.c	\
	decoder/ihevcd_process_slice.c	\
	decoder/ihevcd_utils.c	\
	decoder/ihevcd_job_queue.c	\
	decoder/ihevcd_ref_list.c	\
	decoder/ihevcd_get_mv.c	\
	decoder/ihevcd_mv_pred.c	\
	decoder/ihevcd_mv_merge.c	\
	decoder/ihevcd_iquant_itrans_recon_ctb.c	\
	decoder/ihevcd_itrans_recon_dc.c	\
	decoder/ihevcd_common_tables.c	\
	decoder/ihevcd_boundary_strength.c	\
	decoder/ihevcd_deblk.c	\
	decoder/ihevcd_inter_pred.c	\
	decoder/ihevcd_sao.c	\
	decoder/ihevcd_ilf_padding.c	\
	decoder/ihevcd_fmt_conv.c	\

LOCAL_SRC_FILES += \
	decoder/arm/ihevcd_function_selector.c	\
	decoder/arm/ihevcd_function_selector_noneon.c	\


LOCAL_SRC_FILES += \
	decoder/arm/ihevcd_function_selector_a9q.c	\
	common/arm/ihevc_intra_ref_substitution_a9q.c	\
	common/arm/ihevc_intra_pred_filters_neon_intr.c	\
	common/arm/ihevc_weighted_pred_neon_intr.c	\
	common/arm/ihevc_mem_fns.s	\
	common/arm/ihevc_itrans_recon_32x32.s	\
	common/arm/ihevc_weighted_pred_bi_default.s	\
	common/arm/ihevc_weighted_pred_bi.s	\
	common/arm/ihevc_weighted_pred_uni.s	\
	common/arm/ihevc_deblk_luma_horz.s	\
	common/arm/ihevc_deblk_luma_vert.s	\
	common/arm/ihevc_deblk_chroma_vert.s	\
	common/arm/ihevc_deblk_chroma_horz.s	\
	common/arm/ihevc_sao_band_offset_luma.s	\
	common/arm/ihevc_sao_band_offset_chroma.s	\
	common/arm/ihevc_sao_edge_offset_class0.s	\
	common/arm/ihevc_sao_edge_offset_class0_chroma.s	\
	common/arm/ihevc_sao_edge_offset_class1.s	\
	common/arm/ihevc_sao_edge_offset_class1_chroma.s	\
	common/arm/ihevc_sao_edge_offset_class2.s	\
	common/arm/ihevc_sao_edge_offset_class2_chroma.s	\
	common/arm/ihevc_sao_edge_offset_class3.s	\
	common/arm/ihevc_sao_edge_offset_class3_chroma.s	\
	common/arm/ihevc_inter_pred_luma_horz_w16out.s	\
	common/arm/ihevc_inter_pred_filters_luma_horz.s	\
	common/arm/ihevc_inter_pred_filters_luma_vert.s	\
	common/arm/ihevc_inter_pred_chroma_horz.s	\
	common/arm/ihevc_inter_pred_chroma_horz_w16out.s	\
	common/arm/ihevc_inter_pred_chroma_vert.s	\
	common/arm/ihevc_inter_pred_chroma_vert_w16out.s	\
	common/arm/ihevc_inter_pred_chroma_vert_w16inp.s	\
	common/arm/ihevc_inter_pred_chroma_vert_w16inp_w16out.s	\
	common/arm/ihevc_inter_pred_filters_luma_vert_w16inp.s	\
	common/arm/ihevc_inter_pred_luma_vert_w16inp_w16out.s	\
	common/arm/ihevc_inter_pred_luma_copy_w16out.s	\
	common/arm/ihevc_inter_pred_luma_copy.s	\
	common/arm/ihevc_inter_pred_chroma_copy.s	\
	common/arm/ihevc_inter_pred_chroma_copy_w16out.s	\
	common/arm/ihevc_itrans_recon_4x4_ttype1.s	\
	common/arm/ihevc_itrans_recon_4x4.s	\
	common/arm/ihevc_itrans_recon_8x8.s	\
	common/arm/ihevc_itrans_recon_16x16.s	\
	common/arm/ihevc_intra_pred_chroma_planar.s	\
	common/arm/ihevc_intra_pred_chroma_dc.s	\
	common/arm/ihevc_intra_pred_chroma_horz.s	\
	common/arm/ihevc_intra_pred_chroma_ver.s	\
	common/arm/ihevc_intra_pred_chroma_mode2.s	\
	common/arm/ihevc_intra_pred_chroma_mode_18_34.s	\
	common/arm/ihevc_intra_pred_filters_chroma_mode_11_to_17.s	\
	common/arm/ihevc_intra_pred_filters_chroma_mode_19_to_25.s	\
	common/arm/ihevc_intra_pred_chroma_mode_3_to_9.s	\
	common/arm/ihevc_intra_pred_chroma_mode_27_to_33.s	\
	common/arm/ihevc_intra_pred_luma_planar.s	\
	common/arm/ihevc_intra_pred_luma_horz.s	\
	common/arm/ihevc_intra_pred_luma_mode2.s	\
	common/arm/ihevc_intra_pred_luma_mode_27_to_33.s	\
	common/arm/ihevc_intra_pred_luma_mode_18_34.s	\
	common/arm/ihevc_intra_pred_luma_vert.s	\
	common/arm/ihevc_intra_pred_luma_dc.s	\
	common/arm/ihevc_intra_pred_filters_luma_mode_11_to_17.s	\
	common/arm/ihevc_intra_pred_filters_luma_mode_19_to_25.s	\
	common/arm/ihevc_intra_pred_luma_mode_3_to_9.s	\
	common/arm/ihevc_padding.s

LOCAL_SRC_FILES += \
	decoder/arm/ihevcd_itrans_recon_dc_luma.s		\
	decoder/arm/ihevcd_itrans_recon_dc_chroma.s		\
	decoder/arm/ihevcd_fmt_conv_420sp_to_420p.s		\
	decoder/arm/ihevcd_fmt_conv_420sp_to_420sp.s	\
	decoder/arm/ihevcd_fmt_conv_420sp_to_rgba8888.s

LOCAL_C_INCLUDES:=	\
	$(LOCAL_PATH)/common/arm	\
	$(LOCAL_PATH)/common		\
	$(LOCAL_PATH)/decoder		\
	$(LOCAL_PATH)

LOCAL_ARM_MODE := arm

LOCAL_CFLAGS	+= -D_LIB -DMULTICORE -fPIC 
LOCAL_CFLAGS	+= -O3 -DANDROID 
LOCAL_CFLAGS	+= -DDISABLE_NEONINTR -DARM -DARMGCC
LOCAL_CFLAGS	+= -DDEFAULT_ARCH=D_ARCH_ARM_A9Q

LOCAL_SHARED_LIBRARIES :=
LOCAL_STATIC_LIBRARIES :=

LOCAL_MODULE		:= libhevcdec_and
LOCAL_MODULE_PATH	:= $(LOCAL_PATH)

LOCAL_MODULE_TAGS	:= optional
include $(BUILD_STATIC_LIBRARY)



