/*
 *  LXPixelBuffer_dpx.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 13.11.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXPixelBuffer.h"
#include "LXPixelBuffer_priv.h"

#include "LXStringUtils.h"
#include "LXImageFunctions.h"
#include "LXHalfFloat.h"
#include "LXFileHandlers.h"

#include <math.h>


//#define DEBUGLOG(format, args...)          LXPrintf(format , ## args);
#define DEBUGLOG(format, args...)


// --- private types for cineon parsing ---

#pragma pack(push, 1)

#define DPX_MAGIC_NUM               0x53445058
#define DPX_MAGIC_NUM_FLIPPED       0x58504453

enum {
    CineonTightestPacking = 0,
    Cineon8bitLeftPacking,
    Cineon8bitRightPacking,
    Cineon16bitLeftPacking,
    Cineon16bitRightPacking,
    Cineon32bitLeftPacking,   // for 10-bit RGB Cin files, the default - means that 3 values are packed into 32 bits
    Cineon32bitRightPacking
};
typedef unsigned int LXCineonPacking;

typedef struct {
    unsigned int rowBytes;
    unsigned int pixelBytes;
    unsigned int bitDepth;
    unsigned int descriptor;
    unsigned int width;
    unsigned int height;
    unsigned int inputMin;
    unsigned int inputMax;
    LXCineonPacking packing;
    unsigned int rowEndPadding;
    unsigned int rowStartPadding;
    unsigned int orientation;
} LXCineonDataInfo;



// ---- Cineon & DPX structs ----

// (source: http://www.cineon.com/ff_draft.php#data )

typedef unsigned int U32;
typedef char ASCII;
typedef unsigned char U8;
typedef unsigned short U16;
typedef float R32;


typedef struct
{
    U32     magic_num;
    U32     offset;
    U32     gen_hdr_size;
    U32     ind_hdr_size;
    U32     user_data_size;
    U32     file_size;
    ASCII   vers[8];
    ASCII   file_name[100];
    ASCII   create_date[12];
    ASCII   create_time[12];
    ASCII   reserved[36];
} CineonFileInformation;


typedef struct
{
    U8      designator[2];
    U8      bits_per_pixel;
    U8      unused1;
    U32     width;
    U32     height;
    R32     min_data_value;
    R32     min_quantity;
    R32     max_data_value;
    R32     max_quantity;
} CineonChannelSpecifier;

typedef struct
{
    U8      orientation;
    U8      num_channels;
    U8      unused1[2];
    CineonChannelSpecifier channel_spec[8];
    R32     white_point[2];
    R32     red_primary[2];
    R32     green_primary[2];
    R32     blue_primary[2];
    ASCII   label_text[200];
    ASCII   reserved[28];
    
} CineonImageInformationHeader;

typedef struct
{
    U8      interleave;
    U8      packing;
    U8      signedness;
    U8      negative;
    U32     line_padding_bytes;
    U32     channel_padding_bytes;
    ASCII   reserved[20];
} CineonImageDataInformation;


/*
Explanation of interleave/packing flags, from Cineon 4.5 spec:

1.28    680     1       U8      Data interleave (if all channels are not
the same spatial resolution, data interleave must be 2, channel
interleave)
                                0 = pixel interleave ( rgbrgbrgbrgb... )
                                1 = line interleave (rrr.ggg.bbb.rrr.ggg.bbb.)
                                2 = channel interleave (rrr...ggg...bbb...)
                                3 - 254 = user defined

1.29    681     1       U8      Packing (see note 1 )
                                
0 = user all bits( bitfields) - TIGHTEST - no byte, word or long word
boundaries
1 = byte ( 8 bit ) boundaries - lef justified
2 = byte ( 8 bit ) boundaries - right justified
3 = word (16 bit) bondaries - left justified
4 = word ( 16 bit) boundaries - right justified
5 = longword ( 32 bit ) boundaries - left justified
6 = longword ( 32 bit ) boundaries - right justified 

High order bit = 0 - pack at most one pixel per cell
High order bit = 1 - pack at many fields as possible per cell
*/


// --- DPX versions of the structs ---

typedef struct
{
    U32   magic_num;        /* magic number 0x53445058 (SDPX) or 0x58504453 (XPDS) */
    U32   offset;           /* offset to image data in bytes */
    ASCII vers[8];          /* which header format version is being used (v1.0)*/
    U32   file_size;        /* file size in bytes */
    U32   ditto_key;        /* read time short cut - 0 = same, 1 = new */
    U32   gen_hdr_size;     /* generic header length in bytes */
    U32   ind_hdr_size;     /* industry header length in bytes */
    U32   user_data_size;   /* user-defined data length in bytes */
    ASCII file_name[100];   /* iamge file name */
    ASCII create_time[24];  /* file creation date "yyyy:mm:dd:hh:mm:ss:LTZ" */
    ASCII creator[100];     /* file creator's name */
    ASCII project[200];     /* project name */
    ASCII copyright[200];   /* right to use or copyright info */
    U32   key;              /* encryption ( FFFFFFFF = unencrypted ) */
    ASCII Reserved[104];    /* reserved field TBD (need to pad) */
} DPXFileInformation;


typedef struct
{
        U32    data_sign;        /* data sign (0 = unsigned, 1 = signed ) */
                                 /* "Core set images are unsigned" */
        U32    ref_low_data;     /* reference low data code value */
        R32    ref_low_quantity; /* reference low quantity represented */
        U32    ref_high_data;    /* reference high data code value */
        R32    ref_high_quantity;/* reference high quantity represented */
        U8     descriptor;       /* descriptor for image element */
        U8     transfer;         /* transfer characteristics for element */
        U8     colorimetric;     /* colormetric specification for element */
        U8     bit_size;         /* bit size for element */
        U16    packing;          /* packing for element */
        U16    encoding;         /* encoding for element */
        U32    data_offset;      /* offset to data of element */
        U32    eol_padding;      /* end of line padding used in element */
        U32    eo_image_padding; /* end of image padding used in element */
        ASCII  description[32];  /* description of element */
} DPXImageElementInfo;

static inline void debugPrintDPXImageElementContents(DPXImageElementInfo *elInfo)
{
    DEBUGLOG("data sign: %u  ", elInfo->data_sign);
    DEBUGLOG("ref low data: %u, ", elInfo->ref_low_data);
    DEBUGLOG("ref low quantity: %.6f   ", (float)elInfo->ref_low_quantity);
    DEBUGLOG("ref high data: %u, ", elInfo->ref_high_data);
    DEBUGLOG("ref highquantity: %.6f\n", (float)elInfo->ref_high_quantity);
    DEBUGLOG("descriptor: %i ", (int)elInfo->descriptor);
    DEBUGLOG("transfer: %i ", (int)elInfo->transfer);
    DEBUGLOG("colorimetric: %i ", (int)elInfo->colorimetric);
    DEBUGLOG("bit size: %i\n", (int)elInfo->bit_size);
    DEBUGLOG("packing: %i ", (int)elInfo->packing);
    DEBUGLOG("encoding: %i ", (int)elInfo->encoding);
    DEBUGLOG("data offset: %u ", elInfo->data_offset);
    DEBUGLOG("eol padding: %u ", elInfo->eol_padding);
    DEBUGLOG("eo image padding: %u ", elInfo->eo_image_padding);
    DEBUGLOG(" description: '%s'\n", elInfo->description);
}

typedef struct
{
    U16    orientation;          /* image orientation */
    U16    num_elements;         /* number of image elements */
    U32    pixels_per_line;      /* or x value */
    U32    lines_per_image_ele;  /* or y value, per element */
    DPXImageElementInfo image_element[8];  /* NOTE THERE ARE EIGHT OF THESE */
    U8 reserved[52];             /* reserved for future use (padding) */
} DPXImageInformationHeader;

typedef struct
{
    U32   x_offset;               /* X offset */
    U32   y_offset;               /* Y offset */
    R32   x_center;               /* X center */
    R32   y_center;               /* Y center */
    U32   x_orig_size;            /* X original size */
    U32   y_orig_size;            /* Y original size */
    ASCII file_name[100];         /* source image file name */
    ASCII creation_time[24];      /* source image creation date and time */
    ASCII input_dev[32];          /* input device name */
    ASCII input_serial[32];       /* input device serial number */
    U16   border[4];              /* border validity (XL, XR, YT, YB) */
    U32   pixel_aspect[2];        /* pixel aspect ratio (H:V) */
    U8    reserved[28];           /* reserved for future use (padding) */
} DPXImageOrientationHeader;

static inline void debugPrintDPXImageOrientationHeader(DPXImageOrientationHeader *header)
{
    DEBUGLOG("x offset: %u ", header->x_offset);
    DEBUGLOG("y offset: %u  ", header->y_offset);
    DEBUGLOG("x center: %.4f ", header->x_center);
    DEBUGLOG("y center: %.4f  ", header->y_center);
    DEBUGLOG("x size: %u ", header->x_orig_size);
    DEBUGLOG("y size: %u\n", header->y_orig_size);
    if (strlen(header->file_name) > 0)
        DEBUGLOG("file name in orientation: %s\n", header->file_name);
    DEBUGLOG("creation time: '%s' ", header->creation_time);
    DEBUGLOG("input device '%s' ", header->input_dev);
    DEBUGLOG("device serial '%s'\n", header->input_serial);
    DEBUGLOG("border validity: %i / %i / %i / %i ", (int)header->border[0], (int)header->border[1], (int)header->border[2], (int)header->border[3]);
    DEBUGLOG("pixel aspect h/v: %u / %u\n", header->pixel_aspect[0], header->pixel_aspect[1]);
}

