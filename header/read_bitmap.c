#include "read_bitmap.h" 
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> // memcpy のために必要

// tc はローカルカウンターとしてファイルスコープで定義 (エラー解消)
static int tc = 0; 

// --- 修正されたデータ読み込み関数 (戻り値を int に変更) ---

// 1バイト読み込み。成功時 0-255, 失敗時 EOF (-1)
static int ReadByte(FILE *fp)
{
    unsigned char ii;
    // 1バイト読み込み
    if(fread(&ii,sizeof(unsigned char), 1,fp) != 1) {
        return EOF; // 読み込み失敗時は EOF を返す
    }
    tc++;
    return (int)ii;
}

// 2バイト読み込み。成功時 0-65535, 失敗時 EOF (-1)
static int ReadWord(FILE* fp)
{
    int b1, b2;
    b1 = ReadByte(fp);
    if (b1 == EOF) return EOF;
    b2 = ReadByte(fp);
    if (b2 == EOF) return EOF;
    return (b1 | (b2 << 8));
}

// 4バイト読み込み。成功時 0-0xFFFFFFFF, 失敗時 EOF (-1)
static int ReadDword(FILE* fp)
{
    int b1, b2, b3, b4;
    b1 = ReadByte(fp); if (b1 == EOF) return EOF;
    b2 = ReadByte(fp); if (b2 == EOF) return EOF;
    b3 = ReadByte(fp); if (b3 == EOF) return EOF;
    b4 = ReadByte(fp); if (b4 == EOF) return EOF;
    // DWORD (uint32_t) は最大 0xFFFFFFFF なので、int の範囲を超えないか確認が必要だが、
    // 戻り値の unsigned int を int に変更したため、EOF判定が優先される。
    // 正常な戻り値は 32bit の下位として扱う。
    return (b1 | (b2 << 8) | (b3 << 16) | (b4 << 24));
}
// ----------------------------------------------------------------

