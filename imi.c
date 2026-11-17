#include <stdio.h>
#include "common.h"
#include <raylib.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

/* ==================== PARSING AND BYTECODE ==================== */

struct byteinstr {
  enum instruction opcode;
  union {
    double n;
    size_t v;
  } operand;
};

struct variable {
  char *name;
  size_t index;
};

struct usedvars {
  uint8_t ux, uy, uw, uh, ue, upi, umousex, umousey, uframe;
  ssize_t ix, iy, iw, ih, ie, ipi, imousex, imousey, iframe;
};

// strdup was not ISO until C23
char *str_dup(const char *s) {
  size_t len = strlen(s) + 1;
  char *copy = malloc(len);
  if(copy) memcpy(copy, s, len);
  return copy;
}

struct byteinstr *parse_bytecode(FILE *in, size_t *vcnt, size_t *scnt, size_t *out_len, struct usedvars *usedvars) {
  struct byteinstr *result = NULL;
  size_t length = 0;
  size_t capacity = 0;

  struct variable *vt = NULL;
  size_t vt_len = 0;
  size_t vt_capacity = 0;

  int ch;
  size_t i;

  *vcnt = 0;
  *scnt = 0;
  *out_len = 0;
  if(fgetc(in) != 'I' || fgetc(in) != 'M' || fgetc(in) != 'B' || fgetc(in) != '\0') {
    fprintf(stderr, "invalid bytecode signature\n");
    goto error;
  }
  while((ch = fgetc(in)) != EOF) {
    struct byteinstr instr = {0};
    if(ch < 0 || ch >= I_FINAL) {
      fprintf(stderr, "unknown instruction %02x\n", ch);
      goto error;
    }
    instr.opcode = (enum instruction)ch;
    if(ch == I_PUSHV) {
      char buf[TOKEN_MAX] = {0};
      size_t i = 0, index = 0;
      while(i < TOKEN_MAX - 1) {
        if((ch = fgetc(in)) == EOF) {
          fprintf(stderr, "unterminated variable name '%s'\n", buf);
          goto error;
        }
        if(ch == '\0') break;
        buf[i++] = (char)ch;
      }
      if(ch != '\0') {
        fprintf(stderr, "variable name too long\n");
        goto error;
      }
      buf[i] = '\0';
      while(index < vt_len) {
        if(!strcmp(vt[index].name, buf)) break;
        index++;
      }
      if(index == vt_len) {
        if(vt_len == vt_capacity) {
          size_t new_capacity = vt_capacity ? vt_capacity * 2 : 8;
          struct variable *tmp = realloc(vt, new_capacity * sizeof(*vt));
          if(!tmp) goto error;
          vt = tmp;
          vt_capacity = new_capacity;
        }
        vt[index].name = str_dup(buf);
        if(!vt[index].name) goto error;
        vt[index].index = index;
        vt_len++;

        // Check what builtin variables are used
        // No fixed canvas size so no 'res' like the Scratch version
             if(!strcmp(buf, "x"))      { usedvars->ux      = 1; usedvars->ix      = index; }
        else if(!strcmp(buf, "y"))      { usedvars->uy      = 1; usedvars->iy      = index; }
        else if(!strcmp(buf, "w"))      { usedvars->uw      = 1; usedvars->iw      = index; }
        else if(!strcmp(buf, "h"))      { usedvars->uh      = 1; usedvars->ih      = index; }
        else if(!strcmp(buf, "e"))      { usedvars->ue      = 1; usedvars->ie      = index; }
        else if(!strcmp(buf, "pi"))     { usedvars->upi     = 1; usedvars->ipi     = index; }
        else if(!strcmp(buf, "mousex")) { usedvars->umousex = 1; usedvars->imousex = index; }
        else if(!strcmp(buf, "mousey")) { usedvars->umousey = 1; usedvars->imousey = index; }
        else if(!strcmp(buf, "frame"))  { usedvars->uframe  = 1; usedvars->iframe  = index; }
      }
      instr.operand.v = index;
      (*scnt)++;
    } else if(ch == I_PUSHN) {
      double decimal;
      if(fread(&decimal, sizeof(decimal), 1, in) != 1) {
        fprintf(stderr, "read error\n");
        goto error;
      }
      instr.operand.n = decimal;
      (*scnt)++;
    }
    if(length == capacity) {
      size_t new_capacity = capacity ? capacity * 2 : 16;
      struct byteinstr *tmp = realloc(result, new_capacity * sizeof(*result));
      if(!tmp) goto error;
      result = tmp;
      capacity = new_capacity;
    }
    result[length++] = instr;
  }
  if(ferror(in)) {
    fprintf(stderr, "read error\n");
    goto error;
  }
  for(i = 0; i < vt_len; i++) free(vt[i].name);
  free(vt);
  *vcnt = vt_len;
  *out_len = length;
  return result;

error:
  for(i = 0; i < vt_len; i++) free(vt[i].name);
  free(vt);
  free(result);
  *vcnt = 0;
  *scnt = 0;
  *out_len = 0;
  return NULL;
}

