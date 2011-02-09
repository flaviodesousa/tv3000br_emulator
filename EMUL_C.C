/*--------------------------------------------------------------*/
/* Emulador local de terminal SISCO TV3000/BR - Modulo Emulador */
/* Flavio de Sousa - Marco de 1.990                             */
/*--------------------------------------------------------------*/

#include <stdio.h>
#include <dos.h>
#include <process.h>

#define ZERO       0 /* Zero                                           */
#define BUFSIZE    2 /* Buffer para parametros de sequencias de escape */
#define MAXROW    23 /* Numero da ultima linha                         */
#define MAXCOL    79 /* Numero da ultima coluna                        */
#define MAXWIN     3 /* Maximo de janelas                              */
#define OFFSET  0x20 /* Normalizador de parametros                     */

#define NIBBLE(n)  ((n) & 0x0f) /* Normalizador (extrai nibble inferior) */
#define BYTE(n)    ((n) & 0xff) /* Normalizador (transforma em byte)     */
#define LOBYTE(n)  BYTE(n)      /* Extrai byte inferior                  */
#define HIBYTE(n)  BYTE((n)>>8) /* Extrai byte superior                  */

#define SPEAKERPORT 0x61
#define BELLFREQ    10000 /* Hz */
#define BELLTIME    50    /* ms */

/* Caracteres de controle */
/*------------------------*/
#define BEL  '\x07'
#define BS   '\x08'
#define LF   '\x0a'
#define CR   '\x0d'
#define DC1  '\xa0' /* '\x11' */
#define DC2  '\x90' /* '\x12' */
#define DC3  '\x82' /* '\x13' */
#define ESC  '\x1b'
#define RS   '\x96' /* '\x1e' */

/* Emulacao de atributos de video */
/*--------------------------------*/
#define ATTR_NORM  0x0000
#define ATTR_BLINK 0x0001 
#define ATTR_UNDER 0x0002 
#define ATTR_REV   0x0004 
#define ATTR_DIM   0x0008 
#define ATTR_PROT  0x0010 

#define B_ATTR_BLINK ATTR_BLINK
#define B_ATTR_UNDER ATTR_UNDER
#define B_ATTR_REV   ATTR_REV
#define B_ATTR_DIM   ATTR_DIM
#define B_ATTR_PROT  ATTR_PROT
                           
#define E_ATTR_BLINK (ATTR_BLINK << 8)
#define E_ATTR_UNDER (ATTR_UNDER << 8)
#define E_ATTR_REV   (ATTR_REV   << 8)
#define E_ATTR_DIM   (ATTR_DIM   << 8)
#define E_ATTR_PROT  (ATTR_PROT  << 8)

#define VD_ATTR_NORM   0x0f /* Normal                             */
#define VD_ATTR_B      0x8f /* Piscante                           */
#define VD_ATTR_U      0x07 /* Sublinhado                         */
#define VD_ATTR_UB     0x87 /* Piscante Sublinhado                */
#define VD_ATTR_R      0x70 /* Reverso                            */
#define VD_ATTR_RB     0xf0 /* Piscante Reverso                   */
#define VD_ATTR_RU     0x73 /* Sublinhado Reverso                 */
#define VD_ATTR_RUB    0xf3 /* Piscante Sublinhado Reverso        */
#define VD_ATTR_D      0x05 /* Escuro                             */
#define VD_ATTR_DB     0x85 /* Escuro Piscante                    */
#define VD_ATTR_DU     0x06 /* Escuro Sublinhado                  */
#define VD_ATTR_DUB    0x86 /* Escuro Piscante Sublinhado         */
#define VD_ATTR_DR     0x50 /* Escuro Reverso                     */
#define VD_ATTR_DRB    0xb0 /* Escuro Piscante Reverso            */
#define VD_ATTR_DRU    0x56 /* Escuro Sublinhado Reverso          */
#define VD_ATTR_DRUB   0xb6 /* Escuro Piscante Sublinhado Reverso */

