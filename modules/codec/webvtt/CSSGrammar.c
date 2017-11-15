/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 36 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:339  */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include "css_parser.h"

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#line 79 "codec/webvtt/CSSGrammar.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED
# define YY_YY_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    TOKEN_EOF = 0,
    LOWEST_PREC = 258,
    UNIMPORTANT_TOK = 259,
    WHITESPACE = 260,
    SGML_CD = 261,
    INCLUDES = 262,
    DASHMATCH = 263,
    BEGINSWITH = 264,
    ENDSWITH = 265,
    CONTAINS = 266,
    STRING = 267,
    IDENT = 268,
    IDSEL = 269,
    HASH = 270,
    FONT_FACE_SYM = 271,
    CHARSET_SYM = 272,
    IMPORTANT_SYM = 273,
    CDO = 274,
    CDC = 275,
    LENGTH = 276,
    ANGLE = 277,
    TIME = 278,
    FREQ = 279,
    DIMEN = 280,
    PERCENTAGE = 281,
    NUMBER = 282,
    URI = 283,
    FUNCTION = 284,
    UNICODERANGE = 285
  };
#endif
/* Tokens.  */
#define TOKEN_EOF 0
#define LOWEST_PREC 258
#define UNIMPORTANT_TOK 259
#define WHITESPACE 260
#define SGML_CD 261
#define INCLUDES 262
#define DASHMATCH 263
#define BEGINSWITH 264
#define ENDSWITH 265
#define CONTAINS 266
#define STRING 267
#define IDENT 268
#define IDSEL 269
#define HASH 270
#define FONT_FACE_SYM 271
#define CHARSET_SYM 272
#define IMPORTANT_SYM 273
#define CDO 274
#define CDC 275
#define LENGTH 276
#define ANGLE 277
#define TIME 278
#define FREQ 279
#define DIMEN 280
#define PERCENTAGE 281
#define NUMBER 282
#define URI 283
#define FUNCTION 284
#define UNICODERANGE 285

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 49 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:355  */

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

#line 197 "codec/webvtt/CSSGrammar.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (yyscan_t scanner, vlc_css_parser_t *css_parser);

#endif /* !YY_YY_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED  */

/* Copy the second part of user declarations.  */
#line 65 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:358  */

/* See bison pure calling */
int yylex(union YYSTYPE *, yyscan_t, vlc_css_parser_t *);

static int yyerror(yyscan_t scanner, vlc_css_parser_t *p, const char *msg)
{
    VLC_UNUSED(scanner);VLC_UNUSED(p);VLC_UNUSED(msg);
    return 1;
}


#line 224 "codec/webvtt/CSSGrammar.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

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

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
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
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
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
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   473

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  50
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  42
/* YYNRULES -- Number of rules.  */
#define YYNRULES  135
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  230

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   285

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    48,     2,    49,     2,     2,
       2,    46,    19,    39,    43,    42,    17,    47,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    16,    37,
       2,    45,    41,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    18,     2,    44,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    38,    20,    36,    40,     2,     2,     2,
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
      15,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   176,   176,   180,   181,   184,   186,   187,   190,   192,
     198,   199,   203,   207,   210,   216,   221,   226,   228,   235,
     236,   240,   245,   246,   250,   255,   258,   264,   265,   266,
     270,   271,   275,   276,   280,   291,   296,   309,   316,   322,
     325,   329,   339,   348,   355,   359,   371,   377,   378,   382,
     385,   395,   402,   407,   415,   416,   417,   421,   428,   434,
     438,   451,   454,   457,   460,   463,   466,   472,   473,   477,
     481,   486,   495,   509,   519,   522,   527,   530,   533,   536,
     539,   545,   548,   552,   556,   559,   562,   570,   573,   579,
     588,   593,   602,   609,   614,   620,   626,   634,   640,   641,
     645,   650,   655,   659,   663,   670,   673,   676,   682,   683,
     687,   688,   690,   691,   692,   693,   694,   695,   696,   698,
     701,   707,   708,   709,   710,   711,   712,   716,   722,   727,
     732,   742,   760,   761,   765,   766
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "TOKEN_EOF", "error", "$undefined", "LOWEST_PREC", "UNIMPORTANT_TOK",
  "WHITESPACE", "SGML_CD", "INCLUDES", "DASHMATCH", "BEGINSWITH",
  "ENDSWITH", "CONTAINS", "STRING", "IDENT", "IDSEL", "HASH", "':'", "'.'",
  "'['", "'*'", "'|'", "FONT_FACE_SYM", "CHARSET_SYM", "IMPORTANT_SYM",
  "CDO", "CDC", "LENGTH", "ANGLE", "TIME", "FREQ", "DIMEN", "PERCENTAGE",
  "NUMBER", "URI", "FUNCTION", "UNICODERANGE", "'}'", "';'", "'{'", "'+'",
  "'~'", "'>'", "'-'", "','", "']'", "'='", "')'", "'/'", "'#'", "'%'",
  "$accept", "stylesheet", "maybe_space", "maybe_sgml", "maybe_charset",
  "closing_brace", "charset", "ignored_charset", "rule_list", "valid_rule",
  "rule", "font_face", "combinator", "maybe_unary_operator",
  "unary_operator", "ruleset", "selector_list",
  "selector_with_trailing_whitespace", "selector", "simple_selector",
  "element_name", "specifier_list", "specifier", "class", "attr_name",
  "attrib", "match", "ident_or_string", "pseudo", "declaration_list",
  "decl_list", "declaration", "property", "prio", "expr", "operator",
  "term", "unary_term", "function", "invalid_rule", "invalid_block",
  "invalid_block_list", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,    58,    46,    91,    42,
     124,   271,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   125,    59,   123,    43,
     126,    62,    45,    44,    93,    61,    41,    47,    35,    37
};
# endif

