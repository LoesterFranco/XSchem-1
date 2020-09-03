/* File: token.c
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
#define SPACE(c) ( c=='\n' || c==' ' || c=='\t' || \
                   c=='\0' || c==';' )

/* 20150317 */
#define SPACE2(c) ( SPACE(c) || c=='\'' || c== '"')

/* instance name (refdes) hash table, for unique name checking */
struct inst_hashentry {
                  struct inst_hashentry *next;
                  unsigned int hash;
                  char *token;
                  int value;
                 };


static struct inst_hashentry *table[HASHSIZE];

enum status {XBEGIN, XTOKEN, XSEPARATOR, XVALUE, XEND, XENDTOK};

/* calculate the hash function relative to string s */
static unsigned int hash(char *tok)
{
  unsigned int hash = 0;
  int c;

  while ( (c = *tok++) )
      hash = c + (hash << 6) + (hash << 16) - hash;
  return hash;
}

int name_strcmp(char *s, char *d) /* compare strings up to '\0' or'[' */
{
 int i=0;
 while(1)
 {
  if(d[i]=='\0' || d[i]=='[')
  {
    if(s[i]!='\0' && s[i]!='[') return 1;
    return 0;
  }
  if(s[i]=='\0' || s[i]=='[')
  {
    if(d[i]!='\0' && d[i]!='[') return 1;
    return 0;
  }
  if(s[i]!=d[i]) return 1;
  i++;
 }
}

/* 20180926 added token_size */
/* remove:
 * 0,XINSERT : lookup and insert in hash table (return NULL if token was not found)
 *    if token was found update value
 * 1,XDELETE : delete token entry, return NULL
 * 2,XLOOKUP : lookup only 
 */
static struct inst_hashentry *inst_hash_lookup(struct inst_hashentry **table, char *token, int value, int remove, size_t token_size)
{
  unsigned int hashcode; 
  unsigned int index;
  struct inst_hashentry *entry, *saveptr, **preventry;
  int s;
 
  if(token==NULL) return NULL;
  hashcode=hash(token); 
  index=hashcode % HASHSIZE; 
  entry=table[index];
  preventry=&table[index];
  while(1) {
    if( !entry ) {                         /* empty slot */
      if(remove == XINSERT) {            /* insert data */
        s=sizeof( struct inst_hashentry );
        entry=(struct inst_hashentry *) my_malloc(425, s);
        *preventry=entry;
        entry->next=NULL;
        entry->hash=hashcode;
        entry->token=NULL;
        entry->token = my_malloc(426, token_size + 1);
        memcpy(entry->token,token, token_size + 1);
        entry->value = value;
      }
      return NULL; /* token was not in hash */
    }
    if( entry->hash==hashcode && !strcmp(token,entry->token) ) { /* found a matching token */
      if(remove == XDELETE) {              /* remove token from the hash table ... */
        saveptr=entry->next;
        my_free(968, &entry->token);
        my_free(969, &entry);
        *preventry=saveptr;
        return NULL;
      } else if(remove == XINSERT) {
        entry->value = value;
      }
      /* dbg(1, "inst_hash_lookup: returning: %s , %d\n", entry->token, entry->value); */
      return entry;        /* found matching entry, return the address */
    } 
    preventry=&entry->next; /* descend into the list. */
    entry = entry->next;
  }
}

static struct inst_hashentry *inst_free_hash_entry(struct inst_hashentry *entry)
{
  struct inst_hashentry *tmp;
  while( entry ) {
    tmp = entry -> next;
    my_free(970, &(entry->token));
    dbg(3, "inst_free_hash_entry(): removing entry %lu\n", (unsigned long)entry);
    my_free(971, &entry);
    entry = tmp;
  }
  return NULL;
}

static void inst_free_hash(struct inst_hashentry **table) /* remove the whole hash table  */
{
 int i;
  
 dbg(1, "inst_free_hash(): removing hash table\n");
 for(i=0;i<HASHSIZE;i++)
 {
   table[i] = inst_free_hash_entry( table[i] );
 }
}

void hash_all_names(int n)
{
  int i;
  inst_free_hash(table);
  for(i=0; i<lastinst; i++) {
    /* if(i == n) continue; */
    inst_hash_lookup(table, inst_ptr[i].instname, i, XINSERT, strlen(inst_ptr[i].instname));
  }
}

void clear_instance_hash()
{
  inst_free_hash(table);
}

void check_unique_names(int rename)
{
  int i, j, first = 1;
  char *tmp = NULL;
  int newpropcnt = 0;
  int mult;
  char *start;
  char *comma_pos;
  char *expanded_instname = NULL;
  struct inst_hashentry *entry;
  /* int save_draw; */

  if(hilight_nets) {
    Box boundbox;
    calc_drawing_bbox(&boundbox, 2);
    enable_drill=0;
    delete_hilight_net();
    /* undraw_hilight_net(1); */
    bbox(BEGIN, 0.0 , 0.0 , 0.0 , 0.0);
    bbox(ADD, boundbox.x1, boundbox.y1, boundbox.x2, boundbox.y2);
    bbox(SET , 0.0 , 0.0 , 0.0 , 0.0);
    draw();
    bbox(END , 0.0 , 0.0 , 0.0 , 0.0);
  }
  inst_free_hash(table);
  first = 1;
  for(i=0;i<lastinst;i++) {
    if(inst_ptr[i].instname && inst_ptr[i].instname[0]) {
      my_strdup(118, &expanded_instname, expandlabel(inst_ptr[i].instname, &mult));
      comma_pos = 0;
      for(j =0; j< mult; j++) {
        if(j == 0) start = expanded_instname; 
        else start = comma_pos;
        comma_pos = strchr(start, ',');
        if(comma_pos) *comma_pos = '\0';
        dbg(1, "check_unique_names: checking %s\n", start);
        if( (entry = inst_hash_lookup(table, start, i, XINSERT, strlen(start)) ) && entry->value != i) {
          inst_ptr[i].flags |=4;
          hilight_nets=1;
          if(rename == 1) {
            if(first) {
              bbox(BEGIN,0.0,0.0,0.0,0.0);
              set_modify(1); push_undo();
              prepared_hash_instances=0;
              prepared_netlist_structs=0;
              prepared_hilight_structs=0;
              first = 0;
            }
            bbox(ADD, inst_ptr[i].x1, inst_ptr[i].y1, inst_ptr[i].x2, inst_ptr[i].y2);
          }
        }
        if(comma_pos) {
          *comma_pos=',';
          comma_pos++;
        }
      } /* for(j...) */
      if( (inst_ptr[i].flags & 4) && rename) {
        my_strdup(511, &tmp, inst_ptr[i].prop_ptr);
        new_prop_string(i, tmp, newpropcnt++, !rename);
        my_strdup2(512, &inst_ptr[i].instname, get_tok_value(inst_ptr[i].prop_ptr, "name", 0)); /* 20150409 */
        inst_hash_lookup(table, inst_ptr[i].instname, i, XINSERT, strlen(inst_ptr[i].instname));
        symbol_bbox(i, &inst_ptr[i].x1, &inst_ptr[i].y1, &inst_ptr[i].x2, &inst_ptr[i].y2);
        bbox(ADD, inst_ptr[i].x1, inst_ptr[i].y1, inst_ptr[i].x2, inst_ptr[i].y2);
        my_free(972, &tmp);
      }
      my_free(973, &expanded_instname);
    }
  } /* for(i...) */
  if(rename == 1 && hilight_nets) {
    bbox(SET,0.0,0.0,0.0,0.0);
    draw();
    bbox(END,0.0,0.0,0.0,0.0);
  }
  /* draw_hilight_net(1); */
  redraw_hilights();
  /* draw_window = save_draw; */
}



int match_symbol(const char *name)  /* never returns -1, if symbol not found load systemlib/missing.sym */
{
 int i,found;

 found=0;
 for(i=0;i<lastinstdef;i++)
 {
  /* dbg(1, "match_symbol(): name=%s, instdef[i].name=%s\n",name, instdef[i].name);*/
  if(strcmp(name, instdef[i].name) == 0)
  {
   dbg(1, "match_symbol(): found matching symbol:%s\n",name);
   found=1;break;
  }
 }
 if(!found)
 {
  dbg(1, "match_symbol(): matching symbol not found:%s, loading\n",name);
  load_sym_def(name, NULL); /* append another symbol to the instdef[] array */
 }
 dbg(1, "match_symbol(): returning %d\n",i);
 return i;
}

/* update **s modifying only the token values that are */
/* different between *new and *old */
/* return 1 if s modified 20081221 */
int set_different_token(char **s,const char *new, const char *old, int object, int n)
{
 register int c, state=XBEGIN, space;
 char *token=NULL, *value=NULL;
 int sizetok=0, sizeval=0;
 int token_pos=0, value_pos=0;
 int quote=0;
 int escape=0;
 int mod;
 const char *my_new;

 mod=0;
 my_new = new;
 dbg(1, "set_different_token(): *s=%s, new=%s, old=%s n=%d\n",*s, new, old, n);
 if(new==NULL) return 0;

 sizeval = sizetok = CADCHUNKALLOC;
 my_realloc(427, &token, sizetok);
 my_realloc(429, &value, sizeval);

 /* parse new string and add / change attributes that are missing / different from old */
 while(1) {
  c=*my_new++; 
  space=SPACE(c) ;
  if(c=='"' && !escape) quote=!quote;
  if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
  else if( state==XTOKEN && space) state=XENDTOK;
  else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
  else if( state==XSEPARATOR && !space) state=XVALUE;
  else if( state==XVALUE && space && !quote && !escape) state=XEND;
  if(value_pos>=sizeval) {
   sizeval+=CADCHUNKALLOC;
   my_realloc(431, &value,sizeval);
  }
  if(token_pos>=sizetok) {
   sizetok+=CADCHUNKALLOC;
   my_realloc(432, &token,sizetok);
  }
  if(state==XTOKEN) token[token_pos++]=c;
  else if(state==XVALUE) {
   value[value_pos++]=c;
  }
  else if(state==XENDTOK || state==XSEPARATOR) {
   if(token_pos) {
     token[token_pos]='\0'; 
     token_pos=0;
   }
  } else if(state==XEND) {
   value[value_pos]='\0';
   value_pos=0;
   if(strcmp(value, get_tok_value(old,token,1))) {
    if(event_reporting && object == ELEMENT) {
      char n1[PATH_MAX];
      char n2[PATH_MAX];
      char n3[PATH_MAX];
      printf("xschem setprop %s %s %s\n", 
           escape_chars(n1, inst_ptr[n].instname, PATH_MAX), 
           escape_chars(n2, token, PATH_MAX), 
           escape_chars(n3, value, PATH_MAX)
      );
      fflush(stdout);
    }
    mod=1;
    my_strdup(433, s, subst_token(*s, token, value) );
   }
   state=XBEGIN;
  }
  escape = (c=='\\' && !escape);
  if(c=='\0') break;
 }

 state = XBEGIN;
 escape = quote = token_pos = value_pos = 0;
 /* parse old string and remove attributes that are not present in new */
 while(old) {
  c=*old++;
  space=SPACE(c) ;
  if(c=='"' && !escape) quote=!quote;
  if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
  else if( state==XTOKEN && space) state=XENDTOK;
  else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
  else if( state==XSEPARATOR && !space) state=XVALUE;
  else if( state==XVALUE && space && !quote && !escape) state=XEND;
  if(value_pos>=sizeval) {
   sizeval+=CADCHUNKALLOC;
   my_realloc(415, &value,sizeval);
  }
  if(token_pos>=sizetok) {
   sizetok+=CADCHUNKALLOC;
   my_realloc(416, &token,sizetok);
  }
  if(state==XTOKEN) token[token_pos++]=c;
  else if(state==XVALUE) {
   value[value_pos++]=c;
  }
  else if(state==XENDTOK || state==XSEPARATOR) {
   if(token_pos) {
     token[token_pos]='\0';
     token_pos=0;
   }
   get_tok_value(new,token,1);
   if(get_tok_size == 0 ) {

    if(event_reporting && object == ELEMENT) {
      char n1[PATH_MAX];
      char n2[PATH_MAX];
      printf("xschem setprop %s %s\n",
           escape_chars(n1, inst_ptr[n].instname, PATH_MAX),
           escape_chars(n2, token, PATH_MAX)
      );
      fflush(stdout);
    }
    mod=1;
    my_strdup(443, s, subst_token(*s, token, NULL) );
   }
  } else if(state==XEND) {
   value[value_pos]='\0';
   value_pos=0;
   state=XBEGIN;
  }
  escape = (c=='\\' && !escape);
  if(c=='\0') break;
 }
 my_free(974, &token);
 my_free(975, &value);
 return mod;
}