static const unsigned char VD_ATTR[16] =
   {
      VD_ATTR_NORM, /* Normal                             */
      VD_ATTR_B   , /* Piscante                           */
      VD_ATTR_U   , /* Sublinhado                         */
      VD_ATTR_UB  , /* Piscante Sublinhado                */
      VD_ATTR_R   , /* Reverso                            */
      VD_ATTR_RB  , /* Piscante Reverso                   */
      VD_ATTR_RU  , /* Sublinhado Reverso                 */
      VD_ATTR_RUB , /* Piscante Sublinhado Reverso        */
      VD_ATTR_D   , /* Escuro                             */
      VD_ATTR_DB  , /* Escuro Piscante                    */
      VD_ATTR_DU  , /* Escuro Sublinhado                  */
      VD_ATTR_DUB , /* Escuro Piscante Sublinhado         */
      VD_ATTR_DR  , /* Escuro Reverso                     */
      VD_ATTR_DRB , /* Escuro Piscante Reverso            */
      VD_ATTR_DRU , /* Escuro Sublinhado Reverso          */
      VD_ATTR_DRUB  /* Escuro Piscante Sublinhado Reverso */
   };

#define OP_SELECT    1
#define OP_POSCUR    2
#define OP_SETWIN    3
#define OP_SELWIN    4

/*---------------------------------------------------------------------*/
/* VARIAVEIS GLOBAIS                                                   */
/*---------------------------------------------------------------------*/

/* variaveis de controle de tela */
/*-------------------------------*/
static int waiting               = 0xaaaa; /* TRAP! */
static int operation             = ZERO;
static char buffer[BUFSIZE];
static int atributo_logico_atual = ZERO;
static int atributo_fisico_atual = VD_ATTR_NORM;
static int atualiza_cursor       = ZERO;
static int janela_atual          = ZERO;

static struct VIDEO_CHAR
   {
   unsigned char caracter,
                 atributo;
   } (far *video)[24][80] = (struct VIDEO_CHAR (far *)[24][80])0xb8000000L;
static const struct VIDEO_CHAR CLEAN_CHAR = {' ', VD_ATTR_NORM};

static int atributo[MAXROW+1][MAXCOL+1];

static struct JANELA
   {
   int linha,
       coluna,
       inicio,
       protegido;
   } janela[4] = {{ZERO,  ZERO, ZERO,     ZERO},
                  {ZERO,  ZERO, MAXROW+1, ZERO},
                  {ZERO,  ZERO, MAXROW+1, ZERO},
                  {ZERO,  ZERO, MAXROW+1, ZERO}};
static struct JANELA *tela = janela;

static const struct CURSOR_ADDRESS
   {
   unsigned short int coluna,
                      linha;
   } far * BIOS_CURSOR_ADDRESS = (struct CURSOR_ADDRESS far *)0x00400050L;

/* Variaveis de suporte para a rotina de interrupcao */
/*---------------------------------------------------*/
union REGS r;
unsigned long old_vector;

/*---------------------------------------------------------------------*/
/* PROCESSADOR DE CARACTERES - SELETOR DE OPERACOES                    */
/*---------------------------------------------------------------------*/

static int seletor(caracter)
char caracter;
{
   if (waiting)
      {
      buffer[--waiting] = caracter;
      if (!waiting) switch (operation)
         {
         case OP_SELECT : dispatch(caracter); break;
         case OP_POSCUR : poscur(); break; /* posiciona cursor */
         case OP_SETWIN : setwin(); break; /* define janelas   */
         case OP_SELWIN : selwin(); break; /* seleciona janela */
         }
      }
   else
      switch(caracter)
         {
         case CR   : charCR();           break;
         case LF   : charLF();           break;
         case BS   : charBS();           break;
         case BEL  : return 1; /* charBEL();          break; */
         case RS   : charRS();           break;
         case ESC  : dispatch(caracter); break;
         default   : putcar(caracter);   break;
         }
   return ZERO;
}

/*---------------------------------------------------------------------*/
/* EMULACAO TV-3000BR - SEQUENCIAS DE CONTROLE COMPOSTAS               */
/*---------------------------------------------------------------------*/

