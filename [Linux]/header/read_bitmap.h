#ifndef READ_BITMAP_H
#define READ_BITMAP_H

#include <stdio.h>
#include <stdint.h> // Linux環境での整数型のために必要

// Windowsデータ型 (WORD, DWORD, LONG) の代替定義
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;

// BMPヘッダー構造体の代替定義
typedef struct tagBITMAPFILEHEADER {
    WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    DWORD   biSize;
    LONG    biWidth;
    LONG    biHeight;
    WORD    biPlanes;
    WORD    biBitCount;
    DWORD   biCompression;
    DWORD   biSizeImage;
    LONG    biXPelsPerMeter;
    LONG    biYPelsPerMeter;
    DWORD   biClrUsed;
    DWORD   biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD {
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[256]; 
} BITMAPINFO;

// 外部公開する関数のプロトタイプ宣言
int ReadBitMapData(char *pFilename, int *width, int *height, unsigned char **ppixel);

#endif // READ_BITMAP_H