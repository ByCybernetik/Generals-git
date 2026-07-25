/* Included from wwdebug.h on GENERALS_LINUX — Renegade-style macros without GameEngine. */

#ifdef WWDEBUG
#define WWDEBUG_SAY(x)							WWDebug_Printf x
#define WWDEBUG_WARNING(x)						WWDebug_Printf_Warning x
#define WWDEBUG_ERROR(x)						WWDebug_Printf_Error x
#else
#define WWDEBUG_SAY(x)
#define WWDEBUG_WARNING(x)
#define WWDEBUG_ERROR(x)
#endif

#define WWRELEASE_SAY(x)						WWDebug_Printf x
#define WWRELEASE_WARNING(x)					WWDebug_Printf_Warning x
#define WWRELEASE_ERROR(x)						WWDebug_Printf_Error x

#ifdef WWDEBUG
#define WWASSERT(expr)							((expr)?(void)0:WWDebug_Assert_Fail(#expr,__FILE__,__LINE__))
#define WWASSERT_PRINT( expr, string )		((expr)?(void)0:WWDebug_Assert_Fail_Print(#expr,__FILE__,__LINE__,string))
#define W3D_DIE                               (WWDebug_Assert_Fail("DIE",__FILE__,__LINE__))
#else
#define WWASSERT( expr )
#define WWASSERT_PRINT( expr, string )
#define W3D_DIE
#endif

#if defined(_MSC_VER)
#define WWDEBUG_BREAK							_asm int 0x03
#elif defined(__GNUC__)
#define WWDEBUG_BREAK							__builtin_trap()
#else
#define WWDEBUG_BREAK							((void)0)
#endif
