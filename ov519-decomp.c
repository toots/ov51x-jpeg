/* This is the jpeg hacked version of Mark W. McClelland's Original driver.
*
* This drivers is GPL so use it as you like, but this version is never to be 
* seriously developped. It is only provided for means of compatibility with current v4l.
* Moreover, it is NOT tested with ov511 or ov518 so don't expect it to work - 
* anyway for those cameras, you don't need it.
*
* Culprit for this module: Romain Beauxis <toots@rastageeks.org>
* See http://www.rastageeks.org/ov51x-jpeg/ for more details.
*
* 20050917 - Markus Tavenrath <speedygoo AT speedygoo.de>
*            fixed corrupt lines bug which was produced by gfxPixelYUV 
*
* 20060427 - Romain Beauxis <code AT rastageeks.org>
* 	     Added workaround for deprecated MODULE_PARM
* 
*/



/* OV519 Decompression Support Module 
*
* This code is mostly taken from the zc0302 JPEG decompression code.
* See: http://zc0302.sourceforge.net/zc0302.php
*/

#include <linux/autoconf.h>
#include <linux/version.h>

#include "ov51x-jpeg.h"

extern int debug;


/* Include kernel headers */
#include <asm/types.h>

/* Init the decoder */
void zc030x_jpeg_init(void);


/******************************************************************************
 * Debugging
 ******************************************************************************/

#define PRN_64_ROW(a, i) // PDEBUG(5, "%02x %02x %02x %02x %02x %02x %02x %02x",(a)[(i)], (a)[(i)+1], (a)[(i)+2], (a)[(i)+3], (a)[(i)+4], (a)[(i)+5],(a)[(i)+6], (a)[(i)+7])

static inline void
print_64(unsigned char *p)
{
	PDEBUG(5, "64 next bytes from %d:", *p);
	PRN_64_ROW(p, 0);
	PRN_64_ROW(p, 8);
	PRN_64_ROW(p, 16);
	PRN_64_ROW(p, 24);
	PRN_64_ROW(p, 32);
	PRN_64_ROW(p, 40);
	PRN_64_ROW(p, 48);
	PRN_64_ROW(p, 56);
}

/* Decoder specific structure, shouldn't be exported */
typedef struct
{                  
  int           Index;          // Index of this minimum code entry
  long          CodeValue;      // And its code value
} MinimumCodeEntry;

typedef struct 
{
  MinimumCodeEntry * pMinCodeTable;     // Minimum code table (given the huffman tree) and a length, as index
  long *             pMaxCodeTable;     // Maximum code table (given the huffman tree) and a length, as index
  unsigned char *    pCodeCatTable;     // The code category (bit length) table
  unsigned short *   pCodeTable;        // The code itself table
} HuffmanTable;


typedef unsigned char  byte;
typedef unsigned short word;

// Define functions
// Construct the huffman tables
void ConstructHuffmanTables(void);
// Get n bit from the huffman stream
int GetNBits(int NumberOfBits, byte **); 
// Get code from this stream given the huffman table
int GetCode(HuffmanTable * pTable, byte ** Buf); 
// Decode a code
int DecodeCode(int Code, int NumberOfBits); 

// Invert DCT
//TBRM: static void IDCT (short Data[]);
// InverDCT caller plus dequantifier
int InvertDCT(short vector[], byte quant[]);

/* Define DCT block size and constants */
#define DCTSIZE		8	
#define DCTSIZE2	64	
/* Should remove the L prefix on x64 architecture */
#define ONE         0x00000001			

/* Define architecture independant right shifting operator */
#ifdef RIGHT_SHIFT_IS_UNSIGNED
#define LG2_DCT_SCALE 15
#define RIGHT_SHIFT(_x,_shft)   ((((_x) + (ONE << 30)) >> (_shft)) - (ONE << (30 - (_shft))))
#else
#define LG2_DCT_SCALE 16
#define RIGHT_SHIFT(_x,_shft)   ((_x) >> (_shft))
#endif

/* Define DCT scales */
#define DCT_SCALE (ONE << LG2_DCT_SCALE)

/* Define iDCT scale constants */
#define LG2_OVERSCALE 2
#define OVERSCALE (ONE << LG2_OVERSCALE)

/* Define iDCT macros */
#define FIX(x)  ((int) ((x) * DCT_SCALE + 0.5))
#define FIXO(x)  ((int) ((x) * DCT_SCALE / OVERSCALE + 0.5))
#define UNFIX(x)   RIGHT_SHIFT((x) + (ONE << (LG2_DCT_SCALE-1)), LG2_DCT_SCALE)
#define UNFIXH(x)  RIGHT_SHIFT((x) + (ONE << LG2_DCT_SCALE), LG2_DCT_SCALE+1)
#define UNFIXO(x)  RIGHT_SHIFT((x) + (ONE << (LG2_DCT_SCALE-1-LG2_OVERSCALE)), LG2_DCT_SCALE-LG2_OVERSCALE)
#define OVERSH(x)   ((x) << LG2_OVERSCALE)

/* Define iDCT constants */
#define SIN_1_4 FIX(0.7071067811856476)
#define COS_1_4 SIN_1_4

/* Define iDCT constants */
#define SIN_1_8 FIX(0.3826834323650898)
#define COS_1_8 FIX(0.9238795325112870)
#define SIN_3_8 COS_1_8
#define COS_3_8 SIN_1_8

/* Define iDCT constants */
#define SIN_1_16 FIX(0.1950903220161282)
#define COS_1_16 FIX(0.9807852804032300)
#define SIN_7_16 COS_1_16
#define COS_7_16 SIN_1_16

/* Define iDCT constants */
#define SIN_3_16 FIX(0.5555702330196022)
#define COS_3_16 FIX(0.8314696123025450)
#define SIN_5_16 COS_3_16
#define COS_5_16 SIN_3_16

/* Define iDCT constants */
#define OSIN_1_4 FIXO(0.707106781185647)
#define OCOS_1_4 OSIN_1_4

/* Define iDCT constants */
#define OSIN_1_8 FIXO(0.3826834323650898)
#define OCOS_1_8 FIXO(0.9238795325112870)
#define OSIN_3_8 OCOS_1_8
#define OCOS_3_8 OSIN_1_8

/* Define iDCT constants */
#define OSIN_1_16 FIXO(0.1950903220161282)
#define OCOS_1_16 FIXO(0.9807852804032300)
#define OSIN_7_16 OCOS_1_16
#define OCOS_7_16 OSIN_1_16

/* Define iDCT constants */
#define OSIN_3_16 FIXO(0.5555702330196022)
#define OCOS_3_16 FIXO(0.8314696123025450)
#define OSIN_5_16 OCOS_3_16
#define OCOS_5_16 OSIN_3_16

