#ifndef DW_GARBAGE_H
#define DW_GARBAGE_H

#include <dw/types.h>

#ifdef __MWERKS__
#define GARBAGE(fn, n) \
	static int32_t fn##__garbage__(int32_t i) \
	{ \
		GARBAGE_REPEAT(n, GARBAGE_DECLARE, GARBAGE_DECLARE, i) \
		GARBAGE_REPEAT(n, GARBAGE_ASSIGN, GARBAGE_ASSIGN, i) \
		return GARBAGE_REPEAT(n, GARBAGE_TERM, GARBAGE_PLUS_TERM, i); \
	}
#define GARBAGE_ARRAY(fn, array, m, n) \
	static void fn##__garbage__(void) \
	{ \
		GARBAGE_REPEAT(n, GARBAGE_DECLARE, GARBAGE_DECLARE, (array, m)) \
		GARBAGE_REPEAT(n, GARBAGE_LOAD, GARBAGE_LOAD, (array, m)) \
		GARBAGE_REPEAT(n, GARBAGE_STORE, GARBAGE_STORE, (array, m)) \
	}
#else
#define GARBAGE(fn, n)
#define GARBAGE_ARRAY(fn, array, m, n)
#endif

#define GARBAGE_DECLARE(src, k) int32_t v##k;
#define GARBAGE_ASSIGN(src, k) v##k = src + k;
#define GARBAGE_TERM(src, k) v##k
#define GARBAGE_PLUS_TERM(src, k) + v##k
#define GARBAGE_LOAD(src, k) v##k = GARBAGE_ARRAY_OF src[k % GARBAGE_LENGTH_OF src] + k;
#define GARBAGE_STORE(src, k) GARBAGE_ARRAY_OF src[k % GARBAGE_LENGTH_OF src] = (v##k * v##k) + k;
#define GARBAGE_ARRAY_OF(array, m) array
#define GARBAGE_LENGTH_OF(array, m) m