/* ==================== RENDERER ==================== */

struct framebuf {
  Color *pixels;
  Texture2D texture;
  int width, height;
};

int framebuf_resize(struct framebuf *fb, int width, int height) {
  Color *new_pixels;
  Texture2D new_texture;

  if(width <= 0 || height <= 0) return 0;
  if((size_t)width > SIZE_MAX / (size_t)height / sizeof(Color)) return 0;

  new_pixels = malloc((size_t)width * (size_t)height * sizeof(Color));
  if(!new_pixels) return 0;
  memset(new_pixels, 0, (size_t)width * (size_t)height * sizeof(Color));

  Image image;
  image.data = new_pixels;
  image.width = width;
  image.height = height;
  image.mipmaps = 1;
  image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

  new_texture = LoadTextureFromImage(image);
  if(!new_texture.id) {
    free(new_pixels);
    return 0;
  }

  SetTextureFilter(new_texture, TEXTURE_FILTER_POINT);
  if(fb->texture.id != 0) UnloadTexture(fb->texture);

  free(fb->pixels);
  fb->pixels = new_pixels;
  fb->texture = new_texture;
  fb->width = width;
  fb->height = height;

  return 1;
}

void framebuf_set_pixel(struct framebuf *fb, int x, int y, Color color) {
  if((unsigned)x >= (unsigned)fb->width || (unsigned)y >= (unsigned)fb->height) return;
  fb->pixels[(size_t)y * fb->width + x] = color;
}

void framebuf_upload(struct framebuf *fb) {
  UpdateTexture(fb->texture, fb->pixels);
}

void framebuf_destroy(struct framebuf *fb) {
  if(fb->texture.id) UnloadTexture(fb->texture);
  free(fb->pixels);
  *fb = (struct framebuf){0};
}

/* ==================== VM AND EXECUTION ==================== */

uint32_t hsv_to_hex(double h, double s, double v) {
  double c, x, m, r, g, b;
  uint8_t ri, gi, bi;
  h = fmod(h, 360.0);
  if(h < 0) h += 360.0;
  if(s < 0) s = 0;
  if(s > 1) s = 1;
  if(v < 0) v = 0;
  if(v > 1) v = 1;
  c = v * s;
  x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
  m = v - c;
       if(h < 60)  { r = c; g = x; b = 0; }
  else if(h < 120) { r = x; g = c; b = 0; }
  else if(h < 180) { r = 0; g = c; b = x; }
  else if(h < 240) { r = 0; g = x; b = c; }
  else if(h < 300) { r = x; g = 0; b = c; }
  else             { r = c; g = 0; b = x; }
  ri = (uint8_t)round((r + m) * 255.0);
  gi = (uint8_t)round((g + m) * 255.0);
  bi = (uint8_t)round((b + m) * 255.0);
  return (ri << 16) | (gi << 8) | bi;
}

struct value {
  double  num;
  size_t  var;
  uint8_t has_v;
};

#define BINOP(expr) tmp2 = stack[--(*sp)].num; \
                    tmp1 = stack[--(*sp)].num; \
                    stack[(*sp)++] = (struct value){ expr, 0, 0 }
#define UNARY(expr) tmp1 = stack[--(*sp)].num; \
                    stack[(*sp)++] = (struct value){ expr, 0, 0 }

