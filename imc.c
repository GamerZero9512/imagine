#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "common.h"

struct tokens {
  char **tokens;
  size_t count;
};

struct tokens tokenise(FILE *stream) {
  struct tokens result = {NULL, 0};
  size_t capacity = 16, len = 0, i;
  char token[TOKEN_MAX], **tmp;
  int c;

  result.tokens = malloc(capacity * sizeof(char*));
  if(!result.tokens) return result;

  while((c = fgetc(stream)) != EOF) {
    if(isspace((unsigned char)c) || c == '(' || c == ')') {
      if(len > 0) {
        token[len] = '\0';
        if(result.count == capacity) {
          capacity *= 2;
          tmp = realloc(result.tokens, capacity * sizeof(char *));
          if(!tmp) goto error;
          result.tokens = tmp;
        }
        result.tokens[result.count] = strdup(token);
        if(!result.tokens[result.count]) goto error;
        result.count++;
        len = 0;
      }

      if(c == '(' || c == ')') {
        char *paren;
        if(result.count == capacity) {
          capacity *= 2;
          tmp = realloc(result.tokens, capacity * sizeof(char *));
          if(!tmp) goto error;
          result.tokens = tmp;
        }
        paren = malloc(2);
        if(!paren) goto error;
        paren[0] = (char)c;
        paren[1] = '\0';
        result.tokens[result.count++] = paren;
      }
    } else {
      if(len >= TOKEN_MAX - 1) goto error;
      token[len++] = (char)c;
    }
  }

  if(len > 0) {
    token[len] = '\0';
    if(result.count == capacity) {
      capacity *= 2;
      tmp = realloc(result.tokens, capacity * sizeof(char *));
      if(!tmp) goto error;
      result.tokens = tmp;
    }
    result.tokens[result.count] = strdup(token);
    if(!result.tokens[result.count]) goto error;
    result.count++;
  }

  return result;

error:
  for(i = 0; i < result.count; i++) free(result.tokens[i]);
  free(result.tokens);
  return (struct tokens){NULL, 0};
}

void free_tokens(struct tokens *tokens) {
  size_t i;
  for(i = 0; i < tokens->count; i++) free(tokens->tokens[i]);
  free(tokens->tokens);
  tokens->tokens = NULL;
  tokens->count = 0;
}

int is_valid_decimal(const char *str, double *decimal) {
  char *end;
  errno = 0;
  *decimal = strtod(str, &end);
  return end != str && *end == '\0' && errno != ERANGE;
}