#define YYPACT_NINF -126

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-126)))

#define YYTABLE_NINF -108

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -126,    10,    88,  -126,  -126,   290,  -126,  -126,    68,     6,
     135,  -126,    11,  -126,  -126,  -126,  -126,   328,    73,    75,
     -24,  -126,  -126,  -126,   368,     9,  -126,  -126,    95,  -126,
    -126,  -126,  -126,  -126,  -126,     1,   420,    84,  -126,   292,
      14,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
      35,  -126,  -126,  -126,    -7,  -126,  -126,     3,   117,    30,
     171,   135,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
    -126,   420,   179,  -126,  -126,    73,  -126,  -126,    79,  -126,
     141,  -126,  -126,  -126,  -126,  -126,   186,   248,    70,    70,
      70,  -126,  -126,  -126,   248,  -126,  -126,  -126,    54,  -126,
      70,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,   186,
     121,   119,  -126,  -126,    41,   388,   169,    24,    96,    42,
      -1,  -126,   291,    41,  -126,  -126,    90,    70,    70,  -126,
     173,    58,    66,  -126,   404,  -126,  -126,  -126,     0,  -126,
       2,  -126,  -126,  -126,  -126,    70,   204,  -126,   114,  -126,
     -24,    70,  -126,    70,   285,  -126,  -126,    12,  -126,    70,
     214,    70,    70,   417,  -126,  -126,  -126,  -126,  -126,  -126,
    -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
     441,   245,  -126,  -126,  -126,  -126,    70,  -126,   245,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      70,   364,    70,    70,    70,  -126,  -126,  -126,  -126,  -126,
    -126,    33,   417,   201,    70,  -126,  -126,  -126,   325,    70,
      70,    70,    70,  -126,  -126,   -24,    70,  -126,  -126,    70
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     8,     1,     4,     0,     5,     9,     0,     0,
      17,    14,     0,    13,     3,     7,     6,     0,     0,     0,
       0,    47,    52,    53,     0,     0,     3,    48,     0,     3,
      22,    21,     5,    20,    19,     0,    40,     0,    39,    44,
       0,    49,    54,    55,    56,    23,    11,    10,   133,   134,
       0,    12,   131,    69,     0,     3,    57,     0,     0,     0,
       0,    18,    37,     3,     3,    41,    43,    38,     3,     3,
       3,     0,     0,    51,    50,     0,    70,     3,    31,     3,
       0,    26,    25,     3,     3,    16,     0,     0,    27,    28,
      29,    42,   132,   135,     0,     3,    33,    32,     0,    30,
      58,    62,    63,    64,    65,    66,    59,    61,     3,     0,
       0,    78,     3,     3,     0,     0,    74,     0,     0,     0,
       0,     3,     0,     0,    15,     3,     0,    97,    93,    34,
      79,    75,     0,     3,     0,    90,     3,    96,     0,    73,
       0,    68,    67,     3,    24,    84,    77,     3,     0,     3,
       0,    81,     3,    82,     0,    72,    71,     0,     3,    87,
       0,    86,    83,    95,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       0,     0,   100,   108,   119,    60,    85,     3,     0,   110,
     111,   116,   117,   123,   124,   125,   126,   112,   122,   121,
     114,     0,   115,   118,   120,     3,   109,   104,     3,     3,
       3,     0,     0,     0,    88,    91,   130,     3,     0,   113,
      98,   106,   105,    92,   101,   103,   129,   128,     3,   127
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -126,  -126,    -5,    85,  -126,   -72,  -126,  -126,  -126,  -126,
    -126,  -126,  -126,  -126,    52,  -126,  -126,  -126,   -74,   -10,
    -126,    80,   105,  -126,  -126,  -126,  -126,  -126,  -126,    23,
    -126,    28,  -126,     4,  -125,  -126,   -34,     8,  -126,  -126,
      29,   -17
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    10,     6,    48,     7,    30,    17,    31,
      32,    33,    71,    98,   180,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    80,    43,   108,   143,    44,   114,
     115,   116,   117,   211,   181,   212,   182,   183,   184,    45,
      49,   213
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
       9,    50,    62,    92,     4,     4,    76,     4,     4,    19,
       3,     4,    18,   118,    12,    73,    79,     4,    14,   -46,
     119,    57,    56,    59,    60,   135,    65,    77,    22,    23,
      24,    25,    26,   -89,   223,     4,    75,    13,   188,    63,
     136,    46,   129,    66,    64,   139,   155,    67,   156,    52,
      78,   144,   -46,   -46,   -46,   -46,   185,   -46,    86,    87,
     -46,    91,    12,    88,    89,    90,   -80,   150,    83,   -89,
     -89,   -89,    94,    46,   100,     4,   218,    47,   109,   110,
       4,    68,    69,    70,     4,    66,   121,    82,    -3,    67,
     120,   146,    95,     4,   126,   149,    58,    66,   132,   134,
      -3,    67,   -80,   122,    93,    11,    12,   127,   128,    47,
       5,    12,    51,   148,   138,   160,   140,    61,    96,    72,
     145,    97,   -35,    68,    69,    70,     4,   -35,   151,   153,
      99,   154,   123,    -3,   -36,    68,    69,    70,   157,   -36,
      15,    16,   159,   131,   161,    74,   137,   162,   101,   102,
     103,   104,   105,   186,    81,    12,   125,    12,   124,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,    93,     4,    74,   224,    93,
      73,     0,   214,    84,   -45,   106,   107,   111,   206,    93,
       0,     4,   215,    22,    23,    24,    25,    26,     0,   112,
     219,  -102,   225,   220,   221,   222,   133,    12,    85,   113,
     147,    12,   226,  -102,  -102,  -102,  -102,   -45,   -45,   -45,
     -45,     0,   -45,   229,  -102,   -45,     0,  -102,  -102,  -102,
    -102,  -102,  -102,  -102,  -102,  -102,  -102,  -102,  -102,  -102,
    -102,   158,    12,  -102,  -102,   -99,   207,  -102,  -102,  -102,
    -102,   187,    12,     4,    93,     0,     0,  -107,  -107,  -107,
    -107,    21,    22,    23,    24,    25,    26,    27,   208,     0,
       0,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
    -107,   -99,   -99,    12,  -107,   -94,   163,  -107,   209,     0,
       4,     8,   210,  -107,  -107,    -3,     4,   164,   165,   166,
     167,     0,    -3,   141,   142,     0,    22,    23,    24,    25,
      26,   168,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   -94,   -94,   -94,    96,   227,   207,    97,    -2,    20,
       0,     0,     0,   178,   179,     0,     0,  -107,  -107,  -107,
    -107,    21,    22,    23,    24,    25,    26,    27,     0,    28,
      29,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
    -107,     0,     0,    12,  -107,   216,     0,  -107,   209,     4,
       0,   228,   210,  -107,  -107,     0,   164,   165,   166,   167,
       0,    53,     0,     0,    54,     0,     0,     0,   -76,   130,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
       0,   112,    55,    96,    -3,   150,    97,     0,     0,    -3,
     217,   113,   178,   179,     0,     0,     0,    -3,     0,     0,
       0,     0,     0,     0,   -76,     0,    12,    -3,     0,   164,
     165,   166,   167,    21,    22,    23,    24,    25,    26,    27,
      -3,   152,    -3,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,     0,     0,     0,    96,     0,     0,    97,
       0,     0,     0,     0,     0,   178,   179,   168,   169,   170,
     171,   205,   173,   174
};