typedef struct
{
    ASCII film_mfg_id[2];    /* film manufacturer ID code (2 digits from film edge code) */
    ASCII film_type[2];      /* file type (2 digits from film edge code) */
    ASCII offset[2];         /* offset in perfs (2 digits from film edge code)*/
    ASCII prefix[6];         /* prefix (6 digits from film edge code) */
    ASCII count[4];          /* count (4 digits from film edge code)*/
    ASCII format[32];        /* format (i.e. academy) */
    U32   frame_position;    /* frame position in sequence */
    U32   sequence_len;      /* sequence length in frames */
    U32   held_count;        /* held count (1 = default) */
    R32   frame_rate;        /* frame rate of original in frames/sec */
    R32   shutter_angle;     /* shutter angle of camera in degrees */
    ASCII frame_id[32];      /* frame identification (i.e. keyframe) */
    ASCII slate_info[100];   /* slate information */
    U8    reserved[56];      /* reserved for future use (padding) */
} DPXFilmIndustryHeader;

typedef struct
{
    U32 tim_code;            /* SMPTE time code */
    U32 userBits;            /* SMPTE user bits */
    U8  interlace;           /* interlace ( 0 = noninterlaced, 1 = 2:1 interlace*/
    U8  field_num;           /* field number */
    U8  video_signal;        /* video signal standard (table 4)*/
    U8  unused;              /* used for byte alignment only */
    R32 hor_sample_rate;     /* horizontal sampling rate in Hz */
    R32 ver_sample_rate;     /* vertical sampling rate in Hz */
    R32 frame_rate;          /* temporal sampling rate or frame rate in Hz */
    R32 time_offset;         /* time offset from sync to first pixel */
    R32 gamma;               /* gamma value */
    R32 black_level;         /* black level code value */
    R32 black_gain;          /* black gain */
    R32 break_point;         /* breakpoint */
    R32 white_level;         /* reference white level code value */
    R32 integration_times;   /* integration time(s) */
    U8  reserved[76];        /* reserved for future use (padding) */
} DPXTelevisionIndustryHeader;

#pragma pack(pop)


// --- endianness flipping for Cineon/DPX structs ---

#define FLIP_INT32_VAL(val)  val = LXEndianSwap_uint32(val);
#define FLIP_FLOAT_VAL(val)  val = LXEndianSwap_float(val);
#define FLIP_INT16_VAL(val)  val = LXEndianSwap_uint16(val);

void flipEndian_dpxFileInfo(DPXFileInformation *dpxFileInfo)
{
    FLIP_INT32_VAL(dpxFileInfo->offset);
    FLIP_INT32_VAL(dpxFileInfo->file_size);
    FLIP_INT32_VAL(dpxFileInfo->ditto_key);
    FLIP_INT32_VAL(dpxFileInfo->gen_hdr_size);
    FLIP_INT32_VAL(dpxFileInfo->ind_hdr_size);
    FLIP_INT32_VAL(dpxFileInfo->user_data_size);
}

void flipEndian_dpxImageInfo(DPXImageInformationHeader *dpxImageInfo)
{
    FLIP_INT16_VAL(dpxImageInfo->orientation);
    FLIP_INT16_VAL(dpxImageInfo->num_elements);
    FLIP_INT32_VAL(dpxImageInfo->pixels_per_line);
    FLIP_INT32_VAL(dpxImageInfo->lines_per_image_ele);
    
    int i;
    for (i = 0; i < 8; i++) {
        FLIP_INT32_VAL( dpxImageInfo->image_element[i].data_sign );
        FLIP_INT32_VAL( dpxImageInfo->image_element[i].ref_low_data );
        FLIP_INT32_VAL( dpxImageInfo->image_element[i].ref_high_data );
        FLIP_FLOAT_VAL( dpxImageInfo->image_element[i].ref_low_quantity );
        FLIP_FLOAT_VAL( dpxImageInfo->image_element[i].ref_high_quantity );
        FLIP_INT16_VAL( dpxImageInfo->image_element[i].packing );
        FLIP_INT16_VAL( dpxImageInfo->image_element[i].encoding );
        FLIP_INT32_VAL( dpxImageInfo->image_element[i].data_offset );
        FLIP_INT32_VAL( dpxImageInfo->image_element[i].eol_padding );
        FLIP_INT32_VAL( dpxImageInfo->image_element[i].eo_image_padding );
    }
}

void flipEndian_cinFileInfo(CineonFileInformation *cinFileInfo)
{
    FLIP_INT32_VAL(cinFileInfo->offset);
    FLIP_INT32_VAL(cinFileInfo->gen_hdr_size);
    FLIP_INT32_VAL(cinFileInfo->ind_hdr_size);
    FLIP_INT32_VAL(cinFileInfo->user_data_size);
    FLIP_INT32_VAL(cinFileInfo->file_size);
}

void flipEndian_cinImageInfo(CineonImageInformationHeader *cinImageInfo)
{
    FLIP_FLOAT_VAL(cinImageInfo->white_point[0]);
    FLIP_FLOAT_VAL(cinImageInfo->white_point[1]);
    FLIP_FLOAT_VAL(cinImageInfo->red_primary[0]);
    FLIP_FLOAT_VAL(cinImageInfo->red_primary[1]);
    FLIP_FLOAT_VAL(cinImageInfo->green_primary[0]);
    FLIP_FLOAT_VAL(cinImageInfo->green_primary[1]);
    FLIP_FLOAT_VAL(cinImageInfo->blue_primary[0]);
    FLIP_FLOAT_VAL(cinImageInfo->blue_primary[1]);
    int i;
    for (i = 0; i < 8; i++) {
        FLIP_INT32_VAL( cinImageInfo->channel_spec[i].width );
        FLIP_INT32_VAL( cinImageInfo->channel_spec[i].height );
        FLIP_FLOAT_VAL( cinImageInfo->channel_spec[i].min_data_value );
        FLIP_FLOAT_VAL( cinImageInfo->channel_spec[i].min_quantity );
        FLIP_FLOAT_VAL( cinImageInfo->channel_spec[i].max_data_value );
        FLIP_FLOAT_VAL( cinImageInfo->channel_spec[i].max_quantity );
    }
}

void flipEndian_cinImageDataInfo(CineonImageDataInformation *cinDataInfo)
{
    FLIP_INT32_VAL(cinDataInfo->line_padding_bytes);
    FLIP_INT32_VAL(cinDataInfo->channel_padding_bytes);
}


#pragma mark --- pixel crunching for DPX reading ---

