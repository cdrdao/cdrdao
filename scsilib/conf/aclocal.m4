dnl Checks if structure 'stat' have field 'st_spare1'.
dnl Defines HAVE_ST_SPARE1 on success.
AC_DEFUN(AC_STRUCT_ST_SPARE1,
[AC_CACHE_CHECK([if struct stat contains st_spare1], ac_cv_struct_st_spare1,
                [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/stat.h>],
                                [struct  stat s; s.st_spare1 = 0;],
                                [ac_cv_struct_st_spare1=yes],
                                [ac_cv_struct_st_spare1=no])])
if test $ac_cv_struct_st_spare1 = yes; then
  AC_DEFINE(HAVE_ST_SPARE1)
fi])

dnl Checks if structure 'stat' have field 'st_atim.tv_nsec'.
dnl Defines HAVE_ST_NSEC on success.
AC_DEFUN(AC_STRUCT_ST_NSEC,
[AC_CACHE_CHECK([if struct stat contains st_atim.tv_nsec], ac_cv_struct_st_nsec,
                [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/stat.h>],
                                [struct  stat s; s.st_atim.tv_nsec = 0;],
                                [ac_cv_struct_st_nsec=yes],
                                [ac_cv_struct_st_nsec=no])])
if test $ac_cv_struct_st_nsec = yes; then
  AC_DEFINE(HAVE_ST_NSEC)
fi])

dnl Checks if structure 'mtget' have field 'mt_dsreg'.
dnl Defines HAVE_MTGET_DSREG on success.
AC_DEFUN(AC_STRUCT_MTGET_DSREG,
[AC_CACHE_CHECK([if struct mtget contains mt_dsreg], ac_cv_struct_mtget_dsreg,
                [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/mtio.h>],
                                [struct  mtget t; t.mt_dsreg = 0;],
                                [ac_cv_struct_mtget_dsreg=yes],
                                [ac_cv_struct_mtget_dsreg=no])])
if test $ac_cv_struct_mtget_dsreg = yes; then
  AC_DEFINE(HAVE_MTGET_DSREG)
fi])

dnl Checks if structure 'mtget' have field 'mt_resid'.
dnl Defines HAVE_MTGET_RESID on success.
AC_DEFUN(AC_STRUCT_MTGET_RESID,
[AC_CACHE_CHECK([if struct mtget contains mt_resid], ac_cv_struct_mtget_resid,
                [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/mtio.h>],
                                [struct  mtget t; t.mt_resid = 0;],
                                [ac_cv_struct_mtget_resid=yes],
                                [ac_cv_struct_mtget_resid=no])])
if test $ac_cv_struct_mtget_resid = yes; then
  AC_DEFINE(HAVE_MTGET_RESID)
fi])

dnl Checks if structure 'mtget' have field 'mt_fileno'.
dnl Defines HAVE_MTGET_FILENO on success.
AC_DEFUN(AC_STRUCT_MTGET_FILENO,
[AC_CACHE_CHECK([if struct mtget contains mt_fileno],
                ac_cv_struct_mtget_fileno,
                [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/mtio.h>],
		                [struct  mtget t; t.mt_fileno = 0;],
	        	        [ac_cv_struct_mtget_fileno=yes],
                		[ac_cv_struct_mtget_fileno=no])])
if test $ac_cv_struct_mtget_fileno = yes; then
  AC_DEFINE(HAVE_MTGET_FILENO)
fi])

dnl Checks if structure 'mtget' have field 'mt_blkno'.
dnl Defines HAVE_MTGET_BLKNO on success.
AC_DEFUN(AC_STRUCT_MTGET_BLKNO,
[AC_CACHE_CHECK([if struct mtget contains mt_blkno], ac_cv_struct_mtget_blkno,
                [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/mtio.h>],
                                [struct  mtget t; t.mt_blkno = 0;],
                                [ac_cv_struct_mtget_blkno=yes],
                                [ac_cv_struct_mtget_blkno=no])])
if test $ac_cv_struct_mtget_blkno = yes; then
  AC_DEFINE(HAVE_MTGET_BLKNO)
fi])

dnl Checks for illegal declaration of 'union semun' in sys/sem.h.
dnl Defines HAVE_UNION_SEMUN on success.
AC_DEFUN(AC_STRUCT_UNION_SEMUN,
[AC_CACHE_CHECK([if an illegal declaration for union semun in sys/sem.h exists], ac_cv_struct_union_semun,
                [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>], [union semun s;],
                [ac_cv_struct_union_semun=yes],
                [ac_cv_struct_union_semun=no])])