static const yytype_int16 yycheck[] =
{
       5,    18,     1,    75,     5,     5,    13,     5,     5,    14,
       0,     5,     1,    87,    38,     1,    13,     5,    12,     5,
      94,    26,    13,    28,    29,     1,    36,    34,    14,    15,
      16,    17,    18,     0,     1,     5,     1,     8,   163,    38,
      16,     0,   114,     1,    43,    46,    46,     5,    46,    20,
      55,   123,    38,    39,    40,    41,    44,    43,    63,    64,
      46,    71,    38,    68,    69,    70,     0,     1,    38,    36,
      37,    38,    77,     0,    79,     5,   201,    36,    83,    84,
       5,    39,    40,    41,     5,     1,    32,    58,    46,     5,
      95,     1,    13,     5,   111,    37,     1,     1,   115,   116,
       5,     5,    36,   108,    75,    37,    38,   112,   113,    36,
      22,    38,    37,   130,   119,     1,   121,    32,    39,    39,
     125,    42,    38,    39,    40,    41,     5,    43,   133,   134,
      78,   136,   109,    38,    38,    39,    40,    41,   143,    43,
       5,     6,   147,   115,   149,    40,   117,   152,     7,     8,
       9,    10,    11,   158,    37,    38,    37,    38,    37,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   146,     5,    72,   212,   150,
       1,    -1,   187,    12,     5,    44,    45,     1,   180,   160,
      -1,     5,   188,    14,    15,    16,    17,    18,    -1,    13,
     205,     0,     1,   208,   209,   210,    37,    38,    37,    23,
      37,    38,   217,    12,    13,    14,    15,    38,    39,    40,
      41,    -1,    43,   228,    23,    46,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    37,    38,    42,    43,     0,     1,    46,    47,    48,
      49,    37,    38,     5,   225,    -1,    -1,    12,    13,    14,
      15,    13,    14,    15,    16,    17,    18,    19,    23,    -1,
      -1,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,     0,     1,    42,    43,    -1,
       5,     1,    47,    48,    49,     5,     5,    12,    13,    14,
      15,    -1,    12,    12,    13,    -1,    14,    15,    16,    17,
      18,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,     0,     1,    42,     0,     1,
      -1,    -1,    -1,    48,    49,    -1,    -1,    12,    13,    14,
      15,    13,    14,    15,    16,    17,    18,    19,    -1,    21,
      22,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    -1,    -1,    38,    39,     1,    -1,    42,    43,     5,
      -1,    46,    47,    48,    49,    -1,    12,    13,    14,    15,
      -1,    13,    -1,    -1,    16,    -1,    -1,    -1,     0,     1,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      -1,    13,    34,    39,     0,     1,    42,    -1,    -1,     5,
      46,    23,    48,    49,    -1,    -1,    -1,    13,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    -1,    38,    23,    -1,    12,
      13,    14,    15,    13,    14,    15,    16,    17,    18,    19,
      36,    37,    38,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    39,    -1,    -1,    42,
      -1,    -1,    -1,    -1,    -1,    48,    49,    26,    27,    28,
      29,    30,    31,    32
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    51,    52,     0,     5,    22,    54,    56,     1,    52,
      53,    37,    38,    90,    12,     5,     6,    58,     1,    52,
       1,    13,    14,    15,    16,    17,    18,    19,    21,    22,
      57,    59,    60,    61,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    75,    78,    89,     0,    36,    55,    90,
      91,    37,    90,    13,    16,    34,    13,    52,     1,    52,
      52,    53,     1,    38,    43,    69,     1,     5,    39,    40,
      41,    62,    71,     1,    72,     1,    13,    34,    52,    13,
      74,    37,    90,    38,    12,    37,    52,    52,    52,    52,
      52,    69,    55,    90,    52,    13,    39,    42,    63,    64,
      52,     7,     8,     9,    10,    11,    44,    45,    76,    52,
      52,     1,    13,    23,    79,    80,    81,    82,    68,    68,
      52,    32,    52,    79,    37,    37,    91,    52,    52,    55,
       1,    81,    91,    37,    91,     1,    16,    90,    52,    46,
      52,    12,    13,    77,    55,    52,     1,    37,    91,    37,
       1,    52,    37,    52,    52,    46,    46,    52,    37,    52,
       1,    52,    52,     1,    12,    13,    14,    15,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    48,    49,
      64,    84,    86,    87,    88,    44,    52,    37,    84,    52,
      52,    52,    52,    52,    52,    52,    52,    52,    52,    52,
      52,    52,    52,    52,    52,    30,    87,     1,    23,    43,
      47,    83,    85,    91,    52,    83,     1,    46,    84,    52,
      52,    52,    52,     1,    86,     1,    52,     0,    46,    52
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    50,    51,    52,    52,    53,    53,    53,    54,    54,
      55,    55,    56,    56,    56,    57,    57,    58,    58,    59,
      59,    60,    60,    60,    61,    61,    61,    62,    62,    62,
      63,    63,    64,    64,    65,    66,    66,    66,    67,    68,
      68,    68,    68,    68,    69,    69,    69,    70,    70,    71,
      71,    71,    72,    72,    72,    72,    72,    73,    74,    75,
      75,    76,    76,    76,    76,    76,    76,    77,    77,    78,
      78,    78,    78,    78,    79,    79,    79,    79,    79,    79,
      79,    80,    80,    80,    80,    80,    80,    80,    80,    81,
      81,    81,    81,    81,    81,    81,    81,    82,    83,    83,
      84,    84,    84,    84,    84,    85,    85,    85,    86,    86,
      86,    86,    86,    86,    86,    86,    86,    86,    86,    86,
      86,    87,    87,    87,    87,    87,    87,    88,    88,    88,
      88,    89,    90,    90,    91,    91
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     4,     0,     2,     0,     2,     2,     0,     1,
       1,     1,     5,     3,     3,     5,     3,     0,     3,     1,
       1,     1,     1,     1,     6,     3,     3,     2,     2,     2,
       1,     0,     1,     1,     5,     1,     4,     2,     2,     1,
       1,     2,     3,     2,     1,     2,     1,     1,     1,     1,
       2,     2,     1,     1,     1,     1,     1,     2,     2,     4,
       8,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       3,     7,     7,     6,     1,     2,     1,     3,     1,     2,
       2,     3,     3,     4,     3,     5,     4,     4,     6,     5,
       2,     6,     6,     2,     3,     4,     2,     2,     2,     0,
       1,     3,     2,     3,     2,     2,     2,     0,     1,     2,
       2,     2,     2,     3,     2,     2,     2,     2,     2,     1,
       2,     2,     2,     2,     2,     2,     2,     5,     4,     4,
       3,     2,     5,     3,     1,     3
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
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
#if YYDEBUG

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


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t scanner, vlc_css_parser_t *css_parser)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (scanner);
  YYUSE (css_parser);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t scanner, vlc_css_parser_t *css_parser)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, scanner, css_parser);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, yyscan_t scanner, vlc_css_parser_t *css_parser)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
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
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


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
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
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
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
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
            /* Fall through.  */
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

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
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
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

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
      int yyn = yypact[*yyssp];
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
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
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
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
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
          yyp++;
          yyformat++;
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
          case 12: /* STRING  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1269 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 13: /* IDENT  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1275 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 14: /* IDSEL  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1281 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 15: /* HASH  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1287 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 26: /* LENGTH  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1293 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 27: /* ANGLE  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1299 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 28: /* TIME  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1305 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 29: /* FREQ  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1311 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 30: /* DIMEN  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1317 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 31: /* PERCENTAGE  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1323 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 32: /* NUMBER  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1329 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 33: /* URI  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1335 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 34: /* FUNCTION  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1341 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 35: /* UNICODERANGE  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1347 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 56: /* charset  */