LXPixelBufferRef LXPixelBufferFromCineonData_rgb_float16(uint8_t *cinData, size_t dataLen,
                                                        LXCineonDataInfo info,
                                                        const LXBool flipEndian,
                                                        LXError *outError)
{
    if (info.descriptor != 50)  // only RGB format supported
        return NULL;

    DEBUGLOG("%s\n", __func__);

    // first, compute the row length and pixel size for the pixel format
    const int bpp = info.bitDepth;
    const LXInteger w = info.width;
    const LXInteger h = info.height;
    const LXBool isTopToBottom = (info.orientation == 0);
    
    info.pixelBytes = 0;
    info.rowStartPadding = 0;
    info.rowBytes = 0;
    
    if (bpp == 8) {
        info.pixelBytes = 3;
        info.rowBytes = w * info.pixelBytes + info.rowEndPadding;
        
        DEBUGLOG("8-bit dpx image: w %i, padding %i, packing %i\n", (int)w, info.rowEndPadding, info.packing);
    }
    else if (bpp == 10) {
        if (info.packing == 0) {
            // stupid tightly packed format: 30-bit pixels have no padding at all, so there are 16 pixels packed in 60 bytes
            info.pixelBytes = 0;            
            int bitsPerRow = (w * 30);
            info.rowBytes = ceil(bitsPerRow / 8.0);
            
            // .. on second thought, not really worth bothering about this shit: only file that used this was some ImageMagick sample
            LXErrorSet(outError, 1763, "unsupported packing: 10-bit tight packing not supported");
            return NULL;
        }
        else {
            info.pixelBytes = 4;
            info.rowBytes = w * info.pixelBytes + info.rowEndPadding;
        }
        
        DEBUGLOG("10-bit dpx image: packing %i, rowBytes %i\n", info.packing, (int)info.rowBytes);
    }
    else if (bpp == 16) {
        if (info.packing <= 4)  // use tightest packing - 48 bits (6 bytes) for each RGB pixel
            info.pixelBytes = 6;
        else {
            info.pixelBytes = 8;
            if (info.packing == Cineon32bitRightPacking)
                info.rowStartPadding = 2;
        }
        info.rowBytes = w * info.pixelBytes + info.rowEndPadding;
    }
    else {
        LXPrintf("** error: unsupported Cineon bits per pixel for float conversion (%i)\n", bpp);
        LXErrorSet(outError, 1763, "unsupported bit depth");
        return NULL;
    }
    
    size_t expectedSize = info.rowBytes * info.height;
    if (dataLen < expectedSize) {
        LXPrintf("** error: loading Cineon image data failed (expected %i bytes, got %i; size %i * %i, rb %i; rowendpad %i, pack %i)\n", (int)expectedSize, (int)dataLen,
                                        (int)w, (int)h, (int)info.rowBytes, (int)info.rowEndPadding, (int)info.packing);
        
        LXErrorSet(outError, 1763, "invalid length of data in file");
        return NULL;
    }
    

    // create pixbuf data
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_FLOAT16, outError);
    if ( !newPixbuf)
        return NULL;


    // temp RGBA buffer for float->half conversion
    float *tempFloatBuf = _lx_malloc(w * 4 * sizeof(float));


    size_t dstRowBytes = 0;
    uint8_t *dstHalfBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);

    const size_t srcRowBytes = info.rowBytes;
    LXUInteger i;

    const float scale = 1.0 / (float)(info.inputMax - info.inputMin);
    const unsigned int inputMin = info.inputMin;
    const unsigned int inputMax = info.inputMax;
    #pragma unused (inputMin, inputMax)

    // convert image data according to pixel format
    
    if (bpp == 8) {
        for (i = 0; i < h; i++) {
            register LXUInteger j;
            register float * LXRESTRICT dstBuf = tempFloatBuf;
            uint8_t * LXRESTRICT srcBuf = isTopToBottom ? (uint8_t *)(cinData + i * srcRowBytes) :
                                                                (uint8_t *)(cinData + (h-1-i) * srcRowBytes);
            for (j = 0; j < w; j++) {                
                unsigned int r = srcBuf[0] - inputMin;
                unsigned int g = srcBuf[1] - inputMin;
                unsigned int b = srcBuf[2] - inputMin;
                srcBuf += 3;
                dstBuf[0] = (float)r * scale;
                dstBuf[1] = (float)g * scale;
                dstBuf[2] = (float)b * scale;
                dstBuf[3] = 1.0f;
                dstBuf += 4;
            }
            
            LXConvertFloatToHalfArray(tempFloatBuf,  (LXHalf *) (dstHalfBuf + dstRowBytes * i),  w*4);
        }
    }
    else if (bpp == 10) {
        const LXBool packToLeft = (info.packing == 1 || info.packing == 3 || info.packing == 5);
        const unsigned int sh_r =   packToLeft ? 22 : 20;
        const unsigned int sh_g =   packToLeft ? 12 : 10;
        const unsigned int sh_b =   packToLeft ?  2 :  0;
        const unsigned int mask = 0x3ff;

        for (i = 0; i < h; i++) {
            register LXUInteger j;
            register float * LXRESTRICT dstBuf = tempFloatBuf;
            register unsigned int * LXRESTRICT srcBuf = isTopToBottom ? (unsigned int *)(cinData + i * srcRowBytes) :
                                                                        (unsigned int *)(cinData + (h-1-i) * srcRowBytes);
            if ( !flipEndian) {
                for (j = 0; j < w; j++) {
                    unsigned int v = *srcBuf;
                    srcBuf++;
                #if defined(LXVEC)
                    ((int32_t *)dstBuf)[0] = ((v >> sh_r) & mask);
                    ((int32_t *)dstBuf)[1] = ((v >> sh_g) & mask);
                    ((int32_t *)dstBuf)[2] = ((v >> sh_b) & mask);
                    ((int32_t *)dstBuf)[3] = inputMax;
                #else
                    dstBuf[0] = (float)(((v >> sh_r) & mask) - inputMin) * scale;
                    dstBuf[1] = (float)(((v >> sh_g) & mask) - inputMin) * scale;
                    dstBuf[2] = (float)(((v >> sh_b) & mask) - inputMin) * scale;
                    dstBuf[3] = 1.0f;                    
                #endif
                    dstBuf += 4;
                }
            } else {
                for (j = 0; j < w; j++) {
                    unsigned int v = *srcBuf;
                    srcBuf++;
                    v = LXEndianSwap_uint32(v);
                    
                #if defined(LXVEC)
                    ((int32_t *)dstBuf)[0] = ((v >> sh_r) & mask);
                    ((int32_t *)dstBuf)[1] = ((v >> sh_g) & mask);
                    ((int32_t *)dstBuf)[2] = ((v >> sh_b) & mask);
                    ((int32_t *)dstBuf)[3] = inputMax;
                #else
                    dstBuf[0] = (float)(((v >> sh_r) & mask) - inputMin) * scale;
                    dstBuf[1] = (float)(((v >> sh_g) & mask) - inputMin) * scale;
                    dstBuf[2] = (float)(((v >> sh_b) & mask) - inputMin) * scale;
                    dstBuf[3] = 1.0f;                    
                #endif
                    dstBuf += 4;
                }
            }
            
            #if defined(LXVEC)
            LXPxConvert_int32_to_float32_withRange_inplace_VEC((int32_t *)tempFloatBuf, w*4,  inputMin, inputMax);
            #endif
            
            LXConvertFloatToHalfArray(tempFloatBuf,  (LXHalf *) (dstHalfBuf + dstRowBytes * i),  w*4);
        }
    }
    else if (bpp == 16) {
        const size_t rowOffset = info.rowStartPadding;
        const size_t pixelShorts = info.pixelBytes / 2;
        
        for (i = 0; i < h; i++) {
            register LXUInteger j;
            register float * LXRESTRICT dstBuf = tempFloatBuf;
            unsigned short * LXRESTRICT srcBuf = isTopToBottom ? (unsigned short *)(cinData + i * srcRowBytes + rowOffset) :
                                                                 (unsigned short *)(cinData + (h-1-i) * srcRowBytes + rowOffset);
            
            if ( !flipEndian) {
                for (j = 0; j < w; j++) {
                    unsigned int r = srcBuf[0];
                    unsigned int g = srcBuf[1];
                    unsigned int b = srcBuf[2];
                    srcBuf += pixelShorts;
                #if defined(LXVEC)
                    ((int32_t *)dstBuf)[0] = r;
                    ((int32_t *)dstBuf)[1] = g;
                    ((int32_t *)dstBuf)[2] = b;
                    ((int32_t *)dstBuf)[3] = inputMax;
                #else
                    dstBuf[0] = (float)(r - inputMin) * scale;
                    dstBuf[1] = (float)(g - inputMin) * scale;
                    dstBuf[2] = (float)(b - inputMin) * scale;
                    dstBuf[3] = 1.0f;
                #endif
                    dstBuf += 4;
                }
            } else {
                for (j = 0; j < w; j++) {
                    unsigned int r = LXEndianSwap_uint16(srcBuf[0]);
                    unsigned int g = LXEndianSwap_uint16(srcBuf[1]);
                    unsigned int b = LXEndianSwap_uint16(srcBuf[2]);
                    srcBuf += pixelShorts;
                #if defined(LXVEC)
                    ((int32_t *)dstBuf)[0] = r;
                    ((int32_t *)dstBuf)[1] = g;
                    ((int32_t *)dstBuf)[2] = b;
                    ((int32_t *)dstBuf)[3] = inputMax;
                #else
                    dstBuf[0] = (float)(r - inputMin) * scale;
                    dstBuf[1] = (float)(g - inputMin) * scale;
                    dstBuf[2] = (float)(b - inputMin) * scale;
                    dstBuf[3] = 1.0f;
                #endif
                    dstBuf += 4;
                }
            }
            
            #if defined(LXVEC)
            LXPxConvert_int32_to_float32_withRange_inplace_VEC((int32_t *)tempFloatBuf, w*4,  inputMin, inputMax);
            #endif
            
            LXConvertFloatToHalfArray(tempFloatBuf, (LXHalf *) (dstHalfBuf + dstRowBytes * i),  w*4);
        }
    }
    
    _lx_free(tempFloatBuf);
    
    LXPixelBufferUnlockPixels(newPixbuf);
    return newPixbuf;
}


LXPixelBufferRef LXPixelBufferFromCineonData_yuv(uint8_t *cinData, size_t dataLen,
                                                     LXCineonDataInfo info,
                                                     const LXBool flipEndian,
                                                     const LXInteger preferredBitDepth,
                                                     LXError *outError)
{
	if (info.descriptor != 100)  // only YCbCr 4:2:2 supported
		return NO;
		
    const LXInteger w = info.width;
    const LXInteger h = info.height;
    const int srcBytesPerPixel = 2;
    const size_t srcRowBytes = (info.rowBytes > 0) ? info.rowBytes : (w * srcBytesPerPixel + info.rowEndPadding);

    // if requested, we'll perform the conversion to raw float YUV here
    const LXPixelFormat pxFormat = (preferredBitDepth == 16) ? kLX_RGBA_FLOAT16 : kLX_YCbCr422_INT8;
    //const int dstBytesPerPixel = (preferredBitDepth == 16) ? 8 : 2;
    
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, pxFormat, outError);
    if ( !newPixbuf)
        return NULL;

    size_t dstRowBytes = 0;
    uint8_t *dstBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);

    DEBUGLOG("%s: src rb %i, dst %i - requested bit depth is %i\n", __func__, (int)srcRowBytes, (int)dstRowBytes, (int)preferredBitDepth);

    if (srcRowBytes == dstRowBytes) {
        memcpy(dstBuf, cinData, dstRowBytes * h);
    } else {
        if (pxFormat == kLX_YCbCr422_INT8) {
            LXInteger y;
            size_t rb = MIN(srcRowBytes, dstRowBytes);
            for (y = 0; y < h; y++) {
                uint8_t * LXRESTRICT src = cinData + srcRowBytes * y;
                uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
                memcpy(dst, src, rb);
            }
        } else {
            LXPxConvert_YCbCr422_to_RGBA_float16_rawYUV(w, h, cinData, srcRowBytes, (LXHalf *)dstBuf, dstRowBytes);
        }
    }

    LXPixelBufferUnlockPixels(newPixbuf);
    return newPixbuf;
}


