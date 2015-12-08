#include "CPack.h"
#include "lz4.h"
#include "lz4hc.h"
#include "htable.h"
#include "xxhash.h"
#include <string.h>
#include <malloc.h>

#ifdef _WIN32
#  include <io.h>
#  ifdef _MSC_VER
#  pragma warning(disable:4996)
#  endif
#  define resize(fd, size) _chsize(fd, size)
#else
#  include <unistd.h>
#  define resize(fd, size) ftruncate(fd, size)
#endif

#if USE_MMAN
#  ifdef _WIN32
#    include "mman_win32.h"
#  else
#    include <sys/mman.h>
#  endif
#endif

#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_DEPRECATE     /* VS2005 */
#  pragma warning(disable : 4127)      /* disable: C4127: conditional expression is constant */
#else
#  ifdef __GNUC__
#    define FORCE_INLINE static inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE static inline
#  endif
#endif

/* Compression and decompression macro */
#define COMPRESS(src, dst, size, ratio) \
	LZ4_compressHC2(src, dst, size, ratio)

#define DECOMPRESS(src, dst, size) \
	LZ4_decompress_fast(src, dst, size)

/* Hash function macro */
#define HASH(src, size, version) \
	XXH32(src, size, version)

/* File length function */
FORCE_INLINE long filesize(FILE *fp)
{
	static long prew;
	static long cur;
	prew = ftell(fp);
	fseek(fp, 0, SEEK_END);
	cur = ftell(fp);
	fseek(fp, prew, SEEK_SET);
	return cur;
}

/* Convert string to lower characters */
FORCE_INLINE const char* ToLower(const char *str)
{
	int i;
	char out[stringsize];
	for(i = 0; i < (long)strlen(str); i++)
		out[i] = (char)tolower((int)str[i]);
	return &out[0];
}

/* ERROR FUNCTION */
FORCE_INLINE int ERR(const char* err, int ret)
{
	fputs(err, stderr);
	return ret;
}


/* Package signature */
static const char signature[] = {'P', 'k', 'g', 28};


/*=========================FUNCTIONS===========================*/

int pkg_create(const char* name)
{
	FILE *f;
	HEADER head;
	f = fopen(name, "wb");
	if(!f)	return ERR("Can`t open file\n", 0);
	memcpy(head.FORMAT, signature, sizeof(head.FORMAT));
	head.DCOUNT = 0;
	head.PSIZE = sizeof(HEADER);
	fwrite((char*)&head, sizeof(head), 1, f);
	fclose(f);
	return 1;
}

int pkg_create_h(FILE *handle)
{
	HEADER head;
	if(!handle)
		return ERR("Can`t open file\n", 0);
	memcpy(head.FORMAT, signature, sizeof(head.FORMAT));
	head.DCOUNT = 0;
	head.PSIZE = sizeof(HEADER);
	fwrite((char*)&head, sizeof(head), 1, handle);
	return 1;
}

pkgfile* pkg_open(const char* name, short version)
{
	int			i;
	static KEY	swap;
	pkgfile *pkg = (pkgfile*)malloc(sizeof(pkgfile));
	pkg->file = fopen(name, "rb+");
	if(!pkg->file) {
		free(pkg);
		ERR("Can`t open file\n", 0);
		return NULL;
	}
	pkg->startpos = 0;
	pkg->version = version;
	fread((char*)&pkg->header, sizeof(HEADER), 1, pkg->file);
	if(memcmp(pkg->header.FORMAT, signature, sizeof(pkg->header.FORMAT)) != 0) {
		fclose(pkg->file);
		free(pkg);
		ERR("Bad signature\n", 0);
		return NULL;
	}
	pkg->keys = htable_new();
	fseek(pkg->file, -(pkg->header.DCOUNT * (int)sizeof(KEY)), SEEK_END);
	for(i=0; i<pkg->header.DCOUNT; i++) {
		fread((char*)&swap, sizeof(swap), 1, pkg->file);
		htable_set(pkg->keys, ToLower(&swap.ID[0]), strlen(&swap.ID[0]), swap);
	}
	return pkg;
}