#line 134 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1353 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 57: /* ignored_charset  */
#line 134 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1359 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 59: /* valid_rule  */
#line 134 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1365 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 60: /* rule  */
#line 134 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1371 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 61: /* font_face  */
#line 134 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1377 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 65: /* ruleset  */
#line 134 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1383 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 66: /* selector_list  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selectorList)); }
#line 1389 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 67: /* selector_with_trailing_whitespace  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1395 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 68: /* selector  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1401 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 69: /* simple_selector  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1407 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 70: /* element_name  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1413 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 71: /* specifier_list  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1419 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 72: /* specifier  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1425 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 73: /* class  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1431 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 74: /* attr_name  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1437 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 75: /* attrib  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1443 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 77: /* ident_or_string  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1449 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 78: /* pseudo  */
#line 148 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_selectors_Delete(((*yyvaluep).selector)); }
#line 1455 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 79: /* declaration_list  */
#line 153 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_declarations_Delete(((*yyvaluep).declarationList)); }
#line 1461 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 80: /* decl_list  */
#line 153 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_declarations_Delete(((*yyvaluep).declarationList)); }
#line 1467 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 81: /* declaration  */
#line 153 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_declarations_Delete(((*yyvaluep).declaration)); }
#line 1473 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 82: /* property  */
#line 171 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { free(((*yyvaluep).string)); }
#line 1479 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 84: /* expr  */
#line 166 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_expression_Delete(((*yyvaluep).expr)); }
#line 1485 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 86: /* term  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1491 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 87: /* unary_term  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1497 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 88: /* function  */
#line 119 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_term_Clean(((*yyvaluep).term)); }
#line 1503 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
        break;

    case 89: /* invalid_rule  */
