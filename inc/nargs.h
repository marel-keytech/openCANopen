#ifndef JLSS_ID_NARG_H
#define JLSS_ID_NARG_H

/*
** http://groups.google.com/group/comp.std.c/browse_thread/thread/77ee8c8f92e4a3fb/346fc464319b1ee5?pli=1
**
**    Newsgroups: comp.std.c
**    From: Laurent Deniau <laurent.deniau@cern.ch>
**    Date: Mon, 16 Jan 2006 18:43:40 +0100
**    Subject: __VA_NARG__
**
**    A year ago, I was asking here for an equivalent of __VA_NARG__ which
**    would return the number of arguments contained in __VA_ARGS__ before its
**    expansion. In fact my problem at that time (detecting for a third
**    argument) was solved by the solution of P. Mensonides. But I was still
**    thinking that the standard should have provided such a facilities rather
**    easy to compute for cpp.
**
**    This morning I had to face again the same problem, that is knowing the
**    number of arguments contained in __VA_ARGS__ before its expansion (after
**    its expansion can always be achieved if you can do it before). I found a
**    simple non-iterative solution which may be of interest here as an answer
**    to who will ask in the future for a kind of __VA_NARG__ in the standard
**    and I post it for archiving. May be some more elegant-efficient solution
**    exists?
**
**    Returns NARG, the number of arguments contained in __VA_ARGS__ before
**    expansion as far as NARG is >0 and <64 (cpp limits):
**
**    #define PP_NARG( ...) PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
**    #define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
**    #define PP_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,[..],_61,_62,_63,N,...) N
**    #define PP_RSEQ_N() 63,62,61,60,[..],9,8,7,6,5,4,3,2,1,0
**
**    [..] stands for the continuation of the sequence omitted here for
**    lisibility.
**
**    PP_NARG(A) -> 1
**    PP_NARG(A,B) -> 2
**    PP_NARG(A,B,C) -> 3
**    PP_NARG(A,B,C,D) -> 4
**    PP_NARG(A,B,C,D,E) -> 5
**    PP_NARG(A1,A2,[..],A62,A63) -> 63
**
** ======
**
**    Newsgroups: comp.std.c
**    From: Roland Illig <roland.il...@gmx.de>
**    Date: Fri, 20 Jan 2006 12:58:41 +0100
**    Subject: Re: __VA_NARG__
**
**    Laurent Deniau wrote:
**    > This morning I had to face again the same problem, that is knowing the
**    > number of arguments contained in __VA_ARGS__ before its expansion (after
**    > its expansion can always be achieved if you can do it before). I found a
**    > simple non-iterative solution which may be of interest here as an answer
**    > to who will ask in the future for a kind of __VA_NARG__ in the standard
**    > and I post it for archiving. May be some more elegant-efficient solution
**    > exists?
**
**    Thanks for this idea. I really like it.
**
**    For those that only want to copy and paste it, here is the expanded version:
**
** // Some test cases
** PP_NARG(A) -> 1
** PP_NARG(A,B) -> 2
** PP_NARG(A,B,C) -> 3
** PP_NARG(A,B,C,D) -> 4
** PP_NARG(A,B,C,D,E) -> 5
** PP_NARG(1,2,3,4,5,6,7,8,9,0,    //  1..10
**         1,2,3,4,5,6,7,8,9,0,    // 11..20
**         1,2,3,4,5,6,7,8,9,0,    // 21..30
**         1,2,3,4,5,6,7,8,9,0,    // 31..40
**         1,2,3,4,5,6,7,8,9,0,    // 41..50
**         1,2,3,4,5,6,7,8,9,0,    // 51..60
**         1,2,3) -> 63
**
**Note: using PP_NARG() without arguments would violate 6.10.3p4 of ISO C99.
*/

/* The PP_NARG macro returns the number of arguments that have been
** passed to it.
*/

#define PP_NARG(...) \
    PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
#define PP_NARG_(...) \
    PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N( \
     _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
    _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
    _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
    _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
    _61,_62,_63,  N, ...) N
#define PP_RSEQ_N() \
    63,62,61,60,                   \
    59,58,57,56,55,54,53,52,51,50, \
    49,48,47,46,45,44,43,42,41,40, \
    39,38,37,36,35,34,33,32,31,30, \
    29,28,27,26,25,24,23,22,21,20, \
    19,18,17,16,15,14,13,12,11,10, \
     9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#endif /* JLSS_ID_NARG_H */
