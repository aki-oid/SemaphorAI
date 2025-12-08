//
//	A program to read a BMP file (C version) 
//		2nd/January/2002 by K.Kato
//

#include <windows.h>
#include <stdio.h>

static int tc=0;

struct	RGB_PIXELS {
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};
struct	IMAGE {
	int	xsz;
	int	ysz;
	int x1,y1,x2,y2;
	struct RGB_PIXELS 	*pixels;
};
			
static unsigned int ReadByte(FILE *fp)
{
	unsigned char ii;
	fread(&ii,sizeof(unsigned char), 1,fp);
	//ii=(unsigned int)fgetc(fp);

	//printf("%d: %u\n",tc, ii);
	tc++;
	return(ii);
}

static unsigned int ReadWord(FILE* fp)
{
	unsigned int tmp = ReadByte(fp);
	return tmp |= (ReadByte(fp)<<8);
}
static unsigned int ReadDword(FILE* fp)
{
	unsigned int tmp = ReadByte(fp);
	tmp |= (ReadByte(fp)<<8);
	tmp |= (ReadByte(fp)<<16);
	tmp |= (ReadByte(fp)<<24);
	return tmp;
}
static int ReadBitmapFile(char *pFilename,BITMAPINFO *pbm,unsigned char** pptr)
{
	int hnum;
	int i,len;
	int counter,flag;
	unsigned char tmp;
	unsigned char* ptr;
	FILE *fp=fopen(pFilename,"rb");
	if (fp==NULL) return 0;
	hnum = sizeof(BITMAPFILEHEADER);
	for (i=0;i<hnum;i++) {	// discard the part of BITMAPFILEHEADER
		fgetc(fp);
	}
// Read BITMAPINFOHEADER
	pbm->bmiHeader.biSize=ReadDword(fp); 
	pbm->bmiHeader.biWidth=ReadDword(fp); 
	pbm->bmiHeader.biHeight=ReadDword(fp); 
	pbm->bmiHeader.biPlanes=ReadWord(fp); 
	pbm->bmiHeader.biBitCount=ReadWord(fp); 
	pbm->bmiHeader.biCompression=ReadDword(fp);
	pbm->bmiHeader.biSizeImage=ReadDword(fp);
	pbm->bmiHeader.biXPelsPerMeter=ReadDword(fp);
	pbm->bmiHeader.biYPelsPerMeter=ReadDword(fp);
	pbm->bmiHeader.biClrUsed=ReadDword(fp);
	pbm->bmiHeader.biClrImportant=ReadDword(fp);
/*
	printf(" Header debug\n");
	printf(" pbm->bmiHeader.biSize %d\n", pbm->bmiHeader.biSize);
	printf(" pbm->bmiHeader.biWidth %d\n", pbm->bmiHeader.biWidth);
	printf(" pbm->bmiHeader.biHeight %d\n", pbm->bmiHeader.biHeight);
	printf(" pbm->bmiHeader.biPlanes %d\n", pbm->bmiHeader.biPlanes);
	printf(" pbm->bmiHeader.biBitCount %d\n", pbm->bmiHeader.biBitCount);
	printf(" pbm->bmiHeader.biCompression %d\n", pbm->bmiHeader.biCompression);
	printf(" pbm->bmiHeader.biSizeImage %d\n", pbm->bmiHeader.biSizeImage);
	printf(" pbm->bmiHeader.biXPelsPerMeter %d\n", pbm->bmiHeader.biXPelsPerMeter);
	printf(" pbm->bmiHeader.biYPelsPerMeter %d\n", pbm->bmiHeader.biYPelsPerMeter);
	printf(" pbm->bmiHeader.biClrUsed %d\n", pbm->bmiHeader.biClrUsed);
	printf(" pbm->bmiHeader.biClrImportant %d\n", pbm->bmiHeader.biClrImportant);
*/
	if (pbm->bmiHeader.biBitCount==8) {		// Read RGBQUAD
		for (i=0;i<256;i++) {
			pbm->bmiColors[i].rgbBlue = ReadByte(fp);
			pbm->bmiColors[i].rgbGreen = ReadByte(fp);
			pbm->bmiColors[i].rgbRed = ReadByte(fp);
			pbm->bmiColors[i].rgbReserved = ReadByte(fp);
		}
		len=pbm->bmiHeader.biWidth*pbm->bmiHeader.biHeight;
		ptr = (unsigned char*) malloc(len);
		for (i=0;i<len;i++) {
			ptr[i]=ReadByte(fp);
			//printf(" ReadByte[%d] %d\n",i,ptr[i]);
			if (ptr[i]==EOF) {
				ptr[i]=ptr[i];
			}
		}
	}
	else if (pbm->bmiHeader.biBitCount==4) {		// Read RGBQUAD
		for (i=0;i<16;i++) {
			pbm->bmiColors[i].rgbBlue = ReadByte(fp);
			pbm->bmiColors[i].rgbGreen = ReadByte(fp);
			pbm->bmiColors[i].rgbRed = ReadByte(fp);
			pbm->bmiColors[i].rgbReserved = ReadByte(fp);
		}
		len=pbm->bmiHeader.biWidth*pbm->bmiHeader.biHeight;
		ptr = (unsigned char*) malloc(len);
		for (i=0;i<len;i+=2) {
			tmp=ReadByte(fp);
			ptr[i+1]=tmp & 0x0f;
			ptr[i]=(tmp>>4) & 0x0f;
		}
	}
	*pptr = ptr;
	fclose(fp);
	return 1;
}

int ReadBitMapData(char *pFilename,int *width,int *height,unsigned char **ppixel)
{
	int status,sel;
	int w,h,i,j;
	int counter,flag,color;

	unsigned char *buf;
	unsigned char *pixels;
	BITMAPINFO *pbm = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD));
	if (ReadBitmapFile(pFilename,pbm,&buf)==0) return 0;
	*width=w=pbm->bmiHeader.biWidth; 
	*height=h=pbm->bmiHeader.biHeight; 
	pixels = (unsigned char*)malloc(h*w*4);
	if (pbm->bmiHeader.biBitCount==8) {
		for (j=0;j<h;j++) {
			for (i=0;i<w;i++) {
				pixels[j*w*4+i*4]    = pbm->bmiColors[buf[w*j+i]].rgbRed;
				pixels[j*w*4+i*4+1]  = pbm->bmiColors[buf[w*j+i]].rgbGreen;
				pixels[j*w*4+i*4+2]  = pbm->bmiColors[buf[w*j+i]].rgbBlue;
				pixels[j*w*4+i*4+3]  = 255;

			}
		}
	}
	else if (pbm->bmiHeader.biBitCount==4) {
		counter=0;
		flag=0;
		for (j=0;j<h;j++) {
			for (i=0;i<w;i++) {
				color=buf[counter++];
				pixels[j*w*4+i*4]   = pbm->bmiColors[color].rgbRed;
				pixels[j*w*4+i*4+1] = pbm->bmiColors[color].rgbGreen;
				pixels[j*w*4+i*4+2] = pbm->bmiColors[color].rgbBlue;
				pixels[j*w*4+i*4+3] = 255;
			}
		}
	}
	free(buf);
	*ppixel = pixels;
	return 1;
}