#line 134 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1257  */
      { vlc_css_rules_Delete(((*yyvaluep).rule)); }
#line 1509 "codec/webvtt/CSSGrammar.c" /* yacc.c:1257  */
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

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

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
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
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
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

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
      yychar = yylex (&yylval, scanner, css_parser);
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

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
| yyreduce -- Do a reduction.  |
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
        case 9:
#line 192 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
    vlc_css_rules_Delete((yyvsp[0].rule));
  }
#line 1779 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 12:
#line 203 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
      free( (yyvsp[-2].string) );
      (yyval.rule) = 0;
  }
#line 1788 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 13:
#line 207 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
      (yyval.rule) = 0;
  }
#line 1796 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 14:
#line 210 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
      (yyval.rule) = 0;
  }
#line 1804 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 15:
#line 216 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        // Ignore any @charset rule not at the beginning of the style sheet.
        free( (yyvsp[-2].string) );
        (yyval.rule) = 0;
    }
#line 1814 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 16:
#line 221 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.rule) = 0;
    }
#line 1822 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 18:
#line 228 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
     if( (yyvsp[-1].rule) )
         vlc_css_parser_AddRule( css_parser, (yyvsp[-1].rule) );
 }
#line 1831 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 21:
#line 240 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.rule) = (yyvsp[0].rule);
        if((yyval.rule))
            (yyval.rule)->b_valid = true;
    }
#line 1841 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 24:
#line 251 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_declarations_Delete( (yyvsp[-1].declarationList) );
        (yyval.rule) = NULL;
    }
#line 1850 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 25:
#line 255 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.rule) = NULL;
    }
#line 1858 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 26:
#line 258 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.rule) = NULL;
    }