#define GARBAGE_REPEAT(n, first, next, src) GARBAGE_REPEAT_##n(first, next, src)
#define GARBAGE_REPEAT_1(first, next, src) first(src, 0)
#define GARBAGE_REPEAT_2(first, next, src) GARBAGE_REPEAT_1(first, next, src) next(src, 1)
#define GARBAGE_REPEAT_3(first, next, src) GARBAGE_REPEAT_2(first, next, src) next(src, 2)
#define GARBAGE_REPEAT_4(first, next, src) GARBAGE_REPEAT_3(first, next, src) next(src, 3)
#define GARBAGE_REPEAT_5(first, next, src) GARBAGE_REPEAT_4(first, next, src) next(src, 4)
#define GARBAGE_REPEAT_6(first, next, src) GARBAGE_REPEAT_5(first, next, src) next(src, 5)
#define GARBAGE_REPEAT_7(first, next, src) GARBAGE_REPEAT_6(first, next, src) next(src, 6)
#define GARBAGE_REPEAT_8(first, next, src) GARBAGE_REPEAT_7(first, next, src) next(src, 7)
#define GARBAGE_REPEAT_9(first, next, src) GARBAGE_REPEAT_8(first, next, src) next(src, 8)
#define GARBAGE_REPEAT_10(first, next, src) GARBAGE_REPEAT_9(first, next, src) next(src, 9)
#define GARBAGE_REPEAT_11(first, next, src) GARBAGE_REPEAT_10(first, next, src) next(src, 10)
#define GARBAGE_REPEAT_12(first, next, src) GARBAGE_REPEAT_11(first, next, src) next(src, 11)
#define GARBAGE_REPEAT_13(first, next, src) GARBAGE_REPEAT_12(first, next, src) next(src, 12)
#define GARBAGE_REPEAT_14(first, next, src) GARBAGE_REPEAT_13(first, next, src) next(src, 13)
#define GARBAGE_REPEAT_15(first, next, src) GARBAGE_REPEAT_14(first, next, src) next(src, 14)
#define GARBAGE_REPEAT_16(first, next, src) GARBAGE_REPEAT_15(first, next, src) next(src, 15)
#define GARBAGE_REPEAT_17(first, next, src) GARBAGE_REPEAT_16(first, next, src) next(src, 16)
#define GARBAGE_REPEAT_18(first, next, src) GARBAGE_REPEAT_17(first, next, src) next(src, 17)
#define GARBAGE_REPEAT_19(first, next, src) GARBAGE_REPEAT_18(first, next, src) next(src, 18)
#define GARBAGE_REPEAT_20(first, next, src) GARBAGE_REPEAT_19(first, next, src) next(src, 19)
#define GARBAGE_REPEAT_21(first, next, src) GARBAGE_REPEAT_20(first, next, src) next(src, 20)
#define GARBAGE_REPEAT_22(first, next, src) GARBAGE_REPEAT_21(first, next, src) next(src, 21)
#define GARBAGE_REPEAT_23(first, next, src) GARBAGE_REPEAT_22(first, next, src) next(src, 22)
#define GARBAGE_REPEAT_24(first, next, src) GARBAGE_REPEAT_23(first, next, src) next(src, 23)
#define GARBAGE_REPEAT_25(first, next, src) GARBAGE_REPEAT_24(first, next, src) next(src, 24)
#define GARBAGE_REPEAT_26(first, next, src) GARBAGE_REPEAT_25(first, next, src) next(src, 25)
#define GARBAGE_REPEAT_27(first, next, src) GARBAGE_REPEAT_26(first, next, src) next(src, 26)
#define GARBAGE_REPEAT_28(first, next, src) GARBAGE_REPEAT_27(first, next, src) next(src, 27)
#define GARBAGE_REPEAT_29(first, next, src) GARBAGE_REPEAT_28(first, next, src) next(src, 28)
#define GARBAGE_REPEAT_30(first, next, src) GARBAGE_REPEAT_29(first, next, src) next(src, 29)
#define GARBAGE_REPEAT_31(first, next, src) GARBAGE_REPEAT_30(first, next, src) next(src, 30)
#define GARBAGE_REPEAT_32(first, next, src) GARBAGE_REPEAT_31(first, next, src) next(src, 31)
#define GARBAGE_REPEAT_33(first, next, src) GARBAGE_REPEAT_32(first, next, src) next(src, 32)
#define GARBAGE_REPEAT_34(first, next, src) GARBAGE_REPEAT_33(first, next, src) next(src, 33)
#define GARBAGE_REPEAT_35(first, next, src) GARBAGE_REPEAT_34(first, next, src) next(src, 34)
#define GARBAGE_REPEAT_36(first, next, src) GARBAGE_REPEAT_35(first, next, src) next(src, 35)
#define GARBAGE_REPEAT_37(first, next, src) GARBAGE_REPEAT_36(first, next, src) next(src, 36)
#define GARBAGE_REPEAT_38(first, next, src) GARBAGE_REPEAT_37(first, next, src) next(src, 37)
#define GARBAGE_REPEAT_39(first, next, src) GARBAGE_REPEAT_38(first, next, src) next(src, 38)
#define GARBAGE_REPEAT_40(first, next, src) GARBAGE_REPEAT_39(first, next, src) next(src, 39)
#define GARBAGE_REPEAT_41(first, next, src) GARBAGE_REPEAT_40(first, next, src) next(src, 40)
#define GARBAGE_REPEAT_42(first, next, src) GARBAGE_REPEAT_41(first, next, src) next(src, 41)
#define GARBAGE_REPEAT_43(first, next, src) GARBAGE_REPEAT_42(first, next, src) next(src, 42)
#define GARBAGE_REPEAT_44(first, next, src) GARBAGE_REPEAT_43(first, next, src) next(src, 43)
#define GARBAGE_REPEAT_45(first, next, src) GARBAGE_REPEAT_44(first, next, src) next(src, 44)
#define GARBAGE_REPEAT_46(first, next, src) GARBAGE_REPEAT_45(first, next, src) next(src, 45)
#define GARBAGE_REPEAT_47(first, next, src) GARBAGE_REPEAT_46(first, next, src) next(src, 46)
#define GARBAGE_REPEAT_48(first, next, src) GARBAGE_REPEAT_47(first, next, src) next(src, 47)
#define GARBAGE_REPEAT_49(first, next, src) GARBAGE_REPEAT_48(first, next, src) next(src, 48)
#define GARBAGE_REPEAT_50(first, next, src) GARBAGE_REPEAT_49(first, next, src) next(src, 49)
#define GARBAGE_REPEAT_51(first, next, src) GARBAGE_REPEAT_50(first, next, src) next(src, 50)
#define GARBAGE_REPEAT_52(first, next, src) GARBAGE_REPEAT_51(first, next, src) next(src, 51)
#define GARBAGE_REPEAT_53(first, next, src) GARBAGE_REPEAT_52(first, next, src) next(src, 52)
#define GARBAGE_REPEAT_54(first, next, src) GARBAGE_REPEAT_53(first, next, src) next(src, 53)
#define GARBAGE_REPEAT_55(first, next, src) GARBAGE_REPEAT_54(first, next, src) next(src, 54)
#define GARBAGE_REPEAT_56(first, next, src) GARBAGE_REPEAT_55(first, next, src) next(src, 55)
#define GARBAGE_REPEAT_57(first, next, src) GARBAGE_REPEAT_56(first, next, src) next(src, 56)
#define GARBAGE_REPEAT_58(first, next, src) GARBAGE_REPEAT_57(first, next, src) next(src, 57)
#define GARBAGE_REPEAT_59(first, next, src) GARBAGE_REPEAT_58(first, next, src) next(src, 58)
#define GARBAGE_REPEAT_60(first, next, src) GARBAGE_REPEAT_59(first, next, src) next(src, 59)
#define GARBAGE_REPEAT_61(first, next, src) GARBAGE_REPEAT_60(first, next, src) next(src, 60)
#define GARBAGE_REPEAT_62(first, next, src) GARBAGE_REPEAT_61(first, next, src) next(src, 61)
#define GARBAGE_REPEAT_63(first, next, src) GARBAGE_REPEAT_62(first, next, src) next(src, 62)
#define GARBAGE_REPEAT_64(first, next, src) GARBAGE_REPEAT_63(first, next, src) next(src, 63)
#define GARBAGE_REPEAT_65(first, next, src) GARBAGE_REPEAT_64(first, next, src) next(src, 64)
#define GARBAGE_REPEAT_66(first, next, src) GARBAGE_REPEAT_65(first, next, src) next(src, 65)
#define GARBAGE_REPEAT_67(first, next, src) GARBAGE_REPEAT_66(first, next, src) next(src, 66)
#define GARBAGE_REPEAT_68(first, next, src) GARBAGE_REPEAT_67(first, next, src) next(src, 67)
#define GARBAGE_REPEAT_69(first, next, src) GARBAGE_REPEAT_68(first, next, src) next(src, 68)
#define GARBAGE_REPEAT_70(first, next, src) GARBAGE_REPEAT_69(first, next, src) next(src, 69)
#define GARBAGE_REPEAT_71(first, next, src) GARBAGE_REPEAT_70(first, next, src) next(src, 70)
#define GARBAGE_REPEAT_72(first, next, src) GARBAGE_REPEAT_71(first, next, src) next(src, 71)
#define GARBAGE_REPEAT_73(first, next, src) GARBAGE_REPEAT_72(first, next, src) next(src, 72)
#define GARBAGE_REPEAT_74(first, next, src) GARBAGE_REPEAT_73(first, next, src) next(src, 73)
#define GARBAGE_REPEAT_75(first, next, src) GARBAGE_REPEAT_74(first, next, src) next(src, 74)
#define GARBAGE_REPEAT_76(first, next, src) GARBAGE_REPEAT_75(first, next, src) next(src, 75)
#define GARBAGE_REPEAT_77(first, next, src) GARBAGE_REPEAT_76(first, next, src) next(src, 76)
#define GARBAGE_REPEAT_78(first, next, src) GARBAGE_REPEAT_77(first, next, src) next(src, 77)
#define GARBAGE_REPEAT_79(first, next, src) GARBAGE_REPEAT_78(first, next, src) next(src, 78)
#define GARBAGE_REPEAT_80(first, next, src) GARBAGE_REPEAT_79(first, next, src) next(src, 79)

#endif