/* return a string containing the list of all tokens in s */
/* with_quotes: */
/* 0: eat non escaped quotes (") */
/* 1: return unescaped quotes as part of the token value if they are present */
/* 2: eat backslashes */
const char *list_tokens(const char *s, int with_quotes)
{
  static char *token=NULL;
  int  sizetok=0;
  register int c, state=XBEGIN, space;
  register int token_pos=0;
  int quote=0;
  int escape=0;
 
  if(s==NULL) {
    my_free(435, &token);
    sizetok = 0;
    return "";
  }
  sizetok = CADCHUNKALLOC;
  my_realloc(451, &token, sizetok);
  while(1) {
    c=*s++;
    space=SPACE(c) ;
    if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
    else if( state==XTOKEN && space && !quote && !escape) state=XENDTOK;
    else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
    else if( state==XSEPARATOR && !space) state=XVALUE;
    else if( state==XVALUE && space && !quote && !escape ) state=XEND;
    if(token_pos>=sizetok) {
      sizetok+=CADCHUNKALLOC;
      my_realloc(434, &token,sizetok);
    }
    if(c=='"') {
      if(!escape) quote=!quote;
    }
    if(state==XTOKEN) {
      if(c=='"') {
        if((with_quotes & 1) || escape)  token[token_pos++]=c;
      }
      else if( !(c == '\\' && (with_quotes & 2)) ) token[token_pos++]=c;
      else if(escape && c == '\\') token[token_pos++]=c;
    } else if(state==XVALUE) {
      /* do nothing */
    } else if(state==XENDTOK || state==XSEPARATOR) {
        if(token_pos) {
          token[token_pos++]= ' ';
        }
    } else if(state==XEND) {
      state=XBEGIN;
    }
    escape = (c=='\\' && !escape);
    if(c=='\0') {
      if(token_pos) {
        token[token_pos-1]= (c != '\0' ) ? ' ' : '\0';
      }
      return token;
    }
  }
}

/* state machine that parses a string made up of <token>=<value> ... */
/* couples and returns the value of the given token  */
/* if s==NULL or no match return empty string */
/* NULL tok NOT ALLOWED !!!!!!!! */
/* never returns NULL... */
/* with_quotes: */
/* 0: eat non escaped quotes (") */
/* 1: return unescaped quotes as part of the token value if they are present */
/* 2: eat backslashes */
const char *get_tok_value(const char *s,const char *tok, int with_quotes)
{
  static char *result=NULL;
  static char *token=NULL;
  int size=0;
  int  sizetok=0;
  register int c, state=XBEGIN, space;
  register int token_pos=0, value_pos=0;
  int quote=0;
  int escape=0;
  int cmp = 1;
 
  if(s==NULL) {
    my_free(976, &result);
    my_free(977, &token);
    get_tok_value_size = get_tok_size = 0;
    return "";
  }
  get_tok_value_size = get_tok_size = 0;
  dbg(2, "get_tok_value(): looking for <%s> in <%s>\n",tok,s);
  sizetok = size = CADCHUNKALLOC;
  my_realloc(454, &result, size);
  my_realloc(457, &token, sizetok);
  while(1) {
    c=*s++;
    space=SPACE(c) ;
    if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
    else if( state==XTOKEN && space && !quote && !escape) state=XENDTOK;
    else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
    else if( state==XSEPARATOR && !space) state=XVALUE;
    else if( state==XVALUE && space && !quote && !escape ) state=XEND;
    if(value_pos>=size) {
      size+=CADCHUNKALLOC;
      my_realloc(436, &result,size);
    }
    if(token_pos>=sizetok) {
      sizetok+=CADCHUNKALLOC;
      my_realloc(437, &token,sizetok);
    }
    if(c=='"') {
      if(!escape) quote=!quote;
    }
    if(state==XTOKEN) {
      if(!cmp) { /* previous token matched search and was without value, return get_tok_size */
        result[0] = '\0';
        get_tok_value_size = 0;
        return result;
      }
      if(c=='"') {
        if((with_quotes & 1) || escape)  token[token_pos++]=c;
      }
      else if( !(c == '\\' && (with_quotes & 2)) ) token[token_pos++]=c;
      else if(escape && c == '\\') token[token_pos++]=c;
    } else if(state==XVALUE) {
      if(c=='"') {
        if((with_quotes & 1) || escape)  result[value_pos++]=c;
      }
      else if( !((c == '\\') && (with_quotes & 2)) ) result[value_pos++]=c; /* skip unescaped backslashes */
      else if( (c == '\\') && escape ) result[value_pos++]=c; /* 20170414 add escaped backslashes */
    } else if(state==XENDTOK || state==XSEPARATOR) {
        if(token_pos) {
          token[token_pos]='\0';
          if( !(cmp = strcmp(token,tok)) ) {
            get_tok_size = token_pos; /* report back also token size, useful to check if requested token exists */
          }
          dbg(2, "get_tok_value(): token=%s\n", token);
          token_pos=0;
        }
    } else if(state==XEND) {
      result[value_pos]='\0';
      if( !cmp ) {
        get_tok_value_size = value_pos; /* return also size so to avoid using strlen 20180926 */
        return result;
      }
      value_pos=0;
      state=XBEGIN;
    }
    escape = (c=='\\' && !escape);
    if(c=='\0') {
      result[0]='\0';
      get_tok_size = 0;
      get_tok_value_size = 0; /* return also size so to avoid using strlen 20180926 */
      return result;
    }
  }
}

/* return template string excluding name=... and token=value where token listed in extra */
/* 20081206 */
const char *get_sym_template(char *s,char *extra)
{
 static char *result=NULL;
 int sizeres=0;
 int sizetok=0;
 int sizeval=0;
 char *value=NULL;
 char *token=NULL;
 register int c, state=XBEGIN, space;
 register int token_pos=0, value_pos=0, result_pos=0;
 int quote=0;
 int escape=0;
 int with_quotes=0;
 int l;
/* with_quotes: */
/* 0: eat non escaped quotes (") */
/* 1: return unescaped quotes as part of the token value if they are present */
/* 2: eat backslashes */
/* 3: 1+2  :) */

 if(s==NULL) {
   my_free(978, &result);
   return "";
 }
 l = strlen(s);
 if(l >= sizeres) {
   sizeres = l+1;
   my_realloc(330, &result,sizeres);
 }
 sizetok = sizeval = CADCHUNKALLOC;
 my_realloc(438, &value,sizeval);
 my_realloc(439, &token,sizetok);
 while(1) {
  c=*s++; 
  space=SPACE(c) ;
  if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
  else if( state==XTOKEN && space) state=XENDTOK;
  else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
  else if( state==XSEPARATOR && !space) state=XVALUE;
  else if( state==XVALUE && space && !quote) state=XEND;
  if(value_pos>=sizeval) {
    sizeval+=CADCHUNKALLOC;
    my_realloc(441, &value,sizeval);
  }
  if(token_pos>=sizetok) {
    sizetok+=CADCHUNKALLOC;
    my_realloc(442, &token,sizetok);
  }
  if(state==XBEGIN) {
    result[result_pos++] = c;
  } else if(state==XTOKEN) {
    token[token_pos++]=c;
  } else if(state==XVALUE) {
    if(c=='"') {
     if(!escape) quote=!quote;
     if((with_quotes & 1) || escape)  value[value_pos++]=c;
    }
    else if( (c=='\\') && (with_quotes & 2) )  ;  /* dont store backslash */
    else value[value_pos++]=c;
    escape = (c=='\\' && !escape);

  } else if(state==XEND) {
    value[value_pos]='\0';
    if((!extra || !strstr(extra, token)) && strcmp(token,"name")) {
      memcpy(result+result_pos, value, value_pos+1); /* 20180923 */
      result_pos+=value_pos;
    }
    result[result_pos++] = c;
    value_pos=0;
    token_pos=0;
    state=XBEGIN;
  } else if(state==XENDTOK || state==XSEPARATOR) {
    if(token_pos) {
      token[token_pos]='\0';
      if((!extra || !strstr(extra, token)) && strcmp(token,"name")) {
        memcpy(result+result_pos, token, token_pos+1); /* 20180923 */
        result_pos+=token_pos;
        result[result_pos++] = c;
      }
      token_pos=0;
    }
  }
  if(c=='\0') {
    break;
  }
 }
 my_free(979, &value);
 my_free(980, &token);
 return result;
}

const char *find_bracket(const char *s)
{
 while(*s!='['&& *s!='\0') s++;
 return s;
}

char *get_pin_attr_from_inst(int inst, int pin, const char *attr)
{ 
   int attr_size;
   char *pinname = NULL, *pname = NULL, *pinnumber = NULL;
   char *pnumber = NULL;
   const char *str;


   dbg(1, "get_pin_attr_from_inst: inst=%d pin=%d attr=%s\n", inst, pin, attr);
   pinnumber = NULL;
   str = get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][pin].prop_ptr,"name",0);
   if(str[0]) {
     attr_size = strlen(attr);
     my_strdup(498, &pinname, str);
     pname =my_malloc(49, get_tok_value_size + attr_size + 30);
     my_snprintf(pname, get_tok_value_size + attr_size + 30, "%s(%s)", attr, pinname);
     my_free(981, &pinname);
     str = get_tok_value(inst_ptr[inst].prop_ptr, pname, 0);
     my_free(982, &pname);
     if(get_tok_size) my_strdup2(51, &pinnumber, str);
     else {
       pnumber = my_malloc(52, attr_size + 100);
       my_snprintf(pnumber, attr_size + 100, "%s(%d)", attr, pin);
       str = get_tok_value(inst_ptr[inst].prop_ptr, pnumber, 0);
       dbg(1, "get_pin_attr_from_inst: pnumber=%s\n", pnumber);
       my_free(983, &pnumber);
       if(get_tok_size) my_strdup2(40, &pinnumber, str);
     }
   }
   return pinnumber; /* caller is responsible for freeing up storage for pinnumber */
}

void new_prop_string(int i, const char *old_prop, int fast, int disable_unique_names)
{
/* given a old_prop property string, return a new */
/* property string in inst_ptr[i].prop_ptr such that the element name is */
/* unique in current design (that is, element name is changed */
/* if necessary) */
/* if old_prop=NULL return NULL */
/* if old_prop does not contain a valid "name" or empty return old_prop */
 static char prefix;
 char *old_name=NULL, *new_name=NULL;
 const char *tmp;
 const char *tmp2;
 int q,qq;
 static int last[256];
 int old_name_len; /* 20180926 */
 int new_name_len;
 int n;
 char *old_name_base = NULL;
 struct inst_hashentry *entry;
 
 dbg(1, "new_prop_string(): i=%d, old_prop=%s, fast=%d\n", i, old_prop, fast);
 if(!fast) { /* on 1st invocation of new_prop_string */
   for(q=1;q<=255;q++) last[q]=1;
 }
 if(old_prop==NULL) 
 { 
  my_free(984, &inst_ptr[i].prop_ptr);
  return;
 }
 old_name_len = my_strdup(444, &old_name,get_tok_value(old_prop,"name",0) ); /* added old_name_len 20180926 */

 if(old_name==NULL) 
 { 
  my_strdup(446, &inst_ptr[i].prop_ptr, old_prop);  /* 03102001 changed to copy old props if no name */
  return;
 }
 prefix=old_name[0];
 /* don't change old_prop if name does not conflict. */
 if(disable_unique_names || (entry = inst_hash_lookup(table, old_name, i, XLOOKUP, old_name_len)) == NULL || entry->value == i)
 {
  inst_hash_lookup(table, old_name, i, XINSERT, old_name_len);
  my_strdup(447, &inst_ptr[i].prop_ptr, old_prop);
  my_free(985, &old_name);
  return;
 }
 old_name_base = my_malloc(64, old_name_len+1);
 n = sscanf(old_name, "%[^[0-9]",old_name_base);
 tmp=find_bracket(old_name);
 my_realloc(448, &new_name, old_name_len + 40); /* strlen(old_name)+40); */ /* 20180926 */
 qq=fast ?  last[(int)prefix] : 1; 
 for(q=qq;;q++)
 {
   if(n >= 1 ) {
     new_name_len = my_snprintf(new_name, old_name_len + 40, "%s%d%s", old_name_base, q, tmp);
   } else {
     new_name_len = my_snprintf(new_name, old_name_len + 40, "%c%d%s", prefix,q, tmp); /* added new_name_len 20180926 */
   }
   if((entry = inst_hash_lookup(table, new_name, i, XLOOKUP, new_name_len)) == NULL || entry->value == i) 
   {
    last[(int)prefix]=q+1;
    break;
   }
 } 
 my_free(986, &old_name_base);
 tmp2 = subst_token(old_prop, "name", new_name); 
 if(strcmp(tmp2, old_prop) ) {
   my_strdup(449, &inst_ptr[i].prop_ptr, tmp2);
 }
 inst_hash_lookup(table, new_name, i, XINSERT, new_name_len); /* reinsert in hash */
 my_free(987, &old_name);
 my_free(988, &new_name);
}