LXPixelBufferRef LXPixelBufferFromCineonData_rgba_int8(uint8_t *cinData, size_t dataLen,
                                                        LXCineonDataInfo info,
                                                        LXError *outError)
{
	if (info.descriptor != 51 || info.bitDepth != 8)  // only RGBA supported
		return NO;
		
    const LXInteger w = info.width;
    const LXInteger h = info.height;
    const int bytesPerPixel = 4;
    const size_t srcRowBytes = (info.rowBytes > 0) ? info.rowBytes : (w * bytesPerPixel + info.rowEndPadding);
    
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError);
    if ( !newPixbuf)
        return NULL;

    size_t dstRowBytes = 0;
    uint8_t *dstBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);

    ///DEBUGLOG("%s: %i, %i\n", __func__, srcRowBytes, dstRowBytes);

    if (srcRowBytes == dstRowBytes) {
        memcpy(dstBuf, cinData, dstRowBytes * h);
    } else {
        LXInteger y;
        size_t rb = MIN(srcRowBytes, dstRowBytes);
        for (y = 0; y < h; y++) {
            uint8_t * LXRESTRICT src = cinData + srcRowBytes * y;
            uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
            memcpy(dst, src, rb);
        }
    }

    LXPixelBufferUnlockPixels(newPixbuf);
    return newPixbuf;
}

LXPixelBufferRef LXPixelBufferFromCineonData_lum_int8(uint8_t *cinData, size_t dataLen,
                                                        LXCineonDataInfo info,
                                                        LXError *outError)
{
	if (info.bitDepth != 8)  // only 8-bit supported
		return NO;
		
    const LXInteger w = info.width;
    const LXInteger h = info.height;
    const int bytesPerPixel = 1;
    const size_t srcRowBytes = (info.rowBytes > 0) ? info.rowBytes : (w * bytesPerPixel + info.rowEndPadding);
    
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_Luminance_INT8, outError);
    if ( !newPixbuf)
        return NULL;

    size_t dstRowBytes = 0;
    uint8_t *dstBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);

    ///DEBUGLOG("%s: %i, %i\n", __func__, srcRowBytes, dstRowBytes);

    if (srcRowBytes == dstRowBytes) {
        memcpy(dstBuf, cinData, dstRowBytes * h);
    } else {
        LXInteger y;
        size_t rb = MIN(srcRowBytes, dstRowBytes);
        for (y = 0; y < h; y++) {
            uint8_t * LXRESTRICT src = cinData + srcRowBytes * y;
            uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
            memcpy(dst, src, rb);
        }
    }

    LXPixelBufferUnlockPixels(newPixbuf);
    return newPixbuf;
}


#pragma mark --- read API ---

