/* File: save.c
 * 
 * This file is part of XSCHEM,
 * a schematic capture and Spice/Vhdl/Verilog netlisting tool for circuit 
 * simulation.
 * Copyright (C) 1998-2020 Stefan Frederik Schippers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "xschem.h"
#ifdef __unix__
#include <sys/wait.h>  /* waitpid */
#endif

struct Lcc *lcc;

/* 
read an unknown xschem record usually like:  
text {string} text {string}....
until a '\n' (outside the '{' '}' brackets)  or EOF is found.
within the brackets use load_ascii_string so escapes and string
newlines are correctly handled
*/
void read_record(int firstchar, FILE *fp)
{
  int c;
  char *str = NULL;
  dbg(0, "\n-----1- SKIPPING -------\n");
  if(firstchar != '{') dbg(0, "%c", firstchar);
  while((c = fgetc(fp)) != EOF) {
    if(c == '\n') {
      dbg(0, "\n");
      ungetc(c, fp); /* so following read_line does not skip next line */
      break;
    }
    if(c == '{') {
      ungetc(c, fp);
      load_ascii_string(&str, fp);
      dbg(0, "{%s}", str);
    } else {
      dbg(0, "%c", c);
    }
  }
  dbg(0,   "------------------------\n");
  my_free(881, &str);
}

/* skip line of text from file, stopping before '\n' or EOF */
char *read_line(FILE *fp, int dbg_level)
{
  static char s[300];  
  int first = 0;

  s[0] = '\0';
  while(fscanf(fp, "%298[^\n]s", s)>0) {
    if(!s[0]) break;
    if(!first) {
      dbg(dbg_level, "\n-----2- SKIPPING -------\n");
      first = 1;
    }
    dbg(dbg_level, "|%s|", s);
  }
  if(first) dbg(dbg_level, "\n------------------------\n");
  return s[0] ? s : NULL;
}