const char *subst_token(const char *s, const char *tok, const char *new_val)
/* given a string <s> with multiple "token=value ..." assignments */
/* substitute <tok>'s value with <new_val> */
/* if tok not found in s and new_val!=NULL add tok=new_val at end.*/
/* if new_val is empty ('\0') set token value to "" (token="") */
/* if new_val is NULL *remove* 'token (and =val if any)' from s */
{
  static char *result=NULL;
  int size=0;
  register int c, state=XBEGIN, space;
  int sizetok=0;
  char *token=NULL;
  int token_pos=0, result_pos=0, result_save_pos = 0;
  int quote=0, tmp;
  int done_subst=0;
  int escape=0, matched_tok=0;
 
  if(s==NULL && tok == NULL){
    my_free(989, &result);
    return "";
  }
  if((tok == NULL || tok[0]=='\0') && s ){
    my_strdup2(458, &result, s);
    return result;
  }
  dbg(1, "subst_token(%s, %s, %s)\n", s, tok, new_val);
  sizetok = size = CADCHUNKALLOC;
  my_realloc(1152, &result, size);
  my_realloc(1153, &token, sizetok);
  result[0] = '\0';
  while( s ) {
    c=*s++; 
    space=SPACE(c);
    if(c == '"' && !escape) quote=!quote;
    /* alloc data */
    if(result_pos >= size) {
      size += CADCHUNKALLOC;
      my_realloc(455, &result, size);
    }
    if(token_pos >= sizetok) {
      sizetok += CADCHUNKALLOC;
      my_realloc(456, &token, sizetok);
    }
  
    /* parsing state machine                                    */
    /* states:                                                  */
    /*    XBEGIN XTOKEN XENDTOK XSEPARATOR XVALUE               */
    /*                                                          */
    /*                                                          */
    /* XBEGIN                                                   */
    /* |      XTOKEN                                            */
    /* |      |   XENDTOK                                       */
    /* |      |   |  XSEPARATOR                                 */
    /* |      |   |  | XVALUE                                   */
    /* |      |   |  | | XBEGIN                                 */
    /* |      |   |  | | |   XTOKEN                             */
    /* |      |   |  | | |   |     XENDTOK                      */
    /* |      |   |  | | |   |     |  XTOKEN                    */
    /* |      |   |  | | |   |     |  |                         */
    /* .......name...=.x1....format...type..=..subcircuit....   */
    /* . : space                                                */

    if(state == XBEGIN && !space && c != '=' ) {
      result_save_pos = result_pos;
      token_pos = 0;
      state = XTOKEN;
    } else if(state == XENDTOK  && (!space || c == '\0') && c != '=' ) {
      if(!done_subst && matched_tok) {
        if(new_val) { /* add new_val to matching token with no value */
          if(new_val[0]) {
            tmp = strlen(new_val);
          } else {
            new_val = "\"\"";
            tmp = 2;
          }
          if(result_pos+tmp+2 >= size) {
            size = (1+(result_pos+tmp+2) / CADCHUNKALLOC) * CADCHUNKALLOC;
            my_realloc(1154, &result, size);
          }
          memcpy(result + result_pos, "=", 1);
          memcpy(result + result_pos+1, new_val, tmp);
          memcpy(result + result_pos+1+tmp, " ", 1);
          result_pos += tmp + 2;
          done_subst = 1;
        } else { /* remove token (and value if any) */
          result_pos = result_save_pos;
          done_subst = 1;
        }
      }
      result_save_pos = result_pos;
      if(c != '\0') state = XTOKEN; /* if end of string remain in XENDTOK state */
    } else if( state == XTOKEN && space) {
      token[token_pos] = '\0';
      token_pos = 0;
      matched_tok = !strcmp(token, tok) && !done_subst;
      state=XENDTOK;
      if(c == '\0') {
        s--; /* go to next iteration and process '\0' as XENDTOK */
        continue;
      }
    } else if(state == XTOKEN && c=='=') {
      token[token_pos] = '\0';
      token_pos = 0;
      matched_tok = !strcmp(token, tok) && !done_subst;
      state=XSEPARATOR;
    } else if(state == XENDTOK && c=='=') {
      state=XSEPARATOR;
    } else if( state == XSEPARATOR && !space) {
      if(!done_subst && matched_tok) {
        if(new_val) { /* replace token value with new_val */
          if(new_val[0]) {
            tmp = strlen(new_val);
          } else {
            new_val = "\"\"";
            tmp = 2;
          }
          if(result_pos + tmp >= size) {
            size = (1 + (result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
            my_realloc(1155, &result, size);
          }
          memcpy(result + result_pos ,new_val, tmp + 1);
          result_pos += tmp;
          done_subst = 1;
        } else { /* remove token (and value if any) */
          result_pos = result_save_pos;
          done_subst = 1;
        }
      }
      state=XVALUE;
    } else if( state == XVALUE && space && !quote && !escape) {
      state=XBEGIN;
    }
    /* state actions */
    if(state == XBEGIN) {
      result[result_pos++] = c;
    } else if(state == XTOKEN) {
      token[token_pos++] = c;
      result[result_pos++] = c;
    } else if(state == XENDTOK) {
      result[result_pos++] = c;
    } else if(state == XSEPARATOR) {
      result[result_pos++] = c;
    } else if(state==XVALUE) {
      if(!matched_tok) result[result_pos++] = c; /* skip value for matching token */
    }
    escape = (c=='\\' && !escape);
    if(c == '\0') break;
  }
  if(!done_subst) { /* if tok not found add tok=new_value at end */
    if(result_pos == 0 ) result_pos = 1; /* result="" */
    if(new_val) {
      if(!new_val[0]) new_val = "\"\"";
      tmp = strlen(new_val) + strlen(tok) + 2;
      if(result_pos + tmp  >= size) {
        size = (1 + (result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
        my_realloc(460, &result,size);
      }
      my_snprintf(result + result_pos - 1, size, " %s=%s", tok, new_val ); /* result_pos guaranteed to be > 0 */
    } 
  }
  dbg(2, "subst_token(): returning: %s\n",result);
  my_free(990, &token);
  return result;
}

const char *get_trailing_path(const char *str, int no_of_dir, int skip_ext)
{
  static char s[PATH_MAX];
  size_t len;
  int ext_pos, dir_pos, n_ext, n_dir, c, i;

  my_strncpy(s, str, S(s));
  len = strlen(s);

  for(ext_pos=len, dir_pos=len, n_ext=0, n_dir=0, i=len; i>=0; i--) {
    c = s[i];
    if(c=='.' && ++n_ext==1) ext_pos = i;
    if(c=='/' && ++n_dir==no_of_dir+1) dir_pos = i;
  }
  if(skip_ext) s[ext_pos] = '\0';

  if(dir_pos==len) return s;
  dbg(2, "get_trailing_path(): str=%s, no_of_dir=%d, skip_ext=%d\n", 
                   str, no_of_dir, skip_ext);
  dbg(2, "get_trailing_path(): returning: %s\n", s+(dir_pos<len ? dir_pos+1 : 0));
  return s+(dir_pos<len ? dir_pos+1 : 0);
}

/* skip also extension */
const char *skip_dir(const char *str)
{
  return get_trailing_path(str, 0, 1);
}
   
/* no extension */ 
const char *get_cell(const char *str, int no_of_dir)
{ 
  return get_trailing_path(str, no_of_dir, 1);
}

const char *get_cell_w_ext(const char *str, int no_of_dir)
{ 
  return get_trailing_path(str, no_of_dir, 0);
}


/* not used? */
int count_labels(char *s)
{
  int i=1;
  int c;
 
  if(s==NULL) return 1;
  while( (c=(*s++)) ) {
    if(c==',') i++;
  }
  return i;
}

void print_vhdl_element(FILE *fd, int inst) /* 20071217 */
{
  int i=0, mult, tmp, tmp1;
  const char *str_ptr;
  register int c, state=XBEGIN, space;
  const char *lab;
  char *name=NULL;
  char  *generic_value=NULL, *generic_type=NULL;
  char *template=NULL,*s, *value=NULL,  *token=NULL;
  int no_of_pins=0, no_of_generics=0;
  int sizetok=0, sizeval=0;
  int token_pos=0, value_pos=0;
  int quote=0;
  int escape=0;
 
  if(get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"vhdl_format",2)[0] != '\0') {  /* 20071217 */
   print_vhdl_primitive(fd, inst); /*20071217 */
   return;
  }
  my_strdup(462, &name,inst_ptr[inst].instname);
  if(!name) my_strdup(58, &name, get_tok_value(template, "name", 0));
  if(name==NULL) {
    my_free(991, &name);
    return;
  }
  my_strdup(461, &template, (inst_ptr[inst].ptr+instdef)->templ);
  no_of_pins= (inst_ptr[inst].ptr+instdef)->rects[PINLAYER];
  no_of_generics= (inst_ptr[inst].ptr+instdef)->rects[GENERICLAYER];
 
  s=inst_ptr[inst].prop_ptr;
 
 /* print instance name and subckt */
  dbg(2, "print_vhdl_element(): printing inst name & subcircuit name\n");
  if( (lab = expandlabel(name, &tmp)) != NULL)
    fprintf(fd, "%d %s : %s\n", tmp, lab, skip_dir(inst_ptr[inst].name) );
  else  /*  name in some strange format, probably an error */
    fprintf(fd, "1 %s : %s\n", name, skip_dir(inst_ptr[inst].name) );
  dbg(2, "print_vhdl_element(): printing generics passed as properties\n");
 
 
  /* -------- print generics passed as properties */
 
  tmp=0;
  /* 20080213 use generic_type property to decide if some properties are strings, see later */
  my_strdup(464, &generic_type, get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"generic_type",2));
 
  while(1)
  {
    if (s==NULL) break;
   c=*s++;
   if(c=='\\') {
     escape=1;
     c=*s++;
   }
   else 
    escape=0;
   space=SPACE(c);
   if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
   else if( state==XTOKEN && space) state=XENDTOK;
   else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
   else if( state==XSEPARATOR && !space) state=XVALUE;
   else if( state==XVALUE && space && !quote) state=XEND;
   if(value_pos>=sizeval) {
    sizeval+=CADCHUNKALLOC;
    my_realloc(465, &value,sizeval);
   }
   if(token_pos>=sizetok) {
    sizetok+=CADCHUNKALLOC;
    my_realloc(466, &token,sizetok);
   }
   if(state==XTOKEN) token[token_pos++]=c;
   else if(state==XVALUE) {
     if(c=='"' && !escape) quote=!quote;
     else value[value_pos++]=c;
   }
   else if(state==XENDTOK || state==XSEPARATOR) {
     if(token_pos) {
       token[token_pos]='\0';
       token_pos=0;
     }
   } else if(state==XEND) {
     value[value_pos]='\0';
     value_pos=0;
     get_tok_value(template, token, 0);
     if(get_tok_size) {
       if(strcmp(token, "name") && value[0] != '\0') /* token has a value */
       {
         if(tmp == 0) {fprintf(fd, "generic map(\n");tmp++;tmp1=0;}
         if(tmp1) fprintf(fd, " ,\n");
      
         /* 20080213  put "" around string type generics! */
         if( generic_type && !strcmp(get_tok_value(generic_type,token, 2), "string")  ) {
           fprintf(fd, "  %s => \"%s\"", token, value);
         } else {
           fprintf(fd, "  %s => %s", token, value);
         }
         /* /20080213 */
    
         tmp1=1;
       }
     }
     state=XBEGIN;
   }
   if(c=='\0')  /* end string */
   {
    break ;
   }
  }
 
  /* -------- end print generics passed as properties */
      dbg(2, "print_vhdl_element(): printing generic maps \n");
 
     /* print generic map */
     for(i=0;i<no_of_generics;i++)
     {
       my_strdup(467, &generic_type,get_tok_value(
         (inst_ptr[inst].ptr+instdef)->boxptr[GENERICLAYER][i].prop_ptr,"type",0));
       my_strdup(468, &generic_value,   inst_ptr[inst].node[no_of_pins+i] );
       /*my_strdup(469, &generic_value, get_tok_value( */
       /*  (inst_ptr[inst].ptr+instdef)->boxptr[GENERICLAYER][i].prop_ptr,"value") ); */
       str_ptr = get_tok_value(
         (inst_ptr[inst].ptr+instdef)->boxptr[GENERICLAYER][i].prop_ptr,"name",0);
    if(generic_value) {                  /*03062002 dont print generics if unassigned */
       if(tmp) fprintf(fd, " ,\n");
       if(!tmp) fprintf(fd, "generic map (\n");
       fprintf(fd,"   %s => %s",
                             str_ptr ? str_ptr : "<NULL>",
                             generic_value ? generic_value : "<NULL>"  );
       tmp=1;
    }
  }
  if(tmp) fprintf(fd, "\n)\n");
 
 
 
  
   dbg(2, "print_vhdl_element(): printing port maps \n");
  /* print port map */
  fprintf(fd, "port map(\n" );
  tmp=0;
  for(i=0;i<no_of_pins;i++)
  {
    if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"vhdl_ignore",0), "true")) {
      if( (str_ptr =  pin_node(inst,i, &mult, 0)) )
      {
        if(tmp) fprintf(fd, " ,\n");
        fprintf(fd, "   %s => %s",
          get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"name",0),
          str_ptr);
        tmp=1;
      }
    }
  }
  fprintf(fd, "\n);\n\n");
   dbg(2, "print_vhdl_element(): ------- end ------ \n");
  my_free(992, &name);
  my_free(993, &generic_value);
  my_free(994, &generic_type);
  my_free(995, &template);
  my_free(996, &value);
  my_free(997, &token);
}