static dispatch(caracter)
char caracter;
{
   switch(caracter)
      {
      case ESC  : waiting = 1;
                    operation = OP_SELECT;
                    break;
      case '='  : waiting = 2;
                    operation = OP_POSCUR;
                    break;
      case '8'  : waiting = 1;
                    operation = OP_SETWIN;
                    break;
      case 'A'  : waiting = 1;
                    operation = OP_SELWIN;
                    break;
      case 'T'  : clrlin(); break; /* limpa linha                */
      case 'E'  : inslin(); break; /* insere linha               */
      case 'R'  : dellin(); break; /* deleta linha               */
      case 'Y'  : clreos(); break; /* limpa ate o fim da tela    */
      case '&'  : proton(); break; /* liga modo protegido        */
      case '\'' : protof(); break; /* desliga modo protegido     */
      case ')'  : begpro(); break; /* inicia campo protegido     */
      case '('  : endpro(); break; /* termina campo protegido    */
      case 'F'  : begblk(); break; /* inicia campo piscante      */
      case 'L'  : endblk(); break; /* termina campo piscante     */
      case 'H'  : begrev(); break; /* inicia campo reverso       */
      case 'N'  : endrev(); break; /* termina campo reverso      */
      case 'G'  : begdim(); break; /* inicia campo baixa int.    */
      case 'M'  : enddim(); break; /* termina campo baixa int.   */
      case 'I'  : begund(); break; /* inicia campo sublinhado    */
      case 'P'  : endund(); break; /* termina campo sublinhado   */
      case 'K'  : endatt(); break; /* termina todos os atributos */
      case 'X'  : rematt(); break; /* remove atributos           */
      case DC1  : filund(); break; /* preenche com sublinhado    */
      case DC2  : filrev(); break; /* preenche com reverso       */
      case DC3  : fildim(); break; /* preenche com dim           */
      }
}

static poscur()
{
   union REGS regs;

   tela->linha  = buffer[1] - OFFSET;
   tela->coluna = buffer[0] - OFFSET;
   update_cursor();
}

static setwin()
{
   union REGS regs;
   static int atual = ZERO,
              separacao[MAXWIN - 1] = {ZERO};
   int i;

   if (!atual) /* reseta dados de janela */
      {
      for (i = MAXWIN-1; i >= ZERO; i--)
         {
         janela[i].linha = janela[i].coluna = janela[i].protegido = ZERO,
         janela[i].inicio = i ? (MAXROW + 1) : ZERO;
         }
      tela = janela; /* seleciona primeira janela */
      clear_screen();
      }
   separacao[atual] = buffer[0] - OFFSET;
   if (separacao[atual] && atual < MAXWIN - 2)
      {
      waiting = 1; atual++;
      }
   else
      {
      for (i = atual+1; i; i--)
         {
         janela[i].inicio = separacao[atual] ? separacao[atual] : MAXROW+1;
         separacao[atual--] = ZERO;
         }
      atual = ZERO;
      }
}

static selwin()
{
   int newwin;

   newwin = buffer[0] - '0';
   if (newwin <= MAXWIN && janela[newwin].inicio <= MAXROW)
      {
      tela = janela + newwin;
      update_cursor();
      }
}

static clrlin()
{
   int c;
   struct VIDEO_CHAR far *vidptr;

   if (tela->coluna <= MAXCOL)
      {
      vidptr = (*video)[tela->inicio + tela->linha] + tela->coluna;
      for (c = MAXCOL - tela->coluna - 1; c; c--)
         *(vidptr++) = CLEAN_CHAR;
      }
}

static inslin()
{
   scroll_down();
}

static dellin()
{
   scroll_up();
}

static clreos()
{
   int i;
   int *attrptr;
   struct VIDEO_CHAR far *vidptr;

   vidptr  = (*video)[tela->inicio + tela->linha] + tela->coluna;
   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna;
   if (tela->protegido)
      {
      for (i = ((tela+1)->inicio - tela->inicio - tela->linha)*(MAXCOL + 1) - tela->coluna; i; i--)
         {
         if (!(*attrptr & ATTR_PROT))
            {
            *(vidptr++) = CLEAN_CHAR;
            *attrptr++  = ATTR_NORM;
            }
         else
            {
            vidptr++;
            attrptr++;
            }
         }
      }
   else
      for (i = ((tela+1)->inicio - tela->inicio - tela->linha)*(MAXCOL + 1) - tela->coluna; i; i--)
         {
         *(vidptr++)  = CLEAN_CHAR;
         *(attrptr++) = ATTR_NORM;
         }
}

static proton()
{
   tela->protegido = 1;
}

static protof()
{
   tela->protegido = 0;
}

static begpro()
{
   atributo[tela->inicio + tela->linha][tela->coluna] |= B_ATTR_PROT;
   atributo_logico_atual |= ATTR_PROT;
}

static endpro()
{
   int *attrptr;

   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna;
   if (attrptr > atributo[tela->inicio])
      attrptr--;
   *attrptr |= E_ATTR_PROT;
   atributo_logico_atual &= ~ATTR_PROT;
}

static begblk()
{
   atributo[tela->inicio + tela->linha][tela->coluna] |= B_ATTR_BLINK;
   atributo_fisico_atual = VD_ATTR[ NIBBLE(atributo_logico_atual |= ATTR_BLINK) ];
}

