/***********************************************************************
Copyright (c) 2011, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

#define inline __inline

#define VAR_ARRAYS            1

/* Comment out the next line for floating-point code */
/*#define FIXED_POINT           1 */

#define OPUS_BUILD            1

#include "version.h"

#define __MACOSX__

#ifndef __aarch64__
#define OPUS_HAVE_RTCD 1
#define OPUS_X86_MAY_HAVE_SSE 1
#define OPUS_X86_MAY_HAVE_SSE2 1
#define OPUS_X86_MAY_HAVE_SSE4_1 1

#else

#undef OPUS_HAVE_RTCD
#define OPUS_ARM_MAY_HAVE_NEON 1
#define OPUS_ARM_MAY_HAVE_NEON_INTR 1
#define OPUS_ARM_PRESUME_NEON 1
#define OPUS_ARM_PRESUME_NEON_INTR 1
#endif

#endif CONFIG_H