/* Define fixed point values */
#define FIX_1_082392200  277     /* FIX(1.082392200) */
#define FIX_1_414213562  362     /* FIX(1.414213562) */
#define FIX_1_847759065  473     /* FIX(1.847759065) */
#define FIX_2_613125930  669     /* FIX(2.613125930) */

/* For outputting to file */
#define BGR(B,G,R)  B, G, R
#define JPGToBGR(Y,U,V, B, G, R) _JPGToBGR(Y,U,V,B,G,R)

// Here comes the hacked part: it should be YUV420 planar, but well, I've work around for ages, and I'm no sure yet it is planar YUV420 but well..
// It works ;)
inline void gfxPixelYUV(s16 x, s16 y, u8 y1, u8 u, u8 y2, u8 v, u8* vram, int width, int height, int j, int i)
{

    unsigned char *pVram = (unsigned char *) vram;

    pVram[2 * x + j + (2*y + i)* width] = y1;
    pVram[2 * x + j + (2*y + i)* width + 8] = y2;
    if (!(i & 0x01)) {
      pVram[x + j + (y + i/2) * width/2 + width * height] = u;
      pVram[x + j + (y + i/2) * width/2 + width * height + (width * height)/4] = v;
    }
}




/* Receive bits */
s16 JPGReceiveBits (u16 cat, u8 * jpgdata);

/* Zig-zag reordering indexes */
const u8 JPGZig1 [64] = {
   0,0,1,2,1,0,0,1,2,3,4,3,2,1,0,0,1,2,3,4,5,6,5,4,3,2,1,0,0,1,2,3,
   4,5,6,7,7,6,5,4,3,2,1,2,3,4,5,6,7,7,6,5,4,3,4,5,6,7,7,6,5,6,7,7
   };

/* Zig-zag reordering indexes */
const u8 JPGZig2 [64] = {
   0,1,0,0,1,2,3,2,1,0,0,1,2,3,4,5,4,3,2,1,0,0,1,2,3,4,5,6,7,6,5,4,
   3,2,1,0,1,2,3,4,5,6,7,7,6,5,4,3,2,3,4,5,6,7,7,6,5,4,5,6,7,7,6,7
   };

/* Precomputed fixed point iDCT&descalers coefficient */
const u16 aanscales[64] = {
   /* precomputed values scaled up by 14 bits */
   16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
   22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
   21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
   19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
   16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
   12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
    8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
    4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
   };

/* Type definitions */
struct JpegType
{                           
   u16 Rows;                /* Picture's Height */
   u16 Cols;                /* Picture's Width  */
   u16 SamplesY;            /* Sampling ratios */
   u16 SamplesCbCr;         
   u16 QuantTableY;         /* Quantization table index */
   u16 QuantTableCbCr;
   u16 HuffDCTableY;        /* Huffman table index */
   u16 HuffDCTableCbCr;
   u16 HuffACTableY;
   u16 HuffACTableCbCr;
   u16 NumComp;             /* Components number */
};

/* An huffman entry is defined by its index, its code and its length */
struct JPGHuffmanEntry 
{                   
   u16 Index;
   s16 Code;
   u16 Length;
}__attribute__ ((packed));

u32 findex = 0;
u16 DCTables;
u16 ACTables;
u16 QTables;
u8 curByte;
u8 curBits;
u8 EOI;
struct JPGHuffmanEntry HuffmanDC0[256];
struct JPGHuffmanEntry HuffmanDC1[256];
struct JPGHuffmanEntry HuffmanAC0[256];
struct JPGHuffmanEntry HuffmanAC1[256];
//u16 ZigIndex;
u16 QuantTable[2][8][8];    // 2 quantization tables (Y, CbCr)
struct JpegType Image;
//u16 flen;
//s16 i;

#define CompileTable

// Modified by Cyril (Created)
/* Forward declaration */
typedef struct JPGHuffmanEntry * PJPGHuffmanEntry;
/* Save an huffman table in the structure */
u32 SaveTable(PJPGHuffmanEntry  Huffman, u8 * bits, u8 * vals)
{
    /* Total number of bits in the huffman table */
    u16 total = 0;
    /* Iterators */
    u32 i,j;

    /* Iterator and temporary values */
    u16 CurNum = 0;
    u16 CurIndex = 0;
    u16 HuffAmount[16]; 
 
    /* Read the number of codes for each code size */
    for (i=0; i<16; i++)
    {
       total += bits[i];
       HuffAmount[i] = bits[i];
    }
    /* Read all codes, for each length */ 
    for (i=0; i<total; i++)
    {
        Huffman[i].Code = vals[i];
    }
    
    /* Now create the table correctly */
    CurNum = 0;
    CurIndex = -1;
    for (i=0; i<16; i++)
    {
        for (j=0; j < HuffAmount[i]; j++)
        {
            CurIndex++;
            Huffman[CurIndex].Index = CurNum;
            Huffman[CurIndex].Length = i+1;
            
            CurNum++;
        }
       
        CurNum *= 2;
    }

    /* Exit */
    return 0;
}


/* Left shift macro */
#define JPGpower2(X)    ( 1 << (X) )

/* Get a byte from the stream */
inline u8 JPGGetByte (u8 * jpgdata)
{
    return (jpgdata[findex++]);
}

/* Get a endianness word from the stream */
inline u16 JPGGetWord (u8 * jpgdata)
{
    u16 i = JPGGetByte(jpgdata) << 8;
    i += JPGGetByte(jpgdata);
    return (i);
}

/* Get next byte from the stream */
inline u8 JPGNextBit (u8 * jpgdata)
{
    u8 NextBit;
    
    curBits >>= 1;
    NextBit = (curByte >> 7) & 1;
    curByte <<= 1;
    if (curBits == 0)
    {
        curBits = 128;
        curByte = JPGGetByte(jpgdata);
        if (curByte == 0xFF)
            if (JPGGetByte(jpgdata) == 0xD9)
            {
                EOI = 1;
                return(0);
            }
    }
    return(NextBit);
}