static endblk()
{
   struct VIDEO_CHAR far *vidptr;
   int *attrptr;

   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna;
   if (attrptr > atributo[tela->inicio])
      attrptr--;
   *attrptr |= E_ATTR_BLINK;
   atributo_logico_atual &= ~ATTR_BLINK;
   atributo_fisico_atual = VD_ATTR[NIBBLE(atributo_logico_atual)];
}

static begrev()
{
   atributo[tela->inicio + tela->linha][tela->coluna] |= B_ATTR_REV;
   atributo_fisico_atual = VD_ATTR[ NIBBLE(atributo_logico_atual |= ATTR_REV) ];
}

static endrev()
{
   struct VIDEO_CHAR far *vidptr;
   int *attrptr;

   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna;
   if (attrptr > atributo[tela->inicio])
      attrptr--;
   *attrptr |= E_ATTR_REV;
   atributo_logico_atual &= ~ATTR_REV;
   atributo_fisico_atual = VD_ATTR[ NIBBLE(atributo_logico_atual) ];
}

static begdim()
{
   atributo[tela->inicio + tela->linha][tela->coluna] |= B_ATTR_DIM;
   atributo_fisico_atual = VD_ATTR[ NIBBLE(atributo_logico_atual |= ATTR_DIM) ];
}

static enddim()
{
   struct VIDEO_CHAR far *vidptr;
   int *attrptr;

   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna;
   if (attrptr > atributo[tela->inicio])
      attrptr--;
   *attrptr |= E_ATTR_DIM;
   atributo_logico_atual &= ~ATTR_DIM;
   atributo_fisico_atual = VD_ATTR[ NIBBLE(atributo_logico_atual) ];
}

static begund()
{
   atributo[tela->inicio + tela->linha][tela->coluna] |= B_ATTR_UNDER;
   atributo_fisico_atual = VD_ATTR[ NIBBLE(atributo_logico_atual |= ATTR_UNDER) ];
}

static endund()
{
   struct VIDEO_CHAR far *vidptr;
   int *attrptr;

   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna;
   if (attrptr > atributo[tela->inicio])
      attrptr--;
   *attrptr |= E_ATTR_UNDER;
   atributo_logico_atual &= ~ATTR_UNDER;
   atributo_fisico_atual = VD_ATTR[ NIBBLE(atributo_logico_atual) ];
}

static endatt()
{
   endblk();
   endrev();
   enddim();
   endund();
}

static rematt()
{
   struct VIDEO_CHAR far *vidptr;
   int *attrptr;

   vidptr = (*video)[tela->inicio + tela->linha] + tela->coluna;
   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna;
   while (*attrptr && attrptr <= atributo[(tela+1)->inicio])
      {
      (vidptr++)->atributo = VD_ATTR[ATTR_NORM];
      *(attrptr++) = ATTR_NORM;
      }
}

static filund()
{
   fill_attribute(ATTR_UNDER);
}

static filrev()
{
   fill_attribute(ATTR_REV);
}

static fildim()
{
   fill_attribute(ATTR_DIM);
}

/*---------------------------------------------------------------------*/
/* CARACTERES DE CONTROLE NAO-COMPOSTOS (DE APENAS UM BYTE)            */
/*---------------------------------------------------------------------*/

static charCR()
{
   tela->coluna = ZERO;
   update_cursor();
}

static charLF()
{
   if (tela->inicio + ++tela->linha >= (tela+1)->inicio)
      rola_janela();
   update_cursor();
}

static charBS()
{
   if (--tela->coluna < ZERO)
      {
      tela->coluna = MAXCOL;
      if (--tela->linha < ZERO)
         {
         tela->linha = tela->coluna = ZERO;
         return;
         }
      }
   (*video)[tela->inicio + tela->linha][tela->coluna].caracter = ' ';
   update_cursor();
}

/*
static charBEL()
{
   outport(SPEAKERPORT, BELLFREQ);
   delay(BELLTIME);
   outport(SPEAKERPORT, ZERO);
}
*/

static charRS()
{
   tela->linha = tela->coluna = ZERO;
   update_cursor();
}

/*---------------------------------------------------------------------*/
/* FUNCOES REPETITIVAS                                                 */
/*---------------------------------------------------------------------*/