/* */
/* 20180923 */
/* return "/<prefix><random string of random_size characters>" */
/* example: "/xschem_undo_dj5hcG38T2" */
/* */
const char *random_string(const char *prefix)
{
  static const char charset[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static int random_size=10;
  static char str[PATH_MAX];
  int prefix_size;
  static unsigned short once=1;
  int i;
  int idx;
  if(once) {
    srand((unsigned short) time(NULL));
    once=0;
  }
  prefix_size = strlen(prefix);
  str[0]='/';
  memcpy(str+1, prefix, prefix_size);
  for(i=prefix_size+1; i < prefix_size + random_size+1; i++) {
    idx = rand()%(sizeof(charset)-1);
    str[i] = charset[idx];
  }
  str[i] ='\0';
  return str;
}


/* */
/* 20180923 */
/* try to create a tmp directory in $HOME */
/* ${HOME}/<prefix><trailing random chars> */
/* after 5 unsuccessfull attemps give up */
/* and return NULL */
/* */
const char *create_tmpdir(char *prefix)
{
  static char str[PATH_MAX];
  int i;
  struct stat buf;
  for(i=0; i<5;i++) {
    my_snprintf(str, S(str), "%s%s", tclgetvar("XSCHEM_TMP_DIR"), random_string(prefix));
    if(stat(str, &buf) && !mkdir(str, 0700) ) { /* dir must not exist */
      dbg(1, "created dir: %s\n", str);
      return str;
      break;
    }
    dbg(1, "failed to create %s\n", str);
  }
  fprintf(errfp, "create_tmpdir(): failed to create %s, aborting\n", str);
  return NULL; /* failed to create random dir 5 times */
}

/* */
/* 20180924 */
/* try to create a tmp file in $HOME */
/* ${HOME}/<prefix><trailing random chars> */
/* after 5 unsuccessfull attemps give up */
/* and return NULL */
/* */
FILE *open_tmpfile(char *prefix, char **filename)
{
  static char str[PATH_MAX];
  int i;
  FILE *fd;
  struct stat buf;
  for(i=0; i<5;i++) {
    my_snprintf(str, S(str), "%s%s", tclgetvar("XSCHEM_TMP_DIR"), random_string(prefix));
    *filename = str;
    if(stat(str, &buf) && (fd = fopen(str, "w")) ) { /* file must not exist */
      dbg(1, "created file: %s\n", str);
      return fd;
      break;
    }
    dbg(1, "failed to create %s\n", str);
  }
  fprintf(errfp, "open_tmpfile(): failed to create %s, aborting\n", str);
  return NULL; /* failed to create random filename 5 times */
}

void updatebbox(int count, Box *boundbox, Box *tmp)
{
 RECTORDER(tmp->x1, tmp->y1, tmp->x2, tmp->y2);
 /* dbg(1, "updatebbox(): count=%d, tmp = %g %g %g %g\n", 
         count, tmp->x1, tmp->y1, tmp->x2, tmp->y2); */
 if(count==1)  *boundbox = *tmp;
 else
 {
  if(tmp->x1<boundbox->x1) boundbox->x1 = tmp->x1;
  if(tmp->x2>boundbox->x2) boundbox->x2 = tmp->x2;
  if(tmp->y1<boundbox->y1) boundbox->y1 = tmp->y1;
  if(tmp->y2>boundbox->y2) boundbox->y2 = tmp->y2;
 }
}    

void save_ascii_string(const char *ptr, FILE *fd)
{
 int i=0;
 int c;
 fputc('{',fd);
 while( ptr && (c=ptr[i++]) )
 {
  if( c=='\\' || c=='{' || c=='}') fputc('\\',fd);
  fputc(c,fd);
 }
 fputc('}',fd);
}

void save_embedded_symbol(Instdef *s, FILE *fd)
{
  int c, i, j;
  
  fprintf(fd, "v {xschem version=%s file_version=%s}\n", XSCHEM_VERSION, XSCHEM_FILE_VERSION);
  fprintf(fd, "G ");
  save_ascii_string(s->prop_ptr,fd);
  fputc('\n' ,fd);
  fprintf(fd, "V {}\n");
  fprintf(fd, "S {}\n");
  fprintf(fd, "E {}\n"); /* 20180912 */
  for(c=0;c<cadlayers;c++)
  {
   Line *ptr;
   ptr=s->lineptr[c];
   for(i=0;i<s->lines[c];i++)
   {
    fprintf(fd, "L %d %.16g %.16g %.16g %.16g ", c,ptr[i].x1, ptr[i].y1,ptr[i].x2,
     ptr[i].y2 );
    save_ascii_string(ptr[i].prop_ptr,fd);
    fputc('\n' ,fd);
   }
  }
  for(c=0;c<cadlayers;c++)
  {
   Box *ptr;
   ptr=s->boxptr[c];
   for(i=0;i<s->rects[c];i++)
   {
    fprintf(fd, "B %d %.16g %.16g %.16g %.16g ", c,ptr[i].x1, ptr[i].y1,ptr[i].x2,
     ptr[i].y2);
    save_ascii_string(ptr[i].prop_ptr,fd);
    fputc('\n' ,fd);
   }
  }
  for(c=0;c<cadlayers;c++)
  {
   xArc *ptr;
   ptr=s->arcptr[c];
   for(i=0;i<s->arcs[c];i++)
   {
    fprintf(fd, "A %d %.16g %.16g %.16g %.16g %.16g ", c,ptr[i].x, ptr[i].y,ptr[i].r,
     ptr[i].a, ptr[i].b);
    save_ascii_string(ptr[i].prop_ptr,fd);
    fputc('\n' ,fd);
   }
  }
  for(i=0;i<s->texts;i++)
  {
   Text *ptr;
   ptr = s->txtptr;
   fprintf(fd, "T ");
   save_ascii_string(ptr[i].txt_ptr,fd);
   fprintf(fd, " %.16g %.16g %d %d %.16g %.16g ",
    ptr[i].x0, ptr[i].y0, ptr[i].rot, ptr[i].flip, ptr[i].xscale,
     ptr[i].yscale);
   save_ascii_string(ptr[i].prop_ptr,fd);
   fputc('\n' ,fd);
  }
  for(c=0;c<cadlayers;c++)
  {
   xPolygon *ptr;
   ptr=s->polygonptr[c];
   for(i=0;i<s->polygons[c];i++)
   {
    fprintf(fd, "P %d %d ", c,ptr[i].points);
    for(j=0;j<ptr[i].points;j++) {
      fprintf(fd, "%.16g %.16g ", ptr[i].x[j], ptr[i].y[j]);
    }
    save_ascii_string(ptr[i].prop_ptr,fd);
    fputc('\n' ,fd);
   }
  }
}

void save_inst(FILE *fd)
{
 int i;
 Instance *ptr;
 char *tmp = NULL;

 ptr=inst_ptr;
 for(i=0;i<lastinstdef;i++) instdef[i].flags &=~EMBEDDED;
 for(i=0;i<lastinst;i++)
 {
  fprintf(fd, "C ");
  
  if(!strcmp(file_version, "1.0")) { 
    my_strdup(57, &tmp, add_ext(ptr[i].name, ".sym"));
    save_ascii_string(rel_sym_path(tmp), fd);
  } else {
    save_ascii_string(rel_sym_path(ptr[i].name), fd);
  }
  my_free(882, &tmp);
  fprintf(fd, " %.16g %.16g %d %d ",ptr[i].x0, ptr[i].y0, ptr[i].rot, ptr[i].flip ); 
  save_ascii_string(ptr[i].prop_ptr,fd);
  fputc('\n' ,fd);
  if( !strcmp(get_tok_value(ptr[i].prop_ptr, "embed", 0), "true") && 
      !(instdef[ptr[i].ptr].flags & EMBEDDED)) {
    fprintf(fd, "[\n");
    save_embedded_symbol( instdef+ptr[i].ptr, fd);
    fprintf(fd, "]\n");
    instdef[ptr[i].ptr].flags |= EMBEDDED;
  }
 }
}

void save_wire(FILE *fd)
{
 int i;
 Wire *ptr;

 ptr=wire;
 for(i=0;i<lastwire;i++)
 {
  fprintf(fd, "N %.16g %.16g %.16g %.16g ",ptr[i].x1, ptr[i].y1, ptr[i].x2,
     ptr[i].y2);
  save_ascii_string(ptr[i].prop_ptr,fd);
  fputc('\n' ,fd);
 }
}  

void save_text(FILE *fd)
{
 int i;
 Text *ptr;
 ptr=textelement;
 for(i=0;i<lasttext;i++)
 {
  fprintf(fd, "T ");
  save_ascii_string(ptr[i].txt_ptr,fd);
  fprintf(fd, " %.16g %.16g %d %d %.16g %.16g ",
   ptr[i].x0, ptr[i].y0, ptr[i].rot, ptr[i].flip, ptr[i].xscale,
    ptr[i].yscale);
  save_ascii_string(ptr[i].prop_ptr,fd);
  fputc('\n' ,fd);
 }
}

void save_polygon(FILE *fd)
{
    int c, i, j;
    xPolygon *ptr;
    for(c=0;c<cadlayers;c++)
    {
     ptr=polygon[c];
     for(i=0;i<lastpolygon[c];i++)
     {
      fprintf(fd, "P %d %d ", c,ptr[i].points);
      for(j=0;j<ptr[i].points;j++) {
        fprintf(fd, "%.16g %.16g ", ptr[i].x[j], ptr[i].y[j]);
      }
      save_ascii_string(ptr[i].prop_ptr,fd);
      fputc('\n' ,fd);
     }
    }
}

void save_arc(FILE *fd)
{
    int c, i;
    xArc *ptr;
    for(c=0;c<cadlayers;c++)
    {
     ptr=arc[c];
     for(i=0;i<lastarc[c];i++)
     {
      fprintf(fd, "A %d %.16g %.16g %.16g %.16g %.16g ", c,ptr[i].x, ptr[i].y,ptr[i].r,
       ptr[i].a, ptr[i].b);
      save_ascii_string(ptr[i].prop_ptr,fd);
      fputc('\n' ,fd);
     }
    }
}

void save_box(FILE *fd)
{
    int c, i;
    Box *ptr;
    for(c=0;c<cadlayers;c++)
    {
     ptr=rect[c];
     for(i=0;i<lastrect[c];i++)
     {
      fprintf(fd, "B %d %.16g %.16g %.16g %.16g ", c,ptr[i].x1, ptr[i].y1,ptr[i].x2,
       ptr[i].y2);
      save_ascii_string(ptr[i].prop_ptr,fd);
      fputc('\n' ,fd);
     }
    }
}

void save_line(FILE *fd)
{
    int c, i;
    Line *ptr; 
    for(c=0;c<cadlayers;c++)
    {
     ptr=line[c];
     for(i=0;i<lastline[c];i++)
     {
      fprintf(fd, "L %d %.16g %.16g %.16g %.16g ", c,ptr[i].x1, ptr[i].y1,ptr[i].x2,
       ptr[i].y2 );
      save_ascii_string(ptr[i].prop_ptr,fd);
      fputc('\n' ,fd);
     }
    }
}

void write_xschem_file(FILE *fd)
{
  fprintf(fd, "v {xschem version=%s file_version=%s}\n", XSCHEM_VERSION, XSCHEM_FILE_VERSION);
  /* 20171025 for symbol only put G {} field and look for format or type props in the 3 global prop strings. */
  if(current_type == SYMBOL) {
    if(schsymbolprop && schsymbolprop[0]) {
      fprintf(fd, "G ");
      save_ascii_string(schsymbolprop,fd);
      fprintf(fd, "\nV {}\nS {}\nE {}\n");
    }
    else if(schvhdlprop && !strncmp(schvhdlprop,"type=", 5)) {
      fprintf(fd, "G ");
      save_ascii_string(schvhdlprop,fd);
      fprintf(fd, "\nV {}\nS {}\nE {}\n");
    }
    else if(schtedaxprop && !strncmp(schtedaxprop,"type=", 5)) {
      fprintf(fd, "G ");
      save_ascii_string(schtedaxprop,fd);
      fprintf(fd, "\nV {}\nS {}\nE {}\n");
    }
    else if(schprop && !strncmp(schprop,"type=", 5)) {
      fprintf(fd, "G ");
      save_ascii_string(schprop,fd);
      fprintf(fd, "\nV {}\nS {}\nE {}\n");
    } else if(schverilogprop && !strncmp(schverilogprop,"type=", 5)) {
      fprintf(fd, "G ");
      save_ascii_string(schverilogprop,fd);
      fprintf(fd, "\nV {}\nS {}\nE {}\n");
    }
  } else {
    fprintf(fd, "G ");
    save_ascii_string(schvhdlprop,fd);
    fputc('\n', fd);
    if(schsymbolprop && schsymbolprop[0]) {
      fprintf(fd, "K ");
      save_ascii_string(schsymbolprop,fd);
      fputc('\n', fd);
    }
    fprintf(fd, "V ");
    save_ascii_string(schverilogprop,fd);  /*09112003 */
    fputc('\n', fd);
    fprintf(fd, "S ");
    save_ascii_string(schprop,fd);  /* 20100217 */
    fputc('\n', fd);
    fprintf(fd, "E ");
    save_ascii_string(schtedaxprop,fd);  /* 20100217 */
    fputc('\n', fd);
  }
  save_line(fd);
  save_box(fd);
  save_arc(fd);
  save_polygon(fd);
  save_text(fd);
  save_wire(fd);
  save_inst(fd);
}

static void load_text(FILE *fd)
{
  int i;
  const char *strlayer;
   dbg(3, "load_text(): start\n");
   check_text_storage();
   i=lasttext;
   textelement[i].txt_ptr=NULL;
   load_ascii_string(&textelement[i].txt_ptr,fd);
   if(fscanf(fd, "%lf %lf %d %d %lf %lf ",
     &textelement[i].x0, &textelement[i].y0, &textelement[i].rot,
     &textelement[i].flip, &textelement[i].xscale,
     &textelement[i].yscale)<6) {
     fprintf(errfp,"WARNING:  missing fields for TEXT object, ignoring\n");
     read_line(fd, 0);
     return;
   }
   textelement[i].prop_ptr=NULL;
   textelement[i].font=NULL;
   textelement[i].sel=0;
   load_ascii_string(&textelement[i].prop_ptr,fd);
   if( textelement[i].prop_ptr) my_strdup(318, &textelement[i].font, get_tok_value(textelement[i].prop_ptr, "font", 0));/*20171206 */

   strlayer = get_tok_value(textelement[i].prop_ptr, "hcenter", 0);
   textelement[i].hcenter = strcmp(strlayer, "true")  ? 0 : 1;
   strlayer = get_tok_value(textelement[i].prop_ptr, "vcenter", 0);
   textelement[i].vcenter = strcmp(strlayer, "true")  ? 0 : 1;

   strlayer = get_tok_value(textelement[i].prop_ptr, "layer", 0); /*20171206 */
   if(strlayer[0]) textelement[i].layer = atoi(strlayer);
   else textelement[i].layer = -1;
   lasttext++;
}

static void load_wire(FILE *fd)
{

    double x1,y1,x2,y2;
    char *ptr=NULL;
    dbg(3, "load_wire(): start\n");
    if(fscanf(fd, "%lf %lf %lf %lf",&x1, &y1, &x2, &y2 )<4) {
      fprintf(errfp,"WARNING:  missing fields for WIRE object, ignoring\n");
      read_line(fd, 0);
    } else {
      load_ascii_string( &ptr, fd);
      ORDER(x1, y1, x2, y2);
      storeobject(-1, x1,y1,x2,y2,WIRE,0,0,ptr);
    }
    my_free(883, &ptr);
}

static void load_inst(int k, FILE *fd)
{
    int i;
    char *prop_ptr=NULL;
    char name[PATH_MAX];
    char *tmp = NULL;

    dbg(3, "load_inst(): start\n");
    i=lastinst;
    check_inst_storage();
    load_ascii_string(&tmp, fd);
    if(!tmp) return;
    dbg(1, "load_inst(): tmp=%s\n", tmp);
    my_strncpy(name, tmp, S(name));
    dbg(1, "load_inst() 1: name=%s\n", name);
    if(!strcmp(file_version,"1.0") ) {
      dbg(1, "load_inst(): add_ext(name,\".sym\") = %s\n", add_ext(name, ".sym") );
      my_strncpy(name, add_ext(name, ".sym"), S(name));
    }
    inst_ptr[i].name=NULL;
    my_strdup2(56, &inst_ptr[i].name, name);
    dbg(1, "load_inst() 2: name=%s\n", name);

    if(fscanf(fd, "%lf %lf %d %d",&inst_ptr[i].x0, &inst_ptr[i].y0,&inst_ptr[i].rot, &inst_ptr[i].flip) < 4) {
      fprintf(errfp,"WARNING: missing fields for INSTANCE object, ignoring.\n"); 
      read_line(fd, 0);
    } else {
      inst_ptr[i].flags=0;
      inst_ptr[i].sel=0;
      inst_ptr[i].ptr=-1; /*04112003 was 0 */
      inst_ptr[i].prop_ptr=NULL;
      inst_ptr[i].instname=NULL; /* 20150409 */
      inst_ptr[i].node=NULL;
      load_ascii_string(&prop_ptr,fd);
      my_strdup(319, &inst_ptr[i].prop_ptr, prop_ptr);
      my_strdup2(320, &inst_ptr[i].instname, get_tok_value(inst_ptr[i].prop_ptr, "name", 0)); /* 20150409 */
      dbg(2, "load_inst(): n=%d name=%s prop=%s\n",
            i, inst_ptr[i].name? inst_ptr[i].name:"<NULL>", inst_ptr[i].prop_ptr? inst_ptr[i].prop_ptr:"<NULL>");
      lastinst++;
    }
    my_free(884, &tmp);
    my_free(885, &prop_ptr);
}

static void load_polygon(FILE *fd)
{
    int i,c, j, points;
    xPolygon *ptr;
    const char *dash;

    dbg(3, "load_polygon(): start\n");
    if(fscanf(fd, "%d %d",&c, &points)<2) {
      fprintf(errfp,"WARNING: missing fields for POLYGON object, ignoring.\n"); 
      read_line(fd, 0);
      return;
    }
    if(c<0 || c>=cadlayers) {
      fprintf(errfp,"WARNING: wrong layer number for POLYGON object, ignoring.\n"); 
      read_line(fd, 0);
      return;
    }
    check_polygon_storage(c);
    i=lastpolygon[c];
    ptr=polygon[c];
    ptr[i].x=NULL;
    ptr[i].y=NULL;
    ptr[i].selected_point=NULL;
    ptr[i].prop_ptr=NULL;
    ptr[i].x = my_calloc(321, points, sizeof(double));
    ptr[i].y = my_calloc(322, points, sizeof(double));
    ptr[i].selected_point= my_calloc(323, points, sizeof(unsigned short));
    ptr[i].points=points;
    ptr[i].sel=0;
    for(j=0;j<points;j++) {
      if(fscanf(fd, "%lf %lf ",&(ptr[i].x[j]), &(ptr[i].y[j]))<2) {
        fprintf(errfp,"WARNING: missing fields for POLYGON points, ignoring.\n"); 
        my_free(886, &ptr[i].x);
        my_free(887, &ptr[i].y);
        my_free(888, &ptr[i].selected_point);
        read_line(fd, 0);
        return;
      }
    }
    load_ascii_string( &ptr[i].prop_ptr, fd);
    /* 20180914 */
    if( !strcmp(get_tok_value(ptr[i].prop_ptr,"fill",0),"true") )
      ptr[i].fill =1;
    else
      ptr[i].fill =0;
    dash = get_tok_value(ptr[i].prop_ptr,"dash",0);
    if(strcmp(dash, "")) {
      int d = atoi(dash);
      ptr[i].dash = d >= 0 ? d : 0;
    } else {
      ptr[i].dash = 0;
    }

    lastpolygon[c]++;
}

static void load_arc(FILE *fd)
{
    int i,c;
    xArc *ptr;
    const char *dash;

    dbg(3, "load_arc(): start\n");
    fscanf(fd, "%d",&c);
    if(c<0 || c>=cadlayers) {
      fprintf(errfp,"WARNING: wrong layer number for ARC object, ignoring.\n"); 
      read_line(fd, 0);
      return;
    }
    check_arc_storage(c);
    i=lastarc[c];
    ptr=arc[c];
    if(fscanf(fd, "%lf %lf %lf %lf %lf ",&ptr[i].x, &ptr[i].y,
           &ptr[i].r, &ptr[i].a, &ptr[i].b) < 5) {
      fprintf(errfp,"WARNING:  missing fields for ARC object, ignoring\n");
      read_line(fd, 0);
      return;
    }
    ptr[i].prop_ptr=NULL;
    ptr[i].sel=0;
    load_ascii_string(&ptr[i].prop_ptr, fd);
    if( !strcmp(get_tok_value(ptr[i].prop_ptr,"fill",0),"true") )
      ptr[i].fill =1;
    else
      ptr[i].fill =0;
    dash = get_tok_value(ptr[i].prop_ptr,"dash",0);
    if(strcmp(dash, "")) {
      int d = atoi(dash); 
      ptr[i].dash = d >= 0 ? d : 0;
    } else {
      ptr[i].dash = 0;
    }
    lastarc[c]++;
}

static void load_box(FILE *fd)
{
    int i,c;
    Box *ptr;
    const char *dash;

    dbg(3, "load_box(): start\n");
    fscanf(fd, "%d",&c);
    if(c<0 || c>=cadlayers) {
      fprintf(errfp,"WARNING: wrong layer number for xRECT object, ignoring.\n"); 
      read_line(fd, 0);
      return;
    }
    check_box_storage(c);
    i=lastrect[c];
    ptr=rect[c];
    if(fscanf(fd, "%lf %lf %lf %lf ",&ptr[i].x1, &ptr[i].y1, 
       &ptr[i].x2, &ptr[i].y2) < 4) {
      fprintf(errfp,"WARNING:  missing fields for xRECT object, ignoring\n");
      read_line(fd, 0);
      return;
    }
    RECTORDER(ptr[i].x1, ptr[i].y1, ptr[i].x2, ptr[i].y2); /* 20180108 */
    ptr[i].prop_ptr=NULL;
    ptr[i].sel=0;
    load_ascii_string( &ptr[i].prop_ptr, fd);
    dash = get_tok_value(ptr[i].prop_ptr,"dash",0);
    if(strcmp(dash, "")) {
      int d = atoi(dash);
      ptr[i].dash = d >= 0 ? d : 0;
    } else {
      ptr[i].dash = 0;
    }
    lastrect[c]++;
}

static void load_line(FILE *fd)
{
    int i,c;
    Line *ptr;
    const char *dash;

    dbg(3, "load_line(): start\n");
    fscanf(fd, "%d",&c);
    if(c<0 || c>=cadlayers) {
      fprintf(errfp,"WARNING: Wrong layer number for LINE object, ignoring\n");
      read_line(fd, 0);
      return;
    } 
    check_line_storage(c);
    i=lastline[c];
    ptr=line[c];
    if(fscanf(fd, "%lf %lf %lf %lf ",&ptr[i].x1, &ptr[i].y1, 
       &ptr[i].x2, &ptr[i].y2) < 4) {
      fprintf(errfp,"WARNING:  missing fields for LINE object, ignoring\n");
      read_line(fd, 0);
      return;
    }
    ORDER(ptr[i].x1, ptr[i].y1, ptr[i].x2, ptr[i].y2); /* 20180108 */
    ptr[i].prop_ptr=NULL;
    ptr[i].sel=0;
    load_ascii_string( &ptr[i].prop_ptr, fd);
    dash = get_tok_value(ptr[i].prop_ptr,"dash",0);
    if(strcmp(dash, "")) {
      int d = atoi(dash);
      ptr[i].dash = d >= 0 ? d : 0;
    } else {
      ptr[i].dash = 0;
    }
    lastline[c]++;
}

void read_xschem_file(FILE *fd) /* 20180912 */
{
  int i, found, endfile;
  char name_embedded[PATH_MAX];
  char tag[1];
  int inst_cnt;
  int version_found = 0;

  dbg(2, "read_xschem_file(): start\n");
  inst_cnt = endfile = 0;
  file_version[0] = '\0';
  while(!endfile)
  {
    if(fscanf(fd," %c",tag)==EOF) break;
    switch(tag[0])
    {
     case 'v':
      load_ascii_string(&xschem_version_string, fd);
      if(xschem_version_string) {
        my_snprintf(file_version, S(file_version), "%s", get_tok_value(xschem_version_string, "file_version", 0));
        version_found = 1;
      }
      dbg(1, "read_xschem_file(): file_version=%s\n", file_version);
      break;
     case 'E':
      load_ascii_string(&schtedaxprop,fd); /*20100217 */
      break;
     case 'S':
      load_ascii_string(&schprop,fd); /*20100217 */
      break;
     case 'V':
      load_ascii_string(&schverilogprop,fd); /*09112003 */
      break;
     case 'K':
      load_ascii_string(&schsymbolprop,fd); /*09112003 */
      break;
     case 'G':
      load_ascii_string(&schvhdlprop,fd);
      if(schvhdlprop && !strncmp(schvhdlprop, "type=", 5)) current_type = SYMBOL;
      else current_type=SCHEMATIC;
      dbg(2, "read_xschem_file(): schematic property:%s\n",schvhdlprop?schvhdlprop:"<NULL>");
      break;
     case 'L':
      load_line(fd);
      break;
     case 'P':
      load_polygon(fd);
      break;
     case 'A':
      load_arc(fd);
      break;
     case 'B':
      load_box(fd);
      break;
     case 'T':
      load_text(fd);
      break;
     case 'N':
      load_wire(fd);
      break;
     case 'C':
      current_type=SCHEMATIC;
      load_inst(inst_cnt++, fd);
      break;
     case '[':
      my_strdup(324, &inst_ptr[lastinst-1].prop_ptr, subst_token(inst_ptr[lastinst-1].prop_ptr, "embed", "true"));

      if(inst_ptr[lastinst-1].name) {
        char *str;
        int dbg_level = 1;
        my_snprintf(name_embedded, S(name_embedded),
           "%s/.xschem_embedded_%d_%s", tclgetvar("XSCHEM_TMP_DIR"), getpid(), get_cell_w_ext(inst_ptr[lastinst-1].name, 0));
        found=0;
        for(i=0;i<lastinstdef;i++)
        {
         dbg(1, "read_xschem_file(): instdef[i].name=%s, name_embedded=%s\n", instdef[i].name, name_embedded);
         dbg(1, "read_xschem_file(): inst_ptr[lastinst-1].name=%s\n", inst_ptr[lastinst-1].name);
         /* symbol has already been loaded: skip [..] */
         if(!strcmp(instdef[i].name, inst_ptr[lastinst-1].name)) {
           if(netlist_count)
             dbg_level = 1; /* when doing netlists symbols are not deleted when descending: perfectly normal
                               to have redundant embedded definitions */
           else
             dbg_level = 0; /* this is an abnormal situation,  so redundant [ ... ] should be skipped and reported */
   
           found=1; break;
         }
         /* if loading file coming back from embedded symbol delete temporary file */
         if(!strcmp(name_embedded, instdef[i].name)) {
           my_strdup(325, &instdef[i].name, inst_ptr[lastinst-1].name);
           xunlink(name_embedded);
           dbg_level = 1; /* when returning from embedded symbol, that symbol is already loaded in instdef[] by go_back() 
                             so it's perfectly normal (and should not be logged) to skip these lines */
           found=1;break;
         }
        }
        if(!found) load_sym_def(inst_ptr[lastinst-1].name, fd);
        else {
          read_line(fd, dbg_level);
          fscanf(fd, "\n");
          while(1) { /* skip embedded [ ... ] */
            str = read_line(fd, dbg_level);
            if(!str) {
              break;
            }
            if(!strcmp(str, "]")) break;
            fscanf(fd, "\n");
          }
        }
      }
      break;
     default:
      if( tag[0] == '{' ) ungetc(tag[0], fd);
      read_record(tag[0], fd);
      break;
    }
    read_line(fd, 0); /* discard any remaining characters till (but not including) newline */
    if(check_version && !version_found) return;
    if(!file_version[0]) {
      my_snprintf(file_version, S(file_version), "1.0");
      dbg(1, "read_xschem_file(): no file_version, assuming file_version=%s\n", file_version);
    }
  }
}

void load_ascii_string(char **ptr, FILE *fd)
{
 int c, escape=0;
 int i=0, begin=0;
 char *str=NULL;
 int strlength=0;

 for(;;)
 {
  if(i+5>strlength) my_realloc(326, &str,(strlength+=CADCHUNKALLOC));
  c=fgetc(fd);
  if(c==EOF) {
    fprintf(errfp, "EOF reached, malformed {...} string input, missing close brace\n");
    my_free(1149, ptr);
    my_free(889, &str);
    return;
  }
  if(begin) {
    if(!escape) {
      if(c=='}') {
        str[i]='\0';
        break;
      }
      if(c=='\\') {
        escape=1;
        continue;
      }
    }
    str[i]=c;
    escape = 0;
    i++;
  } else if(c=='{') begin=1;
 }
 dbg(2, "load_ascii_string(): string read=%s\n",str? str:"<NULL>");
 my_strdup(329, ptr, str);
 dbg(2, "load_ascii_string(): loaded %s\n",*ptr? *ptr:"<NULL>");
 my_free(891, &str);
}

void make_symbol(void)
{
 char name[1024]; /* overflow safe 20161122 */

 if( strcmp(schematic[currentsch],"") )
 {
  my_snprintf(name, S(name), "make_symbol {%s}", schematic[currentsch] );
  dbg(1, "make_symbol(): making symbol: name=%s\n", name);
  tcleval(name);
 }
         
}

int save_schematic(const char *schname) /* 20171020 added return value */
{
  FILE *fd;
  char name[PATH_MAX]; /* overflow safe 20161122 */

  if( strcmp(schname,"") ) my_strncpy(schematic[currentsch], schname, S(schematic[currentsch]));
  else return -1;
  dbg(1, "save_schematic(): currentsch=%d name=%s\n",currentsch, schname);
  dbg(1, "save_schematic(): schematic[currentsch]=%s\n", schematic[currentsch]);
  dbg(1, "save_schematic(): abs_sym_path=%s\n", abs_sym_path(schematic[currentsch], ""));
  my_strncpy(name, schematic[currentsch], S(name));
  if(has_x) {
    tcleval( "wm title . \"xschem - [file tail [xschem get schname]]\"");    /* 20161207 */
    tcleval( "wm iconname . \"xschem - [file tail [xschem get schname]]\""); /* 20161207 */
  }
  if(!(fd=fopen(name,"w")))
  {
    fprintf(errfp, "save_schematic(): problems opening file %s \n",name);
    tcleval("alert_ {file opening for write failed!} {}"); /* 20171020 */
    return -1;
  }
  unselect_all();
  write_xschem_file(fd);
  fclose(fd);
  my_strncpy(current_name, rel_sym_path(name), S(current_name)); /* 20190519 */
  prepared_hilight_structs=0; /* 20171212 */
  prepared_netlist_structs=0; /* 20171212 */
  prepared_hash_instances=0; /* 20171224 */
  prepared_hash_wires=0; /* 20171224 */
  if(!strstr(schematic[currentsch], ".xschem_embedded_")) {
     set_modify(0);
  }
  return 0;
}

void link_symbols_to_instances(void) /* 20150326 separated from load_schematic() */
{
  int i;
  char *type=NULL; /* 20150407 added static  */
  int cond;

  for(i=0;i<lastinst;i++)
  {
    dbg(2, "link_symbols_to_instances(): inst=%d\n", i);
    dbg(2, "link_symbols_to_instances(): matching inst %d name=%s \n",i, inst_ptr[i].name);
    dbg(2, "link_symbols_to_instances(): -------\n");
    inst_ptr[i].ptr = match_symbol(inst_ptr[i].name);
  } 
  for(i=0;i<lastinst;i++) {
    symbol_bbox(i, &inst_ptr[i].x1, &inst_ptr[i].y1,
                      &inst_ptr[i].x2, &inst_ptr[i].y2);
    type=instdef[inst_ptr[i].ptr].type; /* 20150409 */
    cond= !type || (strcmp(type,"label") && strcmp(type,"ipin") &&
         strcmp(type,"opin") &&  strcmp(type,"iopin"));
    if(cond) inst_ptr[i].flags|=2;
    else inst_ptr[i].flags &=~2;
  }

}

void load_schematic(int load_symbols, const char *filename, int reset_undo) /* 20150327 added reset_undo */
{
  FILE *fd;
  char name[PATH_MAX];
  static char msg[PATH_MAX+100];
  struct stat buf;
  int i;

  current_type=SCHEMATIC; /* correct type is set later by read_xschem_file() */
  prepared_hilight_structs=0; /* 20171212 */
  prepared_netlist_structs=0; /* 20171212 */
  prepared_hash_instances=0; /* 20171224 */
  prepared_hash_wires=0; /* 20171224 */
  if(reset_undo) clear_undo();
  if(filename && filename[0]) {
    my_strncpy(name, filename, S(name));
    my_strncpy(schematic[currentsch], name, S(schematic[currentsch]));
    my_snprintf(msg, S(msg), "set current_dirname \"[file dirname {%s}]\"", name);
    tcleval(msg);
    my_strncpy(current_name, rel_sym_path(name), S(current_name)); /* 20190519 */
    dbg(1, "load_schematic(): opening file for loading:%s, filename=%s\n", name, filename);
    dbg(1, "load_schematic(): schematic[currentsch]=%s\n", schematic[currentsch]);
    if(!name[0]) return;
    if(event_reporting) { 
      char n[PATH_MAX];
      printf("xschem load %s\n", escape_chars(n, name, PATH_MAX));
      fflush(stdout);
    }
    if( (fd=fopen(name,"r"))== NULL) {
      fprintf(errfp, "load_schematic(): unable to open file: %s, filename=%s\n", 
          name, filename ? filename : "<NULL>");
      my_snprintf(msg, S(msg), "alert_ {Unable to open file: %s}", filename ? filename: "(null)");
      tcleval(msg);
      clear_drawing();
    } else {
      clear_drawing();
      dbg(1, "load_schematic(): reading file: %s\n", name);
      read_xschem_file(fd);
      fclose(fd); /* 20150326 moved before load symbols */
      set_modify(0);
      dbg(2, "load_schematic(): loaded file:wire=%d inst=%d\n",lastwire , lastinst);
      if(load_symbols && current_type == SCHEMATIC) link_symbols_to_instances();
    }
    dbg(1, "load_schematic(): %s, returning\n", schematic[currentsch]);
  } else {
    set_modify(0);
    clear_drawing();
    for(i=0;;i++) {
      if(i == 0) my_snprintf(name, S(name), "%s.sch", "untitled");
      else my_snprintf(name, S(name), "%s-%d.sch", "untitled", i);
      if(stat(name, &buf)) break;
    }
    my_snprintf(schematic[currentsch], S(schematic[currentsch]), "%s/%s", pwd_dir, name);
    my_strncpy(current_name, name, S(current_name)); /* 20190519 */
  }
  if(has_x) { /* 20161207 moved after if( (fd=..))  */
    if(strcmp(get_cell(schematic[currentsch],1), "systemlib/font")) {
      tcleval( "wm title . \"xschem - [file tail [xschem get schname]]\""); /* 20150417 set window and icon title */
      tcleval( "wm iconname . \"xschem - [file tail [xschem get schname]]\"");
    }
  }
  update_conn_cues(0, 0);
}

#ifndef IN_MEMORY_UNDO

void delete_undo(void)  /* 20150327 */
{
  int i;
  char diff_name[PATH_MAX]; /* overflow safe 20161122 */

  for(i=0; i<max_undo; i++) {
    my_snprintf(diff_name, S(diff_name), "%s/undo%d",undo_dirname, i);
    xunlink(diff_name);
  }
  rmdir(undo_dirname);
  my_free(895, &undo_dirname);
}
    
void clear_undo(void) /* 20150327 */
{
  cur_undo_ptr = 0;
  tail_undo_ptr = 0;
  head_undo_ptr = 0;
}

void push_undo(void) /* 20150327 */
{
    #if HAS_PIPE==1
    int pd[2];
    pid_t pid;
    FILE *diff_fd;
    #endif
    FILE *fd;
    char diff_name[PATH_MAX+100]; /* overflow safe 20161122 */

    if(no_undo)return;
    dbg(1, "push_undo(): cur_undo_ptr=%d tail_undo_ptr=%d head_undo_ptr=%d\n", 
       cur_undo_ptr, tail_undo_ptr, head_undo_ptr);


    #if HAS_POPEN==1
    my_snprintf(diff_name, S(diff_name), "gzip --fast -c > %s/undo%d", undo_dirname, cur_undo_ptr%max_undo);
    fd = popen(diff_name,"w");
    if(!fd) {
      fprintf(errfp, "push_undo(): failed to open write pipe %s\n", diff_name);
      no_undo=1;
      return;
    }
    #elif HAS_PIPE==1
    my_snprintf(diff_name, S(diff_name), "%s/undo%d", undo_dirname, cur_undo_ptr%max_undo);
    pipe(pd); 
    if((pid = fork()) ==0) {                                    /* child process */
      static char f[PATH_MAX] = "";
      close(pd[1]);                                     /* close write side of pipe */
      if(!(diff_fd=freopen(diff_name,"w", stdout)))     /* redirect stdout to file diff_name */
      {
        dbg(1, "push_undo(): problems opening file %s \n",diff_name);
        Tcl_Eval(interp, "exit");
      }

      /* the following 2 statements are a replacement for dup2() which is not c89
       * however these are not atomic, if another thread takes stdin 
       * in between we are in trouble */
      close(0); /* close stdin */
      dup(pd[0]); /* duplicate read side of pipe to stdin */
      if(!f[0]) my_strncpy(f, get_file_path("gzip"), S(f));
      execl(f, f, "-c", NULL);       /* replace current process with comand */
      /* never gets here */
      fprintf(errfp, "push_undo(): problems with execl\n");
      Tcl_Eval(interp, "exit");
    }
    close(pd[0]);                                       /* close read side of pipe */
    fd=fdopen(pd[1],"w");
    #else /* uncompressed undo */
    my_snprintf(diff_name, S(diff_name), "%s/undo%d", undo_dirname, cur_undo_ptr%max_undo);
    fd = fopen(diff_name,"w");
    if(!fd) {
      fprintf(errfp, "push_undo(): failed to open undo file %s\n", diff_name);
      no_undo=1;
      return;
    }
    #endif
    write_xschem_file(fd);
    cur_undo_ptr++;
    head_undo_ptr = cur_undo_ptr;
    tail_undo_ptr = head_undo_ptr <= max_undo? 0: head_undo_ptr-max_undo;
    #if HAS_POPEN==1
    pclose(fd);
    #elif HAS_PIPE==1
    fclose(fd);
    waitpid(pid, NULL,0);
    #else
    fclose(fd);
    #endif
}

void pop_undo(int redo)  /* 20150327 */
{
  FILE *fd;
  char diff_name[PATH_MAX+12];
  #if HAS_PIPE==1
  int pd[2];
  pid_t pid;
  FILE *diff_fd;
  #endif

  if(no_undo)return;
  if(redo) { 
    if(cur_undo_ptr < head_undo_ptr) {
      dbg(1, "pop_undo(): redo; cur_undo_ptr=%d tail_undo_ptr=%d head_undo_ptr=%d\n", 
         cur_undo_ptr, tail_undo_ptr, head_undo_ptr);
      cur_undo_ptr++;
    } else {
      return;
    }
  } else {  /*redo=0 (undo) */
    if(cur_undo_ptr == tail_undo_ptr) return;
    dbg(1, "pop_undo(): undo; cur_undo_ptr=%d tail_undo_ptr=%d head_undo_ptr=%d\n", 
       cur_undo_ptr, tail_undo_ptr, head_undo_ptr);
    if(head_undo_ptr == cur_undo_ptr) {
      push_undo();
      head_undo_ptr--;
      cur_undo_ptr--;
    }
    if(cur_undo_ptr<=0) return; /* check undo tail */
    cur_undo_ptr--;
  }
  clear_drawing();
  unselect_all();

  #if HAS_POPEN==1
  my_snprintf(diff_name, S(diff_name), "gunzip -c %s/undo%d", undo_dirname, cur_undo_ptr%max_undo);
  fd=popen(diff_name, "r");
  if(!fd) {
    fprintf(errfp, "pop_undo(): failed to open read pipe %s\n", diff_name);
    no_undo=1;
    return;
  }
  #elif HAS_PIPE==1
  my_snprintf(diff_name, S(diff_name), "%s/undo%d", undo_dirname, cur_undo_ptr%max_undo);
  pipe(pd);
  if((pid = fork())==0) {                                     /* child process */
    static char f[PATH_MAX] = "";
    close(pd[0]);                                    /* close read side of pipe */
    if(!(diff_fd=freopen(diff_name,"r", stdin)))     /* redirect stdin from file name */
    {
      dbg(1, "pop_undo(): problems opening file %s \n",diff_name);
      Tcl_Eval(interp, "exit");
    }
    /* connect write side of pipe to stdout */
    close(1);    /* close stdout */
    dup(pd[1]);  /* write side of pipe --> stdout */
    if(!f[0]) my_strncpy(f, get_file_path("gunzip"), S(f));
    execl(f, f, "-c", NULL);       /* replace current process with command */
    /* never gets here */
    dbg(1, "pop_undo(): problems with execl\n");
    Tcl_Eval(interp, "exit");
  }
  close(pd[1]);                                       /* close write side of pipe */
  fd=fdopen(pd[0],"r");
  #else /* uncompressed undo */
  my_snprintf(diff_name, S(diff_name), "%s/undo%d", undo_dirname, cur_undo_ptr%max_undo);
  fd=fopen(diff_name, "r");
  if(!fd) {
    fprintf(errfp, "pop_undo(): failed to open read pipe %s\n", diff_name);
    no_undo=1;
    return;
  }
  #endif

  read_xschem_file(fd);

  #if HAS_POPEN==1
  pclose(fd); /* 20150326 moved before load symbols */
  #elif HAS_PIPE==1
  fclose(fd);
  waitpid(pid, NULL, 0);
  #else
  fclose(fd);
  #endif
  dbg(2, "pop_undo(): loaded file:wire=%d inst=%d\n",lastwire , lastinst);
  link_symbols_to_instances();
  set_modify(1);
  prepared_hash_instances=0; /* 20171224 */
  prepared_hash_wires=0; /* 20171224 */
  prepared_netlist_structs=0; /* 20171224 */
  prepared_hilight_structs=0; /* 20171224 */
  update_conn_cues(0, 0);
  dbg(2, "pop_undo(): returning\n");
  if(event_reporting) {
    if(redo) printf("xschem redo\n");
    else     printf("xschem undo\n");
    printf("xschem redraw\n");
    fflush(stdout);
  }
}

#endif /* ifndef IN_MEMORY_UNDO */

static int Gcurrent_sym=0;
int CmpSchBbox(const void *a, const void *b)
{
  char *labela, *labelb;
  const char *name;
  Box *boxa = (Box*)a;
  Box *boxb = (Box*)b;
  int i, aa = -1, bb = -1;
  labela=NULL; labelb=NULL;
  my_strdup(658, &labela, get_tok_value(boxa->prop_ptr, "name", 0));
  my_strdup(659, &labelb, get_tok_value(boxb->prop_ptr, "name", 0));
  for (i = 0; i < instdef[Gcurrent_sym].rects[PINLAYER]; ++i)
  {
    name = get_tok_value(instdef[Gcurrent_sym].boxptr[PINLAYER][i].prop_ptr, "name", 0);
    if (!strcmp(name, labela)) {aa = i;}
    if (!strcmp(name, labelb)) {bb = i;}
    if (aa>=0 && bb>=0) break;
  }
  my_free(896, &labela);
  my_free(897, &labelb);
  return(aa-bb);
}

int load_sym_def(const char *name, FILE *embed_fd)
{
  static int recursion_counter=0;
  struct Lcc *lcc; /* size = level */
  FILE *fd_tmp;
  int rot,flip;
  double angle;
  double rx1,ry1,rx2,ry2;
  int current_sym;
  int incremented_level=0;
  int level = 0;
  int max_level, save;
  char name2[PATH_MAX];  /* 20161122 overflow safe */
  char name3[PATH_MAX];  /* 20161122 overflow safe */
  char name4[PATH_MAX];  /* 20161122 overflow safe */
  Box tmp,boundbox;
  int i,c,count=0, k, poly_points; /* 20171115 polygon stuff */
  char *aux_ptr=NULL;
  char *prop_ptr=NULL, *symtype=NULL;
  double inst_x0, inst_y0;
  int inst_rot, inst_flip;
  char *symname = NULL;
  char tag[1]; /* overflow safe 20161122 */
  int *lastl = my_malloc(333, cadlayers * sizeof(lastl)); 
  int *lastr = my_malloc(334, cadlayers * sizeof(int));
  int *lastp = my_malloc(335, cadlayers * sizeof(int));
  int *lasta = my_malloc(336, cadlayers * sizeof(int));
  Line **ll = my_malloc(337, cadlayers * sizeof(Line *));
  Box **bb = my_malloc(338, cadlayers * sizeof(Box *));
  xArc **aa = my_malloc(339, cadlayers * sizeof(xArc *));
  xPolygon **pp = my_malloc(340, cadlayers * sizeof(xPolygon *));
  int lastt; /* 20171115 lastp */
  Text *tt;
  int endfile;
  const char *strlayer;
  const char *label;
  char *pin_label = NULL, *recover_str=NULL;
  char *skip_line;
  const char *dash;

  dbg(1, "load_sym_def(): recursion_counter=%d\n", recursion_counter);
  recursion_counter++;

  dbg(1, "load_sym_def(): name=%s\n", name);
  lcc=NULL;
  my_realloc(647, &lcc, (level + 1) * sizeof(struct Lcc));
  max_level = level + 1;
  if(!strcmp(file_version,"1.0")) {
    my_strncpy(name3, abs_sym_path(name, ".sym"), S(name3));
  } else {
    my_strncpy(name3, abs_sym_path(name, ""), S(name3));
  }
    
  if(!embed_fd) {
    if((lcc[level].fd=fopen(name3,"r"))==NULL)
    {
      if(recursion_counter == 1) dbg(0, "load_sym_def(): Symbol not found: %s\n",name3);
      my_snprintf(name2, S(name2), "%s/%s.sym", tclgetvar("XSCHEM_SHAREDIR"), "systemlib/missing");
      if((lcc[level].fd=fopen(name2, "r"))==NULL)
      { 
       fprintf(errfp, "load_sym_def(): systemlib/missing.sym missing, I give up\n");
       tcleval( "exit");
      }
      my_strncpy(name3, name2, S(name3));
    }
    dbg(1, "load_sym_def(): fopen1(%s), level=%d, fd=%p\n",name3, level, lcc[level].fd);
  } else {
    dbg(1, "load_sym_def(): getting embed_fd, level=%d\n", level);
    lcc[level].fd = embed_fd;
  }

  endfile=0;
  for(c=0;c<cadlayers;c++) 
  {
   lasta[c]=lastl[c]=lastr[c]=lastp[c]=0; /* 20171115 lastp */
   ll[c]=NULL;
   bb[c]=NULL;
   pp[c]=NULL;
   aa[c]=NULL;
  }
  lastt=0;
  tt=NULL;
  check_symbol_storage();
  instdef[lastinstdef].prop_ptr = NULL;
  instdef[lastinstdef].type = NULL; /* 20150409 */
  instdef[lastinstdef].templ = NULL; /* 20150409 */
  instdef[lastinstdef].name=NULL;
  my_strdup(352, &instdef[lastinstdef].name,name); 

  while(!endfile)
  {
   if(fscanf(lcc[level].fd," %c",tag)==EOF) {
     if (level) {
         dbg(1, "load_sym_def(): fclose1, level=%d, fd=%p\n", level, lcc[level].fd);
         fclose(lcc[level].fd);
         my_free(898, &lcc[level].prop_ptr);
         my_free(899, &lcc[level].symname);
         --level;
         continue;
     } else break;
   }
   incremented_level = 0;
   switch(tag[0])
   {
    case 'v':
     load_ascii_string(&aux_ptr, lcc[level].fd);
     break;
    case 'E':
     load_ascii_string(&aux_ptr, lcc[level].fd);
     break;
    case 'V': /*09112003 */
     load_ascii_string(&aux_ptr, lcc[level].fd);
     break;
    case 'S': /*20100217 */
     load_ascii_string(&aux_ptr, lcc[level].fd);
     break;
    case 'K': /* 1.2 file format: symbol attributes for schematics placed as symbols */
     if (level==0) {
       load_ascii_string(&instdef[lastinstdef].prop_ptr, lcc[level].fd);
       if(!instdef[lastinstdef].prop_ptr) break;
       my_strdup2(424, &instdef[lastinstdef].templ, get_tok_value(instdef[lastinstdef].prop_ptr, "template",2)); /* 20150409 */
       my_strdup2(515, &instdef[lastinstdef].type, get_tok_value(instdef[lastinstdef].prop_ptr, "type",0)); /* 20150409 */
     } 
     else {
       load_ascii_string(&aux_ptr, lcc[level].fd);
     }
     break;
    case 'G': /* .sym files or pre-1.2 symbol attributes for schematics placed as symbols */
     if (level==0 && !instdef[lastinstdef].prop_ptr) {
       load_ascii_string(&instdef[lastinstdef].prop_ptr, lcc[level].fd);
       if(!instdef[lastinstdef].prop_ptr) break;
       my_strdup2(341, &instdef[lastinstdef].templ, get_tok_value(instdef[lastinstdef].prop_ptr, "template",2)); /* 20150409 */
       my_strdup2(342, &instdef[lastinstdef].type, get_tok_value(instdef[lastinstdef].prop_ptr, "type",0)); /* 20150409 */
     } 
     else {
       load_ascii_string(&aux_ptr, lcc[level].fd);
     }
     break;
    case 'L':
     fscanf(lcc[level].fd, "%d",&c);
     if(c < 0 || c>=cadlayers) {
       fprintf(errfp,"WARNING: wrong line layer\n");
       read_line(lcc[level].fd, 0);
       continue;
     } /* 20150408 */
     i=lastl[c];
     my_realloc(343, &ll[c],(i+1)*sizeof(Line));
     if(fscanf(lcc[level].fd, "%lf %lf %lf %lf ",&ll[c][i].x1, &ll[c][i].y1,
        &ll[c][i].x2, &ll[c][i].y2) < 4 ) {
       fprintf(errfp,"WARNING:  missing fields for LINE object, ignoring\n");
       read_line(lcc[level].fd, 0);
       continue;
     }
     if (level>0) {
       rot = lcc[level].rot; flip = lcc[level].flip;
       ROTATION(0.0, 0.0, ll[c][i].x1, ll[c][i].y1, rx1, ry1);
       ROTATION(0.0, 0.0, ll[c][i].x2, ll[c][i].y2, rx2, ry2);
       ll[c][i].x1 = lcc[level].x0 + rx1;  ll[c][i].y1 = lcc[level].y0 + ry1;
       ll[c][i].x2 = lcc[level].x0 + rx2;  ll[c][i].y2 = lcc[level].y0 + ry2;
     }
     ORDER(ll[c][i].x1, ll[c][i].y1, ll[c][i].x2, ll[c][i].y2); /* 20180108 */
     ll[c][i].prop_ptr=NULL;
     load_ascii_string( &ll[c][i].prop_ptr, lcc[level].fd);
     dbg(2, "l_d_s(): loaded line: ptr=%lx\n", (unsigned long)ll[c]);

     dash = get_tok_value(ll[c][i].prop_ptr,"dash", 0);
     if( strcmp(dash, "") ) {
       int d = atoi(dash);
       ll[c][i].dash = d >= 0 ? d : 0;
     } else 
       ll[c][i].dash = 0;
     ll[c][i].sel = 0;
     lastl[c]++;
     break;
    case 'P': /* 20171115 */
     if(fscanf(lcc[level].fd, "%d %d",&c, &poly_points) < 2 ) {
       fprintf(errfp,"WARNING: missing fields for POLYGON object, ignoring\n");
       read_line(lcc[level].fd, 0);
       continue;
     }
     if(c < 0 || c>=cadlayers) {
       fprintf(errfp,"WARNING: wrong polygon layer\n");
       read_line(lcc[level].fd, 0);
       continue;
     } /* 20150408 */
     i=lastp[c];
     my_realloc(344, &pp[c],(i+1)*sizeof(xPolygon));
     pp[c][i].x = my_calloc(345, poly_points, sizeof(double));
     pp[c][i].y = my_calloc(346, poly_points, sizeof(double));
     pp[c][i].selected_point = my_calloc(347, poly_points, sizeof(unsigned short));
     pp[c][i].points = poly_points;
     for(k=0;k<poly_points;k++) {
       if(fscanf(lcc[level].fd, "%lf %lf ",&(pp[c][i].x[k]), &(pp[c][i].y[k]) ) < 2 ) {
         fprintf(errfp,"WARNING: missing fields for POLYGON object\n");
       }
       if (level>0) {
         rot = lcc[level].rot; flip = lcc[level].flip;
         ROTATION(0.0, 0.0, pp[c][i].x[k], pp[c][i].y[k], rx1, ry1);
         pp[c][i].x[k] = lcc[level].x0 + rx1;  pp[c][i].y[k] = lcc[level].y0 + ry1;
       }
     }
     pp[c][i].prop_ptr=NULL;
     load_ascii_string( &pp[c][i].prop_ptr, lcc[level].fd);
     /* 20180914 */
     if( !strcmp(get_tok_value(pp[c][i].prop_ptr,"fill",0),"true") )
       pp[c][i].fill =1;
     else
       pp[c][i].fill =0;

     dash = get_tok_value(pp[c][i].prop_ptr,"dash", 0);
     if( strcmp(dash, "") ) {
       int d = atoi(dash);
       pp[c][i].dash = d >= 0 ? d : 0;
     } else 
       pp[c][i].dash = 0;
     pp[c][i].sel = 0;

     dbg(2, "l_d_s(): loaded polygon: ptr=%lx\n", (unsigned long)pp[c]);
     lastp[c]++;
     break;
    case 'A':
     fscanf(lcc[level].fd, "%d",&c);
     if(c < 0 || c>=cadlayers) {
       fprintf(errfp,"Wrong arc layer\n");
       read_line(lcc[level].fd, 0);
       continue;
     }
     i=lasta[c];
     my_realloc(348, &aa[c],(i+1)*sizeof(xArc));
     if( fscanf(lcc[level].fd, "%lf %lf %lf %lf %lf ",&aa[c][i].x, &aa[c][i].y,
        &aa[c][i].r, &aa[c][i].a, &aa[c][i].b) < 5 ) {
       fprintf(errfp,"WARNING: missing fields for ARC object, ignoring\n");
       read_line(lcc[level].fd, 0);
       continue;
     }
     if (level>0) {
       rot = lcc[level].rot; flip = lcc[level].flip;
       if (flip) {
         angle = 270. * rot + 180. - aa[c][i].b - aa[c][i].a;
       }
       else {
         angle = aa[c][i].a + rot * 270.;
       }
       angle = fmod(angle, 360.);
       if (angle < 0.) angle += 360.;
       ROTATION(0.0, 0.0, aa[c][i].x, aa[c][i].y, rx1, ry1);
       aa[c][i].x = lcc[level].x0 + rx1;  aa[c][i].y = lcc[level].y0 + ry1;
       aa[c][i].a = angle;
     }
     aa[c][i].prop_ptr=NULL;
     load_ascii_string( &aa[c][i].prop_ptr, lcc[level].fd);
     if( !strcmp(get_tok_value(aa[c][i].prop_ptr,"fill",0),"true") )
       aa[c][i].fill =1;
     else
       aa[c][i].fill =0;

     dash = get_tok_value(aa[c][i].prop_ptr,"dash", 0);
     if( strcmp(dash, "") ) {
       int d = atoi(dash);
       aa[c][i].dash = d >= 0 ? d : 0;
     } else
       aa[c][i].dash = 0;
     aa[c][i].sel = 0;


     dbg(2, "l_d_s(): loaded arc: ptr=%lx\n", (unsigned long)aa[c]);
     lasta[c]++;
     break;
    case 'B':
     fscanf(lcc[level].fd, "%d",&c);
     if(c>=cadlayers) {
       fprintf(errfp,"FATAL: box layer > defined cadlayers, increase cadlayers\n");
       tcleval( "exit");
     } /* 20150408 */
     if (level>0 && c == PINLAYER)  /* Don't care about pins inside SYM */
       c = 7;
     i=lastr[c];
     my_realloc(349, &bb[c],(i+1)*sizeof(Box));
     fscanf(lcc[level].fd, "%lf %lf %lf %lf ",&bb[c][i].x1, &bb[c][i].y1,
        &bb[c][i].x2, &bb[c][i].y2);
     if (level>0) {
       rot = lcc[level].rot; flip = lcc[level].flip;
       ROTATION(0.0, 0.0, bb[c][i].x1, bb[c][i].y1, rx1, ry1);
       ROTATION(0.0, 0.0, bb[c][i].x2, bb[c][i].y2, rx2, ry2);
       bb[c][i].x1 = lcc[level].x0 + rx1;  bb[c][i].y1 = lcc[level].y0 + ry1;
       bb[c][i].x2 = lcc[level].x0 + rx2;  bb[c][i].y2 = lcc[level].y0 + ry2;
     }
     RECTORDER(bb[c][i].x1, bb[c][i].y1, bb[c][i].x2, bb[c][i].y2); /* 20180108 */
     bb[c][i].prop_ptr=NULL;
     load_ascii_string( &bb[c][i].prop_ptr, lcc[level].fd);
     dbg(2, "l_d_s(): loaded rect: ptr=%lx\n", (unsigned long)bb[c]);
     dash = get_tok_value(bb[c][i].prop_ptr,"dash", 0);
     if( strcmp(dash, "") ) {
       int d = atoi(dash);
       bb[c][i].dash = d >= 0 ? d : 0;
     } else
       bb[c][i].dash = 0;
     bb[c][i].sel = 0;

     lastr[c]++;
     break;
    case 'T':
     i=lastt;
     my_realloc(350, &tt,(i+1)*sizeof(Text));
     tt[i].txt_ptr=NULL;
     tt[i].font=NULL;
     load_ascii_string(&tt[i].txt_ptr, lcc[level].fd);
     fscanf(lcc[level].fd, "%lf %lf %d %d %lf %lf ",&tt[i].x0, &tt[i].y0, &tt[i].rot,
        &tt[i].flip, &tt[i].xscale, &tt[i].yscale);
     if (level>0) {
       const char* tmp = translate2(lcc, level, tt[i].txt_ptr);
       if (tmp) my_strdup(651, &tt[i].txt_ptr, tmp);
       ROTATION(0.0, 0.0, tt[i].x0, tt[i].y0, rx1, ry1);
       tt[i].x0 = lcc[level].x0 + rx1;  tt[i].y0 = lcc[level].y0 + ry1;
       tt[i].rot = (tt[i].rot + ((lcc[level].flip && (tt[i].rot & 1)) ? lcc[level].rot + 2 : lcc[level].rot)) & 0x3; 
       tt[i].flip = lcc[level].flip ^ tt[i].flip;
     }
     tt[i].prop_ptr=NULL;
     load_ascii_string(&tt[i].prop_ptr, lcc[level].fd);
     dbg(1, "l_d_s(): loaded text : t=%s p=%s\n", tt[i].txt_ptr, tt[i].prop_ptr);

     my_strdup(351, &tt[i].font, get_tok_value(tt[i].prop_ptr, "font", 0));/*20171206 */

     strlayer = get_tok_value(tt[i].prop_ptr, "hcenter", 0);
     tt[i].hcenter = strcmp(strlayer, "true")  ? 0 : 1;
     strlayer = get_tok_value(tt[i].prop_ptr, "vcenter", 0);
     tt[i].vcenter = strcmp(strlayer, "true")  ? 0 : 1;

     strlayer = get_tok_value(tt[i].prop_ptr, "layer", 0); /*20171206 */
     if(strlayer[0]) tt[i].layer = atoi(strlayer);
     else tt[i].layer = -1;
     lastt++;
     break;
    case 'N': /* store wires as lines on layer WIRELAYER. */
     i = lastl[WIRELAYER];
     my_realloc(314, &ll[WIRELAYER],(i+1)*sizeof(Line));
     if(fscanf(lcc[level].fd, "%lf %lf %lf %lf ",&ll[WIRELAYER][i].x1, &ll[WIRELAYER][i].y1,
        &ll[WIRELAYER][i].x2, &ll[WIRELAYER][i].y2) < 4 ) {
       fprintf(errfp,"WARNING:  missing fields for LINE object, ignoring\n");
       read_line(lcc[level].fd, 0);
       continue;
     }
     if (level>0) {
       rot = lcc[level].rot; flip = lcc[level].flip;
       ROTATION(0.0, 0.0, ll[WIRELAYER][i].x1, ll[WIRELAYER][i].y1, rx1, ry1);
       ROTATION(0.0, 0.0, ll[WIRELAYER][i].x2, ll[WIRELAYER][i].y2, rx2, ry2);
       ll[WIRELAYER][i].x1 = lcc[level].x0 + rx1;  ll[WIRELAYER][i].y1 = lcc[level].y0 + ry1;
       ll[WIRELAYER][i].x2 = lcc[level].x0 + rx2;  ll[WIRELAYER][i].y2 = lcc[level].y0 + ry2;
     }
     ORDER(ll[WIRELAYER][i].x1, ll[WIRELAYER][i].y1, ll[WIRELAYER][i].x2, ll[WIRELAYER][i].y2); /* 20180108 */
     ll[WIRELAYER][i].prop_ptr=NULL;
     load_ascii_string( &ll[WIRELAYER][i].prop_ptr, lcc[level].fd);
     dbg(2, "l_d_s(): loaded line: ptr=%lx\n", (unsigned long)ll[WIRELAYER]);
     ll[WIRELAYER][i].dash = 0;
     ll[WIRELAYER][i].sel = 0;
     lastl[WIRELAYER]++;
     break;
    case 'C':
      load_ascii_string(&symname, lcc[level].fd);
      dbg(1, "C line: symname=%s\n", symname);
      if (fscanf(lcc[level].fd, "%lf %lf %d %d", &inst_x0, &inst_y0, &inst_rot, &inst_flip) < 4) {
        fprintf(errfp, "WARNING: missing fields for COMPONENT object, ignoring\n");
        read_line(lcc[level].fd, 0);
        continue;
      }
      load_ascii_string(&prop_ptr, lcc[level].fd);
      if(level + 1 >=CADMAXHIER) {
        fprintf(errfp, "Symbol recursively instantiating symbol: max depth reached, skipping\n");
        if(has_x) tcleval("alert_ {Symbol recursively instantiating symbol: max depth reached, skipping} {} 1");
        endfile = 1;
        continue;
      }
      lastinstdef++; /* do not allow match_symbol() to overwrite current symbol we are loading */
      save = lastinstdef;
      dbg(1, "load_sym_def(): call1 match_symbol\n");
      if ((recover_str = strrchr(symname, '.')) && !strcmp(recover_str, ".sch")) 
        current_sym = match_symbol(add_ext(symname, ".sym"));
      else 
        current_sym=match_symbol(symname);

      my_strdup2(316, &symtype, instdef[current_sym].type);
      dbg(1, "load_sym_def(): level=%d, current_sym=%d symname=%s symtype=%s\n", 
                                 level, current_sym, symname, symtype);
      if(lastinstdef> save) remove_symbol(lastinstdef - 1); /* if previous match_symbol() caused a symbol to be loaded unload it now */
      lastinstdef--; /* restore symbol we are loading */

      if(  /* add here symbol types not to consider when loading schematic-as-symbol instances */
          !strcmp(symtype, "logo") ||
          !strcmp(symtype, "netlist_commands") ||
          !strcmp(symtype, "arch_declarations") ||
          !strcmp(symtype, "architecture") ||
          !strcmp(symtype, "attributes") ||
          !strcmp(symtype, "package") ||
          !strcmp(symtype, "port_attributes") ||
          !strcmp(symtype, "use") ||
          !strcmp(symtype, "launcher") ||
          !strcmp(symtype, "verilog_preprocessor") ||
          !strcmp(symtype, "timescale") 
        ) break;
      /* add PINLAYER boxes (symbol pins) at schematic i/o/iopin coordinates. */
      if (level==0 && (!strcmp(symtype, "ipin") || !strcmp(symtype, "opin") || !strcmp(symtype, "iopin"))) {
        i = lastr[PINLAYER];
        my_realloc(652, &bb[PINLAYER], (i + 1) * sizeof(Box));
        bb[PINLAYER][i].x1 = inst_x0 - 2.5; bb[PINLAYER][i].x2 = inst_x0 + 2.5;
        bb[PINLAYER][i].y1 = inst_y0 - 2.5; bb[PINLAYER][i].y2 = inst_y0 + 2.5;
        RECTORDER(bb[PINLAYER][i].x1, bb[PINLAYER][i].y1, bb[PINLAYER][i].x2, bb[PINLAYER][i].y2); /* 20180108 */
        bb[PINLAYER][i].prop_ptr = NULL;
        
        label = get_tok_value(prop_ptr, "lab", 0);
        save = strlen(label)+30;
        pin_label = my_malloc(315, save);
        pin_label[0] = '\0';
        if (!strcmp(symtype, "ipin")) {
          my_snprintf(pin_label, save, "name=%s dir=in ", label);
        } else if (!strcmp(symtype, "opin")) {
          my_snprintf(pin_label, save, "name=%s dir=out ", label);
        } else if (!strcmp(symtype, "iopin")) {
          my_snprintf(pin_label, save, "name=%s dir=inout ", label);
        }
        my_strdup(463, &bb[PINLAYER][i].prop_ptr, pin_label);

        /* add to symbol pins remaining attributes from schematic pins, except name= and lab= */

        my_strdup(157, &pin_label, get_sym_template(prop_ptr, "lab"));   /* remove name=...  and lab=... */
        my_strcat(159, &bb[PINLAYER][i].prop_ptr, pin_label);

        my_free(900, &pin_label);
        lastr[PINLAYER]++;
      }
      if (!strcmp(file_version, "1.0")) {
        my_strncpy(name4, abs_sym_path(symname, ".sym"), S(name4));
      }
      else {
        my_strncpy(name4, abs_sym_path(symname, ""), S(name4));
      }

      /* replace i/o/iopin.sym with better looking (for symbol) pins */
      if(level == 0) {
        if (!strcmp(symtype, "ipin")) {
           my_snprintf(name4, S(name4), "%s/%s", tclgetvar("XSCHEM_SHAREDIR"), "systemlib/ipin_lcc_top.sym");
        } else if (!strcmp(symtype, "opin")) {
           my_snprintf(name4, S(name4), "%s/%s", tclgetvar("XSCHEM_SHAREDIR"), "systemlib/opin_lcc_top.sym");
        } else if (!strcmp(symtype, "iopin")) {
           my_snprintf(name4, S(name4), "%s/%s", tclgetvar("XSCHEM_SHAREDIR"), "systemlib/iopin_lcc_top.sym");
        }
      } else {
        if (!strcmp(symtype, "ipin")) {
           my_snprintf(name4, S(name4), "%s/%s", tclgetvar("XSCHEM_SHAREDIR"), "systemlib/ipin_lcc.sym");
        } else if (!strcmp(symtype, "opin")) {
           my_snprintf(name4, S(name4), "%s/%s", tclgetvar("XSCHEM_SHAREDIR"), "systemlib/opin_lcc.sym");
        } else if (!strcmp(symtype, "iopin")) {
           my_snprintf(name4, S(name4), "%s/%s", tclgetvar("XSCHEM_SHAREDIR"), "systemlib/iopin_lcc.sym");
        }
      }

      if ((fd_tmp = fopen(name4, "r")) == NULL) {
        fprintf(errfp, "load_sym_def(): unable to open file to read schematic: %s\n", name4);
      } else {
        if (level+1 >= max_level) {
          
          my_realloc(653, &lcc, (max_level + 1) * sizeof(struct Lcc));
          max_level++;
        }
        dbg(1, "load_sym_def(): fopen2(%s), level=%d, fd=%p\n", name4, level, fd_tmp);
        ++level;
        incremented_level = 1;
        lcc[level].fd = fd_tmp;
        lcc[level].prop_ptr = NULL;
        lcc[level].symname = NULL;
        lcc[level].x0 = inst_x0;
        lcc[level].y0 = inst_y0;
        lcc[level].rot = inst_rot;
        lcc[level].flip = inst_flip;
        /* calculate LCC sub-schematic x0, y0, rotation and flip */ 
        if (level > 1) {
          int rot, flip;
          static int map[4]={0,3,2,1};

          flip = lcc[level-1].flip;
          rot = lcc[level-1].rot;
          ROTATION(0.0, 0.0, lcc[level].x0, lcc[level].y0,lcc[level].x0, lcc[level].y0);
          lcc[level].rot = (lcc[(level-1)].flip ? map[lcc[level].rot] : lcc[level].rot) + lcc[(level-1)].rot;
          lcc[level].rot &= 0x3;
          lcc[level].flip = lcc[level].flip ^ lcc[level-1].flip;
          lcc[level].x0 += lcc[(level-1)].x0;
          lcc[level].y0 += lcc[(level-1)].y0;
        }
        my_strdup(654, &lcc[level].prop_ptr, prop_ptr);
        my_strdup(657, &lcc[level].symname, symname);
        dbg(1, "level incremented: level=%d, symname=%s, name4=%s\n", level, symname, name4);
      }
      break;
    case '[':
     read_line(lcc[level].fd, 0);
     fscanf(lcc[level].fd, "\n");
     while(1) { /* skip embedded [ ... ] */
       skip_line = read_line(lcc[level].fd, 0);
       if(!skip_line) {
         break;
       }
       if(!strcmp(skip_line, "]")) break;
       fscanf(lcc[level].fd, "\n");
     }
     break;
    case ']':
     read_line(lcc[level].fd, 0);
     endfile=1;
     break;
    default:
     if( tag[0] == '{' ) ungetc(tag[0], lcc[level].fd);
     read_record(tag[0], lcc[level].fd);
     break;
   }
   /* if a 'C' line was encountered and level was incremented, rest of line must be read
      with lcc[level-1].fd file pointer */
   if(incremented_level) 
     read_line(lcc[level-1].fd, 0); /* discard any remaining characters till (but not including) newline */
   else 
     read_line(lcc[level].fd, 0); /* discard any remaining characters till (but not including) newline */
  }
  if(!embed_fd) {
    dbg(1, "load_sym_def(): fclose2, level=%d, fd=%p\n", level, lcc[0].fd);
    fclose(lcc[0].fd);
  }
  if(embed_fd || strstr(name, ".xschem_embedded_")) {
    instdef[lastinstdef].flags |= EMBEDDED;
  } else {
    instdef[lastinstdef].flags &= ~EMBEDDED;
  }
  dbg(2, "l_d_s(): finished parsing file\n");
  for(c=0;c<cadlayers;c++)
  {
   instdef[lastinstdef].arcs[c] = lasta[c];
   instdef[lastinstdef].lines[c] = lastl[c];
   instdef[lastinstdef].rects[c] = lastr[c];
   instdef[lastinstdef].polygons[c] = lastp[c];
   instdef[lastinstdef].arcptr[c] = aa[c];
   instdef[lastinstdef].lineptr[c] = ll[c];
   instdef[lastinstdef].polygonptr[c] = pp[c];
   instdef[lastinstdef].boxptr[c] = bb[c];
  }
  instdef[lastinstdef].texts = lastt;
  instdef[lastinstdef].txtptr = tt;
   

  boundbox.x1 = boundbox.x2 = boundbox.y1 = boundbox.y2 = 0;
  for(c=0;c<cadlayers;c++)
  {
   for(i=0;i<lastl[c];i++)
   {
    count++;
    tmp.x1=ll[c][i].x1;tmp.y1=ll[c][i].y1;tmp.x2=ll[c][i].x2;tmp.y2=ll[c][i].y2;
    updatebbox(count,&boundbox,&tmp);
    dbg(2, "l_d_s(): line[%d][%d]: %g %g %g %g\n", 
			c, i, tmp.x1,tmp.y1,tmp.x2,tmp.y2);
   }
   for(i=0;i<lasta[c];i++)
   {
    count++;
    arc_bbox(aa[c][i].x, aa[c][i].y, aa[c][i].r, aa[c][i].a, aa[c][i].b, 
             &tmp.x1, &tmp.y1, &tmp.x2, &tmp.y2);
    /* printf("arc bbox: %g %g %g %g\n", tmp.x1, tmp.y1, tmp.x2, tmp.y2); */
    updatebbox(count,&boundbox,&tmp);
   }
   for(i=0;i<lastr[c];i++)
   {
    count++;
    tmp.x1=bb[c][i].x1;tmp.y1=bb[c][i].y1;tmp.x2=bb[c][i].x2;tmp.y2=bb[c][i].y2;
    updatebbox(count,&boundbox,&tmp);
   }
   for(i=0;i<lastp[c];i++) /* 20171115 */
   {
     double x1=0., y1=0., x2=0., y2=0.;
     int k;
     count++;
     for(k=0; k<pp[c][i].points; k++) {
       /*fprintf(errfp, "  poly: point %d: %.16g %.16g\n", k, pp[c][i].x[k], pp[c][i].y[k]); */
       if(k==0 || pp[c][i].x[k] < x1) x1 = pp[c][i].x[k];
       if(k==0 || pp[c][i].y[k] < y1) y1 = pp[c][i].y[k];
       if(k==0 || pp[c][i].x[k] > x2) x2 = pp[c][i].x[k];
       if(k==0 || pp[c][i].y[k] > y2) y2 = pp[c][i].y[k];
     }
     tmp.x1=x1;tmp.y1=y1;tmp.x2=x2;tmp.y2=y2;
     updatebbox(count,&boundbox,&tmp);
   }
  }
/*
*   do not include symbol text in bounding box, since text length
*   is variable from one instance to another.
*
*   for(i=0;i<lastt;i++)
*   { 
*    count++;
*    rot=tt[i].rot;flip=tt[i].flip;
*    text_bbox(tt[i].txt_ptr, tt[i].xscale, tt[i].yscale, rot, flip,
*    tt[i].x0, tt[i].y0, &rx1,&ry1,&rx2,&ry2);
*    tmp.x1=rx1;tmp.y1=ry1;tmp.x2=rx2;tmp.y2=ry2;
*    updatebbox(count,&boundbox,&tmp);
*  }
*/
  instdef[lastinstdef].minx = boundbox.x1;
  instdef[lastinstdef].maxx = boundbox.x2;
  instdef[lastinstdef].miny = boundbox.y1;
  instdef[lastinstdef].maxy = boundbox.y2;

  lastinstdef++;
  my_free(901, &lastl);
  my_free(902, &lastr);
  my_free(903, &lastp);
  my_free(904, &lasta);
  my_free(905, &ll);
  my_free(906, &bb);
  my_free(907, &aa);
  my_free(908, &pp);
  my_free(909, &lcc);
  my_free(910, &prop_ptr);
  my_free(911, &aux_ptr);
  my_free(912, &symname);
  my_free(913, &symtype);
  my_strncpy(name4, name, S(name4));
  if ((prop_ptr = strrchr(name4, '.')) && !strcmp(prop_ptr, ".sch")) {
    save = lastinstdef; /* save idx because match_symbol may call load_symbol_definition */
    dbg(1, "load_sym_def(): call2 match_symbol\n");
    current_sym = match_symbol(add_ext(name4, ".sym"));
    if ( instdef[current_sym].type && strcmp(instdef[current_sym].type, "missing")) { /* To ensure SCH's BOX's PINLAYER matches SYM so netlisting will be corect */
      Gcurrent_sym = current_sym;
      qsort(instdef[save-1].boxptr[PINLAYER], instdef[save-1].rects[PINLAYER], sizeof(Box), CmpSchBbox);
    }
    if(lastinstdef > save) remove_symbol(lastinstdef - 1); /* if previous match_symbol() caused a symbol to be loaded unload it now */
  }
  recursion_counter--;
  dbg(1, "load_sym_def(): exiting, recursion_counter=%d\n", recursion_counter);
  return 1;
}

/* 20171004 */
void create_sch_from_sym(void)
{
  Instdef *ptr;
  int i, j, npin, ypos;
  double x;
  int p=0;
  Box *rect;
  FILE *fd;
  char *pindir[3] = {"in", "out", "inout"};
  char *pinname[3] = {"devices/ipin.sym", "devices/opin.sym", "devices/iopin.sym"};
  char *generic_pin = {"devices/generic_pin.sym"};
  char *pinname2[3] = {"ipin.sym", "opin.sym", "iopin.sym"};
  char *generic_pin2 = {"generic_pin.sym"};
  int indirect;

  char *dir = NULL;
  char *prop = NULL;
  char schname[PATH_MAX];
  char *savecmd=NULL;
  char *sub_prop;
  char *sub2_prop=NULL;
  char *str=NULL;
  struct stat buf;
  int ln;

  if(!stat(abs_sym_path(pinname[0], ""), &buf)) {
    indirect=1;
  } else {
    indirect=0;
  }
  /* printf("indirect=%d\n", indirect); */

  rebuild_selected_array();
  if(lastselected > 1)  return;
  if(lastselected==1 && selectedgroup[0].type==ELEMENT)
  {
    my_strncpy(schname, abs_sym_path(get_tok_value(
      (inst_ptr[selectedgroup[0].n].ptr+instdef)->prop_ptr, "schematic",0 ), "")
      , S(schname));
    if(!schname[0]) {
      my_strncpy(schname, add_ext(abs_sym_path(inst_ptr[selectedgroup[0].n].name, ""), ".sch"), S(schname));
    }
    if( !stat(schname, &buf) ) {
      my_strdup(353, &savecmd, "ask_save \" create schematic file: ");
      my_strcat(354, &savecmd, schname);
      my_strcat(355, &savecmd, " ?\nWARNING: This schematic file already exists, it will be overwritten\"");
      tcleval(savecmd);
      if(strcmp(tclresult(), "yes") ) {
        my_free(914, &savecmd);
        return;
      }
    }
    if(!(fd=fopen(schname,"w")))
    {
      fprintf(errfp, "create_sch_from_sym(): problems opening file %s \n",schname);
      tcleval("alert_ {file opening for write failed!} {}"); /* 20171020 */
      my_free(915, &savecmd);
      return;
    }
    fprintf(fd, "v {xschem version=%s file_version=%s}\n", XSCHEM_VERSION, XSCHEM_FILE_VERSION);
    fprintf(fd, "G {}");
    fputc('\n', fd);
    fprintf(fd, "V {}");
    fputc('\n', fd);
    fprintf(fd, "E {}"); /* 20180912 */
    fputc('\n', fd);
    fprintf(fd, "S {}");
    fputc('\n', fd);
    ptr = inst_ptr[selectedgroup[0].n].ptr+instdef;
    npin = ptr->rects[GENERICLAYER];
    rect = ptr->boxptr[GENERICLAYER];
    ypos=0;
    for(i=0;i<npin;i++) {
      my_strdup(356, &prop, rect[i].prop_ptr);
      if(!prop) continue;
      sub_prop=strstr(prop,"name=")+5;
      if(!sub_prop) continue;
      x=-120.0;
      ln = 100+strlen(sub_prop);
      my_realloc(357, &str, ln);
      my_snprintf(str, ln, "name=g%d lab=%s", p++, sub_prop);
      if(indirect)
        fprintf(fd, "C {%s} %.16g %.16g %.16g %.16g ", generic_pin, x, 20.0*(ypos++), 0.0, 0.0 );
      else
        fprintf(fd, "C {%s} %.16g %.16g %.16g %.16g ", generic_pin2, x, 20.0*(ypos++), 0.0, 0.0 );
      save_ascii_string(str, fd);
      fputc('\n' ,fd);
    } /* for(i) */
    npin = ptr->rects[PINLAYER];
    rect = ptr->boxptr[PINLAYER];
    for(j=0;j<3;j++) {
      if(j==1) ypos=0;
      for(i=0;i<npin;i++) {
        my_strdup(358, &prop, rect[i].prop_ptr);
        if(!prop) continue;
        sub_prop=strstr(prop,"name=")+5;
        if(!sub_prop) continue;
        /* remove dir=... from prop string 20171004 */
        my_strdup(360, &sub2_prop, subst_token(sub_prop, "dir", NULL));

        my_strdup(361, &dir, get_tok_value(rect[i].prop_ptr,"dir",0));
        if(!sub2_prop) continue;
        if(!dir) continue;
        if(j==0) x=-120.0; else x=120.0;
        if(!strcmp(dir, pindir[j])) { 
          ln = 100+strlen(sub2_prop);
          my_realloc(362, &str, ln);
          my_snprintf(str, ln, "name=g%d lab=%s", p++, sub2_prop);
          if(indirect)
            fprintf(fd, "C {%s} %.16g %.16g %.16g %.16g ", pinname[j], x, 20.0*(ypos++), 0.0, 0.0);
          else
            fprintf(fd, "C {%s} %.16g %.16g %.16g %.16g ", pinname2[j], x, 20.0*(ypos++), 0.0, 0.0);
          save_ascii_string(str, fd);
          fputc('\n' ,fd);
        } /* if() */
      } /* for(i) */
    }  /* for(j) */
    fclose(fd);
  } /* if(lastselected...) */
  my_free(916, &dir);
  my_free(917, &prop);
  my_free(918, &savecmd);
  my_free(919, &sub2_prop);
  my_free(920, &str);
}

void descend_symbol(void)
{
  char *str=NULL;
  FILE *fd;
  char name[PATH_MAX];
  char name_embedded[PATH_MAX];
  rebuild_selected_array();
  if(lastselected > 1)  return; /*20121122 */
  if(lastselected==1 && selectedgroup[0].type==ELEMENT) {
    if(modified) { /* 20161209 */
      if(save(1)) return;
    }
    my_snprintf(name, S(name), "%s", inst_ptr[selectedgroup[0].n].name);
    /* dont allow descend in the default missing symbol */
    if((inst_ptr[selectedgroup[0].n].ptr+instdef)->type &&
       !strcmp( (inst_ptr[selectedgroup[0].n].ptr+instdef)->type,"missing")) return;
  }
  else return;

  /* build up current hierarchy path */
  my_strdup(363,  &str, inst_ptr[selectedgroup[0].n].instname);  /* 20180911 */
  my_strdup(364, &sch_path[currentsch+1], sch_path[currentsch]);
  my_strcat(365, &sch_path[currentsch+1], str);
  my_strcat(366, &sch_path[currentsch+1], ".");
  sch_inst_number[currentsch+1] = 1;
  my_free(921, &str);
  previous_instance[currentsch]=selectedgroup[0].n;
  zoom_array[currentsch].x=xorigin;
  zoom_array[currentsch].y=yorigin;
  zoom_array[currentsch].zoom=zoom;
  ++currentsch;
  if((inst_ptr[selectedgroup[0].n].ptr+instdef)->flags & EMBEDDED ||
    !strcmp(get_tok_value(inst_ptr[selectedgroup[0].n].prop_ptr,"embed", 0), "true")) {
    /* save embedded symbol into a temporary file */
    my_snprintf(name_embedded, S(name_embedded),
      "%s/.xschem_embedded_%d_%s", tclgetvar("XSCHEM_TMP_DIR"), getpid(), get_cell_w_ext(name, 0));
    if(!(fd = fopen(name_embedded, "w")) ) {
      fprintf(errfp, "descend_symbol(): problems opening file %s \n", name_embedded);
    }
    save_embedded_symbol(inst_ptr[selectedgroup[0].n].ptr+instdef, fd);
    fclose(fd);
    unselect_all();
    remove_symbols(); /* must follow save (if) embedded */
    /* load_symbol(name_embedded); */
    load_schematic(1, name_embedded, 1);
  } else {
    /* load_symbol(abs_sym_path(name, "")); */
    unselect_all();
    remove_symbols(); /* must follow save (if) embedded */
    load_schematic(1, abs_sym_path(name, ""), 1);
  }
  zoom_full(1, 0);
}

/* 20111023 align selected object to current grid setting */
#define SNAP_TO_GRID(a)  (a=ROUND(( a)/cadsnap)*cadsnap )
void round_schematic_to_grid(double cadsnap)
{
 int i, c, n, p;
 rebuild_selected_array();
 for(i=0;i<lastselected;i++)
 {
   c = selectedgroup[i].col; n = selectedgroup[i].n;
   switch(selectedgroup[i].type)
   {
     case xTEXT:
       SNAP_TO_GRID(textelement[n].x0);
       SNAP_TO_GRID(textelement[n].y0);
     break;

     case xRECT:
       if(c == PINLAYER) {
         double midx, midx_round, deltax;
         double midy, midy_round, deltay;
         midx_round = midx = (rect[c][n].x1 + rect[c][n].x2) / 2;
         midy_round = midy = (rect[c][n].y1 + rect[c][n].y2) / 2;
         SNAP_TO_GRID(midx_round);
         SNAP_TO_GRID(midy_round);
         deltax = midx_round - midx;
         deltay = midy_round - midy;
         rect[c][n].x1 += deltax;
         rect[c][n].x2 += deltax;
         rect[c][n].y1 += deltay;
         rect[c][n].y2 += deltay;
       } else {
         SNAP_TO_GRID(rect[c][n].x1);
         SNAP_TO_GRID(rect[c][n].y1);
         SNAP_TO_GRID(rect[c][n].x2);
         SNAP_TO_GRID(rect[c][n].y2);
       }
     break;

     case WIRE:
       SNAP_TO_GRID(wire[n].x1);
       SNAP_TO_GRID(wire[n].y1);
       SNAP_TO_GRID(wire[n].x2);
       SNAP_TO_GRID(wire[n].y2);
     break;

     case LINE:
       SNAP_TO_GRID(line[c][n].x1);
       SNAP_TO_GRID(line[c][n].y1);
       SNAP_TO_GRID(line[c][n].x2);
       SNAP_TO_GRID(line[c][n].y2);
     break;

     case ARC:
       SNAP_TO_GRID(arc[c][n].x);
       SNAP_TO_GRID(arc[c][n].y);
     break;
     
     case POLYGON:
       for(p=0;p<polygon[c][n].points; p++) {
         SNAP_TO_GRID(polygon[c][n].x[p]);
         SNAP_TO_GRID(polygon[c][n].y[p]);
       }
     break;

     case ELEMENT:
       SNAP_TO_GRID(inst_ptr[n].x0);
       SNAP_TO_GRID(inst_ptr[n].y0);
       /*20111111 */
       symbol_bbox(n, &inst_ptr[n].x1, &inst_ptr[n].y1, &inst_ptr[n].x2, &inst_ptr[n].y2);
     break;

     default:
     break;
   }
 }


}

/* what: */
/*                      1: save selection */
/*                      2: save clipboard */
void save_selection(int what)
{
 FILE *fd;
 int i, c, n, k;
 char name[PATH_MAX];

 dbg(3, "save_selection():\n");
 if(what==1)
   my_snprintf(name, S(name), "%s/%s.sch",user_conf_dir , ".selection"); /* 20150502 changed PWD to HOME */
 else /* what=2 */
   my_snprintf(name, S(name), "%s/%s.sch",user_conf_dir , ".clipboard"); /* 20150502 changed PWD to HOME */

 if(!(fd=fopen(name,"w")))
 {
    fprintf(errfp, "save_selection(): problems opening file %s \n", name);
    tcleval("alert_ {file opening for write failed!} {}"); /* 20171020 */
    return;
 }
 fprintf(fd, "v {xschem version=%s file_version=%s}\n", XSCHEM_VERSION, XSCHEM_FILE_VERSION);
 fprintf(fd, "G { %.16g %.16g }\n", mousex_snap, mousey_snap);
 for(i=0;i<lastselected;i++)
 {
   c = selectedgroup[i].col;n = selectedgroup[i].n;
   switch(selectedgroup[i].type)
   {
     case xTEXT:
      fprintf(fd, "T ");
      save_ascii_string(textelement[n].txt_ptr,fd);
      fprintf(fd, " %.16g %.16g %d %d %.16g %.16g ",
       textelement[n].x0, textelement[n].y0, textelement[n].rot, textelement[n].flip, 
       textelement[n].xscale, textelement[n].yscale);
      save_ascii_string(textelement[n].prop_ptr,fd);
      fputc('\n' ,fd);
     break;
    
     case ARC:
      fprintf(fd, "A %d %.16g %.16g %.16g %.16g %.16g ", c, arc[c][n].x, arc[c][n].y, arc[c][n].r,
       arc[c][n].a, arc[c][n].b);
      save_ascii_string(arc[c][n].prop_ptr,fd);
      fputc('\n' ,fd);
     break;

     case xRECT:
      fprintf(fd, "B %d %.16g %.16g %.16g %.16g ", c,rect[c][n].x1, rect[c][n].y1,rect[c][n].x2,
       rect[c][n].y2);
      save_ascii_string(rect[c][n].prop_ptr,fd);
      fputc('\n' ,fd);
     break;

     case POLYGON: /* 20171117 */
      fprintf(fd, "P %d %d ", c, polygon[c][n].points);
      for(k=0; k<polygon[c][n].points; k++) {
        fprintf(fd, "%.16g %.16g ", polygon[c][n].x[k], polygon[c][n].y[k]);
      }
      save_ascii_string(polygon[c][n].prop_ptr,fd);
      fputc('\n' ,fd);
     break;
     
     case WIRE:
      fprintf(fd, "N %.16g %.16g %.16g %.16g ",wire[n].x1, wire[n].y1, wire[n].x2, wire[n].y2);
      save_ascii_string(wire[n].prop_ptr,fd);
      fputc('\n' ,fd);
     break;

     case LINE:
      fprintf(fd, "L %d %.16g %.16g %.16g %.16g ", c,line[c][n].x1, line[c][n].y1,line[c][n].x2,
       line[c][n].y2 );
      save_ascii_string(line[c][n].prop_ptr,fd);
      fputc('\n' ,fd);
     break;

     case ELEMENT:
      fprintf(fd, "C ");
      save_ascii_string(inst_ptr[n].name,fd);
      fprintf(fd, " %.16g %.16g %d %d ",inst_ptr[n].x0, inst_ptr[n].y0, inst_ptr[n].rot, inst_ptr[n].flip ); 
      save_ascii_string(inst_ptr[n].prop_ptr,fd);
      fputc('\n' ,fd);
     break;

     default:
     break;
   }
 }
 fclose(fd);

}