void print_generic(FILE *fd, char *ent_or_comp, int symbol)
{
  int tmp;
  register int c, state=XBEGIN, space;
  char *template=NULL,*format=NULL, *s, *value=NULL,  *token=NULL;
  char *type=NULL, *generic_type=NULL, *generic_value=NULL;
  const char *str_tmp;
  int i, sizetok=0, sizeval=0;
  int token_pos=0, value_pos=0;
  int quote=0;
  int escape=0;
  int token_number=0;
 
  my_strdup(472, &template, instdef[symbol].templ); /* 20150409 */
  if( !template || !(template[0]) ) {
    my_free(998, &template);
    return;
  }
  my_strdup(470, &format, get_tok_value(instdef[symbol].prop_ptr,"format",0));
  my_strdup(471, &generic_type, get_tok_value(instdef[symbol].prop_ptr,"generic_type",0));
  dbg(2, "print_generic(): symbol=%d template=%s \n", symbol, template);
 
  fprintf(fd, "%s %s ",ent_or_comp, skip_dir(instdef[symbol].name));
  if(!strcmp(ent_or_comp,"entity"))
   fprintf(fd, "is\n");
  else
   fprintf(fd, "\n");
  s=template;
  tmp=0;
  while(1)
  {
   c=*s++;
   if(c=='\\')
   {
     escape=1;
     c=*s++;
   }
   else 
    escape=0;
   space=SPACE(c);
   if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
   else if( state==XTOKEN && space) state=XENDTOK;
   else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
   else if( state==XSEPARATOR && !space) state=XVALUE;
   else if( state==XVALUE && space && !quote) state=XEND;
   if(value_pos>=sizeval)
   {
    sizeval+=CADCHUNKALLOC;
    my_realloc(473, &value,sizeval);
   }
   if(token_pos>=sizetok)
   {
    sizetok+=CADCHUNKALLOC;
    my_realloc(474, &token,sizetok);
   }
   if(state==XTOKEN) token[token_pos++]=c;
   else if(state==XVALUE) 
   {
    if(c=='"' && !escape) quote=!quote;
    else value[value_pos++]=c;
   }
   else if(state==XENDTOK || state==XSEPARATOR) {
     if(token_pos) {
       token[token_pos]='\0';
       token_pos=0;
     }
   } else if(state==XEND)                    /* got a token */
   {
    token_number++;
    value[value_pos]='\0';
    value_pos=0;
    my_strdup(475, &type, get_tok_value(generic_type,token,0));
 
    if(value[0] != '\0') /* token has a value */
    {
     if(token_number>1)
     {
       if(!tmp) {fprintf(fd, "generic (\n");}
       if(tmp) fprintf(fd, " ;\n");
       if(!type || strcmp(type,"string") ) { /* 20080213 print "" around string values 20080418 check for type==NULL */
         fprintf(fd, "  %s : %s := %s", token, type? type:"integer", value);
       } else {
         fprintf(fd, "  %s : %s := \"%s\"", token, type? type:"integer", value);
       }                                         /* /20080213 */
 
       tmp=1;
     }
    }
    state=XBEGIN;
   }
   if(c=='\0')  /* end string */
   {
    break ;
   }
  }

  for(i=0;i<instdef[symbol].rects[GENERICLAYER];i++)
  {
    my_strdup(476, &generic_type,get_tok_value(
              instdef[symbol].boxptr[GENERICLAYER][i].prop_ptr,"generic_type",0));
    my_strdup(477, &generic_value, get_tok_value(
              instdef[symbol].boxptr[GENERICLAYER][i].prop_ptr,"value",2) ); /*<< 170402 */
    str_tmp = get_tok_value(instdef[symbol].boxptr[GENERICLAYER][i].prop_ptr,"name",0);
    if(!tmp) fprintf(fd, "generic (\n");
    if(tmp) fprintf(fd, " ;\n");
    fprintf(fd,"  %s : %s",str_tmp ? str_tmp : "<NULL>",
                             generic_type ? generic_type : "<NULL>"  );
    if(generic_value &&generic_value[0])
      fprintf(fd," := %s", generic_value);
    tmp=1;
  }
  if(tmp) fprintf(fd, "\n);\n");
  my_free(999, &template);
  my_free(1000, &format);
  my_free(1001, &value);
  my_free(1002, &token);
  my_free(1003, &type);
  my_free(1004, &generic_type);
  my_free(1005, &generic_value);
}


void print_verilog_param(FILE *fd, int symbol) /*16112003 */
{
 register int c, state=XBEGIN, space;
 char *template=NULL, *s, *value=NULL,  *generic_type=NULL, *token=NULL;
 int sizetok=0, sizeval=0;
 int token_pos=0, value_pos=0;
 int quote=0;
 int escape=0;
 int token_number=0;

 /* my_strdup(478, &template, get_tok_value(instdef[symbol].prop_ptr,"template",0)); */
 my_strdup(479, &template, instdef[symbol].templ); /* 20150409 20171103 */
 my_strdup(480, &generic_type, get_tok_value(instdef[symbol].prop_ptr,"generic_type",0));
 if( !template || !(template[0]) )  {
   my_free(1006, &template);
   my_free(1007, &generic_type);
   return;
 }
 dbg(2, "print_verilog_param(): symbol=%d template=%s \n", symbol, template);

 s=template;
 while(1)
 {
  c=*s++;
  if(c=='\\')
  {
    escape=1;
    c=*s++;
  }
  else 
   escape=0;
  space=SPACE(c);
  if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
  else if( state==XTOKEN && space) state=XENDTOK;
  else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
  else if( state==XSEPARATOR && !space) state=XVALUE;
  else if( state==XVALUE && space && !quote) state=XEND;

  if(value_pos>=sizeval)
  {
   sizeval+=CADCHUNKALLOC;
   my_realloc(481, &value,sizeval);
  }

  if(token_pos>=sizetok)
  {
   sizetok+=CADCHUNKALLOC;
   my_realloc(482, &token,sizetok);
  }

  if(state==XTOKEN) token[token_pos++]=c;
  else if(state==XVALUE) 
  {
   if(c=='"' && !escape) quote=!quote;
   else value[value_pos++]=c;
  }
  else if(state==XENDTOK || state==XSEPARATOR) {
    if(token_pos) {
      token[token_pos]='\0';
      token_pos=0;
    }
  } else if(state==XEND)                    /* got a token */
  {
   token_number++;
   value[value_pos]='\0';
   value_pos=0;

   if(value[0] != '\0') /* token has a value */
   {
    if(token_number>1)
    {

      /* 20080915 put "" around string params */
      if( !generic_type || strcmp(get_tok_value(generic_type,token, 2), "time")  ) {
        if( generic_type && !strcmp(get_tok_value(generic_type,token, 2), "string")  ) {
          fprintf(fd, "  parameter   %s = \"%s\" ;\n", token,  value);
        }
        else  {
          fprintf(fd, "  parameter   %s = %s ;\n", token,  value);
        }
      }
    }
   }
   state=XBEGIN;
  }
  if(c=='\0')  /* end string */
  {
   break ;
  }
 }
 my_free(1008, &template);
 my_free(1009, &generic_type);
 my_free(1010, &value);
 my_free(1011, &token);
}





void print_spice_subckt(FILE *fd, int symbol)
{
 int i=0, mult;
 const char *str_ptr=NULL; 
 register int c, state=XBEGIN, space;
 char *format=NULL,*s, *token=NULL;
 int pin_number; /* 20180911 */
 int sizetok=0;
 int token_pos=0, escape=0;
 int no_of_pins=0;
 int quote=0; /* 20171029 */
 
 my_strdup(103, &format, get_tok_value(instdef[symbol].prop_ptr,"format",0));
 if( (format==NULL) ) {
   my_free(1012, &format);
   return; /* no format */
 }
 no_of_pins= instdef[symbol].rects[PINLAYER];
 s=format;

 /* begin parsing format string */
 while(1)
 {
  c=*s++; 
  if(c=='"' && escape) { 
    quote=!quote; /* 20171029 */
  }
  if(c=='"' && !escape ) c=*s++;
  if(c=='\n' && escape ) c=*s++; /* 20171030 eat escaped newlines */
  /* 20150317 use SPACE2() instead of SPACE() */
  space=SPACE2(c);
  if( state==XBEGIN && c=='@' && !escape) state=XTOKEN;
  else if( state==XTOKEN && (space || c == '@' || c == '\\')  && token_pos > 1 && !escape && !quote) state=XSEPARATOR;
  if(token_pos>=sizetok)
  {
   sizetok+=CADCHUNKALLOC;
   my_realloc(104, &token,sizetok);
  }
  if(state==XTOKEN) {
    if(c!='\\' || escape) token[token_pos++]=c; /* 20171029 remove escaping backslashes */
  }
  else if(state==XSEPARATOR)                    /* got a token */
  {
   token[token_pos]='\0'; 
   token_pos=0;
   if(!strcmp(token, "@name")) {
     /* do nothing */
   }
   else if(strcmp(token, "@symname")==0) {
     break ;
   }
   else if(strcmp(token, "@pinlist")==0) {
    for(i=0;i<no_of_pins;i++)
    {
      if(strcmp(get_tok_value(instdef[symbol].boxptr[PINLAYER][i].prop_ptr,"spice_ignore",0), "true")) {
        str_ptr=
          expandlabel(get_tok_value(instdef[symbol].boxptr[PINLAYER][i].prop_ptr,"name",0), &mult);
        fprintf(fd, "%s ", str_ptr);
      }
    }
   }
   else if(token[0]=='@' && token[1]=='@') {    /* recognize single pins 15112003 */
     for(i = 0; i<no_of_pins; i++) {
       if(!strcmp(get_tok_value(instdef[symbol].boxptr[PINLAYER][i].prop_ptr,"name",0), token + 2)) break;
     }
     if(i<no_of_pins && strcmp(get_tok_value(instdef[symbol].boxptr[PINLAYER][i].prop_ptr,"spice_ignore",0), "true")) {
       fprintf(fd, "%s ", expandlabel(token+2, &mult));
     }
   }
   /* reference by pin number instead of pin name, allows faster lookup of the attached net name 20180911 */
   else if(token[0]=='@' && token[1]=='#') {
     pin_number = atoi(token+2); 
     if(pin_number < no_of_pins) {
       if(strcmp(get_tok_value(instdef[symbol].boxptr[PINLAYER][pin_number].prop_ptr,"spice_ignore",0), "true")) {
       str_ptr =  get_tok_value(instdef[symbol].boxptr[PINLAYER][pin_number].prop_ptr,"name",0);
       fprintf(fd, "%s ",  expandlabel(str_ptr, &mult));
       }
     }
   }
   if(c != '@' && c!='\0' && (c!='\\'  || escape) ) fputc(c,fd);
   if(c == '@') s--;
   state=XBEGIN;
  }
                 /* 20151028 dont print escaping backslashes */
  else if(state==XBEGIN && c!='\0' && (c!='\\' || escape)) {
   /* do nothing */
  }
  if(c=='\0') 
  {
   break ;
  }
  escape = (c=='\\' && !escape);

 }
 my_free(1013, &format);
 my_free(1014, &token);
}