LXPixelBufferRef LXPixelBufferCreateFromDPXImageAtPath(LXUnibuffer unipath, LXMapPtr properties, LXError *outError)
{
    LXFilePtr file = NULL;
    if ( !LXOpenFileForReadingWithUnipath(unipath.unistr, unipath.numOfChar16, &file)) {
        LXPrintf("*** %s: failed to open file\n", __func__);
        LXErrorSet(outError, 1760, "could not open file");
        return NULL;
    }
    
    DEBUGLOG("... got dpx file ptr: %p\n", file);
    
    _lx_fseek64(file, 0, SEEK_END);
    size_t fileLen = (size_t)_lx_ftell64(file);
    
    DEBUGLOG("... file size is: %ld\n", (long)fileLen);
    
    uint8_t *fileData = _lx_malloc(fileLen);
    
    _lx_fseek64(file, 0, SEEK_SET);
    size_t bytesRead = _lx_fread(fileData, 1, fileLen, file);
    _lx_fclose(file);
    file = NULL;    
    // done with file
    
    if (bytesRead != fileLen || bytesRead == 0) {
        LXErrorSet(outError, 1761, "error reading from file");
        _lx_free(fileData);
        return NULL;
    }
    
    DEBUGLOG("finished with dpx file, will interpret data\n");

    LXInteger preferredBitDepth = 0;
    if (properties) {
        LXMapGetInteger(properties, kLXPixelBufferFormatRequestKey_PreferredBitsPerChannel, &preferredBitDepth);
    }


    LXPixelBufferRef newPixbuf = NULL;
    
    // ok, got file data, proceed to parse    
    size_t offset = 0;
    LXBool flipEndian = NO;
    LXCineonDataInfo rawInfo;
    memset(&rawInfo, 0, sizeof(rawInfo));
    rawInfo.rowBytes = 0;  // will be computed later once the pixel format is known
    
    unsigned int magicNum = ((unsigned int *)fileData)[0];
    
    if (magicNum == 0x802a5fd7 || magicNum == 0xd75f2a80) {
        // ---- this is a Cineon file ----
        
        if (fileLen < 680) {
            LXErrorSet(outError, 1762, "data is too short");
            goto bail;
        }
        
        // get file info header
        CineonFileInformation *cinFileInfo = (CineonFileInformation *)fileData;
        CineonImageInformationHeader *cinImageInfo = (CineonImageInformationHeader *)(fileData + sizeof(CineonFileInformation));
        CineonChannelSpecifier *spec = cinImageInfo->channel_spec;
        
        // get image data info
        CineonImageDataInformation *cinDataInfo = (CineonImageDataInformation *)(fileData + 680);
        
        //if magic number is upside down, we need to flip endianness
        if (magicNum == 0xd75f2a80) {
            ///DEBUGLOG("...flipping cineon header endianness...\n");
            flipEndian = YES;
            flipEndian_cinFileInfo(cinFileInfo);
            flipEndian_cinImageInfo(cinImageInfo);
            flipEndian_cinImageDataInfo(cinDataInfo);
        }
        
        DEBUGLOG("cineon orientation %i, numchannels %i\n", cinImageInfo->orientation, cinImageInfo->num_channels);
        DEBUGLOG(".. channel1 bpp %i, size %i * %i, min %f %f, max %f %f\n", spec->bits_per_pixel, spec->width, spec->height,
                            spec->min_data_value, spec->min_quantity, spec->max_data_value, spec->max_quantity);
        
        DEBUGLOG(".. interleave %i, packing %i, signed %i\n", cinDataInfo->interleave, cinDataInfo->packing, cinDataInfo->signedness);
        DEBUGLOG(".. paddings: %i, %i\n", cinDataInfo->line_padding_bytes, cinDataInfo->channel_padding_bytes);
    
        // check that we can load this type of image
        if (cinDataInfo->interleave != 0) {
            LXPrintf("unsupported image format: Cineon image is planar\n");
            LXErrorSet(outError, 1763, "unsupported image format: is planar");
            goto bail;
        }
        if (cinDataInfo->signedness != 0) {
            LXPrintf("unsupported image format: Cineon image is signed\n");
            LXErrorSet(outError, 1763, "unsupported image format: is signed");
            goto bail;
        }
        if (cinImageInfo->orientation > 1) {
            LXPrintf("unsupported image format: Cineon orientation is %i\n", cinImageInfo->orientation);
            LXErrorSet(outError, 1763, "unsupported image format: unknown orientation");
            goto bail;
        }
        
        rawInfo.bitDepth = spec->bits_per_pixel;
        rawInfo.width = spec->width;
        rawInfo.height = spec->height;
        rawInfo.inputMin = spec->min_data_value;
        rawInfo.inputMax = spec->max_data_value;
        rawInfo.packing = cinDataInfo->packing;
        rawInfo.rowEndPadding = cinDataInfo->line_padding_bytes;
        rawInfo.orientation = cinImageInfo->orientation;
        rawInfo.descriptor = 50;  // RGB
        offset = cinFileInfo->offset;
    }
    
    else if (magicNum == DPX_MAGIC_NUM || magicNum == DPX_MAGIC_NUM_FLIPPED) {
        // ---- this is a DPX file ----
        
        if (fileLen < sizeof(DPXFileInformation)+sizeof(DPXImageInformationHeader)) {
            LXErrorSet(outError, 1762, "data is too short");
            goto bail;
        }
        
        // get file info header
        DPXFileInformation *dpxFileInfo = (DPXFileInformation *)fileData;
        DPXImageInformationHeader *dpxImageInfo = (DPXImageInformationHeader *)(fileData + sizeof(DPXFileInformation));
        DPXImageOrientationHeader *dpxOrientation = (DPXImageOrientationHeader *)(fileData + sizeof(DPXFileInformation) + sizeof(DPXImageInformationHeader));

        if (magicNum == DPX_MAGIC_NUM_FLIPPED) {
            ///DEBUGLOG("...flipping dpx header endianness...\n");
            flipEndian = YES;
            flipEndian_dpxFileInfo(dpxFileInfo);
            flipEndian_dpxImageInfo(dpxImageInfo);
        }
        
        DEBUGLOG("dpx version: '%s' (file needs byte flip: %i); filename '%s', creator '%s'\n", dpxFileInfo->vers, flipEndian, dpxFileInfo->file_name, dpxFileInfo->creator);
        DEBUGLOG("   ..file size: %u, offset to data: %u, gen header size: %u, industry header size: %u, user data size: %u\n",
                    dpxFileInfo->file_size, dpxFileInfo->offset, dpxFileInfo->gen_hdr_size, dpxFileInfo->ind_hdr_size, dpxFileInfo->user_data_size);
        
        debugPrintDPXImageElementContents(&(dpxImageInfo->image_element[0]));
        
        debugPrintDPXImageOrientationHeader(dpxOrientation);
        /*
        DEBUGLOG(".. dpx size %u * %u, pixeldescriptor %u, bitsize %u, orientation %u\n", dpxImageInfo->pixels_per_line, dpxImageInfo->lines_per_image_ele,
                            dpxImageInfo->image_element[0].descriptor, dpxImageInfo->image_element[0].bit_size, dpxImageInfo->orientation);
                    
        DEBUGLOG(".. dpx low data %u, quantity %f / high data %u, quantity %f\n", dpxImageInfo->image_element[0].ref_low_data, dpxImageInfo->image_element[0].ref_low_quantity,
                            dpxImageInfo->image_element[0].ref_high_data, dpxImageInfo->image_element[0].ref_high_quantity);
                    
        DEBUGLOG(".. dpx packing %u, encoding %u, rowpadding %u, dataoffset %u, \n",
                            dpxImageInfo->image_element[0].packing, dpxImageInfo->image_element[0].eol_padding);
        */
    
        if (dpxImageInfo->image_element[0].data_sign != 0) {
            LXPrintf("** unsupported image format: DPX image is signed\n");
            LXErrorSet(outError, 1763, "unsupported image format: is signed");
            goto bail;
        }
        if (dpxImageInfo->orientation > 1) {
            LXPrintf("** unsupported image format: DPX orientation is %u\n", dpxImageInfo->orientation);
            LXErrorSet(outError, 1763, "unsupported image format: unknown orientation");
            goto bail;
        }

        rawInfo.bitDepth = dpxImageInfo->image_element[0].bit_size;
        rawInfo.width = dpxImageInfo->pixels_per_line;
        rawInfo.height = dpxImageInfo->lines_per_image_ele;
        rawInfo.inputMin = dpxImageInfo->image_element[0].ref_low_data;
        rawInfo.inputMax = dpxImageInfo->image_element[0].ref_high_data;
        rawInfo.packing = dpxImageInfo->image_element[0].packing;
        rawInfo.rowEndPadding = dpxImageInfo->image_element[0].eol_padding;
        rawInfo.orientation = dpxImageInfo->orientation;
        offset = dpxFileInfo->offset;
        
        rawInfo.descriptor = dpxImageInfo->image_element[0].descriptor;
        switch (rawInfo.descriptor) {
            case 50:  // RGB
                break;
                
            case 100:  // 4:2:2 YCbCr
                rawInfo.bitDepth = 16;
                break;                
        }
    }
    else {
        LXPrintf("** invalid Cin/DPX magic number: 0x%x\n", magicNum);
        LXErrorSet(outError, 1763, "invalid magic number for cin/dpx file");
        goto bail;
    }
    
    if (rawInfo.width < 1 || rawInfo.height < 1) {
        LXPrintf("** error: invalid header for Cineon/DPX file (format ID: %x)\n", magicNum);
        LXErrorSet(outError, 1763, "invalid header for dpx file");
        goto bail;
    }

    // some DPX files seem to lack the encoded max value, so compute it here
    if (rawInfo.inputMax <= 0 || rawInfo.inputMax >= INT32_MAX) {
        rawInfo.inputMax = (1 << rawInfo.bitDepth) - 1;
        DEBUGLOG("... DPX file lacks inputMax, did fix to %u\n", rawInfo.inputMax);
    } else DEBUGLOG("using rawInfo.inputMax %u\n", rawInfo.inputMax);
    
    // fix other weird values
    if (rawInfo.inputMin >= INT32_MAX)
        rawInfo.inputMin = 0;
    if (rawInfo.rowEndPadding >= INT32_MAX)
        rawInfo.rowEndPadding = 0;
    
    DEBUGLOG("%s: going to load image of type %i (bpp %i); size %i * %i\n", __func__, (int)rawInfo.descriptor, (int)rawInfo.bitDepth, (int)rawInfo.width, (int)rawInfo.height);

/*
    -- DPX format descriptor enum values: --
    0		User defined (or unspecified single component)
    1		Red (R)
    2		Green (G)
    3		Blue (B)
    4		Alpha (matte)
    5		
    6		Luminance (Y)
    7		Chrominance (CB, CR, subsampled by two)
    8		Depth (Z)
    9		Composite video
    10 - 49		Reserved for future single components
    50		R,G,B
    51		R,G,B,alpha
    52		Alpha, B, G, R
    53 - 99		Reserved for future RGB ++ formats
    100		CB, Y, CR, Y (4:2:2) -- based on SMPTE 125M
    101		CB, Y, a, CR, Y, a (4:2:2:4)
    102		CB, Y, CR (4:4:4)
    103		CB, Y, CR, a (4:4:4:4)
    104 - 149	Reserved for future CBYCR ++ formats
    150		User-defined 2 component element
    151		User-defined 3 component element
    152		User-defined 4 component element
    153		User-defined 5 component element
    154		User-defined 6 component element
    155		User-defined 7 component element
    156		User-defined 8 component element
    157 - 254	Reserved for future formats
*/

    // headers were successfully read, so proceed to load image data
    switch (rawInfo.descriptor) {
        case 50:
            newPixbuf = LXPixelBufferFromCineonData_rgb_float16(fileData+offset, fileLen-offset, rawInfo, flipEndian, outError);
            break;
            
        case 100:
            newPixbuf = LXPixelBufferFromCineonData_yuv(fileData+offset, fileLen-offset, rawInfo, flipEndian, preferredBitDepth, outError);
            break;
        
        case 51:
            newPixbuf = LXPixelBufferFromCineonData_rgba_int8(fileData+offset, fileLen-offset, rawInfo, outError);
            break;
            
        case 0:
        case 1: case 2: case 3: case 4:
        case 6: case 8:
            newPixbuf = LXPixelBufferFromCineonData_lum_int8(fileData+offset, fileLen-offset, rawInfo, outError);
            break;
        
        default:
            LXPrintf("** unsupported image format: DPX descriptor is %i (bit depth is %i)\n", rawInfo.descriptor, rawInfo.bitDepth);
            LXErrorSet(outError, 1763, "unsupported cineon/dpx image format");
            goto bail;
    }

bail:
    _lx_free(fileData);
    return newPixbuf;
}


#pragma mark --- pixel pushers for DPX writing ---

// supported write formats
enum {
    kLX_DPXWrite_RGB_INT8 = 1,
    kLX_DPXWrite_RGBA_INT8,
    
    kLX_DPXWrite_RGB_INT10 = 10,
    kLX_DPXWrite_RGB_INT16,
    
    kLX_DPXWrite_YCbCr422_INT8 = 20,
    
    kLX_DPXWrite_Luminance_INT8 = 30
};


