#ifndef CPACK_H
#define CPACK_H

#include <stdio.h>
#include <stdint.h>

/*Look for CPack.c if you want change compression algorithm*/
/*Compilation macro*/
#ifdef __cplusplus
	#define EXT extern"C"
#else
	#define EXT extern
#endif


#define USE_MMAN 0		/* For using memory mapping */
#define stringsize 64	/* Key lenght. For more lenght names recompile it with new value */



/*======================PACKAGE STRUCTURE======================*/
/* Header */
typedef struct {
	char		FORMAT[4];	// "Pkg'FS'" signature
	uint32_t	PSIZE;
	int32_t		DCOUNT;
}HEADER;

/* Key */
#pragma pack(push, 1)
typedef struct {
	char		ID[stringsize];
	uint32_t	SP;
	uint32_t	EP;
	int32_t		UNCOMPRESSED_SIZE;
	uint32_t	HASHSUM;
}KEY;
#pragma pack(pop)

enum htable_error {
  HTABLE_ERROR_NONE,
  HTABLE_ERROR_NOT_FOUND,
  HTABLE_ERROR_MALLOC_FAILED,
};

#pragma pack(push, 1)
struct htable_item {
  uint32_t		hash;
  KEY			value;
  char			*key;
  size_t		key_len;
  struct htable_item *next;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct htable {
  uint32_t			 mask;
  enum htable_error  last_error;
  size_t			 size;
  size_t			 items;
  struct htable_item **items_table;
};
#pragma pack(pop)

/*Package*/
#pragma pack(push, 1)
typedef struct
{
	FILE			*file;
	int32_t			startpos;
	short			version;
	HEADER			header;
	struct htable	*keys;
}pkgfile;
#pragma pack(pop)

/* COMPILATION CONSTANTS */
#define COMPRESSION_RATE 0x10	/* max 16 */
#define RESERVED_MEMORY	 128000	/* need for compressing */
/*=============================================================*/


/*=========================FUNCTIONS===========================*/
/* Create empty package. Return 1 if no errors. */
EXT int		 pkg_create(const char* name);

/* Create empty package for file handle. Return 1 if no errors. */
EXT int		 pkg_create_h(FILE *handle);

/* Open the package. Return NULL if error. */
EXT pkgfile* pkg_open(const char* name, short version);

/* Open the package from handle. Return NULL if error. */
EXT pkgfile* pkg_open_h(FILE *handle, short version);

/* Return allocated memory. Return NULL if error. */
EXT void*	 pkg_get(pkgfile *pkg, const char* name);

/* Return data. "DATA" must be allways allocated! Return 1 if no errors. */
EXT int		 pkg_get_s(pkgfile *pkg, const char* name, void* DATA);

/* Add data to package. Return 1 if no errors. */
EXT int		 pkg_add(pkgfile *pkg, const char* name, const void* DATA, int32_t size);

/* Rename section. Return 1 if no errors. */
EXT int		 pkg_rename_key(pkgfile *pkg, const char* oldname, const char* newname);

/* Returns package size or -1 if error. */
EXT int		 pkg_psize(pkgfile *pkg);

/* Returns count of data sections or -1 if error. */
EXT int		 pkg_datacount(pkgfile *pkg);

/* Returns name of data section from index or "" if error. */
EXT char*	 pkg_list(pkgfile *pkg, int index);

/* Returns uncompressed size of data or -1 if error. */
EXT int		 pkg_datasize(pkgfile *pkg, const char* name);

/* Returns 1 if succesfully remove data from package or 0 if error. */
EXT int		 pkg_remdata(pkgfile *pkg, const char* name);

/* Use pkg = pkg_close(pkg) for more safety! */
EXT pkgfile* pkg_close(pkgfile *pkg);

#endif //end of CPACK_H