#line 1866 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 27:
#line 264 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.relation) = RELATION_DIRECTADJACENT; }
#line 1872 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 28:
#line 265 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.relation) = RELATION_INDIRECTADJACENT; }
#line 1878 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 29:
#line 266 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.relation) = RELATION_CHILD; }
#line 1884 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 30:
#line 270 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.integer) = (yyvsp[0].integer); }
#line 1890 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 31:
#line 271 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.integer) = 1; }
#line 1896 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 32:
#line 275 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.integer) = -1; }
#line 1902 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 33:
#line 276 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.integer) = 1; }
#line 1908 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 34:
#line 280 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.rule) = vlc_css_rule_New();
        if((yyval.rule))
        {
            (yyval.rule)->p_selectors = (yyvsp[-4].selectorList);
            (yyval.rule)->p_declarations = (yyvsp[-1].declarationList);
        }
    }
#line 1921 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 35:
#line 291 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        if ((yyvsp[0].selector)) {
            (yyval.selectorList) = (yyvsp[0].selector);
        }
    }
#line 1931 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 36:
#line 296 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
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
#line 1949 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 37:
#line 309 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_selectors_Delete( (yyvsp[-1].selectorList) );
        (yyval.selectorList) = NULL;
    }
#line 1958 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 38:
#line 316 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = (yyvsp[-1].selector);
    }
#line 1966 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 39:
#line 322 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 1974 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 40:
#line 326 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 1982 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 41:
#line 330 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = (yyvsp[-1].selector);
        if ((yyval.selector))
        {
            vlc_css_selector_AddSpecifier( (yyval.selector), (yyvsp[0].selector) );
            (yyvsp[0].selector)->combinator = RELATION_DESCENDENT;
        }
        else (yyval.selector) = (yyvsp[0].selector);
    }
#line 1996 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 42:
#line 339 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = (yyvsp[-2].selector);
        if ((yyval.selector))
        {
            vlc_css_selector_AddSpecifier( (yyval.selector), (yyvsp[0].selector) );
            (yyvsp[0].selector)->combinator = (yyvsp[-1].relation);
        }
        else (yyval.selector) = (yyvsp[0].selector);
    }
#line 2010 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 43:
#line 348 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_selectors_Delete( (yyvsp[-1].selector) );
        (yyval.selector) = NULL;
    }
#line 2019 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 44:
#line 355 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SELECTOR_SIMPLE, (yyvsp[0].string) );
        free( (yyvsp[0].string) );
    }
#line 2028 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 45:
#line 359 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SELECTOR_SIMPLE, (yyvsp[-1].string) );
        if( (yyval.selector) && (yyvsp[0].selector) )
        {
            vlc_css_selector_AddSpecifier( (yyval.selector), (yyvsp[0].selector) );
        }
        else
        {
            vlc_css_selectors_Delete( (yyvsp[0].selector) );
        }
        free( (yyvsp[-1].string) );
    }
#line 2045 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 46:
#line 371 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 2053 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 48:
#line 378 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.string) = strdup("*"); }
#line 2059 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 49:
#line 382 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = (yyvsp[0].selector);
    }
#line 2067 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 50:
#line 385 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        if( (yyvsp[-1].selector) )
        {
            (yyval.selector) = (yyvsp[-1].selector);
            while( (yyvsp[-1].selector)->specifiers.p_first )
                (yyvsp[-1].selector) = (yyvsp[-1].selector)->specifiers.p_first;
            vlc_css_selector_AddSpecifier( (yyvsp[-1].selector), (yyvsp[0].selector) );
        }
        else (yyval.selector) = (yyvsp[0].selector);
    }
#line 2082 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 51:
#line 395 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_selectors_Delete( (yyvsp[-1].selector) );
        (yyval.selector) = NULL;
    }
#line 2091 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 52:
#line 402 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_ID, (yyvsp[0].string) );
        free( (yyvsp[0].string) );
    }
#line 2100 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 53:
#line 407 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        if ((yyvsp[0].string)[0] >= '0' && (yyvsp[0].string)[0] <= '9') {
            (yyval.selector) = NULL;
        } else {
            (yyval.selector) = vlc_css_selector_New( SPECIFIER_ID, (yyvsp[0].string) );
        }
        free( (yyvsp[0].string) );
    }
#line 2113 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 57:
#line 421 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_CLASS, (yyvsp[0].string) );
        free( (yyvsp[0].string) );
    }
#line 2122 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 58:
#line 428 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.string) = (yyvsp[-1].string);
    }
#line 2130 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 59:
#line 434 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_ATTRIB, (yyvsp[-1].string) );
        free( (yyvsp[-1].string) );
    }
#line 2139 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 60:
#line 438 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SPECIFIER_ATTRIB, (yyvsp[-5].string) );
        if( (yyval.selector) )
        {
            (yyval.selector)->match = (yyvsp[-4].integer);
            (yyval.selector)->p_matchsel = vlc_css_selector_New( SPECIFIER_ID, (yyvsp[-2].string) );
        }
        free( (yyvsp[-5].string) );
        free( (yyvsp[-2].string) );
    }
#line 2154 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 61:
#line 451 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.integer) = MATCH_EQUALS;
    }
#line 2162 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 62:
#line 454 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.integer) = MATCH_INCLUDES;
    }
#line 2170 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 63:
#line 457 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.integer) = MATCH_DASHMATCH;
    }