static uint8_t *createDPXHeader(size_t dataSize,
                                LXCineonDataInfo *dataInfo,
                                LXInteger writeFormat,
                                LXMapPtr metadata,
                                size_t *outHeaderDataSize)
{
    DPXFileInformation dpxFileInfo;
    DPXImageInformationHeader dpxImageInfo;
    DPXImageOrientationHeader dpxOrientationInfo;
    DPXFilmIndustryHeader dpxFilmHeader;
    DPXTelevisionIndustryHeader dpxTelevisionHeader;
    memset(&dpxFileInfo, 0, sizeof(dpxFileInfo));
    memset(&dpxImageInfo, 0, sizeof(dpxImageInfo));
    memset(&dpxOrientationInfo, 0, sizeof(dpxOrientationInfo));
    memset(&dpxFilmHeader, 0, sizeof(dpxFilmHeader));
    memset(&dpxTelevisionHeader, 0, sizeof(dpxTelevisionHeader));
 
	size_t headerLen = sizeof(dpxFileInfo) + sizeof(dpxImageInfo) + sizeof(dpxOrientationInfo) + sizeof(dpxFilmHeader) + sizeof(dpxTelevisionHeader);
       
        // image size info
        dpxFileInfo.magic_num = DPX_MAGIC_NUM;
        dpxFileInfo.offset = headerLen;
        dpxFileInfo.file_size = dpxFileInfo.offset + dataSize;
        dpxFileInfo.gen_hdr_size = sizeof(dpxFileInfo) + sizeof(dpxImageInfo) + sizeof(dpxOrientationInfo);
        dpxFileInfo.ind_hdr_size = sizeof(dpxFilmHeader) + sizeof(dpxTelevisionHeader);
        dpxFileInfo.user_data_size = 0;

        // image metadata
        const char *verStr = "V1.0";
        memcpy(dpxFileInfo.vers, verStr, strlen(verStr));
        
        char *creatorStr = NULL;
        size_t creatorStrLen = 0;
        if (metadata && LXMapGetInteger(metadata, "creator_utf8", (LXInteger *)(&creatorStr)) && creatorStr) {
            creatorStrLen = MIN(99, strlen(creatorStr));
            memcpy(dpxFileInfo.creator, creatorStr, creatorStrLen);
        }
        char *mdStr = NULL;
        if (metadata && LXMapGetInteger(metadata, "filename_utf8", (LXInteger *)(&mdStr)) && mdStr) {
            size_t len = MIN(99, strlen(mdStr));
            memcpy(dpxFileInfo.file_name, mdStr, len);
            mdStr = NULL;
        }
        if (metadata && LXMapGetInteger(metadata, "project_utf8", (LXInteger *)(&mdStr)) && mdStr) {
            size_t len = MIN(199, strlen(mdStr));
            memcpy(dpxFileInfo.project, mdStr, len);
            mdStr = NULL;
        }
        
        // image data info
        dpxImageInfo.orientation = 0;
        dpxImageInfo.num_elements = 1;  // the single element is the RGB pixel data
        dpxImageInfo.pixels_per_line = dataInfo->width;
        dpxImageInfo.lines_per_image_ele = dataInfo->height;
        
        // image element info
        {
        DPXImageElementInfo *elInfo = &(dpxImageInfo.image_element[0]);

        int descriptor = 0;  // tells the image pixel data format
        switch (writeFormat) {
            default:
            case kLX_DPXWrite_RGB_INT8:    
            case kLX_DPXWrite_RGB_INT10:
            case kLX_DPXWrite_RGB_INT16:        descriptor = 50;  break;    // RGB
            
            case kLX_DPXWrite_RGBA_INT8:        descriptor = 51;  break;    // RGBA
            
            case kLX_DPXWrite_YCbCr422_INT8:    descriptor = 100;  break;   // CB, Y, CR, Y (4:2:2)
            
            case kLX_DPXWrite_Luminance_INT8:   descriptor = 6;  break;     // luminance
        }
        elInfo->descriptor = descriptor;
        elInfo->data_sign = 0;    // pixel data is unsigned
        elInfo->bit_size = dataInfo->bitDepth;
        elInfo->packing = dataInfo->packing;
        elInfo->eol_padding = dataInfo->rowEndPadding;
        elInfo->ref_low_data = dataInfo->inputMin;
        elInfo->ref_high_data = dataInfo->inputMax;
        elInfo->ref_low_quantity = 0.0;
        elInfo->ref_high_quantity = 2.046;
        elInfo->data_offset = headerLen;
        }
        
        // mysterious orientation info
        dpxOrientationInfo.x_orig_size = dataInfo->width;
        dpxOrientationInfo.y_orig_size = dataInfo->height;
        dpxOrientationInfo.pixel_aspect[0] = UINT32_MAX;
        dpxOrientationInfo.pixel_aspect[1] = UINT32_MAX;
        // this value seems to be common in files
        dpxOrientationInfo.border[0] = dpxOrientationInfo.border[1] = dpxOrientationInfo.border[2] = dpxOrientationInfo.border[3] = 65535;
        if (creatorStr) {
            memcpy(dpxOrientationInfo.input_dev, creatorStr, MIN(31, creatorStrLen));
        }
        
    uint8_t *headerData = _lx_malloc(headerLen);
    size_t n = 0;
    memcpy(headerData, &dpxFileInfo, sizeof(dpxFileInfo));
    n += sizeof(dpxFileInfo);
    memcpy(headerData+n, &dpxImageInfo, sizeof(dpxImageInfo));
    n += sizeof(dpxImageInfo);
    memcpy(headerData+n, &dpxOrientationInfo, sizeof(dpxOrientationInfo));
    n += sizeof(dpxOrientationInfo);
    memcpy(headerData+n, &dpxFilmHeader, sizeof(dpxFilmHeader));
    n += sizeof(dpxFilmHeader);
    memcpy(headerData+n, &dpxTelevisionHeader, sizeof(dpxTelevisionHeader));
    //n += sizeof(dpxTelevisionHeader);

    *outHeaderDataSize = headerLen;
    return headerData;
}


static void createTempBufWithCallerProperties(LXMapPtr metadata, size_t rgbDataSize, uint8_t **outRgbBuffer, LXBool *outBufferNeedsFree)
{
    uint8_t *rgbBuffer = NULL;
    LXBool bufferNeedsFree = NO;

    // the caller may have provided a handle for this temp buffer
    uint8_t **tempBufHandleFromCaller = NULL;
    size_t *tempBufSizePtrFromCaller = NULL;
    if (metadata && LXMapGetInteger(metadata, "tempBuf::dataHandle", (LXInteger *)(&tempBufHandleFromCaller))
                 && LXMapGetInteger(metadata, "tempBuf::dataSizePtr", (LXInteger *)(&tempBufSizePtrFromCaller))
                 && tempBufHandleFromCaller != NULL && tempBufSizePtrFromCaller != NULL)
    {
        if (*tempBufHandleFromCaller) {
            if (*tempBufSizePtrFromCaller < rgbDataSize) {
                _lx_free(*tempBufHandleFromCaller);
                *tempBufHandleFromCaller = _lx_malloc(rgbDataSize);
                *tempBufSizePtrFromCaller = rgbDataSize;
            }
        } else {
            *tempBufHandleFromCaller = _lx_malloc(rgbDataSize);
            *tempBufSizePtrFromCaller = rgbDataSize;
        }
        bufferNeedsFree = NO;
        rgbBuffer = *tempBufHandleFromCaller;
    }

    if ( !rgbBuffer) { 
        bufferNeedsFree = YES;
        rgbBuffer = _lx_malloc(rgbDataSize);
    }
    
    *outRgbBuffer = rgbBuffer;
    *outBufferNeedsFree = bufferNeedsFree;
}


static int writeDPXImageToFile_luminance_int8(FILE *file, LXMapPtr metadata,
                                              uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t rowBytes)
{
    DEBUGLOG("%s\n", __func__);
    const size_t srcDataSize = rowBytes * h;
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = 0;
    info.bitDepth = 8;
    info.pixelBytes = 1;
    info.inputMin = 0;
    info.inputMax = 255;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rowBytes;
    info.rowEndPadding = rowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(srcDataSize, &info, kLX_DPXWrite_Luminance_INT8, metadata, &headerDataSize);
    
    fwrite(header, headerDataSize, 1, file);
    fwrite(srcData, srcDataSize, 1, file);
    
    _lx_free(header);
    return 0;
}


static int writeDPXImageToFile_YCbCr422(FILE *file, LXMapPtr metadata,
                                        uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t rowBytes)
{
    DEBUGLOG("%s\n", __func__);
    const size_t srcDataSize = rowBytes * h;
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = 0;
    info.bitDepth = 16;
    info.pixelBytes = 2;
    info.inputMin = 0;
    info.inputMax = 255;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rowBytes;
    info.rowEndPadding = rowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(srcDataSize, &info, kLX_DPXWrite_YCbCr422_INT8, metadata, &headerDataSize);
    
    fwrite(header, headerDataSize, 1, file);
    fwrite(srcData, srcDataSize, 1, file);   
    
    _lx_free(header);
    return 0;
}

static int writeDPXImageToFile_RGBA_int8(FILE *file, LXMapPtr metadata,
                                        uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t rowBytes)
{
    DEBUGLOG("%s\n", __func__);
    const size_t srcDataSize = rowBytes * h;
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = 0;
    info.bitDepth = 8;
    info.pixelBytes = 4;
    info.inputMin = 0;
    info.inputMax = 255;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rowBytes;
    info.rowEndPadding = rowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(srcDataSize, &info, kLX_DPXWrite_RGBA_INT8, metadata, &headerDataSize);
    
    fwrite(header, headerDataSize, 1, file);
    fwrite(srcData, srcDataSize, 1, file);   
    
    _lx_free(header);
    return 0;
}

static int writeDPXImageToFile_RGBA_to_RGB_int8(FILE *file, LXMapPtr metadata,
                                                uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t srcRowBytes)
{
    DEBUGLOG("%s\n", __func__);
    const int rgbBytesPerPixel = 3;
    const size_t rgbRowBytes = LXAlignedRowBytes(w * rgbBytesPerPixel);
    const size_t rgbDataSize = rgbRowBytes * h;
    uint8_t *rgbBuffer = NULL;  //_lx_malloc(rgbDataSize);
    LXBool bufferNeedsFree = NO;

    createTempBufWithCallerProperties(metadata, rgbDataSize, &rgbBuffer, &bufferNeedsFree);
    
    LXPxConvert_RGBA_to_RGB_int8(w, h, srcData, srcRowBytes, 4,
                                       rgbBuffer, rgbRowBytes, rgbBytesPerPixel);
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = 0;
    info.bitDepth = 8;
    info.pixelBytes = rgbBytesPerPixel;
    info.inputMin = 0;
    info.inputMax = 255;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rgbRowBytes;
    info.rowEndPadding = rgbRowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(rgbDataSize, &info, kLX_DPXWrite_RGB_INT8, metadata, &headerDataSize);
    
    fwrite(header, headerDataSize, 1, file);
    fwrite(rgbBuffer, rgbDataSize, 1, file);   
    
    _lx_free(header);
    if (bufferNeedsFree) _lx_free(rgbBuffer);
    return 0;
}


