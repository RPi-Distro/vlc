/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison interface for Yacc-like parsers in C

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

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

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

#line 145 "codec/webvtt/CSSGrammar.h"

};
typedef union CSSSTYPE CSSSTYPE;
# define CSSSTYPE_IS_TRIVIAL 1
# define CSSSTYPE_IS_DECLARED 1
#endif



int cssparse (yyscan_t scanner, vlc_css_parser_t *css_parser);

#endif /* !YY_CSS_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED  */
