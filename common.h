#ifndef COMMON_H
#define COMMON_H

#define TOKEN_MAX 256

enum instruction {
  /* special */
  I_NOP,  I_PUSHV, I_PUSHN,
  /* normal */
  I_ADD,  I_SUB,   I_MUL,   I_DIV,
  I_POW,  I_MOD,   I_LET,   I_DRAW,
  I_RGB,  I_HSV,   I_DISTX, I_DISTY,
  I_DIST, I_PRINT, I_SIN,   I_COS,
  I_TAN,  I_ABS,   I_SQRT,  I_FLOOR,
  I_CEIL, I_ROUND, I_RPREC, I_ASIN,
  I_ACOS, I_ATAN,  I_LN,    I_LOG,
  I_ETO,  I_TENTO, I_TERN,  I_DEFAULT,
  I_RAND, I_MIN,   I_MAX,   I_ATAN2,
  I_EQ,   I_NE,    I_GT,    I_LT,
  I_GE,   I_LE,    I_OR,    I_AND,
  I_XOR,  I_NOT,   I_FAC,   I_CLAMP,
  I_LERP, I_GETR,  I_GETG,  I_GETB,
  I_SIGN, I_GREY,  I_PCT,

  I_FINAL /* To calculate length */
};

const char *opcode_names[] = {
  /* special */
  "nop",  "pushv", "pushn",
  /* normal */
  "+",    "-",     "*",     "/",
  "^",    "%",     "let",   "draw",
  "rgb",  "hsv",   "distx", "disty",
  "dist", "print", "sin",   "cos",
  "tan",  "abs",   "sqrt",  "floor",
  "ceil", "round", "~",     "asin",
  "acos", "atan",  "ln",    "log",
  "eto",  "10to",  "?",     "default",
  "rand", "min",   "max",   "atan2",
  "=",    "!=",    ">",     "<",
  ">=",   "<=",    "or",    "and",
  "xor",  "not",   "fac",   "clamp",
  "lerp", "getr",  "getg",  "getb",
  "sign", "grey",  "pct"
};

struct full_instr {
  enum instruction opcode;
  char *operand;
};

struct instrs {
  struct full_instr *instrs;
  size_t count;
};

#endif