#define MY_10BIT_PACKING  1   // left-aligned on 8-bit boundaries (this is apparently the only format that Shake understands)

static int writeDPXImageToFile_RGBA_float16_to_RGB_int10(FILE *file, LXMapPtr metadata,
                                                      uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t srcRowBytes)
{
    const int rgbBytesPerPixel = 4;
    const size_t rgbRowBytes = LXAlignedRowBytes(w * rgbBytesPerPixel);
    const size_t rgbDataSize = rgbRowBytes * h;
    uint8_t *rgbBuffer = NULL;
    LXBool bufferNeedsFree = NO;

    createTempBufWithCallerProperties(metadata, rgbDataSize, &rgbBuffer, &bufferNeedsFree);
    
    LXPxConvert_RGB_float16_to_RGB_int10(w, h, (LXHalf *)srcData, srcRowBytes, 4,
                                               rgbBuffer, rgbRowBytes,
                                               1023);  // max coding value
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = MY_10BIT_PACKING;
    info.bitDepth = 10;
    info.pixelBytes = rgbBytesPerPixel;
    info.inputMin = 0;
    info.inputMax = 1023;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rgbRowBytes;
    info.rowEndPadding = rgbRowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(rgbDataSize, &info, kLX_DPXWrite_RGB_INT10, metadata, &headerDataSize);
    
    fwrite(header, headerDataSize, 1, file);
    fwrite(rgbBuffer, rgbDataSize, 1, file);   
    
    _lx_free(header);
    if (bufferNeedsFree) _lx_free(rgbBuffer);
    return 0;
}

static int writeDPXImageToFile_RGBA_float32_to_RGB_int10(FILE *file, LXMapPtr metadata,
                                                      uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t srcRowBytes)
{
    // temp buffer for outdata
    const int rgbBytesPerPixel = 4;
    const size_t rgbRowBytes = LXAlignedRowBytes(w * rgbBytesPerPixel);
    const size_t rgbDataSize = rgbRowBytes * h;
    uint8_t *rgbBuffer = NULL;
    LXBool bufferNeedsFree = NO;

    createTempBufWithCallerProperties(metadata, rgbDataSize, &rgbBuffer, &bufferNeedsFree);
    
    LXPxConvert_RGB_float32_to_RGB_int10(w, h, (float *)srcData, srcRowBytes, 4,
                                               rgbBuffer, rgbRowBytes,
                                               1023);  // max coding value
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = MY_10BIT_PACKING;
    info.bitDepth = 10;
    info.pixelBytes = rgbBytesPerPixel;
    info.inputMin = 0;
    info.inputMax = 1023;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rgbRowBytes;
    info.rowEndPadding = rgbRowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(rgbDataSize, &info, kLX_DPXWrite_RGB_INT10, metadata, &headerDataSize);

    fwrite(header, headerDataSize, 1, file);
    fwrite(rgbBuffer, rgbDataSize, 1, file);   
    
    _lx_free(header);
    if (bufferNeedsFree) _lx_free(rgbBuffer);
    return 0;
}

static int writeDPXImageToFile_RGBA_float16_to_RGB_int16(FILE *file, LXMapPtr metadata,
                                                      uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t srcRowBytes)
{
    // temp buffer for outdata
    const int rgbBytesPerPixel = 6;
    const size_t rgbRowBytes = LXAlignedRowBytes(w * rgbBytesPerPixel);
    const size_t rgbDataSize = rgbRowBytes * h;
    uint8_t *rgbBuffer = NULL;
    LXBool bufferNeedsFree = NO;

    createTempBufWithCallerProperties(metadata, rgbDataSize, &rgbBuffer, &bufferNeedsFree);
    
    LXPxConvert_RGB_float16_to_RGB_int16(w, h, (LXHalf *)srcData, srcRowBytes, 4,
                                               (uint16_t *)rgbBuffer, rgbRowBytes,
                                               65535);  // max coding value
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = 0;
    info.bitDepth = 16;
    info.pixelBytes = rgbBytesPerPixel;
    info.inputMin = 0;
    info.inputMax = 65535;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rgbRowBytes;
    info.rowEndPadding = rgbRowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(rgbDataSize, &info, kLX_DPXWrite_RGB_INT16, metadata, &headerDataSize);
    
    fwrite(header, headerDataSize, 1, file);
    fwrite(rgbBuffer, rgbDataSize, 1, file);   
    
    _lx_free(header);
    if (bufferNeedsFree) _lx_free(rgbBuffer);
    return 0;
}

static int writeDPXImageToFile_RGBA_float32_to_RGB_int16(FILE *file, LXMapPtr metadata,
                                                      uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t srcRowBytes)
{
    // temp buffer for outdata
    const int rgbBytesPerPixel = 6;
    const size_t rgbRowBytes = LXAlignedRowBytes(w * rgbBytesPerPixel);
    const size_t rgbDataSize = rgbRowBytes * h;
    uint8_t *rgbBuffer = NULL;
    LXBool bufferNeedsFree = NO;

    createTempBufWithCallerProperties(metadata, rgbDataSize, &rgbBuffer, &bufferNeedsFree);
    
    LXPxConvert_RGB_float32_to_RGB_int16(w, h, (float *)srcData, srcRowBytes, 4,
                                               (uint16_t *)rgbBuffer, rgbRowBytes,
                                               65535);  // max coding value
    
    LXCineonDataInfo info;
    memset(&info, 0, sizeof(info));
    info.packing = 0;
    info.bitDepth = 16;
    info.pixelBytes = rgbBytesPerPixel;
    info.inputMin = 0;
    info.inputMax = 65535;
    
    info.width = w;
    info.height = h;
    info.rowBytes = rgbRowBytes;
    info.rowEndPadding = rgbRowBytes - (w * info.pixelBytes);

    size_t headerDataSize = 0;
    uint8_t *header = createDPXHeader(rgbDataSize, &info, kLX_DPXWrite_RGB_INT16, metadata, &headerDataSize);
    
    fwrite(header, headerDataSize, 1, file);
    fwrite(rgbBuffer, rgbDataSize, 1, file);   
    
    _lx_free(header);
    if (bufferNeedsFree) _lx_free(rgbBuffer);
    return 0;
}


// the caller assures that source format is either an RGBA format or YCbCr 4:2:2.
// only float formats can be written to 10/16 bit; the caller is responsible for this as well.
static int writeDPXImageToFile(FILE *file, LXMapPtr metadata,
                                uint8_t * LXRESTRICT srcData, const LXInteger w, const LXInteger h, const size_t rowBytes,
                                const LXInteger sourcePxFormat,
                                const LXInteger writeFormat)
{
    int retVal = 3248;  // some random error value
    
    if (sourcePxFormat == kLX_YCbCr422_INT8 && writeFormat == kLX_DPXWrite_YCbCr422_INT8) {
        retVal = writeDPXImageToFile_YCbCr422(file, metadata, srcData, w, h, rowBytes);
    }
    else if (sourcePxFormat == kLX_RGBA_INT8 && writeFormat == kLX_DPXWrite_RGBA_INT8) {
        retVal = writeDPXImageToFile_RGBA_int8(file, metadata, srcData, w, h, rowBytes);
    }
    
    else if (sourcePxFormat == kLX_RGBA_INT8 && writeFormat == kLX_DPXWrite_RGB_INT8) {
        retVal = writeDPXImageToFile_RGBA_to_RGB_int8(file, metadata, srcData, w, h, rowBytes);
    }
    
    else if (sourcePxFormat == kLX_RGBA_FLOAT16 && writeFormat == kLX_DPXWrite_RGB_INT10) {
        retVal = writeDPXImageToFile_RGBA_float16_to_RGB_int10(file, metadata, srcData, w, h, rowBytes);
    }
    else if (sourcePxFormat == kLX_RGBA_FLOAT32 && writeFormat == kLX_DPXWrite_RGB_INT10) {
        retVal = writeDPXImageToFile_RGBA_float32_to_RGB_int10(file, metadata, srcData, w, h, rowBytes);
    }
    
    else if (sourcePxFormat == kLX_RGBA_FLOAT16 && writeFormat == kLX_DPXWrite_RGB_INT16) {
        retVal = writeDPXImageToFile_RGBA_float16_to_RGB_int16(file, metadata, srcData, w, h, rowBytes);
    }
    else if (sourcePxFormat == kLX_RGBA_FLOAT32 && writeFormat == kLX_DPXWrite_RGB_INT16) {
        retVal = writeDPXImageToFile_RGBA_float32_to_RGB_int16(file, metadata, srcData, w, h, rowBytes);
    }
    
    else if (sourcePxFormat == kLX_Luminance_INT8 && writeFormat == kLX_DPXWrite_Luminance_INT8) {
        retVal = writeDPXImageToFile_luminance_int8(file, metadata, srcData, w, h, rowBytes);
    }
    
    return retVal;
}


#pragma mark --- write API ---

