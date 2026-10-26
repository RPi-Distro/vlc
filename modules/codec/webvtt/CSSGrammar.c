/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         CSSSTYPE
/* Substitute the variable and function names.  */
#define yyparse         cssparse
#define yylex           csslex
#define yyerror         csserror
#define yydebug         cssdebug
#define yynerrs         cssnerrs

/* First part of user prologue.  */
#line 36 "codec/webvtt/CSSGrammar.y"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include "css_parser.h"

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#if YYBISON < 30800
# define YYNOMEM YYABORT
#endif

#line 93 "codec/webvtt/CSSGrammar.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_CSS_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED
# define YY_CSS_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef CSSDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define CSSDEBUG 1
#  else
#   define CSSDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define CSSDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined CSSDEBUG */
#if CSSDEBUG
extern int cssdebug;
#endif

/* Token type.  */
#ifndef CSSTOKENTYPE
# define CSSTOKENTYPE
  enum csstokentype
  {
    TOKEN_EOF = 0,
    LOWEST_PREC = 258,
    UNIMPORTANT_TOK = 259,
    WHITESPACE = 260,
    SGML_CD = 261,
    MEMERROR = 262,
    INCLUDES = 263,
    DASHMATCH = 264,
    BEGINSWITH = 265,
    ENDSWITH = 266,
    CONTAINS = 267,
    STRING = 268,
    IDENT = 269,
    IDSEL = 270,
    HASH = 271,
    FONT_FACE_SYM = 272,
    CHARSET_SYM = 273,
    IMPORTANT_SYM = 274,
    CDO = 275,
    CDC = 276,
    LENGTH = 277,
    ANGLE = 278,
    TIME = 279,
    FREQ = 280,
    DIMEN = 281,
    PERCENTAGE = 282,
    NUMBER = 283,
    URI = 284,
    FUNCTION = 285,
    UNICODERANGE = 286
  };
#endif
/* Tokens.  */
#define TOKEN_EOF 0
#define LOWEST_PREC 258
#define UNIMPORTANT_TOK 259
#define WHITESPACE 260
#define SGML_CD 261
#define MEMERROR 262
#define INCLUDES 263
#define DASHMATCH 264
#define BEGINSWITH 265
#define ENDSWITH 266
#define CONTAINS 267
#define STRING 268
#define IDENT 269
#define IDSEL 270
#define HASH 271
#define FONT_FACE_SYM 272
#define CHARSET_SYM 273
#define IMPORTANT_SYM 274
#define CDO 275
#define CDC 276
#define LENGTH 277
#define ANGLE 278
#define TIME 279
#define FREQ 280
#define DIMEN 281
#define PERCENTAGE 282
#define NUMBER 283
#define URI 284
#define FUNCTION 285
#define UNICODERANGE 286

/* Value type.  */
#if ! defined CSSSTYPE && ! defined CSSSTYPE_IS_DECLARED
union CSSSTYPE
{
#line 53 "codec/webvtt/CSSGrammar.y"

    bool boolean;
    char character;
    int integer;
    char *string;
    enum vlc_css_relation_e relation;

    vlc_css_term_t term;
    vlc_css_expr_t *expr;
    vlc_css_rule_t  *rule;
    vlc_css_declaration_t *declaration;
    vlc_css_declaration_t *declarationList;
    vlc_css_selector_t *selector;
    vlc_css_selector_t *selectorList;

#line 233 "codec/webvtt/CSSGrammar.c"

};
typedef union CSSSTYPE CSSSTYPE;
# define CSSSTYPE_IS_TRIVIAL 1
# define CSSSTYPE_IS_DECLARED 1
#endif



int cssparse (yyscan_t scanner, vlc_css_parser_t *css_parser);

#endif /* !YY_CSS_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED  */

/* Second part of user prologue.  */
#line 69 "codec/webvtt/CSSGrammar.y"

/* See bison pure calling */
#define YY_DECL int yylex(union YYSTYPE *, yyscan_t)
YY_DECL;

static void yyerror(yyscan_t scanner, vlc_css_parser_t *p, const char *msg)
{
    VLC_UNUSED(scanner);VLC_UNUSED(p);VLC_UNUSED(msg);
}


#line 260 "codec/webvtt/CSSGrammar.c"


#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))

/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined CSSSTYPE_IS_TRIVIAL && CSSSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  4
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   425

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  51
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  42
/* YYNRULES -- Number of rules.  */
#define YYNRULES  132
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  227

#define YYUNDEFTOK  2
#define YYMAXUTOK   286


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    49,     2,    50,     2,     2,
       2,    47,    20,    40,    44,    43,    18,    48,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    17,    38,
       2,    46,    42,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    19,     2,    45,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    39,    21,    37,    41,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36
};

#if CSSDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   181,   181,   182,   188,   189,   192,   194,   195,   198,
     200,   204,   205,   209,   213,   216,   222,   227,   232,   234,
     241,   242,   246,   251,   252,   256,   261,   264,   270,   271,
     272,   276,   277,   281,   282,   286,   296,   299,   312,   319,
     325,   328,   332,   347,   361,   368,   379,   392,   398,   399,
     403,   406,   417,   424,   436,   448,   449,   450,   454,   468,
     474,   483,   505,   508,   511,   514,   517,   520,   526,   527,
     531,   542,   554,   572,   594,   613,   616,   624,   627,   630,
     633,   636,   642,   645,   649,   653,   656,   659,   667,   670,
     676,   688,   694,   699,   704,   709,   713,   720,   726,   727,
     731,   736,   746,   759,   762,   768,   769,   773,   774,   776,
     777,   778,   779,   780,   781,   782,   784,   787,   793,   794,
     795,   796,   797,   798,   802,   808,   813,   818,   828,   846,
     847,   851,   852
};
#endif

#if CSSDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "TOKEN_EOF", "error", "$undefined", "LOWEST_PREC", "UNIMPORTANT_TOK",
  "WHITESPACE", "SGML_CD", "MEMERROR", "INCLUDES", "DASHMATCH",
  "BEGINSWITH", "ENDSWITH", "CONTAINS", "STRING", "IDENT", "IDSEL", "HASH",
  "':'", "'.'", "'['", "'*'", "'|'", "FONT_FACE_SYM", "CHARSET_SYM",
  "IMPORTANT_SYM", "CDO", "CDC", "LENGTH", "ANGLE", "TIME", "FREQ",
  "DIMEN", "PERCENTAGE", "NUMBER", "URI", "FUNCTION", "UNICODERANGE",
  "'}'", "';'", "'{'", "'+'", "'~'", "'>'", "'-'", "','", "']'", "'='",
  "')'", "'/'", "'#'", "'%'", "$accept", "stylesheet", "maybe_space",
  "maybe_sgml", "maybe_charset", "closing_brace", "charset",
  "ignored_charset", "rule_list", "valid_rule", "rule", "font_face",
  "combinator", "maybe_unary_operator", "unary_operator", "ruleset",
  "selector_list", "selector_with_trailing_whitespace", "selector",
  "simple_selector", "element_name", "specifier_list", "specifier",
  "class", "attr_name", "attrib", "match", "ident_or_string", "pseudo",
  "declaration_list", "decl_list", "declaration", "property", "prio",
  "expr", "operator", "term", "unary_term", "function", "invalid_rule",
  "invalid_block", "invalid_block_list", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,    58,    46,    91,
      42,   124,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   125,    59,   123,
      43,   126,    62,    45,    44,    93,    61,    41,    47,    35,
      37
};
# endif