pkgfile* pkg_open_h(FILE *handle, short version)
{
	int			i;
	static KEY	swap;
	pkgfile *pkg = (pkgfile*)malloc(sizeof(pkgfile));
	if(!handle) {
		free(pkg);
		pkg = NULL;
		ERR("Can`t open file\n", 0);
		return NULL;
	}
	pkg->startpos = ftell(handle);
	pkg->file = handle;
	pkg->version = version;
	fread((char*)&pkg->header, sizeof(HEADER), 1, pkg->file);
	if(memcmp(pkg->header.FORMAT, signature, sizeof(pkg->header.FORMAT)) != 0) {
		free(pkg);
		pkg = NULL;
		ERR("Bad signature\n", 0);
		return NULL;
	}
	pkg->keys = htable_new();
	fseek(pkg->file, (long)(pkg->startpos + pkg->header.PSIZE) - (pkg->header.DCOUNT * (int)sizeof(KEY)), SEEK_SET);
	for(i=0; i<pkg->header.DCOUNT; i++) {
		fread((char*)&swap, sizeof(swap), 1, pkg->file);
		htable_set(pkg->keys, ToLower(&swap.ID[0]), strlen(&swap.ID[0]), swap);
	}
	return pkg;
}

void* pkg_get(pkgfile *pkg, const char* name)
{
	char		*WORKDATA = NULL;
	char		*DATA = NULL;
	#if USE_MMAN
		char	*man;
	#endif
	static KEY	swap;
	if(!pkg) {
		ERR("Package not opened\n", 0);
		return NULL;
	}
	if(strlen(name) >= stringsize) {
		ERR("Name is too big\n", 0);
		return NULL;
	}
	if(!htable_get(pkg->keys, ToLower(name), strlen(name), &swap)) {
		ERR("Key not found\n", 0);
		return NULL;	/* if can`t find data return NULL */
	}

	#if USE_MMAN
		man = (char*)mmap(NULL, filesize(pkg->file), PROT_READ, MAP_SHARED, fileno(pkg->file), 0);
		if(!man) {
			ERR("File mapping error\n", 0);
			return NULL;
		}
		DATA = man + (long)pkg->startpos + swap.SP;
	#else
		DATA = (char*)malloc(swap.EP - swap.SP);
		fseek(pkg->file, pkg->startpos + swap.SP, SEEK_SET);
		fread(DATA, swap.EP - swap.SP, 1, pkg->file);
	#endif
	if(swap.HASHSUM != HASH(DATA, swap.EP - swap.SP, pkg->version)) {
		#if USE_MMAN
			munmap(man, filesize(pkg->file));
		#else
			free(DATA);
		#endif
		ERR("Broken data\n", 0);
		return NULL;
	}
	WORKDATA = (char*)malloc(swap.UNCOMPRESSED_SIZE);
	if(DECOMPRESS(DATA, WORKDATA, swap.UNCOMPRESSED_SIZE) <= 0) {
		#if USE_MMAN
			munmap(man, filesize(pkg->file));
		#else
			free(DATA);
		#endif
			free(WORKDATA);
		ERR("Can`t decompress data\n", 0);
		return NULL;
	}
	#if USE_MMAN
		munmap(man, filesize(pkg->file));
	#else
		free(DATA);
	#endif
	return WORKDATA;
}