void print_spice_element(FILE *fd, int inst)
{
  int i=0, mult, tmp;
  const char *str_ptr=NULL; 
  register int c, state=XBEGIN, space;
  char *template=NULL,*format=NULL,*s, *name=NULL,  *token=NULL;
  const char *lab, *value = NULL;
  int pin_number; /* 20180911 */
  int sizetok=0;
  int token_pos=0, escape=0;
  int no_of_pins=0;
  int quote=0; /* 20171029 */
  /* struct inst_hashentry *ptr; */

  my_strdup(483, &template,
    (inst_ptr[inst].ptr+instdef)->templ); /* 20150409 */

  my_strdup(484, &name,inst_ptr[inst].instname); /* 20161210 */
    /* my_strdup(485, &name,get_tok_value(inst_ptr[inst].prop_ptr,"name",0)); */
  if (!name) my_strdup(43, &name, get_tok_value(template, "name", 0));

  my_strdup(486, &format,
    get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"format",2));

  if ((name==NULL) || (format==NULL)) {
    my_free(1015, &template); 
    my_free(1016, &format); 
    my_free(1017, &name); 
    return; /* do no netlist unwanted insts(no format) */
  }

  no_of_pins= (inst_ptr[inst].ptr+instdef)->rects[PINLAYER];
  s=format;
  dbg(1, "print_spice_element: name=%s, format=%s netlist_count=%d\n",name,format, netlist_count);

  /* begin parsing format string */
  while(1)
  {
    c=*s++; 
    if(c=='"' && !escape) { 
      quote=!quote; /* 20171029 */
      c = *s++;
    }
    if (c=='\n' && escape) c=*s++; /* 20171030 eat escaped newlines */
    /* 20150317 use SPACE2() instead of SPACE() */
    space=SPACE2(c);
                                /* 20151028 */
    if (state==XBEGIN && (c=='@'|| c=='$')  && !escape) state=XTOKEN;

    /* 20171029 added !escape, !quote */
    else if (state==XTOKEN && (space || c == '$' || c == '@' || c == '\\')  && token_pos > 1 && !escape && !quote) {
      dbg(1, "print_spice_element: c=%c, space=%d, escape=%d, quote=%d\n", c, space, escape, quote);
      state=XSEPARATOR;
    }

    if (token_pos>=sizetok)
    {
      sizetok+=CADCHUNKALLOC;
      my_realloc(487, &token,sizetok);
    }

    if(state==XTOKEN) {
      if (c!='\\' || escape) token[token_pos++]=c; /* 20171029 remove escaping backslashes */
    }
    else if (state==XSEPARATOR)                    /* got a token */
    {
      token[token_pos]='\0'; 
      token_pos=0;

      if (!spiceprefix && !strcmp(token, "@spiceprefix")) { 
        value=NULL;
      } else {
        dbg(1, "print_spice_element(): token: |%s|\n", token);
        value = get_tok_value(inst_ptr[inst].prop_ptr, token+1, 2);
        if (value[0] == '\0')
        value=get_tok_value(template, token+1, 0);
      }
      if(!get_tok_size && token[0] =='$') {
        fputs(token + 1, fd);
      } else if (value && value[0]!='\0') {
         /* instance names (name) and node labels (lab) go thru the expandlabel function. */
        /*if something else must be parsed, put an if here! */
  
        if (!(strcmp(token+1,"name") && strcmp(token+1,"lab"))  /* expand name/labels */
              && ((lab = expandlabel(value, &tmp)) != NULL))
          fputs(lab,fd);
        else fputs(value,fd);
      }
      else if (strcmp(token,"@symname")==0) /* of course symname must not be present  */
                                           /* in hash table */
      {
        fputs(skip_dir(inst_ptr[inst].name),fd);
      }
      else if(strcmp(token,"@schname")==0) /* of course schname must not be present  */
                                           /* in hash table */
      {
        /* fputs(schematic[currentsch],fd); */
        fputs(current_name, fd); /* 20190519 */
      }
      else if(strcmp(token,"@pinlist")==0) /* of course pinlist must not be present  */
                                           /* in hash table. print multiplicity */
      {                                    /* and node number: m1 n1 m2 n2 .... */
        for(i=0;i<no_of_pins;i++)
        {
          if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"spice_ignore",0), "true")) {
            str_ptr =  pin_node(inst,i, &mult, 0);
            /* fprintf(errfp, "inst: %s  --> %s\n", name, str_ptr); */
            fprintf(fd, "@%d %s ", mult, str_ptr);
          }
        }
      }
      else if(token[0]=='@' && token[1]=='@') {    /* recognize single pins 15112003 */
        for(i=0;i<no_of_pins;i++) {
          if (!strcmp(
            get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"name",0),
            token+2)) {
            if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"spice_ignore",0), "true")) {
              str_ptr =  pin_node(inst,i, &mult, 0);
              fprintf(fd, "@%d %s", mult, str_ptr);
            }
            break; /* 20171029 */
          }
        }
      }
      /* reference by pin number instead of pin name, allows faster lookup of the attached net name 20180911 */
      else if (token[0]=='@' && token[1]=='#') {
        pin_number = atoi(token+2); 
        if (pin_number < no_of_pins) {
          if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][pin_number].prop_ptr,"spice_ignore",0), "true")) {
            str_ptr =  pin_node(inst,pin_number, &mult, 0);
            fprintf(fd, "@%d %s ", mult, str_ptr);
          }
        }
      }
      else if (!strncmp(token,"@tcleval", 8)) { /* 20171029 */
        /* char tclcmd[strlen(token)+100]; */
        size_t s;
        char *tclcmd=NULL;
        s = token_pos + strlen(name) + strlen(inst_ptr[inst].name) + 100;
        tclcmd = my_malloc(488, s);
        Tcl_ResetResult(interp);
        my_snprintf(tclcmd, s, "tclpropeval {%s} {%s} {%s}", token, name, inst_ptr[inst].name);
        dbg(1, "tclpropeval {%s} {%s} {%s}", token, name, inst_ptr[inst].name);
        tcleval(tclcmd);
        fprintf(fd, "%s", tclresult());
        my_free(1018, &tclcmd);
      } /* /20171029 */
                    /* 20151028 dont print escaping backslashes */
      if (c != '$' && c != '@' && c!='\0' && (c!='\\'  || escape)) fputc(c,fd);
      if (c == '@' || c == '$' ) s--;
      state=XBEGIN;
    }
                   /* 20151028 dont print escaping backslashes */
    else if(state==XBEGIN && c!='\0' && (c!='\\' || escape))  fputc(c,fd);
    if(c=='\0') 
    {
      fputc('\n',fd);
      break;
    }
    if (c=='\\') escape=1; else escape=0;
  }
  my_free(1019, &template);
  my_free(1020, &format);
  my_free(1021, &name);
  my_free(1022, &token);
}





void print_tedax_element(FILE *fd, int inst)
{
 int i=0, mult;
 const char *str_ptr=NULL; 
 register int c, state=XBEGIN, space;
 char *template=NULL,*format=NULL,*s, *name=NULL, *token=NULL;
 const char *value;
 char *extra=NULL, *extra_pinnumber=NULL;
 char *numslots=NULL;
 int pin_number; /* 20180911 */
 const char *extra_token, *extra_token_val;
 char *extra_ptr;
 char *extra_pinnumber_token, *extra_pinnumber_ptr;
 char *saveptr1, *saveptr2;
 const char *tmp;
 int instance_based=0;
 int sizetok=0;
 int token_pos=0, escape=0;
 int no_of_pins=0;
 int quote=0; /* 20171029 */
 /* struct inst_hashentry *ptr; */

 my_strdup(489, &extra, get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"extra",2));
 my_strdup(41, &extra_pinnumber, get_tok_value(inst_ptr[inst].prop_ptr,"extra_pinnumber",2));
 if(!extra_pinnumber) my_strdup(490, &extra_pinnumber, get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"extra_pinnumber",2));
 my_strdup(491, &template,
     (inst_ptr[inst].ptr+instdef)->templ); /* 20150409 */
 my_strdup(492, &numslots, get_tok_value(inst_ptr[inst].prop_ptr,"numslots",2));
 if(!numslots) my_strdup(493, &numslots, get_tok_value(template,"numslots",2));
 if(!numslots) my_strdup(494, &numslots, "1");

 my_strdup(495, &name,inst_ptr[inst].instname); /* 20161210 */
 /* my_strdup(496, &name,get_tok_value(inst_ptr[inst].prop_ptr,"name",0)); */
 if(!name) my_strdup(2, &name, get_tok_value(template, "name", 0));

 my_strdup(497, &format,
     get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"tedax_format",2));
 if(name==NULL || !format || !format[0]) {
   my_free(1023, &extra);
   my_free(1024, &extra_pinnumber);
   my_free(1025, &template);
   my_free(1026, &numslots);
   my_free(1027, &format);
   my_free(1028, &name);
   return; 
 }
 no_of_pins= (inst_ptr[inst].ptr+instdef)->rects[PINLAYER];

 fprintf(fd, "begin_inst %s numslots %s\n", name, numslots);
 for(i=0;i<no_of_pins; i++) {
   char *pinnumber;
   pinnumber = get_pin_attr_from_inst(inst, i, "pinnumber");
   if(!pinnumber) {
     my_strdup2(500, &pinnumber, get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"pinnumber",0));
   }
   if(!get_tok_size) my_strdup(501, &pinnumber, "--UNDEF--");
   tmp = pin_node(inst,i, &mult, 0);
   if(tmp && strcmp(tmp, "<UNCONNECTED_PIN>")) {
     fprintf(fd, "conn %s %s %s %s %d\n",
           name,
           tmp,
           get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"name",0),
           pinnumber,
           i+1);
   }
   my_free(1029, &pinnumber);
 }
 
 if(extra){ 
   char netstring[40];
   /* fprintf(errfp, "extra_pinnumber: |%s|\n", extra_pinnumber); */
   /* fprintf(errfp, "extra: |%s|\n", extra); */
   for(extra_ptr = extra, extra_pinnumber_ptr = extra_pinnumber; ; extra_ptr=NULL, extra_pinnumber_ptr=NULL) {
     extra_pinnumber_token=my_strtok_r(extra_pinnumber_ptr, " ", &saveptr1);
     extra_token=my_strtok_r(extra_ptr, " ", &saveptr2);
     if(!extra_token) break;
     /* fprintf(errfp, "extra_pinnumber_token: |%s|\n", extra_pinnumber_token); */
     /* fprintf(errfp, "extra_token: |%s|\n", extra_token); */
     instance_based=0;

     /* alternate instance based extra net naming: net:<pinumber>=netname */
     my_snprintf(netstring, S(netstring), "net:%s", extra_pinnumber_token);
     dbg(1, "print_tedax_element(): netstring=%s\n", netstring);
     extra_token_val=get_tok_value(inst_ptr[inst].prop_ptr, extra_token, 0);
     if(!extra_token_val[0]) extra_token_val=get_tok_value(inst_ptr[inst].prop_ptr, netstring, 0);
     if(!extra_token_val[0]) extra_token_val=get_tok_value(template, extra_token, 0);
     else instance_based=1;
     if(!extra_token_val[0]) extra_token_val="--UNDEF--";
     
     fprintf(fd, "conn %s %s %s %s %d", name, extra_token_val, extra_token, extra_pinnumber_token, i+1);
     i++;
     if(instance_based) fprintf(fd, " # instance_based");
     fprintf(fd,"\n");
   }
 }
     
 if(format) {
  s=format;
  dbg(1, "print_tedax_element: name=%s, tedax_format=%s netlist_count=%d\n",name,format, netlist_count);
  /* begin parsing format string */
  while(1)
  {
   c=*s++; 
   if(c=='"' && !escape) { 
     quote=!quote; /* 20171029 */
     c = *s++;
   }
   if(c=='\n' && escape ) c=*s++; /* 20171030 eat escaped newlines */
   /* 20150317 use SPACE2() instead of SPACE() */
   space=SPACE2(c);
                               /* 20151028 */
   if( state==XBEGIN && (c=='$' || c=='@') && !escape) state=XTOKEN;
 
   /* 20171029 added !escape, !quote */
   else if( state==XTOKEN && (space || c == '$' || c == '@' || c == '\\')  && token_pos > 1 && !escape && !quote) state=XSEPARATOR;
 
   if(token_pos>=sizetok)
   {
    sizetok+=CADCHUNKALLOC;
    my_realloc(502, &token,sizetok);
   }
  
   if(state==XTOKEN) {
     if(c!='\\' || escape) token[token_pos++]=c; /* 20171029 remove escaping backslashes */
   }
   else if(state==XSEPARATOR)                   /* got a token */
   {
    token[token_pos]='\0'; 
    token_pos=0;
 
    value = get_tok_value(inst_ptr[inst].prop_ptr, token+1, 0);
    if(!get_tok_size) value=get_tok_value(template, token+1, 0);
    if(!get_tok_size && token[0] =='$') {
      fputs(token + 1, fd);
    } else if(value[0]!='\0')
    {
      fputs(value,fd);
    }
    else if(strcmp(token,"@symname")==0)        /* of course symname must not be present  */
                                        /* in hash table */
    {
     fputs(skip_dir(inst_ptr[inst].name),fd);
    }
    else if(strcmp(token,"@schname")==0)        /* of course schname must not be present  */
                                        /* in hash table */
    {
     /* fputs(schematic[currentsch],fd); */
     fputs(current_name, fd); /* 20190519 */
    }
    else if(strcmp(token,"@pinlist")==0)        /* of course pinlist must not be present  */
                                        /* in hash table. print multiplicity */
    {                                   /* and node number: m1 n1 m2 n2 .... */
     for(i=0;i<no_of_pins;i++)
     {
       str_ptr =  pin_node(inst,i, &mult, 0);
       /* fprintf(errfp, "inst: %s  --> %s\n", name, str_ptr); */
       fprintf(fd, "@%d %s ", mult, str_ptr);
     }
    }
    else if(token[0]=='@' && token[1]=='@') {    /* recognize single pins 15112003 */
     for(i=0;i<no_of_pins;i++) {
      if(!strcmp(
           get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"name",0),
           token+2
          )
        ) {
        str_ptr =  pin_node(inst,i, &mult, 0);
        fprintf(fd, "%s", str_ptr);
        break; /* 20171029 */
      }
     }

    /* this allow to print in netlist any properties defined for pins.
     * @#n:property, where 'n' is the pin index (starting from 0) and 
     * 'property' the property defined for that pin (property=value) 
     * in case this property is found the value for it is printed.
     * if device is slotted (U1:m) and property value for pin
     * is also slotted ('a:b:c:d') then print the m-th substring.
     * if property value is not slotted print entire value regardless of device slot.
     * slot numbers start from 1
     */
    } else if(token[0]=='@' && token[1]=='#') {  /* 20180911 */
      if( strchr(token, ':') )  {

        int n;
        char *subtok = my_malloc(503, sizetok * sizeof(char));
        char *subtok2 = my_malloc(42, sizetok * sizeof(char)+20);
        subtok[0]='\0';
        n=-1;
        sscanf(token+2, "%d:%s", &n, subtok);
        if(n!=-1 && subtok[0]) {
          my_snprintf(subtok2, sizetok * sizeof(char)+20, "%s(%d)", subtok, n);
          value = get_tok_value(inst_ptr[inst].prop_ptr,subtok2,0);
          if( n>=0 && n < (inst_ptr[inst].ptr+instdef)->rects[PINLAYER]) {
            if(!value[0]) value = get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][n].prop_ptr,subtok,0);
          } 
          if(value[0]) {
            char *ss;
            int slot;
            if( (ss=strchr(inst_ptr[inst].instname, ':')) ) {
              sscanf(ss+1, "%d", &slot);
              if(strstr(value, ":")) value = find_nth(value, ':', slot);
            }
            fprintf(fd, "%s", value);
          }
        }
        my_free(1030, &subtok);
        my_free(1031, &subtok2);
      } else {
        /* reference by pin number instead of pin name, allows faster lookup of the attached net name 20180911 */
        /* @#n --> return net name attached to pin of index 'n' */
        pin_number = atoi(token+2);
        if(pin_number < no_of_pins) {
          str_ptr =  pin_node(inst,pin_number, &mult, 0);
          fprintf(fd, "%s", str_ptr);
        }
      }
    }
    else if(!strncmp(token,"@tcleval", 8)) { /* 20171029 */
      /* char tclcmd[strlen(token)+100] ; */
      size_t s;
      char *tclcmd=NULL;
      s = token_pos + strlen(name) + strlen(inst_ptr[inst].name) + 100;
      tclcmd = my_malloc(504, s);
      Tcl_ResetResult(interp);
      my_snprintf(tclcmd, s, "tclpropeval {%s} {%s} {%s}", token, name, inst_ptr[inst].name);
      tcleval(tclcmd);
      fprintf(fd, "%s", tclresult());
      my_free(1032, &tclcmd);
      /* fprintf(errfp, "%s\n", tclcmd); */
    } /* /20171029 */
 
 
                  /* 20151028 dont print escaping backslashes */
    if(c!='$' && c!='@' && c!='\0' && (c!='\\'  || escape) ) fputc(c,fd);
    if(c == '@' || c == '$' ) s--;
    state=XBEGIN;
   }
                  /* 20151028 dont print escaping backslashes */
   else if(state==XBEGIN && c!='\0' && (c!='\\' || escape))  fputc(c,fd);
   if(c=='\0') 
   {
    fputc('\n',fd);
    break ;
   }
   escape = (c=='\\' && !escape);

 
  }
 } /* if(format) */
 fprintf(fd,"end_inst\n");
 my_free(1033, &extra);
 my_free(1034, &extra_pinnumber);
 my_free(1035, &template);
 my_free(1036, &numslots);
 my_free(1037, &format);
 my_free(1038, &name);
 my_free(1039, &token);
}