#define YYPACT_NINF (-127)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-100)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      -5,  -127,    11,    62,  -127,  -127,   182,  -127,  -127,   -23,
     128,   100,  -127,    12,  -127,  -127,  -127,  -127,   402,    34,
       5,   -22,  -127,  -127,  -127,   123,    52,  -127,  -127,   138,
    -127,  -127,  -127,  -127,  -127,  -127,     0,   353,   110,  -127,
     263,    13,  -127,  -127,  -127,  -127,  -127,  -127,  -127,  -127,
    -127,    21,  -127,  -127,  -127,     6,  -127,  -127,    75,   158,
      30,   143,   100,  -127,  -127,  -127,  -127,  -127,  -127,  -127,
    -127,  -127,   353,    83,  -127,  -127,    34,  -127,  -127,    78,
    -127,   287,  -127,  -127,  -127,  -127,  -127,   187,   321,    22,
      22,    22,  -127,  -127,  -127,   321,  -127,  -127,  -127,    48,
    -127,    22,  -127,  -127,  -127,  -127,  -127,  -127,  -127,  -127,
     187,    88,   174,  -127,  -127,     8,    95,   177,    96,   213,
      35,    -1,  -127,   171,     8,  -127,  -127,    93,    22,    22,
    -127,   206,    72,    25,  -127,   352,  -127,  -127,  -127,     1,
    -127,    14,  -127,  -127,  -127,  -127,    22,   220,  -127,   111,
    -127,   -22,    22,  -127,    22,   234,  -127,  -127,     2,  -127,
      22,   247,    22,    22,  -127,  -127,  -127,  -127,  -127,  -127,
    -127,  -127,  -127,  -127,  -127,  -127,  -127,  -127,  -127,  -127,
    -127,   355,   193,  -127,  -127,  -127,  -127,    22,  -127,    22,
      22,    22,    22,    22,    22,    22,    22,    22,    22,    22,
      22,   315,    22,    22,    22,  -127,  -127,  -127,  -127,  -127,
    -127,  -127,   365,  -127,    22,  -127,  -127,   275,    22,    22,
      22,    22,  -127,    22,  -127,  -127,    22
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       4,     3,     0,     9,     1,     5,     0,     6,    10,     0,
       0,    18,    15,     0,    14,     4,     8,     7,     0,     0,
       0,     0,    48,    53,    54,     0,     0,     4,    49,     0,
       4,    23,    22,     6,    21,    20,     0,    41,     0,    40,
      45,     0,    50,    55,    56,    57,    24,    12,    11,   130,
     131,     0,    13,   128,    70,     0,     4,    58,     0,     0,
       0,     0,    19,    38,     4,     4,    42,    44,    39,     4,
       4,     4,     0,     0,    52,    51,     0,    71,     4,    32,
       4,     0,    27,    26,     4,     4,    17,     0,     0,    28,
      29,    30,    43,   129,   132,     0,     4,    34,    33,     0,
      31,    59,    63,    64,    65,    66,    67,    60,    62,     4,
       0,     0,    79,     4,     4,     0,     0,    75,     0,     0,
       0,     0,     4,     0,     0,    16,     4,     0,    97,    96,
      35,    80,    76,     0,     4,     0,    93,     4,    95,     0,
      74,     0,    69,    68,     4,    25,    85,    78,     4,     0,
       4,     0,    82,     4,    83,     0,    73,    72,     0,     4,
      88,     0,    87,    84,    92,     4,     4,     4,     4,     4,
       4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
       4,     0,     0,   100,   105,   116,    61,    86,     4,   107,
     108,   113,   114,   120,   121,   122,   123,   109,   119,   118,
     111,     0,   112,   115,   117,     4,   106,    91,     4,     4,
       4,    90,     0,   101,    89,   127,     4,     0,   110,    98,
     104,   103,   102,   126,   125,     4,   124
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -127,  -127,    -6,    84,  -127,   -73,  -127,  -127,  -127,  -127,
    -127,  -127,  -127,  -127,    57,  -127,  -127,  -127,   -83,    -4,
    -127,   105,    -3,  -127,  -127,  -127,  -127,  -127,  -127,    45,
    -127,    41,  -127,  -127,   -21,  -127,  -126,    17,  -127,  -127,
      28,    74
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,    11,     7,    49,     8,    31,    18,    32,
      33,    34,    72,    99,   181,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    81,    44,   109,   144,    45,   115,
     116,   117,   118,   211,   182,   212,   183,   184,   185,    46,
      50,    51
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      10,    63,     1,    93,     5,   119,     5,     5,    47,    20,
       5,     4,   120,    19,    74,    12,    13,    13,   -47,     5,
      77,    58,    76,    60,    61,   -81,   151,     5,    23,    24,
      25,    26,    27,    66,    47,     5,    67,    14,    75,    64,
      68,    78,   130,    52,    65,    48,   140,   186,   156,    53,
      79,   145,   -47,   -47,   -47,   -47,   213,   -47,    87,    88,
     -47,   157,   -81,    89,    90,    91,    57,     5,    92,    84,
      75,    48,    95,    13,   101,    69,    70,    71,   110,   111,
       5,   122,    -4,     5,    74,     6,   222,    83,   -46,    80,
     121,   213,    96,     5,   147,   -77,   131,   136,    23,    24,
      25,    26,    27,   123,    94,    16,    17,   128,   129,   113,
     150,    67,   161,   137,   139,    68,   141,    62,    97,   114,
     146,    98,   -46,   -46,   -46,   -46,   125,   -46,   152,   154,
     -46,   155,   -77,     5,    13,    13,   100,    54,   158,    59,
      55,    15,   160,    -4,   162,    73,   138,   163,     5,   -36,
      69,    70,    71,   187,   -36,   124,    85,   132,    56,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,    94,     5,    -4,     0,    94,
     217,    86,   214,     9,   142,   143,   127,    -4,   112,    94,
     133,   135,     5,   -99,   207,    -4,    82,    13,   206,   218,
       0,   113,   219,   220,   221,   149,   165,   166,   167,   168,
     223,   114,   126,    13,    67,   134,    13,   208,    68,   226,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     -99,   -99,   -99,    97,   -94,   164,    98,   209,     0,     5,
       0,   210,   179,   180,   148,    13,     0,   165,   166,   167,
     168,     0,   -37,    69,    70,    71,     0,   -37,   159,    13,
       0,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   -94,   -94,   -94,    97,   224,     0,    98,    23,    24,
      25,    26,    27,   179,   180,   188,    13,     0,   165,   166,
     167,   168,     0,     0,     0,   102,   103,   104,   105,   106,
       0,     0,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,     0,     0,     0,    97,   215,     0,    98,   209,
       5,     0,   225,   210,   179,   180,     5,     0,   165,   166,
     167,   168,   107,   108,     0,    22,    23,    24,    25,    26,
      27,    28,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,    -4,   151,     0,    97,     0,    -4,    98,     0,
       0,     0,   216,     0,   179,   180,    -4,    22,    23,    24,
      25,    26,    27,    28,     0,     0,    -4,     0,   165,   166,
     167,   168,   169,   170,   171,   172,   205,   174,   175,    -4,
     153,    -4,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,    -2,    21,     0,    97,     0,     0,    98,     0,
       0,     0,     0,     0,   179,   180,    22,    23,    24,    25,
      26,    27,    28,     0,    29,    30
};

