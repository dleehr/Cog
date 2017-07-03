#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* header version */
#define EA_VERSION_NONE         -1
#define EA_VERSION_V0           0x00  // ~early PC (when codec1 was used)
#define EA_VERSION_V1           0x01  // ~PC
#define EA_VERSION_V2           0x02  // ~PS era
#define EA_VERSION_V3           0x03  // ~PS2 era

/* platform constants (unasigned values seem internal only) */
#define EA_PLATFORM_GENERIC     -1    // typically Wii/X360/PS3
#define EA_PLATFORM_PC          0x00
#define EA_PLATFORM_PSX         0x01
#define EA_PLATFORM_N64         0x02
#define EA_PLATFORM_MAC         0x03
//#define EA_PLATFORM_SAT       0x04  // ?
#define EA_PLATFORM_PS2         0x05
#define EA_PLATFORM_GC_WII      0x06  // reused later for Wii
#define EA_PLATFORM_XBOX        0x07
#define EA_PLATFORM_X360        0x09  // also "Xenon"
#define EA_PLATFORM_PSP         0x0A
#define EA_PLATFORM_3DS         0x14

/* codec constants (undefined are probably reserved, ie.- sx.exe encodes PCM24/DVI but no platform decodes them) */
/* CODEC1 values were used early, then they migrated to CODEC2 values */
#define EA_CODEC1_NONE          -1
//#define EA_CODEC1_S16BE       0x00 //LE too?
//#define EA_CODEC1_VAG         0x01
#define EA_CODEC1_MT10          0x07 // Need for Speed 2 PC
//#define EA_CODEC1_N64         ?

#define EA_CODEC2_NONE          -1
#define EA_CODEC2_MT10          0x04
#define EA_CODEC2_VAG           0x05
#define EA_CODEC2_S16BE         0x07
#define EA_CODEC2_S16LE         0x08
#define EA_CODEC2_S8            0x09
#define EA_CODEC2_EAXA          0x0A
#define EA_CODEC2_LAYER2        0x0F
#define EA_CODEC2_LAYER3        0x10
#define EA_CODEC2_GCADPCM       0x12
#define EA_CODEC2_XBOXADPCM     0x14
#define EA_CODEC2_MT5           0x16
#define EA_CODEC2_EALAYER3      0x17

#define EA_MAX_CHANNELS  6

typedef struct {
    uint8_t id;
    int32_t num_samples;
    int32_t sample_rate;
    int32_t channels;
    int32_t platform;
    int32_t version;
    int32_t codec1;
    int32_t codec2;

    int32_t loop_start;
    int32_t loop_end;

    off_t offsets[EA_MAX_CHANNELS];
    off_t coefs[EA_MAX_CHANNELS];

    int big_endian;
    int loop_flag;
    int codec_version;
} ea_header;

static int parse_stream_header(STREAMFILE* streamFile, ea_header* ea, off_t begin_offset, int max_length);
static uint32_t read_patch(STREAMFILE* streamFile, off_t* offset);
static int get_ea_total_samples(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea);
static off_t get_ea_mpeg_start_offset(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea);