#define POP  stack[--(*sp)]
#define PUSH stack[(*sp)++]

int vm_run(struct byteinstr *code, size_t count, struct usedvars *usedvars, double *vars, struct value *stack, size_t *sp, double x,
           double y, double w, double h, double mousex, double mousey, double frame) {
  size_t i;
  double tmp1, tmp2, tmp3;
  struct value tmp;

  if(usedvars->ux)      vars[usedvars->ix]      = x;
  if(usedvars->uy)      vars[usedvars->iy]      = y;
  if(usedvars->uw)      vars[usedvars->iw]      = w;
  if(usedvars->uh)      vars[usedvars->ih]      = h;
  if(usedvars->umousex) vars[usedvars->imousex] = mousex;
  if(usedvars->umousey) vars[usedvars->imousey] = mousey;
  if(usedvars->uframe)  vars[usedvars->iframe]  = frame;

  for(i = 0; i < count; i++) {
    switch(code[i].opcode) {
      case I_NOP:
        continue;
      case I_PUSHV:
        PUSH = (struct value){ vars[code[i].operand.v], code[i].operand.v, 1 };
        break;
      case I_PUSHN:
        PUSH = (struct value){ code[i].operand.n, 0, 0 };
        break;
      case I_ADD:
        BINOP(tmp1 + tmp2);
        break;
      case I_SUB:
        BINOP(tmp1 - tmp2);
        break;
      case I_MUL:
        BINOP(tmp1 * tmp2);
        break;
      case I_DIV:
        BINOP(tmp1 / tmp2);
        break;
      case I_POW:
        BINOP(pow(tmp1, tmp2));
        break;
      case I_MOD:
        BINOP(fmod(tmp1, tmp2));
        break;
      case I_LET:
        tmp1 = POP.num;
        tmp = POP;
        if(!tmp.has_v) {
          TraceLog(LOG_ERROR, "expected variable reference for let");
          return -2;
        }
        vars[tmp.var] = tmp1;
        break;
      case I_DRAW:
        return (int)POP.num & 0xFFFFFF;
      case I_RGB:
        tmp3 = POP.num;
        tmp2 = POP.num;
        tmp1 = POP.num;
        PUSH = (struct value){ ((uint8_t)tmp1 << 16) | ((uint8_t)tmp2 << 8) | (uint8_t)tmp3, 0, 0 };
        break;
      case I_HSV:
        tmp3 = POP.num;
        tmp2 = POP.num;
        tmp1 = POP.num;
        PUSH = (struct value){ hsv_to_hex(tmp1, tmp2, tmp3), 0, 0 };
        break;
      case I_DISTX:
        UNARY(fabs(tmp1 - x));
        break;
      case I_DISTY:
        UNARY(fabs(tmp1 - y));
        break;
      case I_DIST:
        tmp2 = POP.num - y;
        tmp1 = POP.num - x;
        PUSH = (struct value){ hypot(tmp1, tmp2), 0, 0 };
        break;
      case I_PRINT:
        tmp1 = POP.num;
        printf("LOG: %g\n", tmp1);
        break;
      case I_SIN:
        UNARY(sin(tmp1));
        break;
      case I_COS:
        UNARY(cos(tmp1));
        break;
      case I_TAN:
        UNARY(tan(tmp1));
        break;
      case I_ABS:
        UNARY(fabs(tmp1));
        break;
      case I_SQRT:
        UNARY(sqrt(tmp1));
        break;
      case I_FLOOR:
        UNARY(floor(tmp1));
        break;
      case I_CEIL:
        UNARY(ceil(tmp1));
        break;
      case I_ROUND:
        UNARY(round(tmp1));
        break;
      case I_RPREC:
        BINOP(tmp2 * round(tmp1 / tmp2));
        break;
      case I_ASIN:
        UNARY(asin(tmp1));
        break;
      case I_ACOS:
        UNARY(acos(tmp1));
        break;
      case I_ATAN:
        UNARY(atan(tmp1));
        break;
      case I_LN:
        UNARY(log(tmp1));
        break;
      case I_LOG:
        UNARY(log10(tmp1));
        break;
      case I_ETO:
        UNARY(exp(tmp1));
        break;
      case I_TENTO:
        UNARY(exp10(tmp1));
        break;
      case I_TERN:
        tmp3 = POP.num;
        tmp2 = POP.num;
        tmp1 = POP.num;
        if(tmp1) PUSH = (struct value){ tmp2, 0, 0 };
        else     PUSH = (struct value){ tmp3, 0, 0 };
        break;
      case I_DEFAULT: // (default default optional)
        if(*sp >= 2) {
          (*sp)--;
          tmp1 = POP.num;
        } else tmp1 = POP.num;
        PUSH = (struct value){ tmp1, 0, 0 };
        break;
      case I_RAND:
        BINOP(tmp1 + ((double)rand() / (double)RAND_MAX) * (tmp2 - tmp1));
        break;
      case I_MIN:
        BINOP(tmp1 > tmp2 ? tmp2 : tmp1);
        break;
      case I_MAX:
        BINOP(tmp1 > tmp2 ? tmp1 : tmp2);
        break;
      case I_ATAN2:
        BINOP(atan2(tmp1, tmp2));
        break;
      case I_EQ:
        BINOP(tmp1 == tmp2);
        break;
      case I_NE:
        BINOP(tmp1 != tmp2);
        break;
      case I_GT:
        BINOP(tmp1 > tmp2);
        break;
      case I_LT:
        BINOP(tmp1 < tmp2);
        break;
      case I_GE:
        BINOP(tmp1 >= tmp2);
        break;
      case I_LE:
        BINOP(tmp1 <= tmp2);
        break;
      case I_OR:
        BINOP(tmp1 || tmp2);
        break;
      case I_AND:
        BINOP(tmp1 && tmp2);
        break;
      case I_XOR:
        BINOP((tmp1 != 0.0) != (tmp2 != 0.0));
        break;
      case I_NOT:
        UNARY(!tmp1);
        break;
      case I_FAC:
        UNARY(tgamma(tmp1 + 1));
        break;
      case I_CLAMP:
        tmp3 = POP.num;
        tmp2 = POP.num;
        tmp1 = POP.num;
        PUSH = (struct value){ fmax(tmp2, fmin(tmp1, tmp3)), 0, 0 };
        break;
      case I_LERP:
        tmp3 = POP.num;
        tmp2 = POP.num;
        tmp1 = POP.num;
        PUSH = (struct value){ tmp2 + ((tmp3 - tmp2) * (tmp1 / 100)), 0, 0 };
        break;
      case I_GETR:
        UNARY(((int)tmp1 >> 16) & 0xFF);
        break;
      case I_GETG:
        UNARY(((int)tmp1 >> 8) & 0xFF);
        break;
      case I_GETB:
        UNARY((int)tmp1 & 0xFF);
        break;
      case I_SIGN:
        tmp1 = POP.num;
             if(tmp1 < 0) PUSH = (struct value){ -1, 0, 0 };
        else if(tmp1 > 0) PUSH = (struct value){ 1, 0, 0 };
        else              PUSH = (struct value){ 0, 0, 0 };
        break;
      case I_GREY: {
        unsigned n;
        n = (int)POP.num & 0xFF;
        PUSH = (struct value){ (double)(n << 16 | n << 8 | n), 0, 0 };
        break;
      }
      case I_PCT:
        tmp3 = POP.num;
        tmp2 = POP.num;
        tmp1 = POP.num;

        PUSH = (struct value){((tmp1 - tmp2) / (tmp3 - tmp2)) * 100, 0, 0};
        break;
      default:
        continue;
    }
  }
  return 0;
}