/* Decode a JPEG compressed block */
inline s16 JPGDecode(struct JPGHuffmanEntry inArray[256], u8 * jpgdata)
{
    u16 n1; u16 n2; u16 i; u16 l;
    s32 CurVal;
    s16 MatchFound;
    
    /* Check if we have a JPEG marker */
    if (JPGGetByte(jpgdata) == 0xFF)
    {
        n1 = JPGGetByte(jpgdata);
        findex -= 2;
        if ((n1 >= 0xd0) && (n1 <= 0xd7))
        {
            n2 = curBits - 1;
            /* Check for end of image (remaining bits = 1) */
            if ((curByte & n2) == n2)     
            {
                EOI = 1;
                
                return 0;
            }
        }
    }
    else
        findex--;
    
    /* Else try to find the huffman code */
    CurVal = 0;
    MatchFound = -1;
    /* Cycle through the possible 16 bits huffman code length */
    for (l=1; l<16+1; l++)    
    {
        CurVal = (CurVal << 1) + JPGNextBit(jpgdata);
        if (EOI) return(0);
        /* Check the lookup table */ 
        for (i=0; i<256; i++)
        {
            if (inArray[i].Length > l) break;
            if (inArray[i].Length == l)
            if (inArray[i].Index == CurVal)
            {
                MatchFound = i;
                break;
            }
        }
        
        if (MatchFound > -1) break;
    }
    
    if (MatchFound == -1)
    {
        PDEBUG(3,"Error with the current huffman table");
        EOI = 1;
        return(-1);
    }
    /* Return the found code */
    return(inArray[MatchFound].Code);  
}