/* verilog module instantiation:
     cmos_inv
     #(
     .WN ( 1.5e-05 ) ,
     .WP ( 4.5e-05 ) ,
     .LLN ( 3e-06 ) ,
     .LLP ( 3e-06 )
     )
     Xinv ( 
      .A( AA ),
      .Z( Z )
     );
*/
void print_verilog_element(FILE *fd, int inst)
{
 int i=0, mult, tmp;
 const char *str_ptr;
 const char *lab;
 char *name=NULL;
 char  *generic_type=NULL;
 char *template=NULL,*s;
 int no_of_pins=0;
 int  tmp1 = 0;
 register int c, state=XBEGIN, space;
 char *value=NULL,  *token=NULL;
 int sizetok=0, sizeval=0;
 int token_pos=0, value_pos=0;
 int quote=0;
 
 if(get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"verilog_format",2)[0] != '\0') {
  print_verilog_primitive(fd, inst); /*15112003 */
  return;
 }
 my_strdup(506, &template,
     (inst_ptr[inst].ptr+instdef)->templ); /* 20150409 */

 my_strdup(507, &name,inst_ptr[inst].instname); /* 20161210 */
 if(!name) my_strdup(3, &name, get_tok_value(template, "name", 0));
 if(name==NULL) {
   my_free(1040, &template);
   my_free(1041, &name);
   return;
 }
 no_of_pins= (inst_ptr[inst].ptr+instdef)->rects[PINLAYER];

 /* 20080915 use generic_type property to decide if some properties are strings, see later */
 my_strdup(505, &generic_type, get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"generic_type",2));

 s=inst_ptr[inst].prop_ptr;

/* print instance  subckt */
  dbg(2, "print_verilog_element(): printing inst name & subcircuit name\n");
   fprintf(fd, "%s\n", skip_dir(inst_ptr[inst].name) );

 /* -------- print generics passed as properties */
 tmp=0;
 while(1)
 {
   if (s==NULL) break;
  c=*s++;
  if(c=='\\')
  {
    c=*s++;
  }
  space=SPACE(c);
  if( (state==XBEGIN || state==XENDTOK) && !space && c != '=') state=XTOKEN;
  else if( state==XTOKEN && space) state=XENDTOK;
  else if( (state==XTOKEN || state==XENDTOK) && c=='=') state=XSEPARATOR;
  else if( state==XSEPARATOR && !space) state=XVALUE;
  else if( state==XVALUE && space && !quote) state=XEND;

  if(value_pos>=sizeval)
  {
   sizeval+=CADCHUNKALLOC;
   my_realloc(509, &value,sizeval);
  }

  if(token_pos>=sizetok)
  {
   sizetok+=CADCHUNKALLOC;
   my_realloc(510, &token,sizetok);
  }

  if(state==XTOKEN) token[token_pos++]=c;
  else if(state==XVALUE) 
  {
    value[value_pos++]=c;
  }
  else if(state==XENDTOK || state==XSEPARATOR) {
    if(token_pos) {
      token[token_pos]='\0';
      token_pos=0;
    }
  } else if(state==XEND) 
  {
   value[value_pos]='\0';
   value_pos=0;
   get_tok_value(template, token, 0);
   if(strcmp(token, "name") && get_tok_size) {
     if(value[0] != '\0') /* token has a value */
     {
       if(strcmp(token,"spice_ignore") && strcmp(token,"vhdl_ignore") && strcmp(token,"tedax_ignore")) {
         if(tmp == 0) {fprintf(fd, "#(\n---- start parameters\n");tmp++;tmp1=0;}
         if(tmp1) fprintf(fd, " ,\n");
         if( !generic_type || strcmp(get_tok_value(generic_type,token, 2), "time")  ) {
           if( generic_type && !strcmp(get_tok_value(generic_type,token, 2), "string")  ) {
             fprintf(fd, "  .%s ( \"%s\" )", token, value);
           } else {
             fprintf(fd, "  .%s ( %s )", token, value);
           }
           tmp1=1;
         }
       }
     }
   }
   state=XBEGIN;
  }
  if(c=='\0')  /* end string */
  {
   break ;
  }
 }
 if(tmp) fprintf(fd, "\n---- end parameters\n)\n");

 /* -------- end print generics passed as properties */

/* print instance name */
 if( (lab = expandlabel(name, &tmp)) != NULL)
   fprintf(fd, "---- instance %s (\n", lab );
 else  /*  name in some strange format, probably an error */
   fprintf(fd, "---- instance %s (\n", name );
 
  dbg(2, "print_verilog_element(): printing port maps \n");
 /* print port map */
 tmp=0;
 for(i=0;i<no_of_pins;i++)
 {
   if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"verilog_ignore",0), "true")) {
     if( (str_ptr =  pin_node(inst,i, &mult, 0)) )
     {
       if(tmp) fprintf(fd,"\n");
       fprintf(fd, "  @%d %s %s ", mult, 
         get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"name",0),
         str_ptr);
       tmp=1;
     }
   }
 }
 fprintf(fd, "\n);\n\n");
 dbg(2, "print_verilog_element(): ------- end ------ \n");
 my_free(1042, &name);
 my_free(1043, &generic_type);
 my_free(1044, &template);
 my_free(1045, &value);
 my_free(1046, &token);
}


const char *pin_node(int i, int j, int *mult, int hash_prefix_unnamed_net)
{
 int tmp;
 char errstr[2048];
 static const char unconn[]="<UNCONNECTED_PIN>";
 char str_node[40]; /* 20161122 overflow safe */
 if(inst_ptr[i].node[j]!=NULL)
 {
  if((inst_ptr[i].node[j])[0] == '#') /* unnamed net */
  {
   /* get unnamed node multiplicity ( minimum mult found in circuit) */
   *mult = get_unnamed_node(3, 0, atoi((inst_ptr[i].node[j])+4) );
    dbg(2, "pin_node(): node = %s  n=%d mult=%d\n",
     inst_ptr[i].node[j], atoi(inst_ptr[i].node[j]), *mult);
   if(hash_prefix_unnamed_net) {
     if(*mult>1)   /* unnamed is a bus */
      my_snprintf(str_node, S(str_node), "%s[%d:0]", (inst_ptr[i].node[j]), *mult-1);
     else
      my_snprintf(str_node, S(str_node), "%s", (inst_ptr[i].node[j]) );
   } else {
     if(*mult>1)   /* unnamed is a bus */
      my_snprintf(str_node, S(str_node), "%s[%d:0]", (inst_ptr[i].node[j])+1, *mult-1);
     else
      my_snprintf(str_node, S(str_node), "%s", (inst_ptr[i].node[j])+1 );
   }
   expandlabel(get_tok_value(
           (inst_ptr[i].ptr+instdef)->boxptr[PINLAYER][j].prop_ptr,"name",1), mult);
   return expandlabel(str_node, &tmp);
  }
  else
  {
   expandlabel(get_tok_value(
           (inst_ptr[i].ptr+instdef)->boxptr[PINLAYER][j].prop_ptr,"name",1), mult);
   return expandlabel(inst_ptr[i].node[j], &tmp);
  }
 }
 else
 {
   *mult=1;

   my_snprintf(errstr, S(errstr), "Warning: unconnected pin,  Inst idx: %d, Pin idx: %d  Inst:%s\n", i, j, inst_ptr[i].instname ) ;
   statusmsg(errstr,2);
   if(!netlist_count) {
     inst_ptr[i].flags |=4;
     hilight_nets=1;
   }
   return unconn;
 }
}


void print_vhdl_primitive(FILE *fd, int inst) /* netlist  primitives, 20071217 */
{
 int i=0, mult, tmp;
 const char *str_ptr;
 register int c, state=XBEGIN, space;
 const char *lab;
 char *template=NULL,*format=NULL,*s, *name=NULL, *token=NULL;
 const char *value;
 int pin_number; /* 20180911 */
 int sizetok=0;
 int token_pos=0, escape=0;
 int no_of_pins=0;
 int quote=0; /* 20171029 */
 /* struct inst_hashentry *ptr; */

 my_strdup(513, &template, (inst_ptr[inst].ptr+instdef)->templ);
 my_strdup(514, &name, inst_ptr[inst].instname);
 if(!name) my_strdup(50, &name, get_tok_value(template, "name", 0));

 my_strdup(516, &format, get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"vhdl_format",2)); /* 20071217 */
 if((name==NULL) || (format==NULL) ) {
   my_free(1047, &template);
   my_free(1048, &name);
   my_free(1151, &format);
   return; /*do no netlist unwanted insts(no format) */
 }
 no_of_pins= (inst_ptr[inst].ptr+instdef)->rects[PINLAYER];
 s=format;
 dbg(1, "print_vhdl_primitive: name=%s, format=%s netlist_count=%d\n",name,format, netlist_count);

 fprintf(fd, "---- start primitive ");
 lab=expandlabel(name, &tmp);
 fprintf(fd, "%d\n",tmp);
 /* begin parsing format string */
 while(1)
 {
  c=*s++; 
  if(c=='"' && !escape) {
    quote=!quote; /* 20171029 */
    c = *s++;
  }
  if(c=='\n' && escape ) c=*s++; /* 20171030 eat escaped newlines */
  space=SPACE(c);
                               /* 20171029 */
  if( state==XBEGIN && (c=='@' || c=='$') && !escape ) state=XTOKEN;
  /* 20171029 added !escape, !quote */
  else if( state==XTOKEN && (space || c=='$' || c=='@' || c == '\\') && token_pos > 1 && !escape && !quote) state=XSEPARATOR;

  if(token_pos>=sizetok)
  {
   sizetok+=CADCHUNKALLOC;
   my_realloc(517, &token,sizetok);
  }

  if(state==XTOKEN) {
    if(c!='\\' || escape) token[token_pos++]=c; /* 20171029 remove escaping backslashes */
  }
  else if(state==XSEPARATOR)                    /* got a token */
  {
   token[token_pos]='\0'; 
   token_pos=0;
   
   value = get_tok_value(inst_ptr[inst].prop_ptr, token+1, 2);
   if(value[0] == '\0')
   value=get_tok_value(template, token+1, 0);
   if(!get_tok_size && token[0] =='$') {
     fputs(token + 1, fd);
   } else if(value[0]!='\0')
   {  /* instance names (name) and node labels (lab) go thru the expandlabel function. */
      /*if something else must be parsed, put an if here! */

    if(!(strcmp(token+1,"name"))) {
      if( (lab=expandlabel(value, &tmp)) != NULL) 
         fprintf(fd, "----name(%s)", lab);
      else 
         fprintf(fd, "%s", value);
    }
    else if(!(strcmp(token+1,"lab"))) {
      if( (lab=expandlabel(value, &tmp)) != NULL) 
         fprintf(fd, "----pin(%s)", lab);
      else 
         fprintf(fd, "%s", value);
    }
    else  fprintf(fd, "%s", value);
   }
   else if(strcmp(token,"@symname")==0) /* of course symname must not be present  */
                                        /* in hash table */
   {
    fprintf( fd, "%s",skip_dir(inst_ptr[inst].name) );
   }
   else if(strcmp(token,"@schname")==0) /* of course schname must not be present  */
                                        /* in hash table */
   {
     /* fputs(schematic[currentsch],fd); */
     fputs(current_name, fd); /* 20190519 */
   }
   else if(strcmp(token,"@pinlist")==0) /* of course pinlist must not be present  */
                                        /* in hash table. print multiplicity */
   {                                    /* and node number: m1 n1 m2 n2 .... */
    for(i=0;i<no_of_pins;i++)
    {
      if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"vhdl_ignore",0), "true")) {
        str_ptr =  pin_node(inst,i, &mult, 0);
        fprintf(fd, "----pin(%s) ", str_ptr);
      }
    }
   }
   else if(token[0]=='@' && token[1]=='@') {    /* recognize single pins 15112003 */
    for(i=0;i<no_of_pins;i++) {
     if(!strcmp(
          get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"name",0),
          token+2
         )) {
       if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"vhdl_ignore",0), "true")) {
         str_ptr =  pin_node(inst,i, &mult, 0);
         fprintf(fd, "----pin(%s) ", str_ptr);
       }
       break; /* 20171029 */
     }
    }
   }
   /* reference by pin number instead of pin name, allows faster lookup of the attached net name 20180911 */
   else if(token[0]=='@' && token[1]=='#') {
       pin_number = atoi(token+2);
       if(pin_number < no_of_pins) {
         if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][pin_number].prop_ptr,"vhdl_ignore",0), "true")) {
           str_ptr =  pin_node(inst,pin_number, &mult, 0);
           fprintf(fd, "----pin(%s) ", str_ptr);
         }
       }
   }
   else if(!strncmp(token,"@tcleval", 8)) { /* 20171029 */
     /* char tclcmd[strlen(token)+100] ; */
     size_t s;
     char *tclcmd=NULL;
     s = token_pos + strlen(name) + strlen(inst_ptr[inst].name) + 100;
     tclcmd = my_malloc(518, s);
     Tcl_ResetResult(interp);
     my_snprintf(tclcmd, s, "tclpropeval {%s} {%s} {%s}", token, name, inst_ptr[inst].name);
     tcleval(tclcmd);
     fprintf(fd, "%s", tclresult());
     my_free(1049, &tclcmd);
   }

                 /* 20180911 dont print escaping backslashes */
   if(c!='$' && c!='@' && c!='\0' && (c!='\\'  || escape) ) fputc(c,fd);
   if(c == '@' || c == '$') s--;
   state=XBEGIN;
  }
                 /* 20180911 dont print escaping backslashes */
  else if(state==XBEGIN && c!='\0' && (c!='\\' || escape))  fputc(c,fd);

  if(c=='\0') 
  {
   fputc('\n',fd);
   fprintf(fd, "---- end primitive\n");
   break ;
  }
  escape = (c=='\\' && !escape);
 }
 my_free(1050, &template);
 my_free(1051, &format);
 my_free(1052, &name);
 my_free(1053, &token);
}