int parse(struct tokens *tokens, int human, FILE *out) {
  char **operator_stack = malloc(tokens->count * sizeof(char*));
  size_t stack_length = 0, i = 0;

  if(!operator_stack && tokens->count > 0) return 1;

  while (i < tokens->count) {
    char *token = tokens->tokens[i++];

    if(!strcmp(token, "(")) {
      if(i >= tokens->count) {
        fprintf(stderr, "mismatched parenthesis\n");
        free(operator_stack);
        return 1;
      }
      operator_stack[stack_length++] = tokens->tokens[i++];
    } else if(!strcmp(token, ")")) {
      if(stack_length == 0) {
        fprintf(stderr, "mismatched parenthesis\n");
        free(operator_stack);
        return 1;
      }
      if(human) fprintf(out, "%s\n", operator_stack[--stack_length]);
      else {
        char *op = operator_stack[--stack_length];
             if(!strcmp(op, "+"))       fputc(I_ADD, out);
        else if(!strcmp(op, "-"))       fputc(I_SUB, out);
        else if(!strcmp(op, "*"))       fputc(I_MUL, out);
        else if(!strcmp(op, "/"))       fputc(I_DIV, out);
        else if(!strcmp(op, "^"))       fputc(I_POW, out);
        else if(!strcmp(op, "%"))       fputc(I_MOD, out);
        else if(!strcmp(op, "let"))     fputc(I_LET, out);
        else if(!strcmp(op, "draw"))    fputc(I_DRAW, out);
        else if(!strcmp(op, "rgb"))     fputc(I_RGB, out);
        else if(!strcmp(op, "hsv"))     fputc(I_HSV, out);
        else if(!strcmp(op, "distx"))   fputc(I_DISTX, out);
        else if(!strcmp(op, "disty"))   fputc(I_DISTY, out);
        else if(!strcmp(op, "dist"))    fputc(I_DIST, out);
        else if(!strcmp(op, "print"))   fputc(I_PRINT, out);
        else if(!strcmp(op, "sin"))     fputc(I_SIN, out);
        else if(!strcmp(op, "cos"))     fputc(I_COS, out);
        else if(!strcmp(op, "tan"))     fputc(I_TAN, out);
        else if(!strcmp(op, "abs"))     fputc(I_ABS, out);
        else if(!strcmp(op, "sqrt"))    fputc(I_SQRT, out);
        else if(!strcmp(op, "floor"))   fputc(I_FLOOR, out);
        else if(!strcmp(op, "ceil"))    fputc(I_CEIL, out);
        else if(!strcmp(op, "round"))   fputc(I_ROUND, out);
        else if(!strcmp(op, "~"))       fputc(I_RPREC, out);
        else if(!strcmp(op, "asin"))    fputc(I_ASIN, out);
        else if(!strcmp(op, "acos"))    fputc(I_ACOS, out);
        else if(!strcmp(op, "atan"))    fputc(I_ATAN, out);
        else if(!strcmp(op, "ln"))      fputc(I_LN, out);
        else if(!strcmp(op, "log"))     fputc(I_LOG, out);
        else if(!strcmp(op, "eto"))     fputc(I_ETO, out);
        else if(!strcmp(op, "10to"))    fputc(I_TENTO, out);
        else if(!strcmp(op, "?"))       fputc(I_TERN, out);
        else if(!strcmp(op, "default")) fputc(I_DEFAULT, out);
        else if(!strcmp(op, "rand"))    fputc(I_RAND, out);
        else if(!strcmp(op, "min"))     fputc(I_MIN, out);
        else if(!strcmp(op, "max"))     fputc(I_MAX, out);
        else if(!strcmp(op, "atan2"))   fputc(I_ATAN2, out);
        else if(!strcmp(op, "="))       fputc(I_EQ, out);
        else if(!strcmp(op, "!="))      fputc(I_NE, out);
        else if(!strcmp(op, ">"))       fputc(I_GT, out);
        else if(!strcmp(op, "<"))       fputc(I_LT, out);
        else if(!strcmp(op, ">="))      fputc(I_GE, out);
        else if(!strcmp(op, "<="))      fputc(I_LE, out);
        else if(!strcmp(op, "or"))      fputc(I_OR, out);
        else if(!strcmp(op, "and"))     fputc(I_AND, out);
        else if(!strcmp(op, "xor"))     fputc(I_XOR, out);
        else if(!strcmp(op, "not"))     fputc(I_NOT, out);
        else if(!strcmp(op, "fac"))     fputc(I_FAC, out);
        else if(!strcmp(op, "clamp"))   fputc(I_CLAMP, out);
        else if(!strcmp(op, "lerp"))    fputc(I_LERP, out);
        else if(!strcmp(op, "getr"))    fputc(I_GETR, out);
        else if(!strcmp(op, "getg"))    fputc(I_GETG, out);
        else if(!strcmp(op, "getb"))    fputc(I_GETB, out);
        else if(!strcmp(op, "sign"))    fputc(I_SIGN, out);
        else if(!strcmp(op, "grey"))    fputc(I_GREY, out);
        else if(!strcmp(op, "pct"))     fputc(I_PCT, out);
        else {
          fprintf(stderr, "invalid operator '%s'\n", op);
          free(operator_stack);
          return 1;
        }
      }
    }
    else {
      double decimal;
      if(human) fprintf(out, "%s %s\n", is_valid_decimal(token, &decimal) ? "pushn" : "pushv", token);
      else {
        if(is_valid_decimal(token, &decimal)) {
          fputc(I_PUSHN, out);
          fwrite(&decimal, sizeof(double), 1, out);
        } else {
          fprintf(out, "%c%s", I_PUSHV, token);
          fputc('\0', out);
        }
      }
    }
  }

  if(stack_length > 0) {
    fprintf(stderr, "mismatched parenthesis\n");
    free(operator_stack);
    return 1;
  }

  free(operator_stack);
  return 0;
}

