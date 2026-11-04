# 1 "pad.h"
typedef long time_t;
typedef unsigned long size_t;
enum { E_W = 3 };
struct timespec_musl { time_t tv_sec; int :8*(sizeof(time_t)-sizeof(long))*(1234==4321); long tv_nsec; int :8*(sizeof(time_t)-sizeof(long))*(1234!=4321); };
struct pad_be32 { int a; int :8*(sizeof(long)-sizeof(int))*(1234!=4321); long b; long :8*(sizeof(char*)-sizeof(short)); char c; };
struct pad_zero { char a; int :0; char b; unsigned :E_W*8; };
struct pad_odd { int a; int :5; int b; };
struct pad_unknown { int a; int :sizeof(struct pad_zero)*8; int b; };
struct pad_named { int a; int b:3; };