int pkg_get_s(pkgfile *pkg, const char* name, void* DATA)
{
	char		*WORKDATA = NULL;
	#if USE_MMAN
		char	*man;
	#endif
	static KEY	swap;
	if(!pkg)
		return ERR("Package not opened\n", 0);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", 0);
	if(!htable_get(pkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key not found\n", 0);

	#if USE_MMAN
		man = (char*)mmap(NULL, filesize(pkg->file), PROT_READ, MAP_SHARED, fileno(pkg->file), 0);
		if(!man)
			return ERR("File mapping error\n", 0);
		WORKDATA = man + (long)pkg->startpos + swap.SP;
	#else
		WORKDATA = (char*)malloc(swap.EP - swap.SP);
		fseek(pkg->file, pkg->startpos + swap.SP, SEEK_SET);
		fread(WORKDATA, swap.EP - swap.SP, 1, pkg->file);
	#endif

	if(swap.HASHSUM != HASH(WORKDATA, swap.EP - swap.SP, pkg->version)) {
		#if USE_MMAN
			munmap(man, filesize(pkg->file));
		#else
			free(WORKDATA);
		#endif
		return ERR("Broken data\n", 0);
	}
	if(DECOMPRESS(WORKDATA, DATA, swap.UNCOMPRESSED_SIZE) <= 0) {
		#if USE_MMAN
			munmap(man, filesize(pkg->file));
		#else
			free(WORKDATA);
		#endif
		return ERR("Can`t decompress data\n", 0);
	}
	#if USE_MMAN
		munmap(man, filesize(pkg->file));
	#else
		free(WORKDATA);
	#endif
	return 1;
}

int pkg_add(pkgfile *pkg, const char* name, const void* DATA, int32_t size, int complevel)
{
	static KEY		swap;
	static KEY		*swaps;
	int				compressedsize;
	char			*WORKDATA = NULL;
	if(!pkg)
		return ERR("Package not opened\n", 0);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", 0);
	if(htable_get(pkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key are always used\n", 0);

	fseek(pkg->file, (long)(pkg->startpos + pkg->header.PSIZE) - (sizeof(KEY) * pkg->header.DCOUNT), SEEK_SET);
	swaps = (KEY*)malloc(sizeof(KEY) * pkg->header.DCOUNT);
	fread((char*)swaps, sizeof(KEY), pkg->header.DCOUNT, pkg->file);
	fseek(pkg->file, -((int)sizeof(KEY) * pkg->header.DCOUNT), SEEK_CUR);

	WORKDATA = (char*)malloc(size + RESERVED_MEMORY);
	compressedsize = COMPRESS((char*)DATA, WORKDATA, size, complevel);
	if(compressedsize <= 0) {
		free(swaps);
		free(WORKDATA);
		return ERR("Can`t compress data", 0);
	}
	swap.SP = ftell(pkg->file) - (long)pkg->startpos;
	swap.UNCOMPRESSED_SIZE = size;
	fwrite(WORKDATA, compressedsize, 1, pkg->file);
	swap.HASHSUM = HASH(WORKDATA, compressedsize, pkg->version);
	free(WORKDATA);
	swap.EP = swap.SP + compressedsize;
	strcpy(&swap.ID[0], name);
	htable_set(pkg->keys, ToLower(swap.ID), strlen(swap.ID), swap);
	fwrite((char*)swaps, sizeof(swap), pkg->header.DCOUNT, pkg->file);
	free(swaps);
	fwrite((char*)&swap, sizeof(swap), 1, pkg->file);
	pkg->header.DCOUNT++;
	pkg->header.PSIZE += compressedsize + sizeof(swap);
	fseek(pkg->file, (long)pkg->startpos, SEEK_SET);
	fwrite((char*)&pkg->header, sizeof(HEADER), 1, pkg->file);
	return 1;
}

int pkg_rename(pkgfile *pkg, const char* oldname, const char* newname)
{
	int			i;
	static KEY	swap;
	static KEY	*swaps;
	if(!pkg)
		return ERR("Package not opened\n", 0);
	if(pkg->header.DCOUNT == 0)
		return ERR("Nothing to rename\n", 0);
	if(strlen(oldname) >= stringsize)
		return ERR("Name is too big\n", 0);
	if(strlen(newname) >= stringsize)
		return ERR("Name is too big\n", 0);
	if(!htable_get(pkg->keys, ToLower(oldname), strlen(oldname), &swap))
		return ERR("Key not found\n", 0);           /* name are none exist, error */
	if(htable_get(pkg->keys, ToLower(newname), strlen(newname), &swap))
		return ERR("Name are existed\n", 0);        /* cannot be renamed */

	fseek(pkg->file, (long)(pkg->startpos + pkg->header.PSIZE) - (sizeof(KEY) * pkg->header.DCOUNT), SEEK_SET);
	swaps = (KEY*)malloc(sizeof(KEY) * pkg->header.DCOUNT);
	fread((char*)swaps, sizeof(KEY), pkg->header.DCOUNT, pkg->file);
	for(i=0; i<pkg->header.DCOUNT; i++) {
		if(strcmp(&swaps[i].ID[0], oldname) == 0)
			break;
	}
	strcpy(&swaps[i].ID[0], newname);
	swap = swaps[i];
	htable_del(pkg->keys, ToLower(oldname), strlen(oldname));
	htable_set(pkg->keys, ToLower(newname), strlen(newname), swap);
	fseek(pkg->file, -((int)sizeof(KEY) * pkg->header.DCOUNT), SEEK_CUR);
	fwrite(swaps, sizeof(KEY), pkg->header.DCOUNT, pkg->file);
	free(swaps);
	return 1;
}

int pkg_psize(pkgfile *pkg)
{
	if(!pkg)
		return ERR("Package not opened\n", -1);
	else
		return pkg->header.PSIZE;
}

int pkg_datacount(pkgfile *pkg)
{
	if(!pkg)
		return ERR("Package not opened\n", -1);
	else
		return pkg->header.DCOUNT;
}

char* pkg_list(pkgfile *pkg, int index)
{
	static KEY	swap;
	if(pkg == NULL) {
		ERR("Package not opened\n", -1);
		return "";
	}
	if(index < 0 || index >= pkg->header.DCOUNT)
		return "";
	fseek(pkg->file, (long)(pkg->startpos + pkg->header.PSIZE) + (sizeof(KEY) * (-pkg->header.DCOUNT + index)), SEEK_SET);
	fread((char*)&swap, sizeof(KEY), 1, pkg->file);
	return swap.ID;
}

int pkg_datasize(pkgfile *pkg, const char* name)
{
	static KEY	swap;
	if(!pkg)
		return ERR("Package not opened\n", -1);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", -1);
	if(!htable_get(pkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key not found\n", -1);
	return swap.UNCOMPRESSED_SIZE;
}

int pkg_compressedsize(pkgfile *pkg, const char* name)
{
	static KEY	swap;
	if(!pkg)
		return ERR("Package not opened\n", -1);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", -1);
	if(!htable_get(pkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key not found\n", -1);
	return swap.EP - swap.SP;
}

int pkg_remdata(pkgfile *pkg, const char* name)
{
	register int   i;
	static KEY	swap;
	static KEY	buff;
	static KEY	*swaps;
	char		*SWAPDATA;
	size_t		swapsize = 4 << 20;	/* 4 MB swap buffer */
	long		readed;
	long		first, second;		/* pointers to file positions */
	long		endptr;				/* filesize */
	if(!pkg || pkg->header.DCOUNT == 0)
		return ERR("Package not opened\n", 0);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", -1);
	if(!htable_get(pkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key not found\n", -1);

	endptr = filesize(pkg->file);
	fseek(pkg->file, (long)(pkg->startpos + pkg->header.PSIZE) + (sizeof(KEY) * (-pkg->header.DCOUNT)), SEEK_SET);
	swaps = (KEY*)malloc((pkg->header.DCOUNT - 1) * sizeof(KEY));

	/* read keys */
	for(i=0; i < pkg->header.DCOUNT - 1;) {
		fread(&buff, sizeof(KEY), 1, pkg->file);
		if(strncmp(&buff.ID[0], name, stringsize-1) != 0) {
			swaps[i] = buff;
			i++;
		}
	}

	SWAPDATA = (char*)malloc(swapsize);
	first = (long)(swap.SP + pkg->startpos);
	second = (long)(swap.EP + pkg->startpos);
	readed = second;

	/* rewrite package */
	while(1) {
		fseek(pkg->file, second, SEEK_SET);
		fread(SWAPDATA, swapsize, 1, pkg->file);
		readed += (long)swapsize;
		if((long)(pkg->startpos + pkg->header.PSIZE) <= readed) {
			fseek(pkg->file, first, SEEK_SET);
			fwrite(SWAPDATA, swapsize - (readed - (pkg->startpos + pkg->header.PSIZE)), 1, pkg->file);
			break;
		}
		fseek(pkg->file, first, SEEK_SET);
		fwrite(SWAPDATA, swapsize, 1, pkg->file);
		first += (long)swapsize;
		second += (long)swapsize;
	}

	pkg->header.PSIZE -= (swap.EP - swap.SP) + sizeof(KEY);
	pkg->header.DCOUNT--;

	/* update positions */
	for(i=0; i < pkg->header.DCOUNT; i++) {
		if(swaps[i].SP > swap.SP) {
			swaps[i].SP -= swap.EP - swap.SP;
			swaps[i].EP -= swap.EP - swap.SP;
		}
	}

	/* update hash keys */
	htable_free(pkg->keys);
	pkg->keys = NULL;
	pkg->keys = htable_new();
	for(i=0; i<pkg->header.DCOUNT; i++) {
		buff = swaps[i];
		htable_set(pkg->keys, ToLower(buff.ID), strlen(buff.ID), buff);
	}

	fseek(pkg->file, (long)(pkg->startpos + pkg->header.PSIZE) + (sizeof(KEY) * (-pkg->header.DCOUNT)), SEEK_SET);
	fwrite(swaps, sizeof(KEY), pkg->header.DCOUNT, pkg->file);
	free(swaps);
	swaps = NULL;

	/* rewrite other, if have big file */
	first = (long)(pkg->startpos + pkg->header.PSIZE);
	second = (long)(first + (swap.EP - swap.SP) + sizeof(KEY));
	if(second != endptr) {
		readed = second;
		while(1) {
			fseek(pkg->file, second, SEEK_SET);
			fread(SWAPDATA, swapsize, 1, pkg->file);
			readed += (long)swapsize;
			if((long)readed >= endptr) {
				fseek(pkg->file, first, SEEK_SET);
				fwrite(SWAPDATA, swapsize - (readed - endptr), 1, pkg->file);
				break;
			}
			fseek(pkg->file, first, SEEK_SET);
			fwrite(SWAPDATA, swapsize, 1, pkg->file);
			first += (long)swapsize;
			second += (long)swapsize;
		}
	};
	free(SWAPDATA);
	fseek(pkg->file, (long)pkg->startpos, SEEK_SET);
	fwrite(&pkg->header, sizeof(HEADER), 1, pkg->file);
	resize(fileno(pkg->file), endptr - ((swap.EP - swap.SP) + sizeof(KEY)));
	return 1;
}

pkgfile* pkg_close(pkgfile *pkg)
{
	if(!pkg) {
		ERR("Package not opened\n", 0);
		return NULL;
	}
	fclose(pkg->file);
	htable_free(pkg->keys);
	pkg->file = NULL;
	pkg->keys = NULL;
	free(pkg);
	pkg = NULL;
	return NULL;
}


/*=========================IN MEMORY READ===========================*/

mempkgfile* mpkg_open(char* mem, short version)
{
	int			i;
	int			ptr;
	static KEY	swap;
	mempkgfile *mempkg = (mempkgfile*)malloc(sizeof(mempkgfile));
	mempkg->file = mem;
	if(!mempkg->file) {
		ERR("Can`t open file\n", 0);
		return NULL;
	}
	mempkg->version = version;
	memcpy(&mempkg->header, mempkg->file, sizeof(HEADER));
	if(memcmp(mempkg->header.FORMAT, signature, sizeof(mempkg->header.FORMAT)) != 0) {
		free(mempkg);
		ERR("Bad signature\n", 0);
		return NULL;
	}
	mempkg->keys = htable_new();
	ptr = mempkg->header.PSIZE - mempkg->header.DCOUNT * sizeof(KEY);
	for(i=0; i<mempkg->header.DCOUNT; i++) {
		memcpy(&swap, &mempkg->file[ptr], sizeof(KEY));
		htable_set(mempkg->keys, ToLower(&swap.ID[0]), strlen(&swap.ID[0]), swap);
		ptr += sizeof(KEY);
	}
	return mempkg;
}

void* mpkg_get(mempkgfile *mpkg, const char* name)
{
	char		*WORKDATA = NULL;
	char		*DATA = NULL;
	static KEY	swap;
	if(!mpkg) {
		ERR("Package not opened\n", 0);
		return NULL;
	}
	if(strlen(name) >= stringsize) {
		ERR("Name is too big\n", 0);
		return NULL;
	}
	if(!htable_get(mpkg->keys, ToLower(name), strlen(name), &swap)) {
		ERR("Key not found\n", 0);
		return NULL;            /* if can`t find data return NULL */
	}
	DATA = mpkg->file + swap.SP;
	if(swap.HASHSUM != HASH(DATA, swap.EP - swap.SP, mpkg->version)) {
		ERR("Broken data\n", 0);
		return NULL;
	}
	WORKDATA = (char*)malloc(swap.UNCOMPRESSED_SIZE);
	if(DECOMPRESS(DATA, WORKDATA, swap.UNCOMPRESSED_SIZE) <= 0) {
		free(WORKDATA);
		ERR("Can`t decompress data\n", 0);
		return NULL;
	}
	return WORKDATA;
}

int	mpkg_get_s(mempkgfile *mpkg, const char* name, void* DATA)
{
	char		*WORKDATA = NULL;
	static KEY	swap;
	if(!mpkg)
		return ERR("Package not opened\n", 0);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", 0);
	if(!htable_get(mpkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key not found\n", 0);
	WORKDATA = mpkg->file + swap.SP;
	if(swap.HASHSUM != HASH(WORKDATA, swap.EP - swap.SP, mpkg->version)) {
		return ERR("Broken data\n", 0);
	}
	if(DECOMPRESS(WORKDATA, DATA, swap.UNCOMPRESSED_SIZE) <= 0) {
		return ERR("Can`t decompress data\n", 0);
	}
	return 1;
}

int	mpkg_psize(mempkgfile *mpkg)
{
	if(!mpkg)
		return ERR("Package not opened\n", -1);
	else
		return mpkg->header.PSIZE;
}

int	mpkg_datacount(mempkgfile *mpkg)
{
	if(!mpkg)
		return ERR("Package not opened\n", -1);
	else
		return mpkg->header.DCOUNT;
}

char* mpkg_list(mempkgfile *mpkg, int index)
{
	static KEY	swap;
	if(mpkg == NULL) {
		ERR("Package not opened\n", -1);
		return "";
	}
	if(index < 0 || index >= mpkg->header.DCOUNT)
		return "";
	memcpy(&swap, &mpkg->file[mpkg->header.PSIZE - (mpkg->header.DCOUNT - index) * sizeof(KEY)], sizeof(KEY));
	return swap.ID;
}

int mpkg_datasize(mempkgfile *mpkg, const char* name)
{
	static KEY	swap;
	if(!mpkg)
		return ERR("Package not opened\n", -1);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", -1);
	if(!htable_get(mpkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key not found\n", -1);
	return swap.UNCOMPRESSED_SIZE;
}

int mpkg_compressedsize(mempkgfile *mpkg, const char* name)
{
	static KEY	swap;
	if(!mpkg)
		return ERR("Package not opened\n", -1);
	if(strlen(name) >= stringsize)
		return ERR("Name is too big\n", -1);
	if(!htable_get(mpkg->keys, ToLower(name), strlen(name), &swap))
		return ERR("Key not found\n", -1);
	return swap.EP - swap.SP;
}

mempkgfile* mpkg_close(mempkgfile *mpkg)
{
	if(!mpkg) {
		ERR("Package not opened\n", 0);
		return NULL;
	}
	mpkg->file = NULL;
	htable_free(mpkg->keys);
	mpkg->keys = NULL;
	free(mpkg);
	mpkg = NULL;
	return NULL;
}