LXSuccess LXPixelBufferWriteAsDPXImageToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError)
{
    if ( !pixbuf) return NO;

    FILE *file = NULL;
    if ( !LXOpenFileForWritingWithUnipath(unipath.unistr, unipath.numOfChar16, (LXFilePtr *)&file)) {
        LXErrorSet(outError, 1760, "could not open file");
        return NO;
    }

    const uint32_t w = LXPixelBufferGetWidth(pixbuf);
    const uint32_t h = LXPixelBufferGetHeight(pixbuf);
    LXPixelFormat sourcePxFormat = LXPixelBufferGetPixelFormat(pixbuf);
    
    // these are the only supported formats.
    // for other formats, a temp copy is made.
    LXPixelBufferRef tempPixbuf = NULL;
    if (sourcePxFormat != kLX_RGBA_INT8 && sourcePxFormat != kLX_RGBA_FLOAT16 && sourcePxFormat != kLX_RGBA_FLOAT32
        && sourcePxFormat != kLX_YCbCr422_INT8
        && sourcePxFormat != kLX_Luminance_INT8) {
        // check for float luminance formats; for anything else, use 8-bit RGBA and hope that the default converter does the trick.
        sourcePxFormat = (sourcePxFormat == kLX_Luminance_FLOAT16 || sourcePxFormat == kLX_Luminance_FLOAT32) ? kLX_RGBA_FLOAT16 : kLX_RGBA_INT8;
        
        if ( !(tempPixbuf = LXPixelBufferCreate(NULL, w, h, sourcePxFormat, outError)))
            return NO;
        if ( !LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, pixbuf, outError))
            return NO;
    }
    
    LXInteger writeFormat = 0;  // one of the kLX_DPXWrite_* constants
    LXBool allowAlpha = NO;
    LXBool allowYUV = (sourcePxFormat == kLX_YCbCr422_INT8);
    LXInteger preferredBitDepth = 0;
    
    if (properties) {
        LXMapGetBool(properties, kLXPixelBufferFormatRequestKey_AllowAlpha, &allowAlpha);
        LXMapGetBool(properties, kLXPixelBufferFormatRequestKey_AllowYUV, &allowYUV);
        LXMapGetInteger(properties, kLXPixelBufferFormatRequestKey_PreferredBitsPerChannel, &preferredBitDepth);
    }
    
    if (allowYUV) {
        if (sourcePxFormat == kLX_YCbCr422_INT8) {
            writeFormat = kLX_DPXWrite_YCbCr422_INT8;
        }
        else if (sourcePxFormat == kLX_RGBA_FLOAT16) {
            // for float16 -> YUV conversions, assume that the incoming data is raw YUV, and convert it directly to YCbCr.
            
            // if we have a previous temp buffer, clean it up first.
            LXPixelBufferRef prevTemp = tempPixbuf;
            LXPixelBufferRef srcPixbuf = (tempPixbuf) ? tempPixbuf : pixbuf;
    
            if ( !(tempPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_YCbCr422_INT8, outError)))
                return NO;
                
            size_t srcRowBytes = 0;
            uint8_t *srcData = LXPixelBufferLockPixels(srcPixbuf, &srcRowBytes, NULL, outError);
            if ( !srcData) return NO;
            
            size_t yuvRowBytes = 0;
            uint8_t *yuvData = LXPixelBufferLockPixels(tempPixbuf, &yuvRowBytes, NULL, outError);
            if ( !yuvData) return NO;
            
            ///printf("...DPX writer: converting raw yuv float16 to 2vuy: %i * %i, %i / %i\n", (int)w, (int)h, (int)srcRowBytes, (int)yuvRowBytes);

            LXPxConvert_RGBA_float16_rawYUV_to_YCbCr422(w, h, (LXHalf *)srcData, srcRowBytes, yuvData, yuvRowBytes);

            LXPixelBufferUnlockPixels(tempPixbuf);
            LXPixelBufferUnlockPixels(srcPixbuf);

            if (prevTemp) LXPixelBufferRelease(prevTemp);

            sourcePxFormat = kLX_YCbCr422_INT8;
            writeFormat = kLX_DPXWrite_YCbCr422_INT8;
        }
        else {
            LXPrintf("DPX writer: can't honor allowYUV request for this pixel format (%i), will write RGB\n", (int)sourcePxFormat);
        }
    }
    else if (preferredBitDepth <= 0 || preferredBitDepth > 16) {
        preferredBitDepth = (sourcePxFormat == kLX_RGBA_INT8) ? 8 : 10;
    }
    
    if (sourcePxFormat == kLX_Luminance_INT8) {
        preferredBitDepth = 8;
        writeFormat = kLX_DPXWrite_Luminance_INT8;
    }
    
    if (writeFormat == 0) {
        if (allowAlpha) {
            writeFormat = kLX_DPXWrite_RGBA_INT8;  // only supported alpha format is currently 8-bit
        } else {
            if (sourcePxFormat == kLX_RGBA_INT8) {
                writeFormat = kLX_DPXWrite_RGB_INT8;
            } else {
                switch (preferredBitDepth) {
                    case 8:                 writeFormat = kLX_DPXWrite_RGB_INT8;  break;
                    case 10: default:       writeFormat = kLX_DPXWrite_RGB_INT10;  break;
                    case 16:                writeFormat = kLX_DPXWrite_RGB_INT16;  break;
                }
            }
        }
    }
    
    if ((writeFormat == kLX_DPXWrite_RGBA_INT8 || writeFormat == kLX_DPXWrite_RGB_INT8) && (sourcePxFormat != kLX_RGBA_INT8)) {
        // for float->int8 conversions, we must go through a temp buffer as well.
        // if we have a previous temp buffer, clean it up first.
        LXPixelBufferRef prevTemp = tempPixbuf;
        LXPixelBufferRef srcPixbuf = (tempPixbuf) ? tempPixbuf : pixbuf;
    
        if ( !(tempPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError)))
            return NO;
        if ( !LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, srcPixbuf, outError))
            return NO;
        sourcePxFormat = kLX_RGBA_INT8;
        
        if (prevTemp) LXPixelBufferRelease(prevTemp);
    }
    
    DEBUGLOG("%s: sourcepxf %i (orig format %i) -> writeformat %i\n", __func__, (int)sourcePxFormat, (int)LXPixelBufferGetPixelFormat(pixbuf), (int)writeFormat);
    
    // metadata to be written to file
    LXMapPtr metadata = LXMapCreateMutable();
    LXUnibuffer unibuf = { 0, NULL };
    
    char *creatorStr = "Lacefx API by Lacquer";
    LXBool creatorStrWasAlloced = NO;
    if (properties && LXMapGetUTF16(properties, "metadata::creator", &unibuf)) {
        creatorStr = LXStrCreateUTF8_from_UTF16(unibuf.unistr, unibuf.numOfChar16, NULL);
        creatorStrWasAlloced = YES;
        _lx_free(unibuf.unistr);
        unibuf.unistr = NULL;
    }
    LXMapSetInteger(metadata, "creator_utf8", (LXInteger)creatorStr);
    
    char *filenameStr = NULL;
    LXBool filenameStrWasAlloced = NO;
    if (properties && LXMapGetUTF16(properties, "metadata::filename", &unibuf)) {
        filenameStr = LXStrCreateUTF8_from_UTF16(unibuf.unistr, unibuf.numOfChar16, NULL);
        filenameStrWasAlloced = YES;
        _lx_free(unibuf.unistr);
        unibuf.unistr = NULL;
    }
    LXMapSetInteger(metadata, "filename_utf8", (LXInteger)filenameStr);
    
    // to eliminate a malloc() during conversion, the caller can pass a temp buffer in properties
    if (LXMapContainsValueForKey(properties, "tempBuf::dataHandle", NULL) &&
        LXMapContainsValueForKey(properties, "tempBuf::dataSizePtr", NULL)) {
        LXInteger v = 0;
        LXMapGetInteger(properties, "tempBuf::dataHandle", &v);
        LXMapSetInteger(metadata, "tempBuf::dataHandle", v);
        v = 0;
        LXMapGetInteger(properties, "tempBuf::dataSizePtr", &v);
        LXMapSetInteger(metadata, "tempBuf::dataSizePtr", v);
        //printf("writing dpx, has tempbuf handles (sizeptr %p)\n", v);
    }
    
    // finally ready to write the data
    LXPixelBufferRef pbToWrite = (tempPixbuf) ? tempPixbuf : pixbuf;
    LXSuccess success = NO;
    size_t rowBytes = 0;
    uint8_t *data = LXPixelBufferLockPixels(pbToWrite, &rowBytes, NULL, outError);
    if (data) {
        int error = writeDPXImageToFile(file, metadata,
                                      data, w, h, rowBytes,
                                      sourcePxFormat,
                                      writeFormat);
        
        success = (error == 0) ? YES : NO;
        if (error) {
            char msg[512];
            sprintf(msg, "an internal error (%i) occurred during DPX write", (int)error);
            LXErrorSet(outError, 1770, msg);
        }
        
        LXPixelBufferUnlockPixels(pbToWrite);
    }
    
    fclose(file);
    
    if (tempPixbuf)  LXPixelBufferRelease(tempPixbuf);
        
    LXMapDestroy(metadata);
    
    if (creatorStrWasAlloced)  _lx_free(creatorStr);
    if (filenameStrWasAlloced)  _lx_free(filenameStr);
    
    return success;
}