/* print verilog element if verilog_format is specified */
void print_verilog_primitive(FILE *fd, int inst) /* netlist switch level primitives, 15112003 */
{
  int i=0, mult, tmp;
  const char *str_ptr;
  register int c, state=XBEGIN, space;
  const char *lab;
  char *template=NULL,*format=NULL,*s=NULL, *name=NULL, *token=NULL;
  const char *value;
  int pin_number; /* 20180911 */
  int sizetok=0;
  int token_pos=0, escape=0;
  int no_of_pins=0;
  int quote=0; /* 20171029 */
  /* struct inst_hashentry *ptr; */
 
  my_strdup(519, &template,
      (inst_ptr[inst].ptr+instdef)->templ); /* 20150409 */
 
  my_strdup(520, &name,inst_ptr[inst].instname); /*20161210 */
  if(!name) my_strdup(4, &name, get_tok_value(template, "name", 0));
 
  my_strdup(522, &format,
      get_tok_value((inst_ptr[inst].ptr+instdef)->prop_ptr,"verilog_format",2));
  if((name==NULL) || (format==NULL) ) {
    my_free(1054, &template);
    my_free(1055, &name);
    my_free(1056, &format);
    return; /*do no netlist unwanted insts(no format) */
  }
  no_of_pins= (inst_ptr[inst].ptr+instdef)->rects[PINLAYER];
  s=format;
  dbg(1, "print_verilog_primitive: name=%s, format=%s netlist_count=%d\n",name,format, netlist_count);
 
  fprintf(fd, "---- start primitive ");
  lab=expandlabel(name, &tmp);
  fprintf(fd, "%d\n",tmp);
  /* begin parsing format string */
  while(1)
  {
   c=*s++; 
   if(c=='"' && !escape) {
     quote=!quote; /* 20171029 */
     c = *s++;
   }
   if(c=='\n' && escape ) c=*s++; /* 20171030 eat escaped newlines */
   space=SPACE(c);                
   if( state==XBEGIN && (c=='@' || c=='$') && !escape ) state=XTOKEN;
   /* 20171029 added !escape, !quote */
   else if( state==XTOKEN && (space || c == '$' || c == '@' || c == '\\') && token_pos > 1 && !escape && !quote) state=XSEPARATOR;
 
   if(token_pos>=sizetok)
   {
    sizetok+=CADCHUNKALLOC;
    my_realloc(523, &token,sizetok);
   }
 
   if(state==XTOKEN) {
     if(c!='\\' || escape) token[token_pos++]=c; /* 20171029 remove escaping backslashes */
   }
   else if(state==XSEPARATOR)                    /* got a token */
   {
    token[token_pos]='\0'; 
    token_pos=0;
 
    value = get_tok_value(inst_ptr[inst].prop_ptr, token+1, 2);
    if(value[0] == '\0')
    value=get_tok_value(template, token+1, 0);
    if(!get_tok_size && token[0] =='$') {
      fputs(token + 1, fd);
    } else if(value[0]!='\0') {
       /* instance names (name) and node labels (lab) go thru the expandlabel function. */
       /*if something else must be parsed, put an if here! */
 
     if(!(strcmp(token+1,"name"))) {
       if( (lab=expandlabel(value, &tmp)) != NULL) 
          fprintf(fd, "----name(%s)", lab);
       else 
          fprintf(fd, "%s", value);
     }
     else if(!(strcmp(token+1,"lab"))) {
       if( (lab=expandlabel(value, &tmp)) != NULL) 
          fprintf(fd, "----pin(%s)", lab);
       else 
          fprintf(fd, "%s", value);
     }
     else  fprintf(fd, "%s", value);
    }
    else if(strcmp(token,"@symname")==0) /* of course symname must not be present  */
                                         /* in hash table */
    {
     fprintf( fd, "%s",skip_dir(inst_ptr[inst].name) );
    }
    else if(strcmp(token,"@schname")==0) /* of course schname must not be present  */
                                         /* in hash table */
    {
      /* fputs(schematic[currentsch],fd); */
      fputs(current_name, fd); /* 20190519 */
    }
    else if(strcmp(token,"@pinlist")==0) /* of course pinlist must not be present  */
                                         /* in hash table. print multiplicity */
    {                                    /* and node number: m1 n1 m2 n2 .... */
     for(i=0;i<no_of_pins;i++)
     {
       if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"verilog_ignore",0), "true")) {
         str_ptr =  pin_node(inst,i, &mult, 0);
         fprintf(fd, "----pin(%s) ", str_ptr);
       }
     }
    }
    else if(token[0]=='@' && token[1]=='@') {    /* recognize single pins 15112003 */
     for(i=0;i<no_of_pins;i++) {
      if(!strcmp(
           get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"name",0),
           token+2)) {
        if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][i].prop_ptr,"verilog_ignore",0), "true")) {
          str_ptr =  pin_node(inst,i, &mult, 0);
          fprintf(fd, "----pin(%s) ", str_ptr);
        }
        break; /* 20171029 */
      }
     }
    }
    /* reference by pin number instead of pin name, allows faster lookup of the attached net name 20180911 */
    else if(token[0]=='@' && token[1]=='#') {
        pin_number = atoi(token+2);
        if(pin_number < no_of_pins) {
          if(strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][pin_number].prop_ptr,"verilog_ignore",0), "true")) {
            str_ptr =  pin_node(inst,pin_number, &mult, 0);
            fprintf(fd, "----pin(%s) ", str_ptr);
          }
        }
    }
    else if(!strncmp(token,"@tcleval", 8)) { /* 20171029 */
      /* char tclcmd[strlen(token)+100] ; */
      size_t s;
      char *tclcmd=NULL;
      s = token_pos + strlen(name) + strlen(inst_ptr[inst].name) + 100;
      tclcmd = my_malloc(524, s);
      Tcl_ResetResult(interp);
      my_snprintf(tclcmd, s, "tclpropeval {%s} {%s} {%s}", token, name, inst_ptr[inst].name);
      tcleval(tclcmd);
      fprintf(fd, "%s", tclresult());
      my_free(1057, &tclcmd);
    }
                  /* 20180911 dont print escaping backslashes */
    if(c!='$' && c!='@' && c!='\0' && (c!='\\'  || escape) ) fputc(c,fd);
    if(c == '@' || c == '$') s--;
    state=XBEGIN;
   }
                  /* 20180911 dont print escaping backslashes */
   else if(state==XBEGIN && c!='\0' && (c!='\\' || escape))  fputc(c,fd);
   if(c=='\0') 
   {
    fputc('\n',fd);
    fprintf(fd, "---- end primitive\n");
    break ;
   }
   escape = (c=='\\' && !escape);
  }
  my_free(1058, &template);
  my_free(1059, &format);
  my_free(1060, &name);
  my_free(1061, &token);
}

int isonlydigit(const char *s)
{
    char c;
    if(s == NULL || *s == '\0') return 0;
    while( (c = *s) ) {
     if(c < '0' || c > '9') return 0;
     s++;
    }
    return 1;
}

/* 20180911 */
const char *find_nth(const char *str, char sep, int n)
{
  static char *result=NULL;
  static char empty[]="";
  int i;
  char *ptr;
  int count;

  if(!str) {
    my_free(1062, &result);
    return empty;
  }
  my_strdup(525, &result, str);
  if(!result) return empty;
  for(i=0, count=1, ptr=result; result[i] != 0; i++) {
    if(result[i]==sep) {
      result[i]=0;
      if(count==n) {
        return ptr;
      }
      ptr=result+i+1;
      count++;
    }
  }
  if(count==n) return ptr;
  else return empty;
}