/* EA SCHl - from EA games (roughly 1997~2010, generated by EA Canada's sx.exe / Sound eXchange) */
VGMSTREAM * init_vgmstream_ea_schl(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t header_size;
    ea_header ea;


    /* check extension; exts don't seem enforced by EA's tools, but usually:
     * STR/ASF/MUS ~early, EAM ~mid, SNG/AUD ~late, rest uncommon/one game (ex. STRM: MySims Kingdom Wii) */
    if (!check_extensions(streamFile,"str,asf,mus,eam,sng,aud,strm,xa,xsf,exa,stm"))
        goto fail;

    /* check header */
    /* EA's stream files are made of blocks called "chunks" (SCxx, presumably Sound Chunk xx)
     * typically: SCHl=header, SCCl=count of SCDl, SCDl=data xN, SCLl=loop end, SCEl=stream end.
     * The number/size of blocks is affected by: block rate setting, sample rate, channels, CPU location (SPU/main/DSP/others), etc */
    if (read_32bitBE(0x00,streamFile) != 0x5343486C) /* "SCHl" */
        goto fail;

    header_size = read_32bitLE(0x04,streamFile);
    if (header_size > 0xF0000000) /* size is always LE, except in early MAC apparently */
        header_size = read_32bitBE(0x04,streamFile);

    memset(&ea,0,sizeof(ea_header));
    if (!parse_stream_header(streamFile,&ea, 0x08, header_size-4-4))
        goto fail;

    start_offset = header_size; /* start in "SCCl" or very rarely "SCDl" (skipped in block layout, though) */
    if (read_32bitBE(start_offset,streamFile) != 0x5343436C && read_32bitBE(start_offset,streamFile) != 0x5343446C ) /* "SCCl" / "SCDl" */
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ea.channels, ea.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ea.sample_rate;
    vgmstream->num_samples = ea.num_samples;
    vgmstream->loop_start_sample = ea.loop_start;
    vgmstream->loop_end_sample = ea.loop_end;

    vgmstream->codec_endian = ea.big_endian;
    vgmstream->codec_version = ea.codec_version;

    vgmstream->meta_type = meta_EA_SCHL;
    vgmstream->layout_type = layout_ea_blocked;

    /* EA usually implements their codecs in all platforms (PS2/WII do EAXA/MT/EALAYER3) and
     * favors them over platform's natives (ex. EAXA vs VAG/DSP).
     * Unneeded codecs are removed over time (ex. LAYER3 when EALAYER3 was introduced). */
    switch (ea.codec2) {

        case EA_CODEC2_EAXA:        /* EA-XA, CDXA ADPCM variant */
            vgmstream->coding_type = coding_EA_XA;
            break;

        case EA_CODEC2_MT10:        /* MicroTalk (10:1), aka EA ADPCM (stereo or interleaved) */
            vgmstream->coding_type = coding_EA_MT10;
            break;

        case EA_CODEC2_S8:          /* PCM8 */
            vgmstream->coding_type = coding_PCM8;
            break;

        case EA_CODEC2_S16BE:       /* PCM16BE */
            vgmstream->coding_type = coding_PCM16BE;
            break;

        case EA_CODEC2_S16LE:       /* PCM16LE */
            vgmstream->coding_type = coding_PCM16LE;
            break;

        case EA_CODEC2_VAG:         /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            break;

        case EA_CODEC2_XBOXADPCM:   /* XBOX IMA (interleaved mono) */
            vgmstream->coding_type = coding_XBOX; /* stereo decoder actually, but has a special case for EA */
            break;

        case EA_CODEC2_GCADPCM:     /* DSP */
            vgmstream->coding_type = coding_NGC_DSP;

            /* get them coefs (start offsets are not necessarily ordered) */
            {
                int ch, i;
                int16_t (*read_16bit)(off_t,STREAMFILE*) = ea.big_endian ? read_16bitBE : read_16bitLE;

                for (ch=0; ch < vgmstream->channels; ch++) {
                    for (i=0; i < 16; i++) { /* actual size 0x21, last byte unknown */
                        vgmstream->ch[ch].adpcm_coef[i] = read_16bit(ea.coefs[ch] + i*2, streamFile);
                    }
                }
            }
            break;

#ifdef VGM_USE_MPEG
        case EA_CODEC2_LAYER2:      /* MPEG Layer II, aka MP2 */
        case EA_CODEC2_LAYER3: {    /* MPEG Layer III, aka MP3 */
            mpeg_codec_data *mpeg_data = NULL;
            coding_t mpeg_coding_type;

            off_t mpeg_start_offset = get_ea_mpeg_start_offset(streamFile, start_offset, &ea);
            if (!mpeg_start_offset) goto fail;

            mpeg_data = init_mpeg_codec_data_interleaved(streamFile, mpeg_start_offset, &mpeg_coding_type, vgmstream->channels, MPEG_EA, 0);
            if (!mpeg_data) goto fail;
            vgmstream->codec_data = mpeg_data;
            vgmstream->coding_type = mpeg_coding_type;
            //vgmstream->layout_type = layout_mpeg;
            //mpeg_set_error_logging(mpeg_data, 0); /* should not be needed anymore with the interleave decoder */
            break;
        }
#endif

        case EA_CODEC2_MT5:         /* MicroTalk (5:1) */
        case EA_CODEC2_EALAYER3:    /* MP3 variant */
        default:
            VGM_LOG("EA: unknown codec2 0x%02x for platform 0x%02x\n", ea.codec2, ea.platform);
            goto fail;
    }


    /* fix num_samples for multifiles */
    {
        int total_samples = get_ea_total_samples(streamFile, start_offset, &ea);
        if (total_samples > vgmstream->num_samples)
           vgmstream->num_samples = total_samples;
    }

    /* open files; channel offsets are updated below */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    ea_schl_block_update(start_offset,vgmstream);


    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static uint32_t read_patch(STREAMFILE* streamFile, off_t* offset) {
    uint32_t result = 0;
    uint8_t byte_count = read_8bit(*offset, streamFile);
    (*offset)++;

    if (byte_count == 0xFF) { /* signals 32b size (ex. custom user data) */
        (*offset) += 4 + read_32bitBE(*offset, streamFile);
        return 0;
    }

    if (byte_count > 4) { /* uncommon (ex. coef patches) */
        (*offset) += byte_count;
        return 0;
    }

    for ( ; byte_count > 0; byte_count--) { /* count of 0 is also possible, means value 0 */
        result <<= 8;
        result += (uint8_t)read_8bit(*offset, streamFile);
        (*offset)++;
    }

    return result;
}

/* decodes EA's GSTR/PT header (mostly cross-referenced with sx.exe) */
static int parse_stream_header(STREAMFILE* streamFile, ea_header* ea, off_t begin_offset, int max_length) {
    off_t offset = begin_offset;
    uint32_t platform_id;
    int is_header_end = 0;


    /* null defaults as 0 can be valid */
    ea->version  = EA_VERSION_NONE;
    ea->codec1 = EA_CODEC1_NONE;
    ea->codec2 = EA_CODEC2_NONE;

    /* get platform info */
    platform_id = read_32bitBE(offset, streamFile);
    if (platform_id != 0x47535452 && (platform_id & 0xFFFF0000) != 0x50540000) {
        offset += 4; /* skip unknown field (related to blocks/size?) in "nbapsstream" (NBA2000 PS, FIFA2001 PS) */
        platform_id = read_32bitBE(offset, streamFile);
    }
    if (platform_id == 0x47535452) { /* "GSTR" = Generic STReam */
        ea->platform = EA_PLATFORM_GENERIC;
        offset += 4 + 4; /* GSTRs have an extra field (config?): ex. 0x01000000, 0x010000D8 BE */
    }
    else if ((platform_id & 0xFFFF0000) == 0x50540000) { /* "PT" = PlaTform */
        ea->platform = (uint8_t)read_16bitLE(offset + 2,streamFile);
        offset += 4;
    }
    else {
        goto fail;
    }

    /* parse mini-chunks/tags (variable, ommited if default exists) */
    while (offset - begin_offset < max_length) {
        uint8_t patch_type = read_8bit(offset,streamFile);
        offset++;

        switch(patch_type) {

            case 0x00: /* signals non-default block rate and maybe other stuff; or padding after 0xFD */
                if (!is_header_end)
                    read_patch(streamFile, &offset);
                break;

            case 0x06: /* always 0x65 */
                ea->id = read_patch(streamFile, &offset);
                break;

            case 0x05: /* unknown (usually 0x50 except Madden NFL 3DS: 0x3e800) */
            case 0x0B: /* unknown (always 0x02) */
            case 0x13: /* effect bus (0..127) */
            case 0x14: /* emdedded user data (free size/value) */
                read_patch(streamFile, &offset);
                break;

            case 0xFC: /* padding for alignment between patches */
            case 0xFE: /* padding? (actually exists?) */
            case 0xFD: /* info section start marker */
                break;

            case 0xA0: /* codec2 defines */
                ea->codec2 = read_patch(streamFile, &offset);
                break;

            case 0x80: /* version, affecting some codecs */
                ea->version = read_patch(streamFile, &offset);
                break;

            case 0x82: /* channel count */
                ea->channels = read_patch(streamFile, &offset);
                break;

            case 0x83: /* codec1 defines, used early revisions */
                ea->codec1 = read_patch(streamFile, &offset);
                break;

            case 0x84: /* sample rate */
                ea->sample_rate = read_patch(streamFile,&offset);
                break;

            case 0x85: /* sample count */
                ea->num_samples = read_patch(streamFile, &offset);
                break;
            case 0x86: /* loop start sample */
                ea->loop_start = read_patch(streamFile, &offset);
                break;
            case 0x87: /* loop end sample */
                ea->loop_end = read_patch(streamFile, &offset);
                break;

            /* channel offsets (BNK only), can be the equal for all channels or interleaved; not necessarily contiguous */
            case 0x88: /* absolute offset of ch1 */
                ea->offsets[0] = read_patch(streamFile, &offset);
                break;
            case 0x89: /* absolute offset of ch2 */
                ea->offsets[1] = read_patch(streamFile, &offset);
                break;
            case 0x94: /* absolute offset of ch3 */
                ea->offsets[2] = read_patch(streamFile, &offset);
                break;
            case 0x95: /* absolute offset of ch4 */
                ea->offsets[3] = read_patch(streamFile, &offset);
                break;
            case 0xA2: /* absolute offset of ch5 */
                ea->offsets[4] = read_patch(streamFile, &offset);
                break;
            case 0xA3: /* absolute offset of ch6 */
                ea->offsets[5] = read_patch(streamFile, &offset);
                break;

            case 0x8F: /* DSP/N64BLK coefs ch1 */
                ea->coefs[0] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0x90: /* DSP/N64BLK coefs ch2 */
                ea->coefs[1] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0x91: /* DSP coefs ch3 */
                ea->coefs[2] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0xAB: /* DSP coefs ch4 */
                ea->coefs[3] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0xAC: /* DSP coefs ch5 */
                ea->coefs[4] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0xAD: /* DSP coefs ch6 */
                ea->coefs[5] = offset+1;
                read_patch(streamFile, &offset);
                break;

            case 0x8A: /* long padding? (always 0x00000000) */
            case 0x8C: /* platform+codec related? */
                       /* (ex. PS1 VAG=0, PS2 PCM/LAYER2=4, GC EAXA=4, 3DS DSP=512, Xbox EAXA=36, N64 BLK=05E800, N64 MT=01588805E800) */
            case 0x92: /* bytes per sample? */
            case 0x98: /* embedded time stretch 1 (long data for who-knows-what) */
            case 0x99: /* embedded time stretch 2 */
            case 0x9C: /* azimuth ch1 */
            case 0x9D: /* azimuth ch2 */
            case 0x9E: /* azimuth ch3 */
            case 0x9F: /* azimuth ch4 */
            case 0xA6: /* azimuth ch5 */
            case 0xA7: /* azimuth ch6 */
            case 0xA1: /* unknown and very rare, always 0x02 (FIFA 2001 PS2) */
                read_patch(streamFile, &offset);
                break;

            case 0xFF: /* header end (then 0-padded) */
                is_header_end = 1;
                break;

            default:
                VGM_LOG("EA: unknown patch 0x%02x at 0x%04lx\n", patch_type, (offset-1));
                break;
        }
    }

    if (ea->id && ea->id != 0x65) /* very rarely not specified (FIFA 14) */
        goto fail;
    if (ea->channels > EA_MAX_CHANNELS)
        goto fail;


    /* set defaults per platform, as the header ommits them when possible */

    ea->loop_flag = (ea->loop_end);

    if (!ea->channels) {
        ea->channels = 1;
    }

    /* version affects EAXA and MT codecs, but can be found with all other codecs */
    /* For PC/MAC V0 is simply no version when codec1 was used */
    if (ea->version == EA_VERSION_NONE) {
        switch(ea->platform) {
            case EA_PLATFORM_GENERIC:   ea->version = EA_VERSION_V2; break;
            case EA_PLATFORM_PC:        ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_PSX:       ea->version = EA_VERSION_V0; break; // assumed
            case EA_PLATFORM_N64:       ea->version = EA_VERSION_V0; break; // assumed
            case EA_PLATFORM_MAC:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_PS2:       ea->version = EA_VERSION_V1; break;
            case EA_PLATFORM_GC_WII:    ea->version = EA_VERSION_V2; break;
            case EA_PLATFORM_XBOX:      ea->version = EA_VERSION_V2; break;
            case EA_PLATFORM_X360:      ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_PSP:       ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_3DS:       ea->version = EA_VERSION_V3; break;
            default:
                VGM_LOG("EA: unknown default version for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* codec1 to codec2 to simplify later parsing */
    if (ea->codec1 != EA_CODEC1_NONE && ea->codec2 == EA_CODEC2_NONE) {
        switch (ea->codec1) {
            case EA_CODEC1_MT10:        ea->codec2 = EA_CODEC2_MT10; break;
            default:
                VGM_LOG("EA: unknown codec1 0x%02x\n", ea->codec1);
                goto fail;
        }
    }

    /* defaults don't seem to change with version or over time, fortunately */
    if (ea->codec2 == EA_CODEC2_NONE) {
        switch(ea->platform) {
            case EA_PLATFORM_GENERIC:   ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PC:        ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PSX:       ea->codec2 = EA_CODEC2_VAG; break;
            case EA_PLATFORM_MAC:       ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PS2:       ea->codec2 = EA_CODEC2_VAG; break;
            case EA_PLATFORM_GC_WII:    ea->codec2 = EA_CODEC2_S16BE; break;
            case EA_PLATFORM_XBOX:      ea->codec2 = EA_CODEC2_S16LE; break;
            case EA_PLATFORM_X360:      ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PSP:       ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_3DS:       ea->codec2 = EA_CODEC2_GCADPCM; break;
            default:
                VGM_LOG("EA: unknown default codec2 for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* somehow doesn't follow machine's sample rate or anything sensical */
    if (!ea->sample_rate) {
        switch(ea->platform) {
            case EA_PLATFORM_GENERIC:   ea->sample_rate = 48000; break;
            case EA_PLATFORM_PC:        ea->sample_rate = 22050; break;
            case EA_PLATFORM_PSX:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_N64:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_MAC:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_PS2:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_GC_WII:    ea->sample_rate = 24000; break;
            case EA_PLATFORM_XBOX:      ea->sample_rate = 24000; break;
            case EA_PLATFORM_X360:      ea->sample_rate = 44100; break;
            case EA_PLATFORM_PSP:       ea->sample_rate = 22050; break;
            //case EA_PLATFORM_3DS:     ea->sample_rate = 44100; break;//todo (not 22050/16000)
            default:
                VGM_LOG("EA: unknown default sample rate for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* affects blocks/codecs */
    if (ea->platform == EA_PLATFORM_N64
        || ea->platform == EA_PLATFORM_MAC
        || ea->platform == EA_PLATFORM_GC_WII
        || ea->platform == EA_PLATFORM_X360
        || ea->platform == EA_PLATFORM_GENERIC) {
        ea->big_endian = 1;
    }

    /* config MT/EAXA variations */
    if (ea->codec2 == EA_CODEC2_MT10) {
        if (ea->version > EA_VERSION_V0)
            ea->codec_version = 1; /* 0=stereo (early), 1:interleaved */
    }
    else if (ea->codec2 == EA_CODEC2_EAXA) {
        /* console EAXA V2 uses hist, as does PC/MAC V1 */
        if (ea->version > EA_VERSION_V1 && !(ea->version == EA_VERSION_V2
                && (ea->platform == EA_PLATFORM_PS2|| ea->platform == EA_PLATFORM_GC_WII || ea->platform == EA_PLATFORM_XBOX)))
            ea->codec_version = 1; /* 0=has ADPCM history per block (early), 1:doesn't */
    }

    return offset;

fail:
    return 0;
}

/* get total samples by parsing block headers, needed when multiple files are stitched together */
/* Some EA files (.mus, .eam, .sng, etc) concat many small subfiles, used as mapped
 * music (.map/lin). We get total possible samples (counting all subfiles) and pretend
 * they are a single stream. Subfiles always share header, except num_samples. */
static int get_ea_total_samples(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea) {
    int i, num_samples = 0;
    size_t file_size = get_streamfile_size(streamFile);
    off_t block_offset = start_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;

    while (block_offset < file_size) {
        uint32_t id, block_size;

        id = read_32bitBE(block_offset+0x00,streamFile);

        block_size = read_32bitLE(block_offset+0x04,streamFile);
        VGM_ASSERT(block_size > 0xF0000000, "EA: BE block size in MAC\n");
        if (block_size > 0xF0000000) /* size is always LE, except in early MAC apparently */
            block_size = read_32bitBE(block_offset+0x04,streamFile);

        if (id == 0x5343446C) { /* "SCDl" data block found */
            /* use num_samples from header if possible */
            switch (ea->codec2) {
                case EA_CODEC2_VAG:         /* PS-ADPCM */
                    num_samples += ps_bytes_to_samples(block_size-0x10, ea->channels);
                    break;

                default:
                    num_samples += read_32bit(block_offset+0x08,streamFile);
                    break;
            }
        }

        block_offset += block_size; /* size includes header */

        /* EA sometimes concats many small files, so after SCEl there may be a new SCHl.
         * We'll find it and pretend they are a single stream. */
        if (id == 0x5343456C && block_offset + 0x80 > file_size)
            break;
        if (id == 0x5343456C) { /* "SCEl" end block found */
            /* Usually there is padding between SCEl and SCHl (aligned to 0x80) */
            block_offset += (block_offset % 0x04) == 0 ? 0 : 0x04 - (block_offset % 0x04); /* also 32b-aligned */
            for (i = 0; i < 0x80 / 4; i++) {
                id = read_32bitBE(block_offset,streamFile);
                if (id == 0x5343486C) /* "SCHl" new header block found */
                    break; /* next loop will parse and skip it */
                block_offset += 0x04;
            }
        }

        if (block_offset > file_size)
            break;

        if (id == 0 || id == 0xFFFFFFFF)
            return num_samples; /* probably hit padding or EOF */

        VGM_ASSERT(id != 0x5343486C && id != 0x5343436C && id != 0x5343446C && id != 0x53434C6C && id != 0x5343456C,
            "EA: unknown block id 0x%x at 0x%lx\n", id, block_offset);
    }

    return num_samples;
}

/* find data start offset inside the first SCDl; not very elegant but oh well */
static off_t get_ea_mpeg_start_offset(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea) {
    size_t file_size = get_streamfile_size(streamFile);
    off_t block_offset = start_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;

    while (block_offset < file_size) {
        uint32_t id, block_size;

        id = read_32bitBE(block_offset+0x00,streamFile);

        block_size = read_32bitLE(block_offset+0x04,streamFile);
        if (block_size > 0xF0000000) /* size is always LE, except in early MAC apparently */
            block_size = read_32bitBE(block_offset+0x04,streamFile);

        if (id == 0x5343446C) { /* "SCDl" data block found */
            off_t offset = read_32bit(block_offset+0x0c,streamFile); /* first channel offset is ok, MPEG channels share offsets */
            return block_offset + 0x0c + ea->channels*0x04 + offset;
        } else if (id == 0x5343436C) { /* "SCCl" data count found */
            block_offset += block_size; /* size includes header */
            continue;
        } else {
            goto fail;
        }
    }

fail:
    return 0;
}