// Modified by Cyril (Created)
#ifdef CompileTable

    // Note that this is not the default quantization tables, because they
    // integrate apart of the iDCT in them (in fact the cos(2kpi/8) * sqrt(2)
    // This avoid computing those factors for each decoding.

    // If you need to implement an other jpeg decoder, you could use the default
    // tables defined in JpegLib

    // Define default quantification tables
    unsigned char LumaQuantTable[64] = { 
    32, 33 , 36 , 32 , 36 , 37 , 54 , 39, 
    33, 46 , 50 , 58 , 61 , 78 , 96 , 70, 
    26, 50 , 54 , 67 , 99 , 114, 110, 69, 
    37, 65 , 73 , 82 , 131, 118, 111, 63, 
    48, 72 , 104, 122, 136, 128, 112, 61, 
    62, 126, 119, 162, 172, 128, 103, 43, 
    56, 90 , 98 , 101, 112, 96 , 70 , 31, 
    34, 42 , 40 , 40 , 43 , 39 , 30 , 15};
    
    unsigned short ChromaQuantTable[64] = { 
    36 , 49 , 62 , 112, 200, 157, 108, 55, 
    49 , 84 , 94 , 215, 277, 217, 150, 76, 
    62 , 94 , 191, 307, 261, 205, 141, 72, 
    112, 215, 307, 276, 235, 184, 127, 64, 
    200, 277, 261, 235, 200, 157, 108, 55, 
    157, 217, 205, 184, 157, 123, 85 , 43, 
    108, 150, 141, 127, 108, 85 , 58 , 29, 
    55 , 76 , 72 , 64 , 55 , 43 , 29 , 15 };

    //
    // Standard Huffman Decompression Tables (taken from jpeglib)
    //
    
    // DC Luminance 
    /* number-of-symbols-of-each-code-length count */
    unsigned char bits_dc_luminance[16] =
        { /* 0-base */ 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char val_dc_luminance[] =
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    
    
    // DC Chrominance
    /* number-of-symbols-of-each-code-length count */
    unsigned char bits_dc_chrominance[16] =
        { /* 0-base */ 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    unsigned char val_dc_chrominance[] =
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };


    // AC Luminance
    /* number-of-symbols-of-each-code-length count */
    unsigned char bits_ac_luminance[16] =
    { /* 0-base */ 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d }; 
    unsigned char val_ac_luminance[] =
     { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,\
       0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,\
       0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,\
       0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,\
       0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,\
       0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,\
       0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,\
       0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,\
       0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,\
       0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,\
       0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,\
       0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,\
       0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,\
       0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,\
       0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,\
       0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,\
       0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,\
       0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,\
       0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,\
       0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,\
       0xf9, 0xfa };


    // AC Chrominance
    /* number-of-symbols-of-each-code-length count */
    unsigned char bits_ac_chrominance[16] =
     { /* 0-base */ 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
    unsigned char val_ac_chrominance[] =
     { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,\
       0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,\
       0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,\
       0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,\
       0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,\
       0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,\
       0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,\
       0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,\
       0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,\
       0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,\
       0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,\
       0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,\
       0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,\
       0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,\
       0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,\
       0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,\
       0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,\
       0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,\
       0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,\
       0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,\
       0xf9, 0xfa };
#endif


/* Init the decoder */   
void zc030x_jpeg_init()
{
    /* Iterators */
    u32 i, j;

#ifndef CompileTable
    FILE * pFileTable = NULL;
    u8 aoTables[1024];
    u8 tmp1, tmp2;
    u32 lFileSize;

    
    // Read quantizer file (and scale them by square root of 2)
    //
    pFileTable = fopen("tables.quant", "rb");
        
    fseek(pFileTable, 0, SEEK_END);
    lFileSize = ftell(pFileTable);
    fseek(pFileTable, 0, SEEK_SET);
        
    fread(aoTables, 1, 2, pFileTable);
    
    PDEBUG(3,"%02X %02X - %d", aoTables[0], aoTables[1], lFileSize);
    if (aoTables[0] != 0xff || aoTables[1] != 0xdb)
    {
        // Don't remove backet as debugmsg is a macro that can be voided
        PDEBUG(3,"Error with tables.quant files");
    }
    
    fread(aoTables, 1, lFileSize - 2, pFileTable);
    
    JPGGetQuantTables(aoTables);
    // Advance 2 bytes
    tmp1 = JPGGetByte(aoTables);
    tmp2 = JPGGetByte(aoTables);
    PDEBUG(3,"Read %02X %02X - must be FF DB", tmp1, tmp2);

    JPGGetQuantTables(aoTables);

    findex = 0;
    fclose(pFileTable);

   
#else
    /* Save the quantization tables for luminance */
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            QuantTable[0][j][i] = LumaQuantTable[i * 8 + j]; 

    /* Save the quantization tables for chrominance */
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            QuantTable[1][j][i] = ChromaQuantTable[i * 8 + j]; 
#endif
    
    
        
    // The table above are the same as in the file "tables.default"
    // When testing, it's easier to change a file (doesn't need recompiling)


#ifndef CompileTable
    // Save huffman tables
    pFileTable = fopen("tables.default", "rb");
        
    fseek(pFileTable, 0, SEEK_END);
    lFileSize = ftell(pFileTable);
    fseek(pFileTable, 0, SEEK_SET);
        
    fread(aoTables, 1, 2, pFileTable);
    
    PDEBUG(3,"%02X %02X - %d", aoTables[0], aoTables[1], lFileSize);
    if (aoTables[0] != 0xff || aoTables[1] != 0xc4)
    {
        // Don't remove backet as debugmsg is a macro that can be voided
        PDEBUG(3,"Error with tables.default files");
    }
    
    fread(aoTables, 1, lFileSize - 2, pFileTable);
    
    JPGGetHuffTables(aoTables);
    // Advance 2 bytes
    tmp1 = JPGGetByte(aoTables);
    tmp2 = JPGGetByte(aoTables);
    PDEBUG(3,"Read %02X %02X - must be FF C4", tmp1, tmp2);
    
    JPGGetHuffTables(aoTables);

    // Advance 2 bytes
    // Advance 2 bytes
    tmp1 = JPGGetByte(aoTables);
    tmp2 = JPGGetByte(aoTables);
    PDEBUG(3,"Read %02X %02X - must be FF C4", tmp1, tmp2);

    JPGGetHuffTables(aoTables);

    // Advance 2 bytes
    // Advance 2 bytes
    tmp1 = JPGGetByte(aoTables);
    tmp2 = JPGGetByte(aoTables);
    PDEBUG(3,"Read %02X %02X - must be FF C4", tmp1, tmp2);
    
    JPGGetHuffTables(aoTables);
    
    fclose(pFileTable);
    
    findex = 0;
#else
    /* Save the huffman tables */
    SaveTable((PJPGHuffmanEntry)HuffmanDC0, bits_dc_luminance, val_dc_luminance);
    SaveTable((PJPGHuffmanEntry)HuffmanAC0, bits_ac_luminance, val_ac_luminance);
    SaveTable((PJPGHuffmanEntry)HuffmanDC1, bits_dc_chrominance, val_dc_chrominance);
    SaveTable((PJPGHuffmanEntry)HuffmanAC1, bits_ac_chrominance, val_ac_chrominance);
#endif
    PDEBUG(3,"-- JPEG compliant decoder initialized --");
    
}

/* Fast integer IDCT */
void jpeg_idct_ifast (s16 inarray[8][8], s16 outarray[8][8], u16 QuantNum)
{
    /* Temporary variable */
    /* Please note that even if results are 16 bits,
       we are using processor sized variable to speed up the process */
    s32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    s32 tmp10, tmp11, tmp12, tmp13;
    s32 z5, z10, z11, z12, z13;
    
    u32 ctr;
    s16 warray[8][8];
    
    /* Pass 1: process columns from input, store into work array. */
    for (ctr = 0; ctr < 8; ctr++)
    {
        /* Due to quantization, we will usually find that many of the input
         * coefficients are zero, especially the AC terms.  We can exploit this
         * by short-circuiting the IDCT calculation for any column in which all
         * the AC terms are zero.  In that case each output is equal to the
         * DC coefficient (with scale factor as needed).
         * With typical images and quantization tables, half or more of the
         * column DCT calculations can be simplified this way.
         */
    
        if (inarray[1][ctr] == 0 && inarray[2][ctr] == 0 &&
            inarray[3][ctr] == 0 && inarray[4][ctr] == 0 &&
            inarray[5][ctr] == 0 && inarray[6][ctr] == 0 &&
            inarray[7][ctr] == 0)
        {
          /* AC terms all zero */
            s16 dcval = inarray[0][ctr] * QuantTable[QuantNum][0][ctr];
            
            warray[0][ctr] = dcval;
            warray[1][ctr] = dcval;
            warray[2][ctr] = dcval;
            warray[3][ctr] = dcval;
            warray[4][ctr] = dcval;
            warray[5][ctr] = dcval;
            warray[6][ctr] = dcval;
            warray[7][ctr] = dcval;
        
          continue;
        }
    
        /* Even part */
        tmp0 = inarray[0][ctr] * QuantTable[QuantNum][0][ctr];
        tmp1 = inarray[2][ctr] * QuantTable[QuantNum][2][ctr];
        tmp2 = inarray[4][ctr] * QuantTable[QuantNum][4][ctr];
        tmp3 = inarray[6][ctr] * QuantTable[QuantNum][6][ctr];
        
        tmp10 = tmp0 + tmp2;	/* phase 3 */
        tmp11 = tmp0 - tmp2;
        
        tmp13 = tmp1 + tmp3;	/* phases 5-3 */
        tmp12 = (((tmp1 - tmp3)*( FIX_1_414213562)) >> 8) - tmp13; /* 2*c4 */
        
        tmp0 = tmp10 + tmp13;	/* phase 2 */
        tmp3 = tmp10 - tmp13;
        tmp1 = tmp11 + tmp12;
        tmp2 = tmp11 - tmp12;
        
        /* Odd part */
        tmp4 = inarray[1][ctr] * QuantTable[QuantNum][1][ctr];
        tmp5 = inarray[3][ctr] * QuantTable[QuantNum][3][ctr];
        tmp6 = inarray[5][ctr] * QuantTable[QuantNum][5][ctr];
        tmp7 = inarray[7][ctr] * QuantTable[QuantNum][7][ctr];
        
        z13 = tmp6 + tmp5;		/* phase 6 */
        z10 = tmp6 - tmp5;
        z11 = tmp4 + tmp7;
        z12 = tmp4 - tmp7;
        
        tmp7 = z11 + z13;		/* phase 5 */
        tmp11 = ((z11 - z13) * FIX_1_414213562) >> 8; /* 2*c4 */
        
        z5 = ((z10 + z12) * FIX_1_847759065) >> 8; /* 2*c2 */
        tmp10 = ((z12 * FIX_1_082392200) >> 8) - z5; /* 2*(c2-c6) */
        tmp12 = ((z10 * - FIX_2_613125930) >> 8) + z5; /* -2*(c2+c6) */
        
        tmp6 = tmp12 - tmp7;	/* phase 2 */
        tmp5 = tmp11 - tmp6;
        tmp4 = tmp10 + tmp5;
        
        warray[0][ctr] = (tmp0 + tmp7);
        warray[7][ctr] = (tmp0 - tmp7);
        warray[1][ctr] = (tmp1 + tmp6);
        warray[6][ctr] = (tmp1 - tmp6);
        warray[2][ctr] = (tmp2 + tmp5);
        warray[5][ctr] = (tmp2 - tmp5);
        warray[4][ctr] = (tmp3 + tmp4);
        warray[3][ctr] = (tmp3 - tmp4);
    }
        
    /* Pass 2: process rows from work array, store into output array. */
    /* Note that we must descale the results by a factor of 8 == 2**3, */
    /* and also undo the PASS1_BITS scaling. */
    
    for (ctr = 0; ctr < 8; ctr++)
    {
        /* Rows of zeroes can be exploited in the same way as we did with columns.
         * However, the column calculation has created many nonzero AC terms, so
         * the simplification applies less often (typically 5% to 10% of the time).
         * On machines with very fast multiplication, it's possible that the
         * test takes more time than it's worth.  In that case this section
         * may be commented out.
         */
        
        if (warray[ctr][1] == 0 && warray[ctr][2] == 0 && warray[ctr][3] == 0 && warray[ctr][4] == 0 &&
            warray[ctr][5] == 0 && warray[ctr][6] == 0 && warray[ctr][7] == 0)
        {
            /* AC terms all zero */
            s16 dcval = (warray[ctr][0] >> 5)+128;
            if (dcval<0) dcval = 0;
            if (dcval>255) dcval = 255;
            
            outarray[ctr][0] = dcval;
            outarray[ctr][1] = dcval;
            outarray[ctr][2] = dcval;
            outarray[ctr][3] = dcval;
            outarray[ctr][4] = dcval;
            outarray[ctr][5] = dcval;
            outarray[ctr][6] = dcval;
            outarray[ctr][7] = dcval;
            continue;
        }
        
        /* Even part */
        
        tmp10 = warray[ctr][0] + warray[ctr][4];
        tmp11 = warray[ctr][0] - warray[ctr][4];
        
        tmp13 = warray[ctr][2] + warray[ctr][6];
        tmp12 = (((warray[ctr][2] - warray[ctr][6]) * FIX_1_414213562) >> 8) - tmp13;
        
        tmp0 = tmp10 + tmp13;
        tmp3 = tmp10 - tmp13;
        tmp1 = tmp11 + tmp12;
        tmp2 = tmp11 - tmp12;
        
        /* Odd part */
        
        z13 = warray[ctr][5] + warray[ctr][3];
        z10 = warray[ctr][5] - warray[ctr][3];
        z11 = warray[ctr][1] + warray[ctr][7];
        z12 = warray[ctr][1] - warray[ctr][7];
        
        tmp7 = z11 + z13;		/* phase 5 */
        tmp11 = ((z11 - z13) * FIX_1_414213562) >> 8; /* 2*c4 */
        
        z5 = ((z10 + z12) * FIX_1_847759065) >> 8; /* 2*c2 */
        tmp10 = ((z12 * FIX_1_082392200) >> 8) - z5; /* 2*(c2-c6) */
        tmp12 = ((z10 * - FIX_2_613125930) >> 8) + z5; /* -2*(c2+c6) */
        
        tmp6 = tmp12 - tmp7;	/* phase 2 */
        tmp5 = tmp11 - tmp6;
        tmp4 = tmp10 + tmp5;
        
        /* Final output stage: scale down by a factor of 8 and range-limit */
        
        outarray[ctr][0] = ((tmp0 + tmp7) >> 5)+128;
        if ((outarray[ctr][0])<0)  outarray[ctr][0] = 0;
        if ((outarray[ctr][0])>255) outarray[ctr][0] = 255;
        
        outarray[ctr][7] = ((tmp0 - tmp7) >> 5)+128;
        if ((outarray[ctr][7])<0)  outarray[ctr][7] = 0;
        if ((outarray[ctr][7])>255) outarray[ctr][7] = 255;
        
        outarray[ctr][1] = ((tmp1 + tmp6) >> 5)+128;
        if ((outarray[ctr][1])<0)  outarray[ctr][1] = 0;
        if ((outarray[ctr][1])>255) outarray[ctr][1] = 255;
        
        outarray[ctr][6] = ((tmp1 - tmp6) >> 5)+128;
        if ((outarray[ctr][6])<0)  outarray[ctr][6] = 0;
        if ((outarray[ctr][6])>255) outarray[ctr][6] = 255;
        
        outarray[ctr][2] = ((tmp2 + tmp5) >> 5)+128;
        if ((outarray[ctr][2])<0)  outarray[ctr][2] = 0;
        if ((outarray[ctr][2])>255) outarray[ctr][2] = 255;
        
        outarray[ctr][5] = ((tmp2 - tmp5) >> 5)+128;
        if ((outarray[ctr][5])<0)  outarray[ctr][5] = 0;
        if ((outarray[ctr][5])>255) outarray[ctr][5] = 255;
        
        outarray[ctr][4] = ((tmp3 + tmp4) >> 5)+128;
        if ((outarray[ctr][4])<0)  outarray[ctr][4] = 0;
        if ((outarray[ctr][4])>255) outarray[ctr][4] = 255;
        
        outarray[ctr][3] = ((tmp3 - tmp4) >> 5)+128;
        if ((outarray[ctr][3])<0)  outarray[ctr][3] = 0;
        if ((outarray[ctr][3])>255) outarray[ctr][3] = 255;
    
    }
    
}

/* Get a block to decode */
void JPGGetBlock (s16 vector[8][8], u16 HuffDCNum, u16 HuffACNum, u16 QuantNum, s16 *dcCoef, u8 * jpgdata)
{
    s16 array2[8][8];
    s32 d; u16 XPos; u16 YPos;
    //   u32 Sum;
    u16 bits; u16 zeros; s32 bitVal; u16 ACCount;
    u16 x; u16 y;
    //   u16 v; u16 u;
    //   s32 temp;
    s16 temp0;
    //   u16 Add1 = 0;
    u16 ZigIndex;
    
    EOI = 0;
    
    for (x=0; x<8; x++)
        for (y=0; y<8; y++)
            array2[x][y] = 0;
    
    if (HuffDCNum)
        temp0 = JPGDecode(HuffmanDC1, jpgdata);   // Get the DC coefficient
    else
        temp0 = JPGDecode(HuffmanDC0, jpgdata);   // Get the DC coefficient
    
    // Error with the previous decoding
    if (EOI)
    {
        PDEBUG(3,"Error with the previous decoding");
        return;
    }
    
    //   if (EOI) d = 0;
    *dcCoef = *dcCoef + JPGReceiveBits(temp0, jpgdata);
    array2[0][0] = *dcCoef; //* Quant[QuantNum][0][0];
    
    XPos = 0; YPos = 0;
    ZigIndex = 1;
    ACCount = 1;
    do
    {
        if (HuffACNum)
            d = JPGDecode(HuffmanAC1, jpgdata);
        else
            d = JPGDecode(HuffmanAC0, jpgdata);
        if (EOI)
        {
            PDEBUG(3,"Error with the previous decoding");
            return;
        }

        zeros = d >> 4;
        bits = d & 15;
        bitVal = JPGReceiveBits(bits, jpgdata);
        
        if (bits)
        {
            ZigIndex += zeros;
            ACCount += zeros;
            if (ACCount >= 64) break;
            
            XPos = JPGZig1[ZigIndex];
            YPos = JPGZig2[ZigIndex];
            ZigIndex++;
            
            //Read(XPos, YPos);
            array2[XPos][YPos] = bitVal; // * Quant[QuantNum][XPos][YPos];
            ACCount++;
        }
        else
        {
            if (zeros != 15) break;
            ZigIndex += 15;
            ACCount += 16;
        }
    }
    while (ACCount < 64);

//   if (HuffDCNum == Image.HuffDCTableY) Add1 = 128;

    jpeg_idct_ifast (array2, vector, QuantNum);

}

/* Get the huffman tables */
u16 JPGGetHuffTables (u8 * jpgdata)
{
    u16 HuffAmount[17]; //1-16
    u32 l0;
    u16 c0;
    u16 temp0;
    s16 temp1;
    u16 total;
    u16 i;
    u16 t0;
    s32 CurNum;
    u16 CurIndex;
    u16 j;
    
    l0 = JPGGetWord(jpgdata);
    c0 = 2;
    do
    {
        temp0 = JPGGetByte(jpgdata);
        c0++;
        // Get the huffman table type (t0 == 0 => DC, AC else) 
        t0 = (temp0 & 16) >> 4;
        // Now save the luminance or chrominance information (temp0 == 0 => Y, CbCr else)
        temp0 &= 15;
        switch (t0)
        {
        case 0:        // DC Table
            total = 0;
            // Read the number of codes
            for (i=0; i<16; i++)
            {
               temp1 = JPGGetByte(jpgdata);
               c0++;
               total += temp1;
               HuffAmount[i+1] = temp1;
            }
            // Read all codes, for each length 
            for (i=0; i<total; i++)
            {
               if (temp0)
                  HuffmanDC1[i].Code = JPGGetByte(jpgdata);
               else
                  HuffmanDC0[i].Code = JPGGetByte(jpgdata);
               c0++;
            }
            
            // Now create the table correctly
            CurNum = 0;
            CurIndex = -1;
            for (i=1; i<16+1; i++)
               {
               for (j=1; j<HuffAmount[i]+1; j++)
                  {
                  CurIndex++;
                  if (temp0)
                     {
                     HuffmanDC1[CurIndex].Index = CurNum;
                     HuffmanDC1[CurIndex].Length = i;
                     }
                  else
                     {
                     HuffmanDC0[CurIndex].Index = CurNum;
                     HuffmanDC0[CurIndex].Length = i;
                     }
                  CurNum++;
                  }
               CurNum *= 2;
               }
            DCTables++;
            break;
        case 1:
            total = 0;
            for (i=1; i<16+1; i++)
               {
               temp1 = JPGGetByte(jpgdata);
               c0++;
               total += temp1;
               HuffAmount[i] = temp1;
               }
            for (i=0; i<total; i++)
               {
               if (temp0)
                  HuffmanAC1[i].Code = JPGGetByte(jpgdata);
               else
                  HuffmanAC0[i].Code = JPGGetByte(jpgdata);
               c0++;
               }
            
            CurNum = 0;
            CurIndex = -1;
            for (i=1; i<16+1; i++)
               {
               for (j=1; j<HuffAmount[i]+1; j++)
                  {
                  CurIndex++;
                  if (temp0)
                     {
                     HuffmanAC1[CurIndex].Index = CurNum;
                     HuffmanAC1[CurIndex].Length = i;
                     }
                  else
                     {
                     HuffmanAC0[CurIndex].Index = CurNum;
                     HuffmanAC0[CurIndex].Length = i;
                     }
                  CurNum++;
                  }
               CurNum *= 2;
               }
            ACTables++;
            break;
        }
    }
    while (c0 < l0);

return(1);
}

/* Get Image attributes */
u16 JPGGetImageAttr (u8 * jpgdata)
{
    /* Temporary variables */
    u32 temp4;
    u16 temp0;
    u16 temp1;
    u16 i;
    u16 id;
    
    /* Segment length */
    temp4 = JPGGetWord(jpgdata); 
    /* Data precision (should be 8 bits) */
    temp0 = JPGGetByte(jpgdata);          
    if (temp0 != 8)
        return(0);                  
    /* Image height */
    Image.Rows = JPGGetWord(jpgdata);     
    /* Image width */
    Image.Cols = JPGGetWord(jpgdata);
    /* Component number */
    temp0 = JPGGetByte(jpgdata);          
    for (i=1; i<temp0+1; i++)
    {
        /* Get the index of the tables */
        id = JPGGetByte(jpgdata);
        switch (id)
        {
        case 1:
            temp1 = JPGGetByte(jpgdata);
            Image.SamplesY = (temp1 & 15) * (temp1 >> 4);
            Image.QuantTableY = JPGGetByte(jpgdata);
            break;
        case 2:
        case 3:
            temp1 = JPGGetByte(jpgdata);
            Image.SamplesCbCr = (temp1 & 15) * (temp1 >> 4);
            Image.QuantTableCbCr = JPGGetByte(jpgdata);
            break;
        }
    }
    
    /* Return */
    PDEBUG(3,"%x Image size (%dx%d = %d:%d)", findex, Image.Rows, Image.Cols, Image.SamplesCbCr, Image.QuantTableCbCr);
    return(1);
}

/* Get the quantized tables */
u8 JPGGetQuantTables(u8 * jpgdata)
{
   u32 l0 = JPGGetWord(jpgdata);
   u16 c0 = 2;
   u16 temp0;
   u16 xp;
   u16 yp;
   u16 i;
   u16 ZigIndex;

   do
      {
      temp0 = JPGGetByte(jpgdata);
      c0++;
      if (temp0 & 0xf0)
         return(0);        //we don't support 16-bit tables

      temp0 &= 15;
      ZigIndex = 0;
      xp = 0;
      yp = 0;
      for (i=0; i<64; i++)
         {
         xp = JPGZig1[ZigIndex];
         yp = JPGZig2[ZigIndex];
         ZigIndex++;
         /* For AA&N IDCT method, multipliers are equal to quantization
          * coefficients scaled by scalefactor[row]*scalefactor[col], where
          *   scalefactor[0] = 1
          *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
          */
         QuantTable[temp0][xp][yp] = (JPGGetByte(jpgdata) * aanscales[(xp<<3) + yp]) >> 12;
         c0++;
         }
      QTables++;
      }
   while (c0 < l0);

   return(1);
}

/* Get the Start of Scan marker */
u16 JPGGetSOS (u8 * jpgdata)
{
    u32 temp4;
    u16 temp0;
    u16 temp1;
    u16 temp2;
    u16 i;
    
    temp4 = JPGGetWord(jpgdata);
    temp0 = JPGGetByte(jpgdata);
    
    if ((temp0 != 1) && (temp0 != 3))
        return(0);
    Image.NumComp = temp0;
    PDEBUG(3,"Number of components : %d", Image.NumComp);
    for (i=1; i<temp0+1; i++)
    {
        temp1 = JPGGetByte(jpgdata);
        PDEBUG(3,"Identifiant : %d", temp1);
        switch (temp1)
        {
        case 1:
            temp2 = JPGGetByte(jpgdata);
            PDEBUG(3,"IndexY : %d", temp2);
            Image.HuffACTableY = temp2 & 15;
            Image.HuffDCTableY = temp2 >> 4;
            break;
        case 2:
            temp2 = JPGGetByte(jpgdata);
            PDEBUG(3,"IndexCb : %d", temp2);
            Image.HuffACTableCbCr = temp2 & 15;
            Image.HuffDCTableCbCr = temp2 >> 4;
            break;
        case 3:
            temp2 = JPGGetByte(jpgdata);
            PDEBUG(3,"IndexCr : %d", temp2);
            Image.HuffACTableCbCr = temp2 & 15;
            Image.HuffDCTableCbCr = temp2 >> 4;
            break;
        default:
            return(0);
        }
    }
    findex += 3;
    return(1);
}

/* Decode a picture */
u32 gfxJpegYUY2 (u8 bGrey, u16 wWidth, u16 wHeight, u8 *vram, u8 *jpgdata, int format)
{
   u8 exit = 1;
   s16 y1 = 0;
   s16 y2 = 0;
   u16 Restart = 0;
   u16 XPos; u16 YPos;
   s16 dcY; s16 dcCb; s16 dcCr;
   u16 xindex; u16 yindex;
   u16 mcu;
   s16 YVector1[8][8];              // 4 vectors for Y attribute
   s16 YVector2[8][8];              // (not all may be needed)
   s16 CbVector[8][8];              // 1 vector for Cb attribute
   s16 CrVector[8][8];              // 1 vector for Cr attribute
   s16 height;

   u16 i,j;
   s16 cb; s16 cr;
   u16 xj;
   u16 yi;
   s16 r; s16 g; s16 b;
   
    // This is offset to place the decoded picture
    u8 x0 = 0;
    u8 y0 = 0;

    QTables = 0;     // Initialize some checkpoint variables
    ACTables = 0;
    DCTables = 0;

    findex = 0;
    
    PDEBUG(4, "%dx%d jpegdata=%p vram=%p, starting with: %02x, %02x, %02x", wWidth, wHeight, jpgdata, vram, jpgdata[0], jpgdata[1], jpgdata[2]);

    if (JPGGetByte(jpgdata) == 0xff)
    {
      if (JPGGetByte(jpgdata) == 0xd8)
         exit = 0;
    }

    // Exit if not a JPEG file
    if (exit) {
    PDEBUG(1, "Error: frame is not a JPEG frame");
    PDEBUG(5, "First 64 bytes of frame, starting at %s: ", jpgdata);
    print_64(jpgdata);
      return(0); }

    //THOMMY:
    //get Width and Height from Jpeg header instead of relying on global variables.
    //this will prevent crashes if the variables FrameWidth and FrameHeight are set up inproperly.
    //this should allow me to test what happens if i set up framewidth and frameheigt to 640x480
    
    wWidth = ((int)jpgdata[12]<<8) | (int)jpgdata[13];
    height = ((int)jpgdata[14]<<8) | (int)jpgdata[15];

    if (height < wHeight)
	    wHeight = height;

    while (!exit)
    {
        if (JPGGetByte(jpgdata) == 0xff)
        {
            switch (JPGGetByte(jpgdata))
            {
            case 0x00: //not important
               break;
            case 0xc0: //SOF0
                PDEBUG(3,"Found start of frame 0");
                JPGGetImageAttr(jpgdata);
                break;
            case 0xc1: //SOF1
                PDEBUG(3,"Found start of frame 1");
                JPGGetImageAttr(jpgdata);
                break;
            case 0xc4: //DHT
                PDEBUG(3,"Found huffman tables (A:%d,D:%d)", ACTables, DCTables );
                if ((ACTables < 2) || (DCTables < 2))
                  JPGGetHuffTables(jpgdata);
                PDEBUG(3,"NbAC : %d, NbDC : %d", ACTables, DCTables);
                break;
            case 0xc9: //SOF9
                break;
            case 0xd9: //EOI
                PDEBUG(3,"Found end of image");
                exit = 1;
                break;
            case 0xda: //SOS
                PDEBUG(3,"Found start of scan");
                JPGGetSOS(jpgdata);
                if ( ((DCTables == 2) &&
                     (ACTables == 2) &&
                     (QTables == 2)) ||
                     (Image.NumComp == 1) )
                {
                    EOI = 0;
                    exit = 1;        // Go on to secondary control loop
                } else
                {
                    PDEBUG(3,"Not enough tables");
                }
                break;
            case 0xdb: //DQT
                PDEBUG(3,"Found quantification tables");
                if (QTables < 2)
                  JPGGetQuantTables(jpgdata);
                break;
            case 0xdd: //DRI
                PDEBUG(3,"Found restart interval");
                (void) JPGGetWord(jpgdata);        // Length of segment
                Restart = JPGGetWord(jpgdata);
                PDEBUG(3,"Restart read : %d", Restart);
                break;
            case 0xe0: //APP0
                PDEBUG(3,"Found application specific");
                (void) JPGGetWord(jpgdata);        // Length of segment
                findex += 5;
                (void) JPGGetByte(jpgdata);        // Major revision
                (void) JPGGetByte(jpgdata);        // Minor revision
                (void) JPGGetByte(jpgdata);        // Density definition
                (void) JPGGetByte(jpgdata);        // X density
                (void) JPGGetByte(jpgdata);        // Y density
                (void) JPGGetByte(jpgdata);        // Thumbnail width
                (void) JPGGetByte(jpgdata);        // Thumbnail height
                break;
            case 0xfe: //COM
// Modified by Cyril
// In Zc0302 modules, comment block is just before the real data
// So we just have to consider this as an header (need to specify value here)
// Modified by Cyril 26/9/04 - Forgot to think that JPGGetWord also advance findex
// Remark : Should read the picture size here, directly from the comment block.
// But for that, it would be better to be able to send the correct init sequence first. 
                findex += JPGGetWord(jpgdata) - 2;        // Length of segment;
                // Set to 1 for GreyScale pictures
                Image.NumComp = bGrey ? 1 : 3;
                // This is because this stream is in 4:2:2 format (ie YYUV YYUV etc)
                Image.SamplesY = 2; 
                Image.Cols     = wWidth;
                Image.Rows     = wHeight;
		
		PDEBUG(5, "%dx%d", Image.Cols, Image.Rows);
            
                Image.HuffACTableY = 0;
                Image.HuffDCTableY = 0;
                Image.HuffACTableCbCr = 1;
                Image.HuffDCTableCbCr = 1;
            
                EOI = 0;
                exit = 1;
            
               break;
            }
        }
    }

    XPos = 0;
    YPos = 0;                            // Initialize active variables
    dcY = 0; dcCb = 0; dcCr = 0;
    xindex = 0; yindex = 0; mcu = 0;
    r = 0; g = 0; b = 0;
    mcu = 0;
    
    
    curBits = 128;                // Start with the seventh bit
    curByte = JPGGetByte(jpgdata);       // Of the first byte
    PDEBUG(5, "%dx%d", Image.Cols, Image.Rows);
    
    switch (Image.NumComp)        // How many components does the image have?
    {
    case 3:                     // 3 components (Y-Cb-Cr)
    {
        PDEBUG(3,"The sampling ratio is : %d", Image.SamplesY);
        switch (Image.SamplesY)   // What's the sampling ratio of Y to CbCr?
        {
        case 2:           // 2 pixels to 1
        do
        {
            JPGGetBlock (YVector1, Image.HuffDCTableY,    Image.HuffACTableY,    Image.QuantTableY,    &dcY, jpgdata);
            JPGGetBlock (YVector2, Image.HuffDCTableY,    Image.HuffACTableY,    Image.QuantTableY,    &dcY, jpgdata);
            JPGGetBlock (CbVector, Image.HuffDCTableCbCr, Image.HuffACTableCbCr, Image.QuantTableCbCr, &dcCb, jpgdata);
            JPGGetBlock (CrVector, Image.HuffDCTableCbCr, Image.HuffACTableCbCr, Image.QuantTableCbCr, &dcCr, jpgdata);
            // YCbCr vectors have been obtained
            mcu++;
            
            if (EOI)
            {
                PDEBUG(3,"Error appeared with this picture");
                return 0;
            }
            
            for (i=0; i<8; i++)       // Draw 16x16 pixels
                for (j=0; j<8; j++)
                {
                    y1 = YVector1[i][j];
		    y2 = YVector2[i][j];
                    cb = CbVector[i][j];
                    cr = CrVector[i][j];
                    xj = xindex;
                    yi = yindex;
                    if ((xj < Image.Cols) && (yi < Image.Rows))
                        gfxPixelYUV (xj + x0, yi + y0, y1, cb, y2, cr, vram, Image.Cols, Image.Rows, j, i);
                }
        

                xindex += 8;
                    
                    
                if (Restart && mcu == Restart)
                {
                    // eject restart marker now
                    u8 marker0 = JPGGetByte(jpgdata);
                    u8 marker1 = JPGGetByte(jpgdata);
                    
                    if (marker0 != 0xFF || (marker1 < 0xD0 && marker1 > 0xD7))
                    {
                            // A restart marker should be here, but not found
                            PDEBUG(3,"Restart should be here, but not found, arounding byte are :");
                            PDEBUG(3,"%02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X", 
                            jpgdata[findex - 10], jpgdata[findex - 9], jpgdata[findex - 8], jpgdata[findex - 6],
                            jpgdata[findex -  5], jpgdata[findex - 4], jpgdata[findex - 3], jpgdata[findex - 2],
                            jpgdata[findex -  1], jpgdata[findex    ], jpgdata[findex + 1], jpgdata[findex + 2],
                            jpgdata[findex +  3], jpgdata[findex + 4], jpgdata[findex + 5], jpgdata[findex + 6]);
                        
                            PDEBUG(3,"Resync the stream now");
                            // Look in the +/- 8 bytes to find restart marker
                            for (j = 0; j < 16; j++)
                            {
                                if (jpgdata[findex - 10 + j] == 0xFF && (jpgdata[findex - 9 + j] >= 0xD0 && jpgdata[findex - 9 + j] <= 0xD7 ))
                                {
                                    // Found restart marker, sync the stream now
                                    findex -=  (10 - j);
                                    marker0 = JPGGetByte(jpgdata); 
                                    marker1 = JPGGetByte(jpgdata);
                                    PDEBUG(3,"Marker found : %02X, %02X", marker0, marker1);
                                    break;
                                }
                            }
                            
                            // No restart marker found, so stop decoding
                            if (j == 16)
                                return(0);
                        
                    }                    
    
                    // Feed again the decoder
                    curByte = JPGGetByte(jpgdata); PDEBUG(3,"%02X", curByte);
                    curBits = 128;
                    
                    dcY = 0; dcCb = 0; dcCr = 0; mcu = 0;  //Reset the DC value
                }
            
            if ( xindex >= Image.Cols/2)
            {
                xindex = 0; yindex += 4;
            }
        }
        while (yindex < Image.Rows/2);
        break;
        }
        
    }
    case 1:
        do
        { //Nothing to do for ov519, the sampling is fixed at 2..
            xindex += 8;
            if (xindex >= Image.Cols/2)
            {
                xindex = 0; yindex += 8; mcu = 1;
            }
        
       
        }
        while (yindex < Image.Rows/2);
    break;
    }

    return 1;
}

/* Store some bits from the stream */
s16 JPGReceiveBits (u16 cat, u8 * jpgdata)
{
    u32 temp0 = 0;
    u16 i;
    s32 ReceiveBits;
    
    for (i=0; i<cat; i++)
        temp0 = temp0 * 2 + JPGNextBit(jpgdata);
    if ((temp0*2) >= (JPGpower2(cat)) )
        ReceiveBits = temp0;
    else
        ReceiveBits = -(JPGpower2(cat) - 1) + temp0;
    return (ReceiveBits);
}

/* Input format is raw isoc. data (with intact SOF header, packet numbers
 * stripped, and all-zero blocks removed).
 * Output format is planar YUV400
 * Returns uncompressed data length if success, or zero if error
 */
static int 
Decompress400(unsigned char *pIn,
	      unsigned char *pOut,
	      unsigned char *pTmp,
	      int	     w,
	      int	     h,
	      int	     inSize)
{
	int numpix = w * h;

	/* Decompress*/
	if (gfxJpegYUY2(0, w, h, (u8 *)pOut, pIn, VIDEO_PALETTE_UYVY) < 1) 
	PDEBUG(2, "Error while decompressing frame");
		 /* Don't return error yet */

	return (numpix);
}

/* Input format is raw isoc. data (with intact JPEG header, packet numbers
 * stripped, and all-zero blocks removed).
 * Output format is planar YUV420
 * Returns uncompressed data length if success, or zero if error
 */
static int 
Decompress420(unsigned char *pIn,
	      unsigned char *pOut,
	      unsigned char *pTmp,
	      int	     w,
	      int	     h,
	      int	     inSize)
{
	int numpix = w * h;	


	PDEBUG(4, "%dx%d pIn=%p pOut=%p pTmp=%p inSize=%d", w, h, pIn, pOut,
	       pTmp, inSize);

	/* Decompress*/
	if (gfxJpegYUY2(0, w, h, (u8 *)pOut, pIn, VIDEO_PALETTE_UYVY) < 1)
	PDEBUG(2, "Error while decompressing frame");	



	
	return (numpix * 3 / 2);
}



/******************************************************************************
 * Module Functions
 ******************************************************************************/

struct ov51x_decomp_ops ov519_decomp_ops = {
	.decomp_400 =	Decompress400,	
	.decomp_420 =	Decompress420,	
	.owner =	THIS_MODULE,
};



