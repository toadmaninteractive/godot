/* Copyright (c) 2011 The WebM project authors. All Rights Reserved. */
/*  */
/* Use of this source code is governed by a BSD-style license */
/* that can be found in the LICENSE file in the root of the source */
/* tree. An additional intellectual property rights grant can be found */
/* in the file PATENTS.  All contributing project authors may */
/* be found in the AUTHORS file in the root of the source tree. */
/* This file automatically generated by configure. Do not edit! */
#ifndef VPX_CONFIG_H
#define VPX_CONFIG_H
#define RESTRICT
#if defined(_MSC_VER) && (_MSC_VER < 1900)
	#define INLINE __inline
#else
	#define INLINE inline
#endif

#define HAVE_MIPS32 0
#define HAVE_MEDIA 0

#if defined(__i386) || defined(__i386__) || defined(_M_IX86)
	#define ARCH_X86 1
	#define ARCH_X86_64 0

	#define ARCH_ARM 0
	#define HAVE_NEON 0
	#define HAVE_NEON_ASM 0

	#define HAVE_MMX 1
	#define HAVE_SSE2 1
	#define HAVE_SSSE3 1
	#define HAVE_AVX2 0
#elif defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
	#define ARCH_X86 0
	#define ARCH_X86_64 1

	#define ARCH_ARM 0
	#define HAVE_NEON 0
	#define HAVE_NEON_ASM 0

	#define HAVE_MMX 1
	#define HAVE_SSE2 1
	#define HAVE_SSSE3 1
	#define HAVE_AVX2 0
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM) || defined(_M_ARM)
	#define ARCH_X86 0
	#define ARCH_X86_64 0

	#define ARCH_ARM 1
	#define HAVE_NEON 1
	#define HAVE_NEON_ASM 1
#elif defined(__aarch64__)
	#define ARCH_X86 0
	#define ARCH_X86_64 0

	#define ARCH_ARM 1
	#define HAVE_NEON 0
	#define HAVE_NEON_ASM 0
#else
	#define ARCH_X86 0
	#define ARCH_X86_64 0

	#define ARCH_ARM 0
	#define HAVE_NEON 0
	#define HAVE_NEON_ASM 0
#endif

#define CONFIG_BIG_ENDIAN 0 //TODO: Autodetect

#ifdef __EMSCRIPTEN__
#define CONFIG_MULTITHREAD 0
#else
#define CONFIG_MULTITHREAD 1
#endif

#ifdef _WIN32
	#define HAVE_PTHREAD_H 0
	#define HAVE_UNISTD_H 0
#elif __ORBIS__
	#define HAVE_PTHREAD_H 0
	#define HAVE_UNISTD_H 0
#else
	#define HAVE_PTHREAD_H 1
	#define HAVE_UNISTD_H 1
#endif

/**/

#define HAVE_VPX_PORTS 1
#define CONFIG_DEPENDENCY_TRACKING 0
#define CONFIG_EXTERNAL_BUILD 0
#define CONFIG_INSTALL_DOCS 0
#define CONFIG_INSTALL_BINS 0
#define CONFIG_INSTALL_LIBS 0
#define CONFIG_INSTALL_SRCS 0
#define CONFIG_DEBUG 0
#define CONFIG_GPROF 0
#define CONFIG_GCOV 0
#define CONFIG_RVCT 0
#define CONFIG_CODEC_SRCS 0
#define CONFIG_DEBUG_LIBS 0
#define CONFIG_DEQUANT_TOKENS 0
#define CONFIG_DC_RECON 0
#define CONFIG_RUNTIME_CPU_DETECT 1
#define CONFIG_POSTPROC 0
#define CONFIG_VP9_POSTPROC 0
#define CONFIG_INTERNAL_STATS 0
#define CONFIG_VP8_ENCODER 0
#define CONFIG_VP8_DECODER 1
#define CONFIG_VP9_ENCODER 0
#define CONFIG_VP9_DECODER 1
#define CONFIG_VP8 1
#define CONFIG_VP9 1
#define CONFIG_ENCODERS 0
#define CONFIG_DECODERS 1
#define CONFIG_STATIC_MSVCRT 0
#define CONFIG_SPATIAL_RESAMPLING 0
#define CONFIG_REALTIME_ONLY 0
#define CONFIG_ONTHEFLY_BITPACKING 0
#define CONFIG_ERROR_CONCEALMENT 0
#define CONFIG_SHARED 0
#define CONFIG_STATIC 0
#define CONFIG_SMALL 0
#define CONFIG_POSTPROC_VISUALIZER 0
#define CONFIG_OS_SUPPORT 1
#define CONFIG_UNIT_TESTS 0
#define CONFIG_WEBM_IO 0
#define CONFIG_LIBYUV 0
#define CONFIG_DECODE_PERF_TESTS 0
#define CONFIG_ENCODE_PERF_TESTS 0
#define CONFIG_MULTI_RES_ENCODING 0
#define CONFIG_TEMPORAL_DENOISING 0
#define CONFIG_VP9_TEMPORAL_DENOISING 0
#define CONFIG_COEFFICIENT_RANGE_CHECKING 0
#define CONFIG_VP9_HIGHBITDEPTH 0
#define CONFIG_BETTER_HW_COMPATIBILITY 0
#define CONFIG_EXPERIMENTAL 0
#define CONFIG_SIZE_LIMIT 0
#define CONFIG_SPATIAL_SVC 0
#define CONFIG_FP_MB_STATS 0
#define CONFIG_EMULATE_HARDWARE 0
#define CONFIG_MISC_FIXES 0
#endif /* VPX_CONFIG_H */
