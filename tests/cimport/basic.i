# 1 "<stdin>"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "<stdin>"
# 1 "/usr/include/x.h" 1 3 4
typedef long unsigned int size_t;
typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef short int __int16_t;
typedef int __int32_t;
typedef unsigned int __uint32_t;
typedef long int __int64_t;
typedef unsigned long int __uint64_t;
typedef long int __ssize_t;
typedef __ssize_t ssize_t;
typedef __int32_t int32_t;
typedef __uint8_t uint8_t;
typedef __uint64_t uint64_t;
typedef long int ptrdiff_t;
typedef int wchar_t;
typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;
typedef struct _IO_FILE FILE;
struct _IO_FILE;
typedef int (*cmp_t)(const void *, const void *);
typedef void (*sig_t)(int);
typedef void fn_type(int);
typedef unsigned char flag_t[8];

extern int puts (const char *__s);
extern size_t strlen (const char *__s) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
extern void *memcpy (void *__restrict __dest, const void *__restrict __src, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern ssize_t write (int __fd, const void *__buf, size_t __n);
extern int printf (const char *__restrict __format, ...);
extern int vprintf (const char *__restrict __format, __gnuc_va_list __arg);
extern double ldexp (double __x, int __exp) __attribute__ ((__nothrow__ , __leaf__));
extern float sqrtf (float);
extern long double powl (long double, long double);
extern int32_t abs32 (int32_t __x);
extern uint8_t getbyte (void);
extern void qsort (void *__base, size_t __nmemb, size_t __size, cmp_t __compar);
extern void (*signal (int __sig, sig_t __handler)) (int);
extern int atexit (void (*__func) (void));
extern int sigaction_like (int, const void *restrict, void *restrict) __asm__ ("" "sigaction_real");
extern FILE *fopen (const char *__restrict __filename, const char *__restrict __modes);
extern int fclose (FILE *__stream);
extern int fputs_a (const char *s, FILE *f, int tab[16], const char *names[]);
extern int unspecified ();
extern _Bool is_ok (_Bool v);
extern unsigned short ushort_fn (unsigned short a, short b, signed char c, unsigned char d, long long e, unsigned long long f);
extern long long ll_fn (long long a, unsigned long long b);
extern unsigned lonely (unsigned, long, unsigned long);
extern int a_decl_twice (int);
extern int a_decl_twice (int);

enum color { RED, GREEN = 5, BLUE, MAUVE = -3, TEAL, SHIFT = 1 << 4, MASK = (0x0f | 0x100), ODD = sizeof(int) };
typedef enum { ALPHA = 10, BETA } greek_t;
extern int paint (enum color __c, greek_t __g);

struct pt { int x; int y; };
typedef struct { double re; double im; } cpx;
struct line { struct pt a; struct pt b; char name[20]; unsigned char flags[2][3]; void *user; int (*cb)(int); };
struct wrap { cpx c; struct line l; };
struct bits { unsigned int a : 3; unsigned int b : 5; };
struct uni { int kind; union { int i; double d; } u; };
struct un2 { union { int i; float f; } v; };
union plain { int i; float f; };
struct packed_s { char c; int i; } __attribute__ ((__packed__));
struct flexi { int n; char data[]; };
struct withptr { struct withptr *next; int v; };
struct with_ld { long double x; };
struct IS { int IN; long LONG; };
struct incomplete;
struct holder { struct incomplete *p; };
struct usesinc { struct incomplete v; };
struct later { size_t n; ssize_t m; uint64_t id; int32_t k; wchar_t w; ptrdiff_t d; _Bool b; float f; };
struct expr_len { char a[2 * 8]; char b[(3 + 1)]; };
struct sz_len { char a[sizeof(long)]; };

extern struct pt make_pt (int, int);
extern int take_pt (struct pt __p);
extern int take_ptr (struct pt *__p);
extern int dist (const struct pt *a, const struct pt *b);
extern int PRINT (int);
extern void INT (void);
extern int IS_THIS (int IS);

static inline int twice (int x) { return x * 2; }
static inline int thrice (int x) {
  int y = x * 3;
  if (y) { return y; }
  return 0;
}
extern int after_body (const char *s);
int mystery_t_fn (unknown_t, int);
unknown_t mystery_ret (int);
extern int stat (const char *, struct pt *);
_Static_assert (sizeof (int) == 4, "int is 4 bytes");
extern char *const strs[3];
extern const char *names_str = "has (parens) and ; semicolon";
extern int with_str (char c) __attribute__ ((__format__ (__printf__, 1, 0))) ;
# 99 "<stdin>" 2
struct fwd_user { struct fwd_late *p; struct fwd_early e; };
struct fwd_early { int a; };
struct fwd_late { struct fwd_early first; long second; };
struct under { char _[4]; int ok; };
enum flags { F_A = (1 << 0), F_B = ((1 << 1) | F_A), F_C = (F_B << 2) + 1, F_D = (1 ? 2 : 3), F_E = -F_A, F_F = 100 / 7 % 4 };
extern int _;