if test $ac_cv_struct_union_semun = yes; then
  AC_DEFINE(HAVE_UNION_SEMUN)
fi])

dnl Checks if 'union wait' is declared in 'wait.h' or 'sys/wait.h'.
dnl Defines HAVE_UNION_WAIT on success.
AC_DEFUN(AC_STRUCT_UNION_WAIT,
[AC_CACHE_CHECK([if union wait is declared in wait.h or sys/wait.h], ac_cv_struct_union_wait,
                [AC_TRY_COMPILE([#include <sys/types.h>
#if	defined(HAVE_WAIT_H)
#	include <wait.h>
#else
#include <sys/wait.h>
#endif], [union wait w;],
                [ac_cv_struct_union_wait=yes],
                [ac_cv_struct_union_wait=no])])
if test $ac_cv_struct_union_wait = yes; then
  AC_DEFINE(HAVE_UNION_WAIT)
fi])

dnl Checks if 'struct rusage' is declared in sys/resource.h.
dnl Defines HAVE_STRUCT_RUSAGE on success.
AC_DEFUN(AC_STRUCT_RUSAGE,
[AC_CACHE_CHECK([if struct rusage is declared in sys/resource.h], ac_cv_struct_rusage,
                [AC_TRY_COMPILE([#include <sys/time.h>
#include <sys/resource.h>], [struct rusage r;],
                [ac_cv_struct_rusage=yes],
                [ac_cv_struct_rusage=no])])
if test $ac_cv_struct_rusage = yes; then
  AC_DEFINE(HAVE_STRUCT_RUSAGE)
fi])

dnl Checks wether major(), minor() and makedev() are defined in
dnl 'sys/mkdev.h' or in 'sys/sysmacros.h. Defines MAJOR_IN_MKDEV or
dnl MAJOR_IN_SYSMACROS or nothing.
AC_DEFUN(AC_HEADER_MAKEDEV,
[AC_CACHE_CHECK([for header file containing  major(), minor() and makedev()],
               ac_cv_header_makedev,
[ac_cv_header_makedev=none
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/mkdev.h>],
               [int i = major(0); i = minor(0); i = makedev(0,0);],
                [ac_cv_header_makedev=sys/mkdev.h])
if test $ac_cv_header_makedev = none; then
  AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/sysmacros.h>],
                 [int i = major(0); i = minor(0); i = makedev(0,0);],
                 [ac_cv_header_makedev=sys/sysmacros.h])
fi])
if test $ac_cv_header_makedev = sys/mkdev.h; then
  AC_DEFINE(MAJOR_IN_MKDEV)
fi
if test $ac_cv_header_makedev = sys/sysmacros.h; then
  AC_DEFINE(MAJOR_IN_SYSMACROS)
fi])