static int ReadBitmapFile(char *pFilename,BITMAPINFO *pbm,unsigned char** pptr)
{
    BITMAPFILEHEADER bmFile;
    int i, len, read_val;
    unsigned char* ptr;
    FILE *fp=fopen(pFilename,"rb");
    if (fp==NULL) return 0;
    
    // 1. BITMAPFILEHEADER の読み込み (14バイト)
    // 構造体を一括で読み込むのが最も安全だが、ここでは ReadWord/Dword を使う
    read_val = ReadWord(fp); if (read_val == EOF) { fclose(fp); return 0; }
    bmFile.bfType = (WORD)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    bmFile.bfSize = (DWORD)read_val;
    
    read_val = ReadWord(fp); if (read_val == EOF) { fclose(fp); return 0; }
    bmFile.bfReserved1 = (WORD)read_val;
    
    read_val = ReadWord(fp); if (read_val == EOF) { fclose(fp); return 0; }
    bmFile.bfReserved2 = (WORD)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    bmFile.bfOffBits = (DWORD)read_val;

    // ファイルタイプチェック
    if (bmFile.bfType != 0x4D42) { // 'BM'
        fclose(fp);
        return 0; // BMPファイルではない
    }

    // 2. BITMAPINFOHEADER の読み込み
    // ReadDword/Word の戻り値が int になり、EOFチェックが確実になった
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biSize=(DWORD)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biWidth=(LONG)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biHeight=(LONG)read_val;
    
    read_val = ReadWord(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biPlanes=(WORD)read_val;
    
    read_val = ReadWord(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biBitCount=(WORD)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biCompression=(DWORD)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biSizeImage=(DWORD)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biXPelsPerMeter=(LONG)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biYPelsPerMeter=(LONG)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biClrUsed=(DWORD)read_val;
    
    read_val = ReadDword(fp); if (read_val == EOF) { fclose(fp); return 0; }
    pbm->bmiHeader.biClrImportant=(DWORD)read_val;

    // 3. カラーパレットの読み込み
    int num_colors = 0;
    if (pbm->bmiHeader.biBitCount == 8) {
        num_colors = 256;
    } else if (pbm->bmiHeader.biBitCount == 4) {
        num_colors = 16;
    }

    for (i = 0; i < num_colors; i++) {
        read_val = ReadByte(fp); if (read_val == EOF) { fclose(fp); return 0; }
        pbm->bmiColors[i].rgbBlue = (uint8_t)read_val;

        read_val = ReadByte(fp); if (read_val == EOF) { fclose(fp); return 0; }
        pbm->bmiColors[i].rgbGreen = (uint8_t)read_val;

        read_val = ReadByte(fp); if (read_val == EOF) { fclose(fp); return 0; }
        pbm->bmiColors[i].rgbRed = (uint8_t)read_val;

        read_val = ReadByte(fp); if (read_val == EOF) { fclose(fp); return 0; }
        pbm->bmiColors[i].rgbReserved = (uint8_t)read_val;
    }

    // 4. ピクセルデータへのシーク
    if (fseek(fp, bmFile.bfOffBits, SEEK_SET) != 0) {
        fclose(fp);
        return 0; // シーク失敗
    }

    // 5. ピクセルデータの読み込み (元のコードのロジックを維持)
    if (pbm->bmiHeader.biBitCount==8) {     
        len=pbm->bmiHeader.biWidth*pbm->bmiHeader.biHeight;
        ptr = (unsigned char*) malloc(len);
        for (i=0;i<len;i++) {
            read_val=ReadByte(fp);
            if (read_val == EOF) { // EOFチェック
                // EOFが発生した場合、データ不足とみなし、残りを0で埋めて終了
                memset(&ptr[i], 0, len - i);
                break;
            }
            ptr[i]=(unsigned char)read_val;
        }
    }
    else if (pbm->bmiHeader.biBitCount==4) {        
        // 4bppの場合、データサイズは (Width * Height) / 2
        // ただし、元のコードは Width * Height のピクセル配列に展開している
        len = pbm->bmiHeader.biWidth * pbm->bmiHeader.biHeight;
        ptr = (unsigned char*) malloc(len);
        
        // 1バイトに2ピクセルが格納されている
        for (i=0;i<len;i+=2) {
            read_val=ReadByte(fp);
            if (read_val == EOF) {
                // EOFが発生した場合、データ不足とみなし、残りを0で埋めて終了
                memset(&ptr[i], 0, len - i);
                break;
            }
            unsigned char tmp = (unsigned char)read_val;
            ptr[i+1]=tmp & 0x0f;   // 下位4bitが2つ目のピクセル
            ptr[i]=(tmp>>4) & 0x0f; // 上位4bitが1つ目のピクセル
        }
    }
    else {
        // サポートされていないビット深度
        fclose(fp);
        return 0;
    }

    *pptr = ptr;
    fclose(fp);
    return 1;
}

// ReadBitMapData は static を外し、外部から参照可能にする (リンクエラー解消)
int ReadBitMapData(char *pFilename,int *width,int *height,unsigned char **ppixel)
{
    int w,h,i,j;
    int counter;

    unsigned char *buf;
    unsigned char *pixels;
    
    // BITMAPINFO のサイズを正確に確保
    BITMAPINFO *pbm = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD));
    if (!pbm) return 0;

    // ReadBitmapFileが失敗した場合、pbmを解放して終了
    if (ReadBitmapFile(pFilename,pbm,&buf)==0) {
        free(pbm); 
        return 0;
    }
    
    *width=w=pbm->bmiHeader.biWidth;    
    *height=h=pbm->bmiHeader.biHeight;  
    
    // ピクセルデータ形式: RGBA (4バイト/ピクセル)
    pixels = (unsigned char*)malloc((size_t)h * (size_t)w * 4);
    
    // メモリ確保失敗時のチェック
    if (!pixels) {
        free(buf);
        free(pbm);
        return 0;
    }
    
    // --- ピクセルデータ変換ロジック (変更なし) ---
    if (pbm->bmiHeader.biBitCount==8) {
        for (j=0;j<h;j++) {
            for (i=0;i<w;i++) {
                // BMPは下から上へ格納されている場合があるが、元のコードがその対応をしていないためそのまま
                pixels[j*w*4+i*4]    = pbm->bmiColors[buf[w*j+i]].rgbRed;
                pixels[j*w*4+i*4+1]  = pbm->bmiColors[buf[w*j+i]].rgbGreen;
                pixels[j*w*4+i*4+2]  = pbm->bmiColors[buf[w*j+i]].rgbBlue;
                pixels[j*w*4+i*4+3]  = 255; // アルファチャンネルを255に設定
            }
        }
    }
    else if (pbm->bmiHeader.biBitCount==4) {
        counter=0;
        for (j=0;j<h;j++) {
            for (i=0;i<w;i++) {
                int color=buf[counter++];
                pixels[j*w*4+i*4]   = pbm->bmiColors[color].rgbRed;
                pixels[j*w*4+i*4+1] = pbm->bmiColors[color].rgbGreen;
                pixels[j*w*4+i*4+2] = pbm->bmiColors[color].rgbBlue;
                pixels[j*w*4+i*4+3] = 255; // アルファチャンネルを255に設定
            }
        }
    }
    
    free(buf);
    free(pbm); 
    *ppixel = pixels;
    return 1;
}