#line 2178 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 64:
#line 460 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.integer) = MATCH_BEGINSWITH;
    }
#line 2186 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 65:
#line 463 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.integer) = MATCH_ENDSWITH;
    }
#line 2194 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 66:
#line 466 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.integer) = MATCH_CONTAINS;
    }
#line 2202 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 69:
#line 477 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, (yyvsp[0].string) );
        free( (yyvsp[0].string) );
    }
#line 2211 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 70:
#line 481 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOELEMENT, (yyvsp[0].string) );
        free( (yyvsp[0].string) );
    }
#line 2220 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 71:
#line 486 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        if(*(yyvsp[-5].string) != 0)
            (yyvsp[-5].string)[strlen((yyvsp[-5].string)) - 1] = 0;
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, (yyvsp[-5].string) );
        (yyvsp[-2].term).val *= (yyvsp[-3].integer);
        free( (yyvsp[-5].string) );
        vlc_css_term_Clean( (yyvsp[-2].term) );
    }
#line 2233 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 72:
#line 495 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        if(*(yyvsp[-4].string) != 0)
            (yyvsp[-4].string)[strlen((yyvsp[-4].string)) - 1] = 0;
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOELEMENT, (yyvsp[-4].string) );
        free( (yyvsp[-4].string) );
        if( (yyval.selector) && (yyvsp[-2].selector) )
        {
            vlc_css_selector_AddSpecifier( (yyval.selector), (yyvsp[-2].selector) );
            (yyvsp[-2].selector)->combinator = RELATION_SELF;
        }
        else
            vlc_css_selectors_Delete( (yyvsp[-2].selector) );
    }
#line 2251 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 73:
#line 509 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        if(*(yyvsp[-4].string) != 0)
            (yyvsp[-4].string)[strlen((yyvsp[-4].string)) - 1] = 0;
        (yyval.selector) = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, (yyvsp[-4].string) );
        free( (yyvsp[-4].string) );
        free( (yyvsp[-2].string) );
    }
#line 2263 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 74:
#line 519 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[0].declaration);
    }
#line 2271 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 75:
#line 522 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[-1].declarationList);
        if( (yyval.declarationList) )
            vlc_css_declarations_Append( (yyval.declarationList), (yyvsp[0].declaration) );
    }
#line 2281 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 76:
#line 527 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[0].declarationList);
    }
#line 2289 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 77:
#line 530 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = NULL;
    }
#line 2297 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 78:
#line 533 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = NULL;
    }
#line 2305 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 79:
#line 536 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[-1].declarationList);
    }
#line 2313 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 80:
#line 539 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[-1].declarationList);
    }
#line 2321 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 81:
#line 545 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[-2].declaration);
    }
#line 2329 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 82:
#line 548 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_declarations_Delete( (yyvsp[-2].declaration) );
        (yyval.declarationList) = NULL;
    }
#line 2338 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 83:
#line 552 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_declarations_Delete( (yyvsp[-3].declaration) );
        (yyval.declarationList) = NULL;
    }
#line 2347 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 84:
#line 556 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = NULL;
    }
#line 2355 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 85:
#line 559 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = NULL;
    }
#line 2363 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 86:
#line 562 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        if( (yyvsp[-3].declarationList) )
        {
            (yyval.declarationList) = (yyvsp[-3].declarationList);
            vlc_css_declarations_Append( (yyval.declarationList), (yyvsp[-2].declaration) );
        }
        else (yyval.declarationList) = (yyvsp[-2].declaration);
    }
#line 2376 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 87:
#line 570 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[-3].declarationList);
    }
#line 2384 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 88:
#line 573 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declarationList) = (yyvsp[-5].declarationList);
    }
#line 2392 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 89:
#line 579 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.declaration) = vlc_css_declaration_New( (yyvsp[-4].string) );
        if( (yyval.declaration) )
            (yyval.declaration)->expr = (yyvsp[-1].expr);
        else
            vlc_css_expression_Delete( (yyvsp[-1].expr) );
        free( (yyvsp[-4].string) );
    }
#line 2405 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 90:
#line 588 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        free( (yyvsp[-1].string) );
        (yyval.declaration) = NULL;
    }
#line 2414 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 91:
#line 593 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        free( (yyvsp[-5].string) );
        vlc_css_expression_Delete( (yyvsp[-1].expr) );
        /* The default movable type template has letter-spacing: .none;  Handle this by looking for
        error tokens at the start of an expr, recover the expr and then treat as an error, cleaning
        up and deleting the shifted expr.  */
        (yyval.declaration) = NULL;
    }
#line 2427 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 92:
#line 602 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        free( (yyvsp[-5].string) );
        vlc_css_expression_Delete( (yyvsp[-2].expr) );
        /* When we encounter something like p {color: red !important fail;} we should drop the declaration */
        (yyval.declaration) = NULL;
    }
#line 2438 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 93:
#line 609 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        (yyval.declaration) = NULL;
    }
