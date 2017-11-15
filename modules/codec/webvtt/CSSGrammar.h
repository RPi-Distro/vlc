/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

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
#line 49 "../../modules/codec/webvtt/CSSGrammar.y" /* yacc.c:1909  */

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

#line 132 "codec/webvtt/CSSGrammar.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (yyscan_t scanner, vlc_css_parser_t *css_parser);

#endif /* !YY_YY_CODEC_WEBVTT_CSSGRAMMAR_H_INCLUDED  */