/* substitute given tokens in a string with their corresponding values */
/* ex.: name=@name w=@w l=@l ---> name=m112 w=3e-6 l=0.8e-6 */
/* if s==NULL return emty string */
const char *translate(int inst, char* s)
{
 static char empty[]="";
 static char *result=NULL;
 int size=0, tmp;
 register int c, state=XBEGIN, space;
 char *token=NULL;
 const char *tmp_sym_name;
 int sizetok=0;
 int result_pos=0, token_pos=0;
 /* struct inst_hashentry *ptr; */
 struct stat time_buf;
 struct tm *tm;
 char file_name[PATH_MAX];
 const char *value;
 int escape=0;
 char date[200];


 if(!s) {
   my_free(1063, &result);
   return empty;
 }
    
 size=CADCHUNKALLOC;
 my_realloc(527, &result,size);
 result[0]='\0';

 dbg(2, "translate(): substituting props in <%s>, instance <%s>\n",
        s?s:"NULL",inst_ptr[inst].instname?inst_ptr[inst].instname:"NULL");

 while(1)
 {
  c=*s++; 
  if(c=='\\') {
    escape=1;
    c=*s++;
  }
  else escape=0;
  space=SPACE2(c);
  if( state==XBEGIN && (c=='@' || c=='$' ) && !escape  ) state=XTOKEN; /* 20161210 escape */
  else if( state==XTOKEN && ( 
                              (space && !escape)  || 
                               (c =='@' || c == '$') ||
                              (!space && escape)
                            ) 
                         && token_pos > 1 ) state=XSEPARATOR;

  if(result_pos>=size)
  {
   size+=CADCHUNKALLOC;
   my_realloc(528, &result,size);
  }

  if(token_pos>=sizetok)
  {
   sizetok+=CADCHUNKALLOC;
   my_realloc(529, &token,sizetok);
  }
  if(state==XTOKEN) token[token_pos++]=c;
  else if(state==XSEPARATOR) 
  {
   token[token_pos]='\0'; 
   dbg(2, "translate(): token=%s\n", token);

   if(!spiceprefix && !strcmp(token, "@spiceprefix")) {
     value = NULL;
     get_tok_size = 0;
   } else {
     value = get_tok_value(inst_ptr[inst].prop_ptr, token+1, 2);
     if(!get_tok_size) value=get_tok_value((inst_ptr[inst].ptr+instdef)->templ, token+1, 2); /* 20190310 2 instead of 0 */
   }

   if(!get_tok_size && token[0] =='$') {
    tmp=token_pos - 1; /* strlen(token+1), excluding leading '$' */
    if(result_pos + tmp>=size) {
     size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
     my_realloc(368, &result,size);
    }
    dbg(2, "translate(): token=%s, token_pos = %d\n", token, token_pos);
    memcpy(result+result_pos, token + 1, tmp+1); /* 20180923 */
    result_pos+=tmp;
   }
   token_pos = 0;
   if(get_tok_size) {
    tmp=get_tok_value_size;  /* strlen(value); */  /* 20180926 */
    if(result_pos + tmp>=size) {
     size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
     my_realloc(530, &result,size);
    }
    memcpy(result+result_pos, value, tmp+1); /* 20180923 */
    result_pos+=tmp;
   } else if(strcmp(token,"@symname")==0) {
    tmp_sym_name=inst_ptr[inst].name ? get_cell(inst_ptr[inst].name, 0) : "";
    tmp=strlen(tmp_sym_name);
    if(result_pos + tmp>=size) {
     size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
     my_realloc(531, &result,size);
    }
    memcpy(result+result_pos,tmp_sym_name, tmp+1); /* 20180923 */
    result_pos+=tmp;
   } else if(strcmp(token,"@symname_ext")==0) {
    tmp_sym_name=inst_ptr[inst].name ? get_cell_w_ext(inst_ptr[inst].name, 0) : "";
    tmp=strlen(tmp_sym_name);
    if(result_pos + tmp>=size) {
     size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
     my_realloc(453, &result,size);
    }
    memcpy(result+result_pos,tmp_sym_name, tmp+1); /* 20180923 */
    result_pos+=tmp;

   } else if(token[0]=='@' && token[1]=='#') {  /* 20180911 */
     int n;
     char *pin_attr = my_malloc(532, sizetok * sizeof(char));
     char *pin_num_or_name = my_malloc(55, sizetok * sizeof(char));

     pin_num_or_name[0]='\0';
     pin_attr[0]='\0';
     n=-1;
     sscanf(token+2, "%[^:]:%[^:]", pin_num_or_name, pin_attr);
     if(isonlydigit(pin_num_or_name)) {
       n = atoi(pin_num_or_name);
     }
     else if(pin_num_or_name[0]) {
       for(n = 0 ; n < (inst_ptr[inst].ptr+instdef)->rects[PINLAYER]; n++) {
         if(!strcmp(get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][n].prop_ptr,"name",0), pin_num_or_name)) break;
       }
     }
     if(n>=0  && pin_attr[0] && n < (inst_ptr[inst].ptr+instdef)->rects[PINLAYER]) {
       char *pinnumber;
       pinnumber = get_pin_attr_from_inst(inst, n, pin_attr);
       if(!pinnumber) {
         my_strdup2(499, &pinnumber, get_tok_value((inst_ptr[inst].ptr+instdef)->boxptr[PINLAYER][n].prop_ptr, pin_attr, 2));
       }
       if(!get_tok_size) my_strdup(379, &pinnumber, "--UNDEF--");
       value = pinnumber;
       if(value[0] != 0) {
         char *ss;
         int slot;
         if( (ss=strchr(inst_ptr[inst].instname, ':')) ) {
           sscanf(ss+1, "%d", &slot);
           if(strstr(value,":")) value = find_nth(value, ':', slot);
         }
         tmp=strlen(value);
         if(result_pos + tmp>=size) {
           size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
           my_realloc(533, &result,size);
         }
         memcpy(result+result_pos, value, tmp+1); /* 20180923 */
         result_pos+=tmp;
       }
       my_free(1064, &pinnumber);
     }
     my_free(1065, &pin_attr);
     my_free(1066, &pin_num_or_name);
   } else if(strcmp(token,"@sch_last_modified")==0) {

    my_strncpy(file_name, abs_sym_path(get_tok_value(
      (inst_ptr[inst].ptr+instdef)->prop_ptr, "schematic",0 ), "")
      , S(file_name));
    if(!file_name[0]) {
      my_strncpy(file_name, add_ext(abs_sym_path(inst_ptr[inst].name, ""), ".sch"), S(file_name));
    }
    if(!stat(file_name , &time_buf)) {
      tm=localtime(&(time_buf.st_mtime) );
      tmp=strftime(date, sizeof(date), "%Y-%m-%d  %H:%M:%S", tm);
      if(result_pos + tmp>=size) {
       size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
       my_realloc(534, &result,size);
      }
      memcpy(result+result_pos, date, tmp+1); /* 20180923 */
      result_pos+=tmp;
    } 
   } else if(strcmp(token,"@sym_last_modified")==0) {
    my_strncpy(file_name, abs_sym_path(inst_ptr[inst].name, ""), S(file_name));
    if(!stat(file_name , &time_buf)) {
      tm=localtime(&(time_buf.st_mtime) );
      tmp=strftime(date, sizeof(date), "%Y-%m-%d  %H:%M:%S", tm);
      if(result_pos + tmp>=size) {
       size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
       my_realloc(535, &result,size);
      }
      memcpy(result+result_pos, date, tmp+1); /* 20180923 */
      result_pos+=tmp;
    }
   } else if(strcmp(token,"@time_last_modified")==0) {
    my_strncpy(file_name, abs_sym_path(schematic[currentsch], ""), S(file_name));
    if(!stat(file_name , &time_buf)) {  /* 20161211 */
      tm=localtime(&(time_buf.st_mtime) );
      tmp=strftime(date, sizeof(date), "%Y-%m-%d  %H:%M:%S", tm);
      if(result_pos + tmp>=size) {
       size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
       my_realloc(536, &result,size);
      }
      memcpy(result+result_pos, date, tmp+1); /* 20180923 */
      result_pos+=tmp;
    }
   } else if(strcmp(token,"@schname")==0) {
     /* tmp=strlen(schematic[currentsch]);*/
     tmp = strlen(current_name); /* 20190519 */
     if(result_pos + tmp>=size) {
      size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
      my_realloc(537, &result,size);
     }
     /* memcpy(result+result_pos,schematic[currentsch], tmp+1); */ /* 20180923 */
     memcpy(result+result_pos, current_name, tmp+1); /* 20190519 */
     result_pos+=tmp;
   }
   else if(strcmp(token,"@prop_ptr")==0 && inst_ptr[inst].prop_ptr) {
     tmp=strlen(inst_ptr[inst].prop_ptr);
     if(result_pos + tmp>=size)
     {
      size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
      my_realloc(538, &result,size);
     }
     memcpy(result+result_pos,inst_ptr[inst].prop_ptr, tmp+1); /* 20180923 */
     result_pos+=tmp;
   }
   else if(strcmp(token,"@schvhdlprop")==0 && schvhdlprop)
   {
     tmp=strlen(schvhdlprop);
     if(result_pos + tmp>=size)
     {
      size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
      my_realloc(539, &result,size);
     }
     memcpy(result+result_pos,schvhdlprop, tmp+1); /* 20180923 */
     result_pos+=tmp;
   }
   /* 20100217 */
   else if(strcmp(token,"@schprop")==0 && schprop)
   {
     tmp=strlen(schprop);
     if(result_pos + tmp>=size)
     {
      size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
      my_realloc(331, &result,size);
     }
     memcpy(result+result_pos,schprop, tmp+1); /* 20180923 */
     result_pos+=tmp;
   }
   /* /20100217 */

   else if(strcmp(token,"@schsymbolprop")==0 && schsymbolprop)
   {
     tmp=strlen(schsymbolprop);
     if(result_pos + tmp>=size)
     {
      size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
      my_realloc(540, &result,size);
     }
     memcpy(result+result_pos,schsymbolprop, tmp+1); /* 20180923 */
     result_pos+=tmp;
   }
   else if(strcmp(token,"@schtedaxprop")==0 && schtedaxprop)
   {
     tmp=strlen(schtedaxprop);
     if(result_pos + tmp>=size)
     {
      size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
      my_realloc(541, &result,size);
     }
     memcpy(result+result_pos,schtedaxprop, tmp+1); /* 20180923 */
     result_pos+=tmp;
   }
   /* /20100217 */

   else if(strcmp(token,"@schverilogprop")==0 && schverilogprop) /*09112003 */
   {
     tmp=strlen(schverilogprop);
     if(result_pos + tmp>=size)
     {
      size=(1+(result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
      my_realloc(542, &result,size);
     }
     memcpy(result+result_pos,schverilogprop, tmp+1); /* 20180923 */
     result_pos+=tmp;
   }

   if(c == '@' || c == '$') s--;
   else result[result_pos++]=c;
   state=XBEGIN;
  }
  else if(state==XBEGIN) result[result_pos++]=c; 
  if(c=='\0') 
  {
   result[result_pos]='\0';
   break;
  }
 }
 dbg(2, "translate(): returning %s\n", result);
 my_free(1067, &token);
 return result;
}

const char *translate2(struct Lcc *lcc, int level, char* s)
{
  static char empty[]="";
  static char *result = NULL;
  int i, size = 0, tmp, save_tok_size, save_value_size;
  register int c, state = XBEGIN, space;
  char *token = NULL;
  const char *tmp_sym_name;
  int sizetok = 0;
  int result_pos = 0, token_pos = 0;
  char *value1 = NULL;
  char *value2 = NULL;
  char *value = NULL;
  int escape = 0;

  if(!s) {
    my_free(1068, &result);
    return empty;
  }
  size = CADCHUNKALLOC;
  my_realloc(661, &result, size);
  result[0] = '\0';

  while (1) {
    c = *s++;
    if (c == '\\') {
      escape = 1;
      /* we keep backslashes as they should mark end of tokens as for example in: @token1\xxxx@token2
         these backslashes will be 'eaten' at drawing time by translate() */
      /* c = *s++; */
    }
    else escape = 0;
    space = SPACE2(c);
    if (state == XBEGIN && c == '@' && !escape) state = XTOKEN;
    else if (state == XTOKEN && ( (space && !escape) || c == '@' || (!space && escape)) && token_pos > 1) state = XSEPARATOR;
    if (result_pos >= size) {
      size += CADCHUNKALLOC;
      my_realloc(662, &result, size);
    }
    if (token_pos >= sizetok) {
      sizetok += CADCHUNKALLOC;
      my_realloc(663, &token, sizetok);
    }
    if (state == XTOKEN) token[token_pos++] = c;
    else if (state == XSEPARATOR) {
      token[token_pos] = '\0';
      token_pos = 0;

      if(!spiceprefix && !strcmp(token, "@spiceprefix")) {
        my_free(1069, &value1);
        get_tok_size = 0;
      } else {
        my_strdup2(332, &value1, get_tok_value(lcc[level].prop_ptr, token + 1, 2));
      }
      
      value = "";
      if(get_tok_size) {
        value = value1;
        i = level;
        /* recursive substitution of value using parent level prop_str attributes */
        while(i > 1) {
          save_tok_size = get_tok_size;
          save_value_size = get_tok_value_size;
          my_strdup2(440, &value2, get_tok_value(lcc[i-1].prop_ptr, value, 2));
          if(get_tok_size && value2[0]) {
            value = value2;
          } else {
            /* restore last successful get_tok_value() size parameters */
            get_tok_size = save_tok_size;
            get_tok_value_size = save_value_size;
            break;
          }
          i--;
        }
        tmp = get_tok_value_size;  /* strlen(value); */
        if (result_pos + tmp + 1 >= size) { /* +1 to add leading '$' */
          size = (1 + (result_pos + tmp + 1) / CADCHUNKALLOC) * CADCHUNKALLOC;
          my_realloc(664, &result, size);
        }
        /* prefix substituted token with a '$' so it will be recognized by translate() for last level translation with 
           instance placement prop_ptr attributes  at drawing/netlisting time. */
        memcpy(result + result_pos , "$", 1);
        memcpy(result + result_pos + 1 , value, tmp + 1);
        result_pos += tmp + 1;
      }
      else if (strcmp(token, "@symname") == 0) {
        tmp_sym_name = lcc[level].symname ? get_cell(lcc[level].symname, 0) : "";
        tmp = strlen(tmp_sym_name);
        if (result_pos + tmp >= size) {
          size = (1 + (result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
          my_realloc(655, &result, size);
        }
        memcpy(result + result_pos, tmp_sym_name, tmp + 1);
        result_pos += tmp;
      }
      else if (strcmp(token, "@symname_ext") == 0) {
        tmp_sym_name = lcc[level].symname ? get_cell_w_ext(lcc[level].symname, 0) : "";
        tmp = strlen(tmp_sym_name);
        if (result_pos + tmp >= size) {
          size = (1 + (result_pos + tmp) / CADCHUNKALLOC) * CADCHUNKALLOC;
          my_realloc(665, &result, size);
        }
        memcpy(result + result_pos, tmp_sym_name, tmp + 1);
        result_pos += tmp;
      }

      if (c == '@') s--;
      else result[result_pos++] = c;
      state = XBEGIN;
    }
    else if (state == XBEGIN) result[result_pos++] = c;
    if (c == '\0') {
      result[result_pos] = '\0';
      break;
    }
  }
  my_free(1070, &token);
  my_free(1071, &value1);
  my_free(1072, &value2);
  return result;
}