#line 2447 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 94:
#line 614 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        free( (yyvsp[-2].string) );
        /* div { font-family: } Just reduce away this property with no value. */
        (yyval.declaration) = NULL;
    }
#line 2457 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 95:
#line 620 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        free( (yyvsp[-3].string) );
        /* if we come across rules with invalid values like this case: p { weight: *; }, just discard the rule */
        (yyval.declaration) = NULL;
    }
#line 2467 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 96:
#line 626 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        /* if we come across: div { color{;color:maroon} }, ignore everything within curly brackets */
        free( (yyvsp[-1].string) );
        (yyval.declaration) = NULL;
    }
#line 2477 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 97:
#line 634 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.string) = (yyvsp[-1].string);
    }
#line 2485 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 98:
#line 640 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.boolean) = true; }
#line 2491 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 99:
#line 641 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.boolean) = false; }
#line 2497 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 100:
#line 645 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.expr) = vlc_css_expression_New( (yyvsp[0].term) );
        if( !(yyval.expr) )
            vlc_css_term_Clean( (yyvsp[0].term) );
    }
#line 2507 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 101:
#line 650 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.expr) = (yyvsp[-2].expr);
        if( !(yyvsp[-2].expr) || !vlc_css_expression_AddTerm((yyvsp[-2].expr), (yyvsp[-1].character), (yyvsp[0].term)) )
            vlc_css_term_Clean( (yyvsp[0].term) );
    }
#line 2517 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 102:
#line 655 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_expression_Delete( (yyvsp[-1].expr) );
        (yyval.expr) = NULL;
    }
#line 2526 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 103:
#line 659 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_expression_Delete( (yyvsp[-2].expr) );
        (yyval.expr) = NULL;
    }
#line 2535 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 104:
#line 663 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        vlc_css_expression_Delete( (yyvsp[-1].expr) );
        (yyval.expr) = NULL;
    }
#line 2544 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 105:
#line 670 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.character) = '/';
    }
#line 2552 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 106:
#line 673 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.character) = ',';
    }
#line 2560 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 107:
#line 676 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.character) = 0;
  }
#line 2568 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 108:
#line 682 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term) = (yyvsp[0].term); }
#line 2574 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 109:
#line 683 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
      (yyval.term) = (yyvsp[0].term);
      (yyval.term).val *= (yyvsp[-1].integer);
  }
#line 2583 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 110:
#line 687 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term).type = TYPE_STRING; (yyval.term).psz = (yyvsp[-1].string); }
#line 2589 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 111:
#line 688 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term).type = TYPE_IDENTIFIER; (yyval.term).psz = (yyvsp[-1].string); }
#line 2595 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 112:
#line 690 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term) = (yyvsp[-1].term); }
#line 2601 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 113:
#line 691 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term) = (yyvsp[-1].term); }
#line 2607 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 114:
#line 692 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term).type = TYPE_URI; (yyval.term).psz = (yyvsp[-1].string); }
#line 2613 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 115:
#line 693 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term).type = TYPE_UNICODERANGE; (yyval.term).psz = (yyvsp[-1].string); }
#line 2619 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 116:
#line 694 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term).type = TYPE_HEXCOLOR; (yyval.term).psz = (yyvsp[-1].string); }
#line 2625 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 117:
#line 695 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term).type = TYPE_HEXCOLOR; (yyval.term).psz = (yyvsp[-1].string); }
#line 2631 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 118:
#line 696 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { (yyval.term).type = TYPE_HEXCOLOR; (yyval.term).psz = NULL; }
#line 2637 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 119:
#line 698 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
      (yyval.term) = (yyvsp[0].term);
  }
#line 2645 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 120:
#line 701 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    { /* Handle width: %; */
      (yyval.term).type = TYPE_PERCENT; (yyval.term).val = 0;
  }
#line 2653 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 127:
#line 716 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = (yyvsp[-2].expr);
        (yyval.term).psz = (yyvsp[-4].string);
        if(*(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
    }
#line 2664 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 128:
#line 722 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = (yyvsp[-1].expr); (yyval.term).psz = (yyvsp[-3].string);
        if(*(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
    }
#line 2674 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 129:
#line 727 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = NULL; (yyval.term).psz = (yyvsp[-3].string);
        if(*(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
    }
#line 2684 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 130:
#line 732 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.term).type = TYPE_FUNCTION; (yyval.term).function = NULL; (yyval.term).psz = (yyvsp[-2].string);
        if(*(yyval.term).psz != 0)
            (yyval.term).psz[strlen((yyval.term).psz) - 1] = 0;
  }
#line 2694 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;

  case 131:
#line 742 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1646  */
    {
        (yyval.rule) = NULL;
    }
#line 2702 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
    break;


#line 2706 "codec/webvtt/CSSGrammar.c" /* yacc.c:1646  */
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

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

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
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
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

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

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
                  yystos[*yyssp], yyvsp, scanner, css_parser);
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
#line 769 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1906  */


#ifdef YYDEBUG
    int yydebug=1;
#else
    int yydebug=0;
#endif