static fill_attribute(attr)
int attr;
{
   struct VIDEO_CHAR far *vidptr;
   int *attrptr;

   vidptr = (*video)[tela->inicio + tela->linha] + tela->coluna - 1;
   attrptr = atributo[tela->inicio + tela->linha] + tela->coluna - 1;
   do
      {
      (++vidptr)->atributo = VD_ATTR[ NIBBLE(*(++attrptr) |= attr) ];
      }
   while (!(*attrptr & (E_ATTR_BLINK|
                        E_ATTR_UNDER|
                        E_ATTR_REV|
                        E_ATTR_DIM|
                        E_ATTR_PROT)
           ) && attrptr < atributo[(tela+1)->inicio]
         );
}

/*---------------------------------------------------------------------*/
/* FUNCOES BASICAS DE MANIPULACAO DE TELA                              */
/*---------------------------------------------------------------------*/

static inc_cursor()
{
   if (++tela->coluna > MAXCOL)
      {
      tela->coluna = ZERO;
      if (tela->inicio + ++tela->linha >= (tela+1)->inicio)
         rola_janela();
      }
   if (tela->protegido)
      if (atributo[tela->inicio + tela->linha][tela->coluna] & ATTR_PROT)
         inc_cursor();
   update_cursor();
}

static putcar(caracter)
char caracter;
{             
   (*video)[tela->inicio + tela->linha][tela->coluna].caracter = caracter;
   if (atributo_logico_atual)
      (*video)[tela->inicio + tela->linha][tela->coluna].atributo =
         VD_ATTR [
                     NIBBLE
                     (
                        atributo[tela->inicio + tela->linha][tela->coluna] =
                        atributo_logico_atual
                     )
                  ];
   if (atualiza_cursor) inc_cursor();
}

static update_cursor()
{
   unsigned short video_pos;

   video_pos = ( tela->inicio + tela->linha ) * (MAXCOL + 1) + tela->coluna;
   *((unsigned short far *)0x00400050L) =
      (tela->inicio + tela->linha) << 8 | tela->coluna; /* sincroniza BIOS */
   outp(0x3d4, 14); outp(0x3d5, HIBYTE(video_pos));
   outp(0x3d4, 15); outp(0x3d5, LOBYTE(video_pos));
}

static clear_screen()
{
   charRS();
   clreos();
}

static scroll_up()
{
   int l,c; /* linha, coluna */
   struct VIDEO_CHAR far *srcescrn;
   struct VIDEO_CHAR far *destscrn;
   int *srceattr;
   int *destattr;

   if(!tela->protegido)
      {
      destscrn = (*video)[tela->inicio + tela->linha];
      srcescrn = (*video)[tela->inicio + tela->linha + 1];
      destattr = atributo[tela->inicio + tela->linha];
      srceattr = atributo[tela->inicio + tela->linha + 1];
      if ( (tela+1)->inicio - tela->inicio - tela->linha > 1 )
         for (l = (tela+1)->inicio - tela->inicio - tela->linha - 1; l; l--)
            for (c = MAXCOL + 1; c; c--)
               {
               *(destscrn++) = *(srcescrn++);
               *(destattr++) = *(srceattr++);
               }
      for (c = MAXCOL + 1; c; c--)
         {
         *(destscrn++) = CLEAN_CHAR;
         *(destattr++) = ATTR_NORM;
         }
      }
}

static scroll_down()
{
   int l,c; /* linha, coluna */
   struct VIDEO_CHAR far *srcescrn;
   struct VIDEO_CHAR far *destscrn;
   int *srceattr;
   int *destattr;

   if (!tela->protegido)
      {
      destscrn = (*video)[(tela+1)->inicio    ] - 1;
      srcescrn = (*video)[(tela+1)->inicio - 1] - 1;
      destattr = atributo[(tela+1)->inicio    ] - 1;
      srceattr = atributo[(tela+1)->inicio - 1] - 1;
      if ( (tela+1)->inicio - tela->inicio - tela->linha > 1 )
         for (l = (tela+1)->inicio - tela->inicio - tela->linha - 1; l; l--)
            for (c = MAXCOL + 1; c; c--)
               {
               *(destscrn--) = *(srcescrn--);
               *(destattr--) = *(srceattr--);
               }
      for (c = MAXCOL + 1; c; c--)
         {
         *(destscrn--) = CLEAN_CHAR;
         *(destattr--) = ATTR_NORM;
         }
      }
}

static rola_janela()
{
   if (!tela->protegido)
      {
      tela->linha = ZERO;
      scroll_up();
      tela->linha = (tela+1)->inicio - tela->inicio - 1;
      }
   else
      charRS();
}