dnl Checks for USG derived STDIO
dnl Defines HAVE_USG_STDIO on success.
AC_DEFUN(AC_HEADER_USG_STDIO,
[AC_CACHE_CHECK([for USG derived STDIO], ac_cv_header_usg_stdio,
                [AC_TRY_COMPILE([#include <stdio.h>],
[FILE    *f;
int     flag;
int     count;
char    *ptr;
f = fopen("confdefs.h", "r");
flag  = f->_flag & _IONBF;
flag |= f->_flag & _IOERR;
flag |= f->_flag & _IOEOF;
count = f->_cnt;
ptr = (char *)f->_ptr;
fclose(f);],
                [ac_cv_header_usg_stdio=yes],
                [ac_cv_header_usg_stdio=no])])
if test $ac_cv_header_usg_stdio = yes; then
  AC_DEFINE(HAVE_USG_STDIO)
fi])

dnl Checks for errno definition in <errno.h>
dnl Defines HAVE_ERRNO_DEF on success.
AC_DEFUN(AC_HEADER_ERRNO_DEF,
[AC_CACHE_CHECK([for errno definition in errno.h], ac_cv_header_errno_def,
                [AC_TRY_COMPILE([#include <errno.h>],
[errno = 0;],
                [ac_cv_header_errno_def=yes],
                [ac_cv_header_errno_def=no])])
if test $ac_cv_header_errno_def = yes; then
  AC_DEFINE(HAVE_ERRNO_DEF)
fi])

dnl Checks for UNIX-98 compliant <inttypes.h>
dnl Defines HAVE_INTTYPES_H on success.
AC_DEFUN(AC_HEADER_INTTYPES,
[AC_CACHE_CHECK([for UNIX-98 compliant inttypes.h], ac_cv_header_inttypes,
                [AC_TRY_COMPILE([#include <inttypes.h>],
[int8_t c; uint8_t uc; int16_t s; uint16_t us; int32_t i; uint32_t ui;
int64_t ll; uint64_t ull;
intptr_t ip; uintptr_t uip;],
                [ac_cv_header_inttypes=yes],
                [ac_cv_header_inttypes=no])])
if test $ac_cv_header_inttypes = yes; then
  AC_DEFINE(HAVE_INTTYPES_H)
fi])

dnl Checks for type long long
dnl Defines HAVE_LONGLONG on success.
AC_DEFUN(AC_TYPE_LONGLONG,
[AC_CACHE_CHECK([for type long long], ac_cv_type_longlong,
                [AC_TRY_COMPILE([], [long long i;],
                [ac_cv_type_longlong=yes],
                [ac_cv_type_longlong=no])])
if test $ac_cv_type_longlong = yes; then
  AC_DEFINE(HAVE_LONGLONG)
fi])

dnl Checks if C-compiler orders bitfields htol
dnl Defines BITFIELDS_HTOL on success.
AC_DEFUN(AC_C_BITFIELDS,
[AC_CACHE_CHECK([whether bitorder in bitfields is htol], ac_cv_c_bitfields_htol,
                [AC_TRY_RUN([
struct {
	unsigned char	x1:4;
	unsigned char	x2:4;
} a;
main()
{
char	*cp;

cp = (char *)&a;
*cp = 0x12;
exit(a.x1 == 2);}],
                [ac_cv_c_bitfields_htol=yes],
                [ac_cv_c_bitfields_htol=no])])
if test $ac_cv_c_bitfields_htol = yes; then
  AC_DEFINE(BITFIELDS_HTOL)
fi])

dnl Checks if C-compiler understands prototypes
dnl Defines PROTOTYPES on success.
AC_DEFUN(AC_TYPE_PROTOTYPES,
[AC_CACHE_CHECK([for prototypes], ac_cv_type_prototypes,
                [AC_TRY_RUN([
doit(int i, ...)
{return 0;}
main(int ac, char *av[])
{ doit(1, 2, 3);
exit(0);}],
                [ac_cv_type_prototypes=yes],
                [ac_cv_type_prototypes=no])])
if test $ac_cv_type_prototypes = yes; then
  AC_DEFINE(PROTOTYPES)
fi])

dnl Checks for type size_t
dnl Defines HAVE_SIZE_T_ on success.
AC_DEFUN(AC_TYPE_SIZE_T_,
[AC_CACHE_CHECK([for type size_t], ac_cv_type_size_t_,
                [AC_TRY_COMPILE([#include <sys/types.h>], [size_t s;],
                [ac_cv_type_size_t_=yes],
                [ac_cv_type_size_t_=no])])
if test $ac_cv_type_size_t_ = yes; then
  AC_DEFINE(HAVE_SIZE_T)
else
  AC_DEFINE(NO_SIZE_T)
fi])

dnl Checks if type char is unsigned
dnl Defines CHAR_IS_UNSIGNED on success.
AC_DEFUN(AC_TYPE_CHAR,
[AC_CACHE_CHECK([if char is unsigned], ac_cv_type_char_unsigned,
                [AC_TRY_RUN([
main()
{
	char c;

	c = -1;
	exit(c < 0);}],
		[ac_cv_type_char_unsigned=yes],
		[ac_cv_type_char_unsigned=no],
		[ac_cv_type_char_unsigned=no])]) 
if test $ac_cv_type_char_unsigned = yes; then
  AC_DEFINE(CHAR_IS_UNSIGNED)
fi])

dnl Checks if function/macro va_copy() is available
dnl Defines HAVE_VA_COPY on success.
AC_DEFUN(AC_FUNC_VA_COPY,
[AC_CACHE_CHECK([for va_copy], ac_cv_func_va_copy,
                [AC_TRY_LINK([
#ifdef	HAVE_STDARG_H
#	include <stdarg.h>
#else
#	include <vararg.h>
#endif], 
		[
va_list a, b;

va_copy(a, b);],
                [ac_cv_func_va_copy=yes],
                [ac_cv_func_va_copy=no])])
if test $ac_cv_func_va_copy = yes; then
  AC_DEFINE(HAVE_VA_COPY)
fi])

dnl Checks if function/macro __va_copy() is available
dnl Defines HAVE__VA_COPY on success.
AC_DEFUN(AC_FUNC__VA_COPY,
[AC_CACHE_CHECK([for __va_copy], ac_cv_func__va_copy,
                [AC_TRY_LINK([
#ifdef	HAVE_STDARG_H
#	include <stdarg.h>
#else
#	include <vararg.h>
#endif], 
		[
va_list a, b;

__va_copy(a, b);],

                [ac_cv_func__va_copy=yes],
                [ac_cv_func__va_copy=no])])
if test $ac_cv_func__va_copy = yes; then
  AC_DEFINE(HAVE__VA_COPY)
fi])

dnl Checks if va_list is an array
dnl Defines VA_LIST_IS_ARRAY on success.
AC_DEFUN(AC_TYPE_VA_LIST,
[AC_CACHE_CHECK([if va_list is an array], ac_cv_type_va_list_array,
                [AC_TRY_RUN([
#ifdef	HAVE_STDARG_H
#	include <stdarg.h>
#else
#	include <vararg.h>
#endif
func(x)
	va_list x;
{
	return (sizeof(x));
}

main()
{
	va_list val;

	if (sizeof(val) == sizeof(void *))
		exit(1);

	if (func(val) == sizeof(void *))
		exit(0);
 exit(1);}],
		[ac_cv_type_va_list_array=yes],
		[ac_cv_type_va_list_array=no],
		[ac_cv_type_va_list_array=no])]) 
if test $ac_cv_type_va_list_array = yes; then
  AC_DEFINE(VA_LIST_IS_ARRAY)
fi])

dnl Checks if quotactl is present as ioctl
dnl Defines HAVE_QUOTAIOCTL on success.
AC_DEFUN(AC_FUNC_QUOTAIOCTL,
[AC_CACHE_CHECK([if quotactl is an ioctl], ac_cv_func_quotaioctl,
                [AC_TRY_LINK([#include <sys/types.h>
#include <sys/fs/ufs_quota.h>],
		[struct quotctl q; ioctl(0, Q_QUOTACTL, &q)],
		[ac_cv_func_quotaioctl=yes],
		[ac_cv_func_quotaioctl=no])]) 
if test $ac_cv_func_quotaioctl = yes; then
  AC_DEFINE(HAVE_QUOTAIOCTL)
fi])

dnl Checks if function __dtoa() is available
dnl Defines HAVE_DTOA on success.
AC_DEFUN(AC_FUNC_DTOA,
[AC_CACHE_CHECK([for __dtoa], ac_cv_func_dtoa,
                [AC_TRY_LINK([extern  char *__dtoa();], 
[int decpt; int sign; char *ep; char *bp;
bp = __dtoa(0.0, 2, 6, &decpt, &sign, &ep);],
                [ac_cv_func_dtoa=yes],
                [ac_cv_func_dtoa=no])])
if test $ac_cv_func_dtoa = yes; then
  AC_DEFINE(HAVE_DTOA)
fi])

dnl Checks if working ecvt() exists
dnl Defines HAVE_ECVT on success.
AC_DEFUN(AC_FUNC_ECVT,
[AC_CACHE_CHECK([for working ecvt() ], ac_cv_func_ecvt,
                [AC_TRY_RUN([
sprintf(s)
	char	*s;
{
	strcpy(s, "DEAD");
}

main()
{
	int a, b;

/*	exit (strcmp("DEAD", ecvt(1.9, 2, &a, &b)) == 0);*/
	exit (strcmp("19", ecvt(1.9, 2, &a, &b)) != 0);
}],
                [ac_cv_func_ecvt=yes],
                [ac_cv_func_ecvt=no])])
if test $ac_cv_func_ecvt = yes; then
  AC_DEFINE(HAVE_ECVT)
fi])

dnl Checks if working fcvt() exists
dnl Defines HAVE_FCVT on success.
AC_DEFUN(AC_FUNC_FCVT,
[AC_CACHE_CHECK([for working fcvt() ], ac_cv_func_fcvt,
                [AC_TRY_RUN([
sprintf(s)
	char	*s;
{
	strcpy(s, "DEAD");
}

main()
{
	int a, b;

/*	exit (strcmp("DEAD", fcvt(1.9, 2, &a, &b)) == 0);*/
	exit (strcmp("190", fcvt(1.9, 2, &a, &b)) != 0);
}],
                [ac_cv_func_fcvt=yes],
                [ac_cv_func_fcvt=no])])
if test $ac_cv_func_fcvt = yes; then
  AC_DEFINE(HAVE_FCVT)
fi])

dnl Checks if working gcvt() exists
dnl Defines HAVE_GCVT on success.
AC_DEFUN(AC_FUNC_GCVT,
[AC_CACHE_CHECK([for working gcvt() ], ac_cv_func_gcvt,
                [AC_TRY_RUN([
sprintf(s)
	char	*s;
{
	strcpy(s, "DEAD");
}

main()
{
	char	buf[32];

/*	exit (strcmp("DEAD", gcvt(1.9, 10, buf)) == 0);*/
	exit (strcmp("1.9", gcvt(1.9, 10, buf)) != 0);
}],
                [ac_cv_func_gcvt=yes],
                [ac_cv_func_gcvt=no])])
if test $ac_cv_func_gcvt = yes; then
  AC_DEFINE(HAVE_GCVT)
fi])

dnl Checks if function uname() is available
dnl Defines HAVE_UNAME on success.
AC_DEFUN(AC_FUNC_UNAME,
[AC_CACHE_CHECK([for uname], ac_cv_func_uname,
                [AC_TRY_LINK([#include <sys/utsname.h>], 
[struct	utsname un;
uname(&un);],
                [ac_cv_func_uname=yes],
                [ac_cv_func_uname=no])])
if test $ac_cv_func_uname = yes; then
  AC_DEFINE(HAVE_UNAME)
fi])

dnl Checks if function mlockall() is available
dnl beware HP-UX 10.x it contains a bad mlockall() in libc
dnl Defines HAVE_MLOCKALL on success.
AC_DEFUN(AC_FUNC_MLOCKALL,
[AC_CACHE_CHECK([for mlockall], ac_cv_func_mlockall,
                [AC_TRY_RUN([
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>

main()
{
	if (mlockall(MCL_CURRENT|MCL_FUTURE) < 0) {
		if (errno == EPERM || errno ==  EACCES)
			exit(0);
		exit(-1);
	}
	exit(0);
}
],
                [ac_cv_func_mlockall=yes],
                [ac_cv_func_mlockall=no])])
if test $ac_cv_func_mlockall = yes; then
  AC_DEFINE(HAVE_MLOCKALL)
fi])

dnl Checks if mmap() works to get shared memory
dnl Defines HAVE_SMMAP on success.
AC_DEFUN(AC_FUNC_SMMAP,
[AC_CACHE_CHECK([if mmap works to get shared memory], ac_cv_func_smmap,
                [AC_TRY_RUN([
#include <sys/types.h>
#include <sys/mman.h>

char *
mkshare()
{
        int     size = 8192;
        int     f;
        char    *addr;

#ifdef  MAP_ANONYMOUS   /* HP/UX */
        f = -1;
        addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, f, 0);
#else
        if ((f = open("/dev/zero", 2)) < 0)
                exit(1);
        addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
#endif
        if (addr == (char *)-1)
                exit(1);
        close(f);

        return (addr);
}

main()
{
        char    *addr;
        
        addr = mkshare(8192);
        *addr = 'I';

        switch (fork()) {

        case -1:
                printf("help\n"); exit(1);

        case 0: /* child */
                *addr = 'N';
                _exit(0);
                break;
        default: /* parent */
                wait(0);
                sleep(1);
                break;
        }

        if (*addr != 'N')
                exit(1);
        exit(0);
}
], 
                [ac_cv_func_smmap=yes],
                [ac_cv_func_smmap=no],
                [ac_cv_func_smmap=no])])
if test $ac_cv_func_smmap = yes; then
  AC_DEFINE(HAVE_SMMAP)
fi])

dnl Checks if sys_siglist[] exists
dnl Defines HAVE_SYS_SIGLIST on success.
AC_DEFUN(AC_FUNC_SYS_SIGLIST,
[AC_CACHE_CHECK([for sys_siglist], ac_cv_func_sys_siglist,
                [AC_TRY_RUN([
main()
{ extern char *sys_siglist[];
if (sys_siglist[1] == 0)
	exit(1);
exit(0);}],
                [ac_cv_func_sys_siglist=yes],
                [ac_cv_func_sys_siglist=no])])
if test $ac_cv_func_sys_siglist = yes; then
  AC_DEFINE(HAVE_SYS_SIGLIST)
fi])

dnl Checks for maximum number of bits in minor device number
AC_DEFUN(AC_CHECK_MINOR_BITS,
[AC_REQUIRE([AC_HEADER_MAKEDEV])dnl
changequote(<<, >>)dnl
define(<<AC_MACRO_NAME>>, DEV_MINOR_BITS)dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, ac_cv_dev_minor_bits)dnl
changequote([, ])dnl
AC_MSG_CHECKING(bits in minor device number)
AC_CACHE_VAL(AC_CV_NAME,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
#ifdef major
#	define _FOUND_MAJOR_
#endif

#ifdef MAJOR_IN_MKDEV
#	include <sys/mkdev.h>
#	define _FOUND_MAJOR_
#endif

#ifndef _FOUND_MAJOR_
#	ifdef MAJOR_IN_SYSMACROS
#		include <sys/sysmacros.h>
#		define _FOUND_MAJOR_
#	endif
#endif

#ifndef _FOUND_MAJOR_
#	if defined(hpux) || defined(__hpux__) || defined(__hpux)
#		include <sys/mknod.h>
#		define _FOUND_MAJOR_
#	endif
#endif

#ifndef _FOUND_MAJOR_
#	define major(dev)		(((dev) >> 8) & 0xFF)
#	define minor(dev)		((dev) & 0xFF)
#	define makedev(majo, mino)	(((majo) << 8) | (mino))
#endif
main()
{
	long	l = 1;
	int	i;
	int	m;
	int	c = 0;
	FILE	*f=fopen("conftestval", "w");

	if (!f) exit(1);

	for (i=1, m=0; i <= 32; i++, l<<=1) {
		if (minor(l) == 0 && c == 0)
			c = m;
		if (minor(l) != 0)
			m = i;
	}
	fprintf(f, "%d\n", m);
	exit(0);
}], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0, ifelse([$2], , , AC_CV_NAME=$2))])dnl
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_MACRO_NAME, $AC_CV_NAME)
undefine([AC_MACRO_NAME])dnl
undefine([AC_CV_NAME])dnl
])

dnl Checks for maximum number of bits in minor device numbers are non contiguous
dnl Defines DEV_MINOR_NONCONTIG on success.
AC_DEFUN(AC_CHECK_MINOR_NONCONTIG,
[AC_REQUIRE([AC_HEADER_MAKEDEV])dnl
AC_CACHE_CHECK([whether bits in minor device numbers are non contiguous], ac_cv_dev_minor_noncontig,
                [AC_TRY_RUN([
#include <sys/types.h>
#ifdef major
#	define _FOUND_MAJOR_
#endif

#ifdef MAJOR_IN_MKDEV
#	include <sys/mkdev.h>
#	define _FOUND_MAJOR_
#endif

#ifndef _FOUND_MAJOR_
#	ifdef MAJOR_IN_SYSMACROS
#		include <sys/sysmacros.h>
#		define _FOUND_MAJOR_
#	endif
#endif

#ifndef _FOUND_MAJOR_
#	if defined(hpux) || defined(__hpux__) || defined(__hpux)
#		include <sys/mknod.h>
#		define _FOUND_MAJOR_
#	endif
#endif

#ifndef _FOUND_MAJOR_
#	define major(dev)		(((dev) >> 8) & 0xFF)
#	define minor(dev)		((dev) & 0xFF)
#	define makedev(majo, mino)	(((majo) << 8) | (mino))
#endif
main()
{
	long	l = 1;
	int	i;
	int	m;
	int	c = 0;

	for (i=1, m=0; i <= 32; i++, l<<=1) {
		if (minor(l) == 0 && c == 0)
			c = m;
		if (minor(l) != 0)
			m = i;
	}
exit (m == c);}],
                [ac_cv_dev_minor_noncontig=yes],
                [ac_cv_dev_minor_noncontig=no])])
if test $ac_cv_dev_minor_noncontig = yes; then
  AC_DEFINE(DEV_MINOR_NONCONTIG)
fi])

