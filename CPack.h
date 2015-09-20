#ifndef CPACK_H
#define CPACK_H

#include <stdio.h>
#include <stdint.h>

/*Look for CPack.c if you want change compression algorithm*/
/*Compilation macro*/
#ifdef __cplusplus
	extern"C" { 
#endif

//#define __DLL__
#ifdef __DLL__
#	define EXT __declspec(dllexport)
#else
#	define EXT
#endif

/* For using memory mapping */
#define USE_MMAN 1
/* Key lenght. For more lenght names recompile it with new value */
#define stringsize 64
/* COMPILATION CONSTANTS */
#define RESERVED_MEMORY	 128000	/* need for compressing */


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


enum htable_error {
  HTABLE_ERROR_NONE,
  HTABLE_ERROR_NOT_FOUND,
  HTABLE_ERROR_MALLOC_FAILED,
};


struct htable_item {
  uint32_t		hash;
  KEY			value;
  char			*key;
  size_t		key_len;
  struct htable_item *next;  
};

struct htable {
  uint32_t			 mask;
  enum htable_error  last_error;
  size_t			 size;
  size_t			 items;
  struct htable_item **items_table;
};

/*Package*/
typedef struct
{
	FILE			*file;
	int32_t			startpos;
	short			version;
	HEADER			header;
	struct htable	*keys;
}pkgfile;

typedef struct
{
	char			*file;
	short			version;
	HEADER			header;
	struct htable	*keys;
}mempkgfile;
#pragma pack(pop)


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
EXT int		 pkg_add(pkgfile *pkg, const char* name, const void* DATA, int32_t size, int complevel);

/* Rename section. Return 1 if no errors. */
EXT int		 pkg_rename(pkgfile *pkg, const char* oldname, const char* newname);

/* Returns package size or -1 if error. */
EXT int		 pkg_psize(pkgfile *pkg);

/* Returns count of data sections or -1 if error. */
EXT int		 pkg_datacount(pkgfile *pkg);

/* Returns name of data section from index or "" if error. */
EXT char*	 pkg_list(pkgfile *pkg, int index);

/* Returns uncompressed size of data or -1 if error. */
EXT int		 pkg_datasize(pkgfile *pkg, const char* name);

/* Returns compressed size of data or -1 if error. */
EXT int		 pkg_compressedsize(pkgfile *pkg, const char* name);

/* Returns 1 if succesfully remove data from package or 0 if error. */
EXT int		 pkg_remdata(pkgfile *pkg, const char* name);

/* Use pkg = pkg_close(pkg) for more safety! */
EXT pkgfile* pkg_close(pkgfile *pkg);



/*=========================IN MEMORY READ===========================*/

/* Open the package. Return NULL if error. */
EXT mempkgfile* mpkg_open(char* mem, short version);

/* Return allocated memory. Return NULL if error. */
EXT void*	 mpkg_get(mempkgfile *mpkg, const char* name);

/* Return data. "DATA" must be allways allocated! Return 1 if no errors. */
EXT int		 mpkg_get_s(mempkgfile *mpkg, const char* name, void* DATA);

/* Returns package size or -1 if error. */
EXT int		 mpkg_psize(mempkgfile *mpkg);

/* Returns count of data sections or -1 if error. */
EXT int		 mpkg_datacount(mempkgfile *mpkg);

/* Returns name of data section from index or "" if error. */
EXT char*	 mpkg_list(mempkgfile *mpkg, int index);

/* Returns uncompressed size of data or -1 if error. */
EXT int		 mpkg_datasize(mempkgfile *mpkg, const char* name);

/* Returns compressed size of data or -1 if error. */
EXT int		 mpkg_compressedsize(mempkgfile *mpkg, const char* name);

/* Use mpkg = mpkg_close(mpkg) for more safety! */
EXT mempkgfile* mpkg_close(mempkgfile *mpkg);

#ifdef __cplusplus
	}
#endif

#endif //end of CPACK_H
