// If using VC you need to disable precompiled headers
//
// Syntax:
//   mpfsx <filename>
//
#ifdef _WIN32
#define makedir _mkdir
#define STRUCT_PACKER 
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#include "direct.h"
#else
#include "sys/types.h"
#include "sys/stat.h"
#define makedir(__MYDIR__) mkdir(__MYDIR__,S_IRWXU)
#define STRUCT_PACKER __attribute__((__packed__))
#define PACK( __Declaration__ ) __Declaration__
#endif
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"

#define FILENAME_SIZE 64



PACK(
struct mpfsheader {
	char sign[4];
	char majorv;
	char minorv;
	uint16_t entries;
});

PACK(
struct fileheader {
	uint32_t  filename;
	uint32_t  filestart;
	uint32_t  filesize;
	uint32_t  timestamp;
	uint32_t  microtime;
	uint16_t  flags;	// 1 => GZIPPED
});

// Fix sizeof problem with packed structs
const unsigned int fileheadersize = 22;
const unsigned int mpfsheadersize = 8;


const char SIGN[] = "MPFS";
const char expath[] = "extract";

int main(int argc,char *argv[])
{
	FILE *F;
	char ok;
	mpfsheader header;
	char *files;
	uint32_t filetable;
	char oldfilename[FILENAME_SIZE + 1] = "";
	if (argc != 2) {
		printf("Syntax: \n");
		printf("  mpfs <filename>");
		return(-1);
	}
	F=fopen(argv[1], "rb");
	if (F == NULL) {
		printf("Can't open %s\n", argv[1]);
		return(-1);
	}

	if (fread(&header, mpfsheadersize, 1, F) != 1) {
		printf("Can't read %s\n",argv[1]);
		fclose(F);
		return(-1);
	}

	ok = 1;
	for (int i = 0; SIGN[i] && ok; i++)
		if (SIGN[i] != header.sign[i]) ok = 0;

	if (ok == 0) {
		printf("Wrong file type\n");
		fclose(F);
		return(-1);
	}
	printf("MPFS v%c.%c\n", (header.majorv + '0'), (header.minorv + '0'));
	printf("Files count: %d\n", header.entries);
	printf("Listing...\n");

	// Skip hashs
	filetable = 0x08 + header.entries * 2;
	fseek(F, filetable,SEEK_SET);

	// Load FAT
	files = (char *)malloc(header.entries * fileheadersize);

	if (files == NULL) {
		printf("Error allocating memory\n");
		fclose(F);
		return(-1);
	}

	if (fread(files, fileheadersize, header.entries, F) != header.entries) {
		printf("Error reading file list\n");
		free(files);
		fclose(F);
		return(-1);
	}

	// Create "extract" dir
	makedir(expath);

	for (int i = 0; i < header.entries; i++) {
		// Todo: some more boring checks
		char filename[FILENAME_SIZE + 8 + 1];
		char temp[FILENAME_SIZE + 8 + 1];
		char path[FILENAME_SIZE + sizeof(expath) + 1];
		filename[FILENAME_SIZE + 8] = 0;
		FILE *FF;
		fileheader *filehdr;
		filehdr = (fileheader *)(files + i*fileheadersize);
		uint32_t name = filehdr->filename;
		uint32_t file = filehdr->filestart;
		uint32_t size = filehdr->filesize;
		tm *t = localtime((time_t *)&filehdr->timestamp);
		char res[40];
		char *buffer;
		strftime(res, sizeof(res), "%c", t);
		fseek(F, name, SEEK_SET);
		fread(filename, FILENAME_SIZE, 1, F);
		if (strlen(filename) == 0)
			sprintf(filename, "%s.idx", oldfilename);
		if (filehdr->flags&0x01)
			strcat(filename,".gz");	// Todo: decompress files
		printf("%-025s", res);
		printf("[%02x] ", filehdr->flags);
		printf("%-64s ", filename);
		printf("%9d ", filehdr->filesize);
		for (int k = 0,m=0; filename[k]; k++) {
			if (filename[k] == '/') {
				temp[m] = 0;
				sprintf(path, "%s/%s", expath, temp);
				makedir(path);
			}
			temp[m++] = filename[k];
		}
		sprintf(path, "%s/%s", expath, filename);
		FF=fopen(path, "wb");
		if (FF != NULL) {
			buffer = (char *)malloc(size);
			if (buffer != NULL) {
				fseek(F, file, SEEK_SET);
				fread(buffer, size, 1, F);
				fwrite(buffer, size, 1, FF);
				free(buffer);
			}
			else {
				printf("MALLOC ERROR!");
			}
			fclose(FF);
		}
		else{
			printf("FILE ERROR!");
		}
		printf("\n");
		strcpy(oldfilename, filename);
	}

	free(files);
	fclose(F);
	return 0;
}