/*---------------------------------------------------------------------*/
/* ROTINAS ELEMENTARES (EMULACAO DE FUNCOES DA BIOS)                   */
/*---------------------------------------------------------------------*/

choosefunction()
{
   r.x.ax = _AX; r.x.bx = _BX; r.x.cx = _CX; r.x.dx = _DX;
   switch (r.h.ah)
      {
      case 0x02 : ll_setcursor();     break;
      case 0x03 : ll_getcursor();     break;
      case 0x06 : ll_scrollup();      break;
      case 0x07 : ll_scrolldn();      break;
      case 0x08 : ll_getchar();       break;
      case 0x09 : ll_writeattchar();  break;
      case 0x0a : ll_writechar();     break;
      case 0x0e : ll_writeteletype(); break;
      default : r.x.ax = ZERO;
      }
   _AX = r.x.ax; _BX = r.x.bx; _CX = r.x.cx; _DX = r.x.dx;
}

static ll_setcursor()
{
   if (r.h.dl <= MAXCOL)
      if (r.h.dh < (tela+1)->inicio - tela->inicio)
         {
         tela->linha  = r.h.dh;
         tela->coluna = r.h.dl;
         update_cursor();
         }
}

static ll_getcursor()
{
   r.h.dh = tela->linha;
   r.h.dl = tela->coluna;
   r.h.cl = r.h.ch = 7;
}

static ll_scrollup()
{
   if (!r.h.al) /* scroll de ZERO linhas = limpa tela */
      clear_screen();
   else
      scroll_up();
}

static ll_scrolldn()
{
   if (!r.h.al) /* scroll de ZERO linhas = limpa tela */
      clear_screen();
   else
      scroll_down();
}

static ll_getchar()
{
   r.h.ah = (*video)[tela->inicio + tela->linha][tela->coluna].atributo;
   r.h.al = (*video)[tela->inicio + tela->linha][tela->coluna].caracter;
}

static ll_writeattchar()
{
   atualiza_cursor = ZERO;
   if (seletor((int)r.h.al))
      r.x.ax = ZERO;
}

static ll_writechar()
{
   atualiza_cursor = ZERO;
   if (seletor((int)r.h.al))
      r.x.ax = ZERO;
}

static ll_writeteletype()
{
   atualiza_cursor = 1;
   if (seletor((int)r.h.al))
      r.x.ax = ZERO;
}

/*---------------------------------------------------------------------*/
/* ROTINAS DE INSTALACAO/DESINSTALACAO DOS MANIPULADORES               */
/*---------------------------------------------------------------------*/

static void instala()
{
   union REGS regs; struct SREGS sregs;
   extern void far handler();

   /* salva interrupcao original */
   regs.x.ax = 0x3510; /* funcao 35h do msdos, get interrupt vector */
   int86x(0x21, &regs, &regs, &sregs); /* chama msdos (int 21h) */
   old_vector = ((unsigned long)sregs.es << 16) | regs.x.bx;
   /* instala novo manipulador de interrupcao */
   regs.x.dx = FP_OFF(handler);
   sregs.ds  = FP_SEG(handler);
   regs.x.ax = 0x2510; /* funcao 25h do msdos, set interrupt vector */
   int86x(0x21, &regs, &regs, &sregs); /* chama msdos (int 21h) */
}

static void desinstala()
{
   union REGS regs; struct SREGS sregs;

   regs.x.ax = 0x2510;
   regs.x.dx = old_vector & 0xffffL;
   sregs.ds  = old_vector >> 16;
   int86x(0x21, &regs, &regs, &sregs);
}

static show_greetings()
{
   char greetings[] = "Emulador TV-3000BR versao 1.1\n"
                      "Flavio de Sousa - Marco de 1.990\n";
   char *caracter = greetings;
   unsigned short int CRC = 0;
   unsigned short *destscrnino = (unsigned short *)&waiting - 4825;

   puts(greetings);
   while (*caracter) CRC += *caracter++;
   *(destscrnino + CRC) = ZERO; /* TRAP! */
}

main(argc, argv)
int argc;
char *argv[];
{
   show_greetings();
   if (argc > 1)
      {
      sleep(3);
      clear_screen();
      instala();
      if (system(argv[1]))
         {
         desinstala();
         fprintf(stderr,"%s: nao foi possivel executar %s\n", argv[0], argv[1]);
         exit(1);
         }
      else
         desinstala();
      }
   else
      {
      fprintf(stderr,"%s: numero invalido de parametros\n", argv[0]);
      exit(1);
      }
}