int parse_bytecode(FILE *in, FILE *out) {
  int ch;
  if(fgetc(in) != 'I' || fgetc(in) != 'M' || fgetc(in) != 'B' || fgetc(in) != '\0') {
    fprintf(stderr, "invalid bytecode signature\n");
    return 1;
  }
  while((ch = fgetc(in)) != EOF) {
    if(ch < 0 || ch >= I_FINAL) {
      fprintf(stderr, "unknown instruction %02x\n", ch);
      return 1;
    }
    fprintf(out, "%s", opcode_names[ch]);
    if(ch == I_PUSHV || ch == I_PUSHN) {
      if(ch == I_PUSHV) {
        char buf[TOKEN_MAX];
        size_t i = 0;
        while (i < (TOKEN_MAX - 1) && (ch = fgetc(in)) != EOF && ch != '\0') buf[i++] = (char)ch;
        buf[i] = '\0';
        fprintf(out, " %s", buf);
      } else {
        double decimal;
        char buf[TOKEN_MAX], *end;

        if(fread(&decimal, sizeof(double), 1, in) != 1) {
          fprintf(stderr, "read error\n");
          return 1;
        }

        snprintf(buf, sizeof(buf), "%.15f", decimal);
        end = buf + strlen(buf) - 1;
        while(end > buf && *end == '0') *end-- = '\0';
        if(end > buf && *end == '.') *end = '\0';
        printf(" %s", buf);
      }
    }
    fputc('\n', out);
  }
  return 0;
}

enum {
  OP_COMPILE,
  OP_FORMAT
};

int main(int argc, char **argv) {
  int i, j, human = 0, result, operation = OP_COMPILE;
  char *arg, *input = NULL, *output = NULL;
  FILE *in = stdin, *out = stdout;
  struct tokens tokens;

  for(i = 1; i < argc; i++) {
    arg = argv[i];
    if(strcmp(arg, "--") == 0) {
      i++;
      break;
    }
    if(arg[0] != '-' || arg[1] == '\0') {
      input = arg;
      i++;
      break;
    }
    for(j = 1; arg[j]; j++) {
      switch(arg[j]) {
        case 'h':
          printf("imc: Imagine compiler\n"
                 "\n"
                 "usage: %s [options] [--] [infile]\n"
                 "\n"
                 "options:\n"
                 "  -h            show this help message\n"
                 "  -o <file>     set output filename\n"
                 "  -H            produce human-readable output\n"
                 "  -f            format bytecode to human-readable syntax\n"
          , argv[0]);
          return 0;
        case 'o':
          if(arg[j + 1]) output = &arg[j + 1];
          else if(++i < argc) output = argv[i];
          else {
            fprintf(stderr, "flag 'o' requires an argument\n");
            return 1;
          }
          j = strlen(arg) - 1;

          if(strcmp(output, "-") == 0) out = stdout;
          else {
            out = fopen(output, "wb");
            if(!out) {
              fprintf(stderr, "error opening '%s': %s\n", output, strerror(errno));
              return 1;
            }
          }
          break;
        case 'H':
          human = true;
          break;
        case 'f':
          operation = OP_FORMAT;
          break;
        default:
          fprintf(stderr, "unrecognised flag: '%c'\n"
                          "try '-h' for help\n", arg[j]);
          return 1;
      }
    }
  }

  while(i < argc) {
    if(input) {
      fprintf(stderr, "too many input files\n");
      return 1;
    }
    input = argv[i++];
  }

  if(input) {
    if(strcmp(input, "-") == 0) in = stdin;
    else {
      in = fopen(input, "rb");
      if(!in) {
        fprintf(stderr, "error opening '%s': %s\n", input, strerror(errno));
        return 1;
      }
    }
  }

  if(operation == OP_COMPILE) {
    tokens = tokenise(in);
    if(!tokens.tokens) {
      fclose(in);
      fclose(out);
      return 1;
    }
    if(!human) {
      fputs("IMB", out);
      fputc('\0', out);
    }
    result = parse(&tokens, human, out);
  } else result = parse_bytecode(in, out);
  if(in != stdin)   fclose(in);
  if(out != stdout) fclose(out);
  return result;
}