static const yytype_int16 yycheck[] =
{
       6,     1,     7,    76,     5,    88,     5,     5,     0,    15,
       5,     0,    95,     1,     1,    38,    39,    39,     5,     5,
      14,    27,     1,    29,    30,     0,     1,     5,    15,    16,
      17,    18,    19,    37,     0,     5,     1,     9,    41,    39,
       5,    35,   115,    38,    44,    37,    47,    45,    47,    21,
      56,   124,    39,    40,    41,    42,   182,    44,    64,    65,
      47,    47,    37,    69,    70,    71,    14,     5,    72,    39,
      73,    37,    78,    39,    80,    40,    41,    42,    84,    85,
       5,    33,    47,     5,     1,    23,   212,    59,     5,    14,
      96,   217,    14,     5,     1,     0,     1,     1,    15,    16,
      17,    18,    19,   109,    76,     5,     6,   113,   114,    14,
      38,     1,     1,    17,   120,     5,   122,    33,    40,    24,
     126,    43,    39,    40,    41,    42,    38,    44,   134,   135,
      47,   137,    37,     5,    39,    39,    79,    14,   144,     1,
      17,    13,   148,     5,   150,    40,   118,   153,     5,    39,
      40,    41,    42,   159,    44,   110,    13,   116,    35,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   147,     5,    39,    -1,   151,
     201,    38,   188,     1,    13,    14,   112,     5,     1,   161,
     116,   117,     5,     0,     1,    13,    38,    39,   181,   205,
      -1,    14,   208,   209,   210,   131,    13,    14,    15,    16,
     216,    24,    38,    39,     1,    38,    39,    24,     5,   225,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,     1,    43,    44,    -1,     5,
      -1,    48,    49,    50,    38,    39,    -1,    13,    14,    15,
      16,    -1,    39,    40,    41,    42,    -1,    44,    38,    39,
      -1,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    -1,    43,    15,    16,
      17,    18,    19,    49,    50,    38,    39,    -1,    13,    14,
      15,    16,    -1,    -1,    -1,     8,     9,    10,    11,    12,
      -1,    -1,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    40,     1,    -1,    43,    44,
       5,    -1,    47,    48,    49,    50,     5,    -1,    13,    14,
      15,    16,    45,    46,    -1,    14,    15,    16,    17,    18,
      19,    20,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,     0,     1,    -1,    40,    -1,     5,    43,    -1,
      -1,    -1,    47,    -1,    49,    50,    14,    14,    15,    16,
      17,    18,    19,    20,    -1,    -1,    24,    -1,    13,    14,
      15,    16,    27,    28,    29,    30,    31,    32,    33,    37,
      38,    39,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,     0,     1,    -1,    40,    -1,    -1,    43,    -1,
      -1,    -1,    -1,    -1,    49,    50,    14,    15,    16,    17,
      18,    19,    20,    -1,    22,    23
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     7,    52,    53,     0,     5,    23,    55,    57,     1,
      53,    54,    38,    39,    91,    13,     5,     6,    59,     1,
      53,     1,    14,    15,    16,    17,    18,    19,    20,    22,
      23,    58,    60,    61,    62,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    76,    79,    90,     0,    37,    56,
      91,    92,    38,    91,    14,    17,    35,    14,    53,     1,
      53,    53,    54,     1,    39,    44,    70,     1,     5,    40,
      41,    42,    63,    72,     1,    73,     1,    14,    35,    53,
      14,    75,    38,    91,    39,    13,    38,    53,    53,    53,
      53,    53,    70,    56,    91,    53,    14,    40,    43,    64,
      65,    53,     8,     9,    10,    11,    12,    45,    46,    77,
      53,    53,     1,    14,    24,    80,    81,    82,    83,    69,
      69,    53,    33,    53,    80,    38,    38,    92,    53,    53,
      56,     1,    82,    92,    38,    92,     1,    17,    91,    53,
      47,    53,    13,    14,    78,    56,    53,     1,    38,    92,
      38,     1,    53,    38,    53,    53,    47,    47,    53,    38,
      53,     1,    53,    53,     1,    13,    14,    15,    16,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    49,
      50,    65,    85,    87,    88,    89,    45,    53,    38,    53,
      53,    53,    53,    53,    53,    53,    53,    53,    53,    53,
      53,    53,    53,    53,    53,    31,    88,     1,    24,    44,
      48,    84,    86,    87,    53,     1,    47,    85,    53,    53,
      53,    53,    87,    53,     0,    47,    53
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] =
{
       0,    51,    52,    52,    53,    53,    54,    54,    54,    55,
      55,    56,    56,    57,    57,    57,    58,    58,    59,    59,
      60,    60,    61,    61,    61,    62,    62,    62,    63,    63,
      63,    64,    64,    65,    65,    66,    67,    67,    67,    68,
      69,    69,    69,    69,    69,    70,    70,    70,    71,    71,
      72,    72,    72,    73,    73,    73,    73,    73,    74,    75,
      76,    76,    77,    77,    77,    77,    77,    77,    78,    78,
      79,    79,    79,    79,    79,    80,    80,    80,    80,    80,
      80,    80,    81,    81,    81,    81,    81,    81,    81,    81,
      82,    82,    82,    82,    82,    82,    82,    83,    84,    84,
      85,    85,    85,    86,    86,    87,    87,    87,    87,    87,
      87,    87,    87,    87,    87,    87,    87,    87,    88,    88,
      88,    88,    88,    88,    89,    89,    89,    89,    90,    91,
      91,    92,    92
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     4,     1,     0,     2,     0,     2,     2,     0,
       1,     1,     1,     5,     3,     3,     5,     3,     0,     3,
       1,     1,     1,     1,     1,     6,     3,     3,     2,     2,
       2,     1,     0,     1,     1,     5,     1,     4,     2,     2,
       1,     1,     2,     3,     2,     1,     2,     1,     1,     1,
       1,     2,     2,     1,     1,     1,     1,     1,     2,     2,
       4,     8,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     3,     7,     7,     6,     1,     2,     1,     3,     1,
       2,     2,     3,     3,     4,     3,     5,     4,     4,     6,
       5,     5,     4,     2,     3,     2,     2,     2,     2,     0,
       1,     2,     3,     2,     2,     1,     2,     2,     2,     2,
       3,     2,     2,     2,     2,     2,     1,     2,     2,     2,
       2,     2,     2,     2,     5,     4,     4,     3,     2,     5,
       3,     1,     3
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (scanner, css_parser, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if CSSDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, scanner, css_parser); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, yyscan_t scanner, vlc_css_parser_t *css_parser)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  YYUSE (scanner);
  YYUSE (css_parser);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, yyscan_t scanner, vlc_css_parser_t *css_parser)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyo, yytype, yyvaluep, scanner, css_parser);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule, yyscan_t scanner, vlc_css_parser_t *css_parser)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[+yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                                              , scanner, css_parser);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, scanner, css_parser); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !CSSDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !CSSDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#  else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                yy_state_t *yyssp, int yytoken)
{
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Actual size of YYARG. */
  int yycount = 0;
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[+*yyssp];
      YYPTRDIFF_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
      yysize = yysize0;
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYPTRDIFF_T yysize1
                    = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                    yysize = yysize1;
                  else
                    return 2;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    /* Don't count the "%s"s in the final size, but reserve room for
       the terminator.  */
    YYPTRDIFF_T yysize1 = yysize + (yystrlen (yyformat) - 2 * yycount) + 1;
    if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
      yysize = yysize1;
    else
      return 2;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, yyscan_t scanner, vlc_css_parser_t *css_parser)
{
  YYUSE (yyvaluep);
  YYUSE (scanner);
  YYUSE (css_parser);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yytype)
    {
    case 13: /* STRING  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1372 "codec/webvtt/CSSGrammar.c"
        break;

    case 14: /* IDENT  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1378 "codec/webvtt/CSSGrammar.c"
        break;

    case 15: /* IDSEL  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1384 "codec/webvtt/CSSGrammar.c"
        break;

    case 16: /* HASH  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1390 "codec/webvtt/CSSGrammar.c"
        break;

    case 27: /* LENGTH  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1396 "codec/webvtt/CSSGrammar.c"
        break;

    case 28: /* ANGLE  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1402 "codec/webvtt/CSSGrammar.c"
        break;

    case 29: /* TIME  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1408 "codec/webvtt/CSSGrammar.c"
        break;

    case 30: /* FREQ  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1414 "codec/webvtt/CSSGrammar.c"
        break;

    case 31: /* DIMEN  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1420 "codec/webvtt/CSSGrammar.c"
        break;

    case 32: /* PERCENTAGE  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1426 "codec/webvtt/CSSGrammar.c"
        break;

    case 33: /* NUMBER  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1432 "codec/webvtt/CSSGrammar.c"
        break;

    case 34: /* URI  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1438 "codec/webvtt/CSSGrammar.c"
        break;

    case 35: /* FUNCTION  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1444 "codec/webvtt/CSSGrammar.c"
        break;

    case 36: /* UNICODERANGE  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1450 "codec/webvtt/CSSGrammar.c"
        break;

    case 57: /* charset  */
#line 139 "codec/webvtt/CSSGrammar.y"
            { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1456 "codec/webvtt/CSSGrammar.c"
        break;

    case 58: /* ignored_charset  */
#line 139 "codec/webvtt/CSSGrammar.y"
            { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1462 "codec/webvtt/CSSGrammar.c"
        break;

    case 60: /* valid_rule  */
#line 139 "codec/webvtt/CSSGrammar.y"
            { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1468 "codec/webvtt/CSSGrammar.c"
        break;

    case 61: /* rule  */
#line 139 "codec/webvtt/CSSGrammar.y"
            { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1474 "codec/webvtt/CSSGrammar.c"
        break;

    case 62: /* font_face  */
#line 139 "codec/webvtt/CSSGrammar.y"
            { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1480 "codec/webvtt/CSSGrammar.c"
        break;

    case 66: /* ruleset  */
#line 139 "codec/webvtt/CSSGrammar.y"
            { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1486 "codec/webvtt/CSSGrammar.c"
        break;

    case 67: /* selector_list  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selectorList)); }
#line 1492 "codec/webvtt/CSSGrammar.c"
        break;

    case 68: /* selector_with_trailing_whitespace  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1498 "codec/webvtt/CSSGrammar.c"
        break;

    case 69: /* selector  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1504 "codec/webvtt/CSSGrammar.c"
        break;

    case 70: /* simple_selector  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1510 "codec/webvtt/CSSGrammar.c"
        break;

    case 71: /* element_name  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1516 "codec/webvtt/CSSGrammar.c"
        break;

    case 72: /* specifier_list  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1522 "codec/webvtt/CSSGrammar.c"
        break;

    case 73: /* specifier  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1528 "codec/webvtt/CSSGrammar.c"
        break;

    case 74: /* class  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1534 "codec/webvtt/CSSGrammar.c"
        break;

    case 75: /* attr_name  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1540 "codec/webvtt/CSSGrammar.c"
        break;

    case 76: /* attrib  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1546 "codec/webvtt/CSSGrammar.c"
        break;

    case 78: /* ident_or_string  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1552 "codec/webvtt/CSSGrammar.c"
        break;

    case 79: /* pseudo  */
#line 153 "codec/webvtt/CSSGrammar.y"
            { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1558 "codec/webvtt/CSSGrammar.c"
        break;

    case 80: /* declaration_list  */
#line 158 "codec/webvtt/CSSGrammar.y"
            { vlc_css_declarations_Delete(((*yyvaluep).declarationList)); }
#line 1564 "codec/webvtt/CSSGrammar.c"
        break;

    case 81: /* decl_list  */
#line 158 "codec/webvtt/CSSGrammar.y"
            { vlc_css_declarations_Delete(((*yyvaluep).declarationList)); }
#line 1570 "codec/webvtt/CSSGrammar.c"
        break;

    case 82: /* declaration  */
#line 158 "codec/webvtt/CSSGrammar.y"
            { vlc_css_declarations_Delete(((*yyvaluep).declaration)); }
#line 1576 "codec/webvtt/CSSGrammar.c"
        break;

    case 83: /* property  */
#line 176 "codec/webvtt/CSSGrammar.y"
            { free(((*yyvaluep).string)); }
#line 1582 "codec/webvtt/CSSGrammar.c"
        break;

    case 85: /* expr  */
#line 171 "codec/webvtt/CSSGrammar.y"
            { vlc_css_expression_Delete(((*yyvaluep).expr)); }
#line 1588 "codec/webvtt/CSSGrammar.c"
        break;

    case 87: /* term  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1594 "codec/webvtt/CSSGrammar.c"
        break;

    case 88: /* unary_term  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1600 "codec/webvtt/CSSGrammar.c"
        break;

    case 89: /* function  */
#line 124 "codec/webvtt/CSSGrammar.y"
            { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1606 "codec/webvtt/CSSGrammar.c"
        break;

    case 90: /* invalid_rule  */
#line 139 "codec/webvtt/CSSGrammar.y"
            { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1612 "codec/webvtt/CSSGrammar.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (yyscan_t scanner, vlc_css_parser_t *css_parser)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYPTRDIFF_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
# undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 3:
#line 182 "codec/webvtt/CSSGrammar.y"
               { // catch alloc failures from lexer
        YYNOMEM;
    }
#line 1888 "codec/webvtt/CSSGrammar.c"
    break;

  case 13:
#line 209 "codec/webvtt/CSSGrammar.y"
                                                 {
      free( (yyvsp[-2].string) );
      (yyval.rule) = NULL;
  }
#line 1897 "codec/webvtt/CSSGrammar.c"
    break;

  case 14:
#line 213 "codec/webvtt/CSSGrammar.y"
                                    {
      (yyval.rule) = NULL;
  }
#line 1905 "codec/webvtt/CSSGrammar.c"
    break;

  case 15:
#line 216 "codec/webvtt/CSSGrammar.y"
                          {
      (yyval.rule) = NULL;
  }
#line 1913 "codec/webvtt/CSSGrammar.c"
    break;

  case 16:
#line 222 "codec/webvtt/CSSGrammar.y"
                                                   {
        // Ignore any @charset rule not at the beginning of the style sheet.
        free( (yyvsp[-2].string) );
        (yyval.rule) = NULL;
    }
#line 1923 "codec/webvtt/CSSGrammar.c"
    break;

  case 17:
#line 227 "codec/webvtt/CSSGrammar.y"
                                  {
        (yyval.rule) = NULL;
    }
#line 1931 "codec/webvtt/CSSGrammar.c"
    break;

  case 19:
#line 234 "codec/webvtt/CSSGrammar.y"
                             {
     if( (yyvsp[-1].rule) )
         vlc_css_parser_AddRule( css_parser, (yyvsp[-1].rule) );
 }
#line 1940 "codec/webvtt/CSSGrammar.c"
    break;

  case 22:
#line 246 "codec/webvtt/CSSGrammar.y"
               {
        (yyval.rule) = (yyvsp[0].rule);
        if((yyval.rule))
            (yyval.rule)->b_valid = true;
    }
#line 1950 "codec/webvtt/CSSGrammar.c"
    break;

  case 25:
#line 257 "codec/webvtt/CSSGrammar.y"
                                                   {
        vlc_css_declarations_Delete( (yyvsp[-1].declarationList) );
        (yyval.rule) = NULL;
    }
#line 1959 "codec/webvtt/CSSGrammar.c"
    break;

  case 26:
#line 261 "codec/webvtt/CSSGrammar.y"
                                        {
        (yyval.rule) = NULL;
    }
#line 1967 "codec/webvtt/CSSGrammar.c"
    break;

  case 27:
#line 264 "codec/webvtt/CSSGrammar.y"
                              {
        (yyval.rule) = NULL;
    }
#line 1975 "codec/webvtt/CSSGrammar.c"
    break;

  case 28:
#line 270 "codec/webvtt/CSSGrammar.y"
                    { (yyval.relation) = RELATION_DIRECTADJACENT; }
#line 1981 "codec/webvtt/CSSGrammar.c"
    break;

  case 29:
#line 271 "codec/webvtt/CSSGrammar.y"
                    { (yyval.relation) = RELATION_INDIRECTADJACENT; }
#line 1987 "codec/webvtt/CSSGrammar.c"
    break;

  case 30:
#line 272 "codec/webvtt/CSSGrammar.y"
                    { (yyval.relation) = RELATION_CHILD; }
#line 1993 "codec/webvtt/CSSGrammar.c"
    break;

  case 31:
#line 276 "codec/webvtt/CSSGrammar.y"
                   { (yyval.integer) = (yyvsp[0].integer); }
#line 1999 "codec/webvtt/CSSGrammar.c"
    break;

  case 32:
#line 277 "codec/webvtt/CSSGrammar.y"
      { (yyval.integer) = 1; }
#line 2005 "codec/webvtt/CSSGrammar.c"
    break;

  case 33:
#line 281 "codec/webvtt/CSSGrammar.y"
        { (yyval.integer) = -1; }
#line 2011 "codec/webvtt/CSSGrammar.c"
    break;

  case 34:
#line 282 "codec/webvtt/CSSGrammar.y"
        { (yyval.integer) = 1; }
#line 2017 "codec/webvtt/CSSGrammar.c"
    break;

  case 35:
#line 286 "codec/webvtt/CSSGrammar.y"
                                                                 {
        (yyval.rule) = vlc_css_rule_New();
        if( !(yyval.rule) )
            YYNOMEM;
        (yyval.rule)->p_selectors = (yyvsp[-4].selectorList);
        (yyval.rule)->p_declarations = (yyvsp[-1].declarationList);
    }
#line 2029 "codec/webvtt/CSSGrammar.c"
    break;

  case 36:
#line 296 "codec/webvtt/CSSGrammar.y"
                                   {
        (yyval.selectorList) = (yyvsp[0].selector);
    }
#line 2037 "codec/webvtt/CSSGrammar.c"
    break;

  case 37:
#line 299 "codec/webvtt/CSSGrammar.y"
                                                                   {
        if ((yyvsp[-3].selectorList) && (yyvsp[0].selector) )
        {
            (yyval.selectorList) = (yyvsp[-3].selectorList);
            vlc_css_selector_Append( (yyval.selectorList), (yyvsp[0].selector) );
        }
        else
        {
            vlc_css_selectors_Delete( (yyvsp[-3].selectorList) );
            vlc_css_selectors_Delete( (yyvsp[0].selector) );
            (yyval.selectorList) = NULL;
        }
    }
#line 2055 "codec/webvtt/CSSGrammar.c"
    break;

  case 38:
#line 312 "codec/webvtt/CSSGrammar.y"
                        {
        vlc_css_selectors_Delete( (yyvsp[-1].selectorList) );
        (yyval.selectorList) = NULL;
    }
#line 2064 "codec/webvtt/CSSGrammar.c"
    break;

  case 39:
#line 319 "codec/webvtt/CSSGrammar.y"
                        {
        (yyval.selector) = (yyvsp[-1].selector);
    }
#line 2072 "codec/webvtt/CSSGrammar.c"
    break;

  case 40:
#line 325 "codec/webvtt/CSSGrammar.y"
                    {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 2080 "codec/webvtt/CSSGrammar.c"
    break;

  case 41:
#line 329 "codec/webvtt/CSSGrammar.y"
    {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 2088 "codec/webvtt/CSSGrammar.c"
    break;

  case 42:
#line 333 "codec/webvtt/CSSGrammar.y"
    {
        if( (yyvsp[-1].selector) && (yyvsp[0].selector) )
        {
            vlc_css_selector_AddSpecifier( (yyvsp[-1].selector), (yyvsp[0].selector) );
            (yyvsp[0].selector)->combinator = RELATION_DESCENDENT;
            (yyval.selector) = (yyvsp[-1].selector);
        }
        else
        {
            vlc_css_selectors_Delete( (yyvsp[-1].selector) );
            vlc_css_selectors_Delete( (yyvsp[0].selector) );
            (yyval.selector) = NULL;
        }
    }
#line 2107 "codec/webvtt/CSSGrammar.c"
    break;

  case 43:
#line 347 "codec/webvtt/CSSGrammar.y"
                                          {
        if( (yyvsp[-2].selector) && (yyvsp[0].selector) )
        {
            vlc_css_selector_AddSpecifier( (yyvsp[-2].selector), (yyvsp[0].selector) );
            (yyvsp[0].selector)->combinator = (yyvsp[-1].relation);
            (yyval.selector) = (yyvsp[-2].selector);
        }
        else
        {
            vlc_css_selectors_Delete( (yyvsp[-2].selector) );
            vlc_css_selectors_Delete( (yyvsp[0].selector) );
            (yyval.selector) = NULL;
        }
    }
#line 2126 "codec/webvtt/CSSGrammar.c"
    break;

  case 44:
#line 361 "codec/webvtt/CSSGrammar.y"
                     {
        vlc_css_selectors_Delete( (yyvsp[-1].selector) );
        (yyval.selector) = NULL;
    }
#line 2135 "codec/webvtt/CSSGrammar.c"
    break;

  case 45:
#line 368 "codec/webvtt/CSSGrammar.y"
                 {
        if( !(yyvsp[0].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SELECTOR_SIMPLE, (yyvsp[0].string) );
        if( !(yyval.selector) )
            YYNOMEM; // destructors called
        free( (yyvsp[0].string) );
    }
#line 2151 "codec/webvtt/CSSGrammar.c"
    break;

  case 46:
#line 379 "codec/webvtt/CSSGrammar.y"
                                  {
        if( !(yyvsp[-1].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SELECTOR_SIMPLE, (yyvsp[-1].string) );
        if( !(yyval.selector) )
            YYNOMEM; // destructors called
        if( (yyvsp[0].selector) )
            vlc_css_selector_AddSpecifier( (yyval.selector), (yyvsp[0].selector) );
        free( (yyvsp[-1].string) );
    }
#line 2169 "codec/webvtt/CSSGrammar.c"
    break;

  case 47:
#line 392 "codec/webvtt/CSSGrammar.y"
                     {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 2177 "codec/webvtt/CSSGrammar.c"
    break;

  case 49:
#line 399 "codec/webvtt/CSSGrammar.y"
          { (yyval.string) = strdup("*"); }
#line 2183 "codec/webvtt/CSSGrammar.c"
    break;

  case 50:
#line 403 "codec/webvtt/CSSGrammar.y"
              {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 2191 "codec/webvtt/CSSGrammar.c"
    break;

  case 51:
#line 406 "codec/webvtt/CSSGrammar.y"
                               {
        if( (yyvsp[-1].selector) )
        {
            (yyval.selector) = (yyvsp[-1].selector);
            while( (yyvsp[-1].selector)->specifiers.p_first )
                (yyvsp[-1].selector) = (yyvsp[-1].selector)->specifiers.p_first;
            if( (yyvsp[0].selector) )
                vlc_css_selector_AddSpecifier( (yyvsp[-1].selector), (yyvsp[0].selector) );
        }
        else (yyval.selector) = (yyvsp[0].selector);
    }
#line 2207 "codec/webvtt/CSSGrammar.c"
    break;

  case 52:
#line 417 "codec/webvtt/CSSGrammar.y"
                           {
        vlc_css_selectors_Delete( (yyvsp[-1].selector) );
        (yyval.selector) = NULL;
    }
#line 2216 "codec/webvtt/CSSGrammar.c"
    break;

  case 53:
#line 424 "codec/webvtt/CSSGrammar.y"
          {
        if( !(yyvsp[0].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_ID, (yyvsp[0].string) );
        if( !(yyval.selector) )
            YYNOMEM; // $1 destructor called
        free( (yyvsp[0].string) );
    }
#line 2232 "codec/webvtt/CSSGrammar.c"
    break;

  case 54:
#line 436 "codec/webvtt/CSSGrammar.y"
         {
        if ( !(yyvsp[0].string) || ((yyvsp[0].string)[0] >= '0' && (yyvsp[0].string)[0] <= '9') )
        {
            (yyval.selector) = NULL;
            YYERROR; // $1 destructor called
        } else {
            (yyval.selector) = vlc_css_selector_New( SPECIFIER_ID, (yyvsp[0].string) );
            if( !(yyval.selector) )
                YYNOMEM;  // $1 destructor called
            free( (yyvsp[0].string) );
        }
    }
#line 2249 "codec/webvtt/CSSGrammar.c"
    break;

  case 58:
#line 454 "codec/webvtt/CSSGrammar.y"
              {
        if( !(yyvsp[0].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_CLASS, (yyvsp[0].string) );
        if( !(yyval.selector) )
            YYNOMEM; // $2 destructor called
        free( (yyvsp[0].string) );
    }
#line 2265 "codec/webvtt/CSSGrammar.c"
    break;

  case 59:
#line 468 "codec/webvtt/CSSGrammar.y"
                      {
        (yyval.string) = (yyvsp[-1].string);
    }
#line 2273 "codec/webvtt/CSSGrammar.c"
    break;

  case 60:
#line 474 "codec/webvtt/CSSGrammar.y"
                                  {
        if( !(yyvsp[-1].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_ATTRIB, (yyvsp[-1].string) );
        free( (yyvsp[-1].string) );
    }
#line 2287 "codec/webvtt/CSSGrammar.c"
    break;

  case 61:
#line 483 "codec/webvtt/CSSGrammar.y"
                                                                                  {
        if( !(yyvsp[-2].string) || !(yyvsp[-5].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_ATTRIB, (yyvsp[-5].string) );
        if( !(yyval.selector) )
            YYNOMEM;
        (yyval.selector)->match = (yyvsp[-4].integer);
        (yyval.selector)->p_matchsel = vlc_css_selector_New( SPECIFIER_ID, (yyvsp[-2].string) );
        if ( !(yyval.selector)->p_matchsel )
        {
            vlc_css_selectors_Delete( (yyval.selector) );
            YYNOMEM;
        }
        free( (yyvsp[-5].string) );
        free( (yyvsp[-2].string) );
    }
#line 2311 "codec/webvtt/CSSGrammar.c"
    break;

  case 62:
#line 505 "codec/webvtt/CSSGrammar.y"
        {
        (yyval.integer) = MATCH_EQUALS;
    }
#line 2319 "codec/webvtt/CSSGrammar.c"
    break;

  case 63:
#line 508 "codec/webvtt/CSSGrammar.y"
               {
        (yyval.integer) = MATCH_INCLUDES;
    }
#line 2327 "codec/webvtt/CSSGrammar.c"
    break;

  case 64:
#line 511 "codec/webvtt/CSSGrammar.y"
                {
        (yyval.integer) = MATCH_DASHMATCH;
    }
#line 2335 "codec/webvtt/CSSGrammar.c"
    break;

  case 65:
#line 514 "codec/webvtt/CSSGrammar.y"
                 {
        (yyval.integer) = MATCH_BEGINSWITH;
    }
#line 2343 "codec/webvtt/CSSGrammar.c"
    break;

  case 66:
#line 517 "codec/webvtt/CSSGrammar.y"
               {
        (yyval.integer) = MATCH_ENDSWITH;
    }
#line 2351 "codec/webvtt/CSSGrammar.c"
    break;

  case 67:
#line 520 "codec/webvtt/CSSGrammar.y"
               {
        (yyval.integer) = MATCH_CONTAINS;
    }
#line 2359 "codec/webvtt/CSSGrammar.c"
    break;

  case 70:
#line 531 "codec/webvtt/CSSGrammar.y"
              {
        if( !(yyvsp[0].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, (yyvsp[0].string) );
        if( !(yyval.selector) )
            YYNOMEM;
        free( (yyvsp[0].string) );
    }
#line 2375 "codec/webvtt/CSSGrammar.c"
    break;

  case 71:
#line 542 "codec/webvtt/CSSGrammar.y"
                    {
        if( !(yyvsp[0].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOELEMENT, (yyvsp[0].string) );
        if( !(yyval.selector) )
            YYNOMEM;
        free( (yyvsp[0].string) );
    }
#line 2391 "codec/webvtt/CSSGrammar.c"
    break;

  case 72:
#line 554 "codec/webvtt/CSSGrammar.y"
                                                                           {
        if( !(yyvsp[-5].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }

        if(*(yyvsp[-5].string) != 0)
            (yyvsp[-5].string)[strlen((yyvsp[-5].string)) - 1] = 0;
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, (yyvsp[-5].string) );
        if( !(yyval.selector) )
            YYNOMEM;
        (yyvsp[-2].term).val *= (yyvsp[-3].integer);

        free( (yyvsp[-5].string) );
        vlc_css_term_Clean( (yyvsp[-2].term) );
    }
#line 2413 "codec/webvtt/CSSGrammar.c"
    break;

  case 73:
#line 572 "codec/webvtt/CSSGrammar.y"
                                                            {
        if( !(yyvsp[-4].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }

        if(*(yyvsp[-4].string) != 0)
            (yyvsp[-4].string)[strlen((yyvsp[-4].string)) - 1] = 0;
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOELEMENT, (yyvsp[-4].string) );
        if( !(yyval.selector) )
            YYNOMEM;

        if( (yyvsp[-2].selector) )
        {
            vlc_css_selector_AddSpecifier( (yyval.selector), (yyvsp[-2].selector) );
            (yyvsp[-2].selector)->combinator = RELATION_SELF;
        }

        free( (yyvsp[-4].string) );
    }
#line 2439 "codec/webvtt/CSSGrammar.c"
    break;

  case 74:
#line 594 "codec/webvtt/CSSGrammar.y"
                                                     {
        if( !(yyvsp[-4].string) )
        {
            (yyval.selector) = NULL;
            YYERROR;
        }

        if(*(yyvsp[-4].string) != 0)
            (yyvsp[-4].string)[strlen((yyvsp[-4].string)) - 1] = 0;
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, (yyvsp[-4].string) );
        if( !(yyval.selector) )
            YYNOMEM;

        free( (yyvsp[-4].string) );
        free( (yyvsp[-2].string) );
    }
#line 2460 "codec/webvtt/CSSGrammar.c"
    break;

  case 75:
#line 613 "codec/webvtt/CSSGrammar.y"
                {
        (yyval.declarationList) = (yyvsp[0].declaration);
    }
#line 2468 "codec/webvtt/CSSGrammar.c"
    break;

  case 76:
#line 616 "codec/webvtt/CSSGrammar.y"
                            {
        if ((yyvsp[-1].declarationList)) {
            (yyval.declarationList) = (yyvsp[-1].declarationList);
            vlc_css_declarations_Append( (yyval.declarationList), (yyvsp[0].declaration) );
        } else {
            (yyval.declarationList) = (yyvsp[0].declaration);
        }
    }
#line 2481 "codec/webvtt/CSSGrammar.c"
    break;

  case 77:
#line 624 "codec/webvtt/CSSGrammar.y"
                {
        (yyval.declarationList) = (yyvsp[0].declarationList);
    }
#line 2489 "codec/webvtt/CSSGrammar.c"
    break;

  case 78:
#line 627 "codec/webvtt/CSSGrammar.y"
                                     {
        (yyval.declarationList) = NULL;
    }
#line 2497 "codec/webvtt/CSSGrammar.c"
    break;

  case 79:
#line 630 "codec/webvtt/CSSGrammar.y"
            {
        (yyval.declarationList) = NULL;
    }
#line 2505 "codec/webvtt/CSSGrammar.c"
    break;

  case 80:
#line 633 "codec/webvtt/CSSGrammar.y"
                      {
        (yyval.declarationList) = (yyvsp[-1].declarationList);
    }
#line 2513 "codec/webvtt/CSSGrammar.c"
    break;

  case 81:
#line 636 "codec/webvtt/CSSGrammar.y"
                                   {
        (yyval.declarationList) = (yyvsp[-1].declarationList);
    }
#line 2521 "codec/webvtt/CSSGrammar.c"
    break;

  case 82:
#line 642 "codec/webvtt/CSSGrammar.y"
                                {
        (yyval.declarationList) = (yyvsp[-2].declaration);
    }
#line 2529 "codec/webvtt/CSSGrammar.c"
    break;

  case 83:
#line 645 "codec/webvtt/CSSGrammar.y"
                                                 {
        vlc_css_declarations_Delete( (yyvsp[-2].declaration) );
        (yyval.declarationList) = NULL;
    }
#line 2538 "codec/webvtt/CSSGrammar.c"
    break;

  case 84:
#line 649 "codec/webvtt/CSSGrammar.y"
                                                     {
        vlc_css_declarations_Delete( (yyvsp[-3].declaration) );
        (yyval.declarationList) = NULL;
    }
#line 2547 "codec/webvtt/CSSGrammar.c"
    break;

  case 85:
#line 653 "codec/webvtt/CSSGrammar.y"
                            {
        (yyval.declarationList) = NULL;
    }
#line 2555 "codec/webvtt/CSSGrammar.c"
    break;

  case 86:
#line 656 "codec/webvtt/CSSGrammar.y"
                                                     {
        (yyval.declarationList) = NULL;
    }
#line 2563 "codec/webvtt/CSSGrammar.c"
    break;

  case 87:
#line 659 "codec/webvtt/CSSGrammar.y"
                                            {
        if( (yyvsp[-3].declarationList) )
        {
            (yyval.declarationList) = (yyvsp[-3].declarationList);
            vlc_css_declarations_Append( (yyval.declarationList), (yyvsp[-2].declaration) );
        }
        else (yyval.declarationList) = (yyvsp[-2].declaration);
    }
#line 2576 "codec/webvtt/CSSGrammar.c"
    break;

  case 88:
#line 667 "codec/webvtt/CSSGrammar.y"
                                      {
        (yyval.declarationList) = (yyvsp[-3].declarationList);
    }
#line 2584 "codec/webvtt/CSSGrammar.c"
    break;

  case 89:
#line 670 "codec/webvtt/CSSGrammar.y"
                                                               {
        (yyval.declarationList) = (yyvsp[-5].declarationList);
    }
#line 2592 "codec/webvtt/CSSGrammar.c"
    break;

  case 90:
#line 676 "codec/webvtt/CSSGrammar.y"
                                       {
        if( !(yyvsp[-4].string) || !(yyvsp[-1].expr) )
        {
            (yyval.declaration) = NULL;
            YYERROR;
        }
        (yyval.declaration) = vlc_css_declaration_New( (yyvsp[-4].string) );
        if( !(yyval.declaration) )
            YYNOMEM;
        (yyval.declaration)->expr = (yyvsp[-1].expr);
        free( (yyvsp[-4].string) );
    }
#line 2609 "codec/webvtt/CSSGrammar.c"
    break;

  case 91:
#line 688 "codec/webvtt/CSSGrammar.y"
                                          {
        /* e.g. color: red !important fail;  or color: red; garbage */
        free( (yyvsp[-4].string) );
        vlc_css_expression_Delete( (yyvsp[-1].expr) );
        (yyval.declaration) = NULL;
    }
#line 2620 "codec/webvtt/CSSGrammar.c"
    break;

  case 92:
#line 694 "codec/webvtt/CSSGrammar.y"
                                     {
        /* color: garbage */
        free( (yyvsp[-3].string) );
        (yyval.declaration) = NULL;
    }
#line 2630 "codec/webvtt/CSSGrammar.c"
    break;

  case 93:
#line 699 "codec/webvtt/CSSGrammar.y"
                     {
        /* color garbage */
        free( (yyvsp[-1].string) );
        (yyval.declaration) = NULL;
    }
#line 2640 "codec/webvtt/CSSGrammar.c"
    break;

  case 94:
#line 704 "codec/webvtt/CSSGrammar.y"
                               {
        /* color: ; */
        free( (yyvsp[-2].string) );
        (yyval.declaration) = NULL;
    }
#line 2650 "codec/webvtt/CSSGrammar.c"
    break;

  case 95:
#line 709 "codec/webvtt/CSSGrammar.y"
                             {
        free( (yyvsp[-1].string) );
        (yyval.declaration) = NULL;
    }
#line 2659 "codec/webvtt/CSSGrammar.c"
    break;

  case 96:
#line 713 "codec/webvtt/CSSGrammar.y"
                                {
        /* stray !important */
        (yyval.declaration) = NULL;
    }
#line 2668 "codec/webvtt/CSSGrammar.c"
    break;

  case 97:
#line 720 "codec/webvtt/CSSGrammar.y"
                      {
        (yyval.string) = (yyvsp[-1].string);
    }
#line 2676 "codec/webvtt/CSSGrammar.c"
    break;

  case 98:
#line 726 "codec/webvtt/CSSGrammar.y"
                              { (yyval.boolean) = true; }
#line 2682 "codec/webvtt/CSSGrammar.c"
    break;

  case 99:
#line 727 "codec/webvtt/CSSGrammar.y"
                  { (yyval.boolean) = false; }
#line 2688 "codec/webvtt/CSSGrammar.c"
    break;

  case 100:
#line 731 "codec/webvtt/CSSGrammar.y"
         {
        (yyval.expr) = vlc_css_expression_New( (yyvsp[0].term) );
        if( !(yyval.expr) )
            YYNOMEM;
    }
#line 2698 "codec/webvtt/CSSGrammar.c"
    break;

  case 101:
#line 736 "codec/webvtt/CSSGrammar.y"
                {
        if( !(yyvsp[-1].expr) )
        {
            (yyval.expr) = NULL;
            YYERROR;
        }
        (yyval.expr) = (yyvsp[-1].expr);
        if( !vlc_css_expression_AddTerm( (yyvsp[-1].expr), ' ', (yyvsp[0].term) ) )
            YYNOMEM;
    }
#line 2713 "codec/webvtt/CSSGrammar.c"
    break;

  case 102:
#line 746 "codec/webvtt/CSSGrammar.y"
                         {
        if( !(yyvsp[-2].expr) )
        {
            (yyval.expr) = NULL;
            YYERROR;
        }
        (yyval.expr) = (yyvsp[-2].expr);
        if( !vlc_css_expression_AddTerm( (yyvsp[-2].expr), (yyvsp[-1].character), (yyvsp[0].term) ) )
            YYNOMEM;
    }
#line 2728 "codec/webvtt/CSSGrammar.c"
    break;

  case 103:
#line 759 "codec/webvtt/CSSGrammar.y"
                      {
          (yyval.character) = '/';
      }
#line 2736 "codec/webvtt/CSSGrammar.c"
    break;

  case 104:
#line 762 "codec/webvtt/CSSGrammar.y"
                      {
          (yyval.character) = ',';
      }
#line 2744 "codec/webvtt/CSSGrammar.c"
    break;

  case 105:
#line 768 "codec/webvtt/CSSGrammar.y"
             { (yyval.term) = (yyvsp[0].term); }
#line 2750 "codec/webvtt/CSSGrammar.c"
    break;

  case 106:
#line 769 "codec/webvtt/CSSGrammar.y"
                              {
      (yyval.term) = (yyvsp[0].term);
      (yyval.term).val *= (yyvsp[-1].integer);
  }
#line 2759 "codec/webvtt/CSSGrammar.c"
    break;

  case 107:
#line 773 "codec/webvtt/CSSGrammar.y"
                       { (yyval.term).type = TYPE_STRING; (yyval.term).psz = (yyvsp[-1].string); }
#line 2765 "codec/webvtt/CSSGrammar.c"
    break;

  case 108:
#line 774 "codec/webvtt/CSSGrammar.y"
                      { (yyval.term).type = TYPE_IDENTIFIER; (yyval.term).psz = (yyvsp[-1].string); }
#line 2771 "codec/webvtt/CSSGrammar.c"
    break;

  case 109:
#line 776 "codec/webvtt/CSSGrammar.y"
                      { (yyval.term) = (yyvsp[-1].term); }
#line 2777 "codec/webvtt/CSSGrammar.c"
    break;

  case 110:
#line 777 "codec/webvtt/CSSGrammar.y"
                                     { (yyval.term) = (yyvsp[-1].term); }
#line 2783 "codec/webvtt/CSSGrammar.c"
    break;

  case 111:
#line 778 "codec/webvtt/CSSGrammar.y"
                    { (yyval.term).type = TYPE_URI; (yyval.term).psz = (yyvsp[-1].string); }
#line 2789 "codec/webvtt/CSSGrammar.c"
    break;

  case 112:
#line 779 "codec/webvtt/CSSGrammar.y"
                             { (yyval.term).type = TYPE_UNICODERANGE; (yyval.term).psz = (yyvsp[-1].string); }
#line 2795 "codec/webvtt/CSSGrammar.c"
    break;

  case 113:
#line 780 "codec/webvtt/CSSGrammar.y"
                      { (yyval.term).type = TYPE_HEXCOLOR; (yyval.term).psz = (yyvsp[-1].string); }
#line 2801 "codec/webvtt/CSSGrammar.c"
    break;

  case 114:
#line 781 "codec/webvtt/CSSGrammar.y"
                     { (yyval.term).type = TYPE_HEXCOLOR; (yyval.term).psz = (yyvsp[-1].string); }
#line 2807 "codec/webvtt/CSSGrammar.c"
    break;

  case 115:
#line 782 "codec/webvtt/CSSGrammar.y"
                    { (yyval.term).type = TYPE_INVALID; }
#line 2813 "codec/webvtt/CSSGrammar.c"
    break;

  case 116:
#line 784 "codec/webvtt/CSSGrammar.y"
             {
      (yyval.term) = (yyvsp[0].term);
  }
#line 2821 "codec/webvtt/CSSGrammar.c"
    break;

  case 117:
#line 787 "codec/webvtt/CSSGrammar.y"
                    { /* Handle width: %; */
      (yyval.term).type = TYPE_PERCENT; (yyval.term).val = 0;
  }
#line 2829 "codec/webvtt/CSSGrammar.c"
    break;

  case 124:
#line 802 "codec/webvtt/CSSGrammar.y"
                                              {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = (yyvsp[-2].expr);
        (yyval.term).psz = (yyvsp[-4].string);
        if((yyvsp[-4].string) && *(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
    }
#line 2840 "codec/webvtt/CSSGrammar.c"
    break;

  case 125:
#line 808 "codec/webvtt/CSSGrammar.y"
                                        {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = (yyvsp[-1].expr); (yyval.term).psz = (yyvsp[-3].string);
        if((yyvsp[-3].string) && *(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
    }
#line 2850 "codec/webvtt/CSSGrammar.c"
    break;

  case 126:
#line 813 "codec/webvtt/CSSGrammar.y"
                                         {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = NULL; (yyval.term).psz = (yyvsp[-3].string);
        if((yyvsp[-3].string) && *(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
    }
#line 2860 "codec/webvtt/CSSGrammar.c"
    break;

  case 127:
#line 818 "codec/webvtt/CSSGrammar.y"
                               {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = NULL; (yyval.term).psz = (yyvsp[-2].string);
        if((yyvsp[-2].string) && *(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
  }
#line 2870 "codec/webvtt/CSSGrammar.c"
    break;

  case 128:
#line 828 "codec/webvtt/CSSGrammar.y"
                        {
        (yyval.rule) = NULL;
    }
#line 2878 "codec/webvtt/CSSGrammar.c"
    break;


#line 2882 "codec/webvtt/CSSGrammar.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (scanner, css_parser, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *, YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (scanner, css_parser, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, scanner, css_parser);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, scanner, css_parser);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (scanner, css_parser, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, scanner, css_parser);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[+*yyssp], yyvsp, scanner, css_parser);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 855 "codec/webvtt/CSSGrammar.y"


#ifdef YYDEBUG
    int yydebug=1;
#else
    int yydebug=0;
#endif