/* ==================== MAIN ==================== */

int main(int argc, char **argv) {
  int i, j, stats = 0, cap = 0, dragging = 0;
  char *arg, *input = NULL, *tmp;
  FILE *in = stdin;
  size_t vcnt, scnt, icnt, frame = 0, sp = 0;
  struct byteinstr *program;
  double *vars;
  struct value *stack;
  struct framebuf fb = {0};
  struct usedvars usedvars = {0};
  double zoom = 1;
  Vector2 cameraOffset = { 0, 0 }, lastMouse = { 0, 0 };

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
          printf("imi: Imagine interpreter\n"
                 "\n"
                 "usage: %s [options] [--] [infile]\n"
                 "\n"
                 "options:\n"
                 "  -h            show this help message\n"
                 "  -s            show performance metrics\n"
                 "  -c            cap FPS at 60\n"
          , argv[0]);
          return 0;
        case 's':
          stats = 1;
          break;
        case 'c':
          cap = 1;
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
  } else input = "-";

  program = parse_bytecode(in, &vcnt, &scnt, &icnt, &usedvars);
  if(!program) return 1;

  tmp = malloc(strlen(input) + 10);
  if(!tmp) {
    fprintf(stderr, "memory allocation failure\n");
    free(program);
    return 1;
  }

  snprintf(tmp, strlen(input) + 10, "Imagine: %s", input);

  srand(time(NULL));

  vars  = calloc(vcnt, sizeof(double));
  stack = calloc(scnt, sizeof(struct value));
  if(!vars || !stack) {
    fprintf(stderr, "memory allocation failure\n");
    if(stack) free(stack);
    if(vars)  free(vars);
    free(program);
    if(in != stdin) fclose(in);
    return 1;
  }

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(480, 360, tmp);
  if(cap) SetTargetFPS(60);
  free(tmp);

  if(!framebuf_resize(&fb, GetScreenWidth(), GetScreenHeight())) {
    CloseWindow();
    free(stack);
    free(vars);
    free(program);
    if(in != stdin) fclose(in);
    return 1;
  }

  if(usedvars.upi) vars[usedvars.ipi] = 3.1415926535;
  if(usedvars.ue)  vars[usedvars.ie]  = 2.7182818284;

  while(!WindowShouldClose()) {
    int width, height, x, y, hex;
    Color colour;
    Vector2 mouse;
    if(IsWindowResized()) {
      width = GetScreenWidth();
      height = GetScreenHeight();
      if(!framebuf_resize(&fb, width, height)) TraceLog(LOG_WARNING, "Failed to resize framebuffer");
    }
    width  = fb.width;
    height = fb.height;

    mouse = GetMousePosition();

    // Keyboard zoom
    if(IsKeyPressed(KEY_EQUAL)) zoom += 0.1;
    if(IsKeyPressed(KEY_MINUS)) zoom -= 0.1;
    // Scroll wheel zoom
    zoom += GetMouseWheelMove() * 0.1;

    if(zoom <= 0) zoom = 0;

    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      if(!dragging) {
        dragging = 1;
        lastMouse = mouse;
      }

      cameraOffset.x += mouse.x - lastMouse.x;
      cameraOffset.y += mouse.y - lastMouse.y;

      lastMouse = mouse;
    } else dragging = 0;

    for(y = 0; y < height; y++) for(x = 0; x < width; x++) {
      sp = 0;
      hex = vm_run(program, icnt, &usedvars, vars, stack, &sp, (double)(x - (width >> 1) - cameraOffset.x) / zoom, (double)((height >> 1) - y + cameraOffset.y) / zoom, (double)width,
                   (double)height, (double)(mouse.x - (width >> 1) - cameraOffset.x) / zoom, (double)((height >> 1) - mouse.y + cameraOffset.y) / zoom, (double)frame);
      if(hex >= 0) {
        hex &= 0xFFFFFF;
        fb.pixels[(size_t)y * width + x] = (Color){(hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, 255 };
      } else if(hex == -2) {
        framebuf_destroy(&fb);
        CloseWindow();
        free(program);
        free(stack);
        free(vars);
        if(in != stdin) fclose(in);
        return 1;
      }
    }

    framebuf_upload(&fb);
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(fb.texture, 0, 0, WHITE);
    if(stats) {
      int fps = GetFPS();
      DrawText(TextFormat("FPS %d", fps), 10, 10, 20, GREEN);
      DrawText(TextFormat("MS %d", (int)(GetFrameTime() * 1000)), 120, 10, 20, GREEN);
      DrawText(TextFormat("KIPS %d", (fps * width * height * icnt) / 1000), 230, 10, 20, GREEN);
    }
    EndDrawing();

    frame++;
  }

  framebuf_destroy(&fb);
  CloseWindow();
  free(program);
  free(stack);
  free(vars);
  if(in != stdin) fclose(in);
  return 0;
}
