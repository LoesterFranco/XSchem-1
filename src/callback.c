/* File: callback.c
 * 
 * This file is part of XSCHEM,
 * a schematic capture and Spice/Vhdl/Verilog netlisting tool for circuit 
 * simulation.
 * Copyright (C) 1998-2016 Stefan Frederik Schippers
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

// main window callback
// mx and my are set to the mouse coord. relative to window 
int callback(int event, int mx, int my, KeySym key, 
                 int button, int aux, int state)  
{
 static int mx_save, my_save;
 static double mx_double_save, my_double_save; // 20070322
 char str[4096];// overflow safe 20161122
 FILE *fp;
 unsigned short sel;

 state &=~Mod2Mask; // 20170511 filter out NumLock status
 if(semaphore)
 {
   if(debug_var>=1) 
     fprintf(errfp, "callback(): reentrant call of callback(), semaphore=%d\n", semaphore);
   if(event==Expose) {
     XCopyArea(display, save_pixmap, window, gctiled, mx,my,button,aux,mx,my);
     
   }
   //return 0;
 }
 semaphore++;		// used to debug Tcl-Tk frontend
 mousex=mx*zoom - xorigin;
 mousey=my*zoom - yorigin;
 mousex_snap=rint(( mousex)/cadsnap)*cadsnap;
 mousey_snap=rint(( mousey)/cadsnap)*cadsnap;
 {
  snprintf(str, S(str), "mouse = %g %g - %s  selected: %d", 
    mousex_snap, mousey_snap, schematic[currentsch],
    lastselected );
  statusmsg(str,1);
 }
 switch(event)
 {
  case EnterNotify:
  my_snprintf(str, S(str), "%s/%s",getenv("HOME"), ".selection.sch"); // 20161115 PWD->HOME
  if( (fp=fopen(str, "r"))==NULL && (ui_state & STARTCOPY) ) 
  {
   copy_objects(ABORT);
   unselect_all();
  }
  else if(fp) fclose(fp);
  if(lastselected==0)
  {
    if(debug_var>=2) fprintf(errfp, "callback(): Enter event\n");
   mousex_snap = 490;
   mousey_snap = -340;
   
   merge_file(1, ".sch");
   unlink(str);
  }
  break;

  case Expose:
    XCopyArea(display, save_pixmap, window, gctiled, mx,my,button,aux,mx,my);
    {
      XRectangle xrect[1];
      xrect[0].x=mx;
      xrect[0].y=my;
      xrect[0].width=button;
      xrect[0].height=aux;
      // redraw selection on expose, needed if no backing store available on the server 20171112
      XSetClipRectangles(display, gc[SELLAYER], 0,0, xrect, 1, Unsorted);
      rebuild_selected_array();
      draw_selection(gc[SELLAYER],0);
      XSetClipMask(display, gc[SELLAYER], None);
    }

    if(debug_var>=1) fprintf(errfp, "callback(): Expose\n");
    break;
  case ConfigureNotify:
    resetwin();
    draw();
    break;

  case MotionNotify:
    if(ui_state & STARTPAN2)   pan2(RUBBER, mx, my); //20121123 -  20160425 moved up
    if(semaphore==2) break;
    if(ui_state) {
      snprintf(str, S(str), "mouse = %g %g - %s  selected: %d w=%g h=%g", 
        mousex_snap, mousey_snap, schematic[currentsch], 
        lastselected ,
        mousex_snap-mx_double_save, mousey_snap-my_double_save // 20070322
      );
      statusmsg(str,1);
    }
    if(ui_state & STARTPAN)    pan(RUBBER);
    if(ui_state & STARTPAN2)   pan2(RUBBER, mx, my); //20121123
    if(ui_state & STARTZOOM)   zoom_box(RUBBER);
    if(ui_state & STARTSELECT) {
      if(state & Button3Mask) { // 20171026 added unselect by area 
          select_rect(RUBBER,0);
      } else if(state & Button1Mask) {
          select_rect(RUBBER,1);
      }
    }
    if(ui_state & STARTWIRE) {
      if(horizontal_move) mousey_snap = my_double_save; // 20171023
      if(vertical_move) mousex_snap = mx_double_save;
      new_wire(RUBBER, mousex_snap, mousey_snap);
    }
    if(ui_state & STARTLINE) {
      if(horizontal_move) mousey_snap = my_double_save; // 20171023
      if(vertical_move) mousex_snap = mx_double_save;
      new_line(RUBBER);
    }
    if(ui_state & STARTMOVE) {
      if(horizontal_move) mousey_snap = my_double_save; // 20171023
      if(vertical_move) mousex_snap = mx_double_save;
      move_objects(RUBBER,0,0,0);
    }
    if(ui_state & STARTCOPY) {
      if(horizontal_move) mousey_snap = my_double_save; // 20171023
      if(vertical_move) mousex_snap = mx_double_save;
      copy_objects(RUBBER);
    }
    if(ui_state & STARTRECT) new_rect(RUBBER);
    if(ui_state & STARTPOLYGON) {
      if(horizontal_move) mousey_snap = my_double_save;
      if(vertical_move) mousex_snap = mx_double_save;
      new_polygon(RUBBER); // 20171115
    }

    if(!(ui_state & STARTPOLYGON) && (state&Button1Mask) && !(state & ShiftMask))  // start of a mouse area selection
    {
      static int onetime=0;
      if(mx != mx_save || my != my_save) {
        if( !(ui_state & STARTSELECT)) {
          select_rect(BEGIN,1);
          onetime=1;
        }
        if(abs(mx-mx_save) > 8 || abs(my-my_save) > 8 ) { // 20121123 set some reasonable threshold before unselecting
          if(onetime) {
            unselect_all(); // 20171026 avoid multiple calls of unselect_all()
            onetime=0;
          }
          ui_state|=STARTSELECT; // set it again cause unselect_all() clears it... 20121123
        }
      }
    }
 
    if((state&Button3Mask) && (state & ShiftMask)) { // 20150927 unselect area
      if( !(ui_state & STARTSELECT)) {
        select_rect(BEGIN,0);
      }
    }
 
    if((state&Button1Mask) && (state & ShiftMask)) {
      if(mx != mx_save || my != my_save) {
        if( !(ui_state & STARTSELECT)) {
          select_rect(BEGIN,1);
        }
        if(abs(mx-mx_save) > 8 || abs(my-my_save) > 8 ) {  // 20121130 set some reasonable threshold before unselecting
          select_object(mx_save*zoom-xorigin, my_save*zoom -yorigin, 0); // 20121130 remove near object if dragging
        }
      }
    }

  break;
  case KeyRelease:  // 20161118
    if(key==' ') {  // pan schematic
      if(ui_state & STARTPAN2) {  // 20121123
        ui_state &=~STARTPAN2;
      }
    }
  break;
  case KeyPress: // 20161118
   if(key==' ') {
     if(semaphore<2) { // 20160425
       rebuild_selected_array();
       if(lastselected==0) ui_state &=~SELECTION;
     }
     // if(!ui_state) {   // 20121123     //20121127 to be validated : pan2 even when some other ui_state action in progress
     pan2(BEGIN, mx, my);
     ui_state |= STARTPAN2;
     // }                               //20121127
     break;
   }
   if(key == '_' )		// toggle change line width
   {
    change_lw =!change_lw;
    if(change_lw) {
	tkeval("alert_ { enabling change line width} {}");
	Tcl_SetVar(interp,"change_lw","1",TCL_GLOBAL_ONLY);
    }
    else {
	tkeval("alert_ { disabling change line width} {}");
	Tcl_SetVar(interp,"change_lw","0",TCL_GLOBAL_ONLY);
    }
    break;
   }
   if(key == 'b' && state==ControlMask)		// toggle show text in symbol
   {
    sym_txt =!sym_txt;
    if(sym_txt) {
	//tkeval("alert_ { enabling text in symbol} {}");
	Tcl_SetVar(interp,"sym_txt","1",TCL_GLOBAL_ONLY);
        draw();
    }
    else {
	//tkeval("alert_ { disabling text in symbol} {}");
	Tcl_SetVar(interp,"sym_txt","0",TCL_GLOBAL_ONLY);
        draw();
    }
    break;
   }
   if(key == '%' )		// toggle draw grid
   {
    draw_grid =!draw_grid;
    if(draw_grid) {
	//tkeval("alert_ { enabling draw grid} {}");
	Tcl_SetVar(interp,"draw_grid","1",TCL_GLOBAL_ONLY);
        draw();
    }
    else {
	//tkeval("alert_ { disabling draw grid} {}");
	Tcl_SetVar(interp,"draw_grid","0",TCL_GLOBAL_ONLY);
        draw();
    }
    break;
   }
   if(key == 'j'  && state==0 )			// print list of highlight nets
   {
     print_hilight_net(1);
     break;
   }
   if(key == 'j'  && state==ControlMask)	// create ipins from highlight nets
   {
     print_hilight_net(0);
     break;
   }
   if(key == 'j'  && state==Mod1Mask)	// create labels without i prefix from hilight nets
   {
     print_hilight_net(4);
     break;
   }
   if(key == 'J'  && state==(Mod1Mask | ShiftMask) )	// create labels with i prefix from hilight nets 20120913
   {
     print_hilight_net(2);
     break;
   }
   if(key == 'h'  && state==ControlMask )	// 20161102 go to http link
   {
     launcher();
     break;
   }
   if(key == 'h' && state == 0) {
     // horizontally constrained drag 20171023
     if ( horizontal_move ) {
       Tcl_EvalEx(interp,"set horizontal_move 0" , -1, TCL_EVAL_GLOBAL);
     } else {
       Tcl_EvalEx(interp,"set horizontal_move 1" , -1, TCL_EVAL_GLOBAL);
       tkeval("xschem set horizontal_move");
     }
     if(ui_state & STARTWIRE) {
       if(horizontal_move) mousey_snap = my_double_save; // 20171023
       if(vertical_move) mousex_snap = mx_double_save;
       new_wire(RUBBER, mousex_snap, mousey_snap);
     }
     if(ui_state & STARTLINE) {
       if(horizontal_move) mousey_snap = my_double_save; // 20171023
       if(vertical_move) mousex_snap = mx_double_save;
       new_line(RUBBER);
     }
     break;
   }
   if(key=='H' && state==ShiftMask) {		// attach labels to selected instances
    attach_labels_to_inst();
    break;
   }
   if(key == 'v' && state==0) {
     // vertically constrained drag 20171023
     if ( vertical_move ) {
       Tcl_EvalEx(interp,"set vertical_move 0" , -1, TCL_EVAL_GLOBAL);
     } else {
       Tcl_EvalEx(interp,"set vertical_move 1" , -1, TCL_EVAL_GLOBAL);
       tkeval("xschem set vertical_move");
     }
     if(ui_state & STARTWIRE) {
       if(horizontal_move) mousey_snap = my_double_save; // 20171023
       if(vertical_move) mousex_snap = mx_double_save;
       new_wire(RUBBER, mousex_snap, mousey_snap);
     }
     if(ui_state & STARTLINE) {
       if(horizontal_move) mousey_snap = my_double_save; // 20171023
       if(vertical_move) mousex_snap = mx_double_save;
       new_line(RUBBER);
     }
     break;
   }
   if(key == 'j'  && state == (ControlMask | Mod1Mask) )  // print list of highlight net with label expansion
   {
     print_hilight_net(3);
     break;
   }
   if(key == 'J' && state==ShiftMask)		// create cell and symbol from pin list
   {
    static char *ss=NULL; // overflow safe 20161122
    int mx,my;
    int found,i;
    mx = mousex_snap;
    my = mousey_snap;
    rebuild_selected_array();
    if(lastselected && selectedgroup[0].type==ELEMENT) {
      my_snprintf(str, S(str), "xschem gensch %s %s", schematic[currentsch], inst_ptr[selectedgroup[0].n].name);
      if(debug_var>=1) fprintf(errfp,"selected %s\n", inst_ptr[selectedgroup[0].n].name);
    }
    else {
      my_snprintf(str, S(str), "xschem gensch %s {}", schematic[currentsch]); 
    }
    Tcl_EvalEx(interp,str, -1, TCL_EVAL_GLOBAL);
    if(strcmp(Tcl_GetStringResult(interp), "") ) {
        my_strdup(&ss, Tcl_GetStringResult(interp));
        push_undo(); // 20150327
        my_snprintf(str, S(str), "make_symbol %s", Tcl_GetStringResult(interp));
        if(debug_var>=1) fprintf(errfp, "make_symbol(): making symbol: name=%s\n", str);
        tkeval(str);
        found=0;
        for(i=0;i<lastinstdef;i++)
        {
         if(strcmp(ss, instdef[i].name) == 0)
         {
          found=1;break;
         }
        }
        if( lastselected && strcmp(ss, inst_ptr[selectedgroup[0].n].name) ) 
          place_symbol(-1,ss,mx, my, 0, 0, NULL,3);
        else if( lastselected==0 ) 
          place_symbol(-1,ss,mx, my, 0, 0, NULL,3);
        if(found) {
          save_schematic(NULL);
          remove_symbols();
          load_schematic(1,NULL, 0);
          draw();
        }
    }
    break;
   }
   if(key == '$' )		// toggle pixmap  saving
   {
    draw_pixmap =!draw_pixmap;
    if(draw_pixmap) tkeval("alert_ { enabling draw pixmap} {}");
    else tkeval("alert_ { disabling draw pixmap} {}");

    break;
   }
   if(key == '=' )		// toggle fill rectangles
   {
    int x;
    fill++;
    if(fill==3) fill=0;

    if(fill==1) {
     tkeval("alert_ { Stippled pattern fill} {}");
     for(x=0;x<cadlayers;x++) {
       if(fill_type[x]==1) XSetFillStyle(display,gcstipple[x],FillSolid);
       else XSetFillStyle(display,gcstipple[x],FillStippled);
     }
    }
    else if(fill==2) {
     tkeval("alert_ { solid pattern fill} {}");
     for(x=0;x<cadlayers;x++) 
      XSetFillStyle(display,gcstipple[x],FillSolid);
    }
    else  {
     tkeval("alert_ { No pattern fill} {}");
     for(x=0;x<cadlayers;x++)     
      XSetFillStyle(display,gcstipple[x],FillStippled);
    }

    draw();
    break;
   }
   if(key == '+' )		// change line width
   {
    lw_double+=0.1;
    change_linewidth(lw_double,1);
    break;
   }
   if(key == '-' )		// change line width
   {
    lw_double-=0.1;if(lw_double<0.0) lw_double=0.0;
    change_linewidth(lw_double,1);
    break;
   }
   if(key == 'X' && state == ShiftMask) 			// highlight discrepanciens between selected instance pin and net names
   {
     // 20130628

     int i,j,k;
     Instdef *symbol;
     int npin;
     static char *type=NULL;
     static char *labname=NULL;
     static char *lab=NULL;
     static char *netname=NULL;
     int mult;
     Box *rect;

     rebuild_selected_array();
     prepare_netlist_structs();
     for(k=0; k<lastselected; k++) {
       if(selectedgroup[k].type!=ELEMENT) continue;
       j = selectedgroup[k].n ;
       // my_strdup(&type,get_tok_value((inst_ptr[j].ptr+instdef)->prop_ptr,"type",0)); // 20150409
       my_strdup(&type,(inst_ptr[j].ptr+instdef)->type); //20150409
       if( type && (strcmp(type,"label") && strcmp(type,"ipin")&&strcmp(type,"opin")&&strcmp(type,"iopin") )==0) break;
       symbol = instdef + inst_ptr[j].ptr;
       npin = symbol->rects[PINLAYER];
       rect=symbol->boxptr[PINLAYER];
       if(debug_var>=1) fprintf(errfp, "\n");
       for(i=0;i<npin;i++) {
         my_strdup(&labname,get_tok_value(rect[i].prop_ptr,"name",0));
         my_strdup(&lab, expandlabel(labname, &mult));
         my_strdup(&netname, pin_node(j,i,&mult));
         if(debug_var>=1) fprintf(errfp, "i=%d labname=%s explabname = %s  net = %s\n", i, labname, lab, netname);
         if(netname && strcmp(lab, netname)) { 
           if(debug_var>=1) fprintf(errfp, "hilight: %s\n", netname);
           bus_hilight_lookup(netname, hilight_color,0);
           if(incr_hilight) hilight_color++;
         }
       }

     } 
     draw_hilight_net();
     //delete_netlist_structs(); // 20161222 done in prepare_netlist_structs() when needed

     // /20130628
     break;
   }
   if(key== 'W' && state == ShiftMask) {			// 20171022 create wire snapping to closest instance pin
     double x, y;
     int xx, yy;
     if(!(ui_state & STARTWIRE)){
       find_closest_net_or_symbol_pin(mousex, mousey, &x, &y);
       xx = (x+xorigin)*mooz;
       yy = (y+yorigin)*mooz;
       mx_save = xx; my_save = yy; // 20070323
       mx_double_save=rint(( xx*zoom - xorigin)/cadsnap)*cadsnap;
       my_double_save=rint(( yy*zoom - yorigin)/cadsnap)*cadsnap;
       new_wire(PLACE, x, y);
     }
     else {
       find_closest_net_or_symbol_pin(mousex, mousey, &x, &y);
       new_wire(RUBBER, x, y);
       new_wire(PLACE|END, x, y);
       horizontal_move = vertical_move=0; // 20171023
       Tcl_EvalEx(interp,"set vertical_move 0; set horizontal_move 0" , -1, TCL_EVAL_GLOBAL);
     }
   }
   if(key == 'w'&& state==0)	// place wire.
   {
     if(ui_state & STARTWIRE) {
       if(!vertical_move) {
         mx_save = mx;
         mx_double_save=mousex_snap;
       }
       if(!horizontal_move) {
         my_save = my;   // 20070323
         my_double_save=mousey_snap;
       }
       if(horizontal_move) mousey_snap = my_double_save; // 20171023
       if(vertical_move) mousex_snap = mx_double_save;
     } else {
       mx_save = mx; my_save = my;       // 20070323
       mx_double_save=mousex_snap;
       my_double_save=mousey_snap;
     }
     new_wire(PLACE,mousex_snap, mousey_snap); 
    break;
   }
   if(key == XK_Return && ui_state & STARTPOLYGON) { // quick way to finish a polygon placement
    new_polygon(ADD|END);
    break;
   }
   if(key == XK_Escape)			// abort & redraw
   {
    Tcl_EvalEx(interp,"set vertical_move 0; set horizontal_move 0" , -1, TCL_EVAL_GLOBAL);

    horizontal_move = vertical_move = 0; // 20171023
    if(debug_var>=1) fprintf(errfp, "callback(): Escape: ui_state=%ld\n", ui_state);
    if(ui_state & STARTMOVE)
    {
     move_objects(ABORT,0,0,0);
     break;
    }
    if(ui_state & STARTCOPY)
    {
     copy_objects(ABORT);
     break;
    }
    if(ui_state & STARTMERGE) delete();
    ui_state = 0;
    unselect_all(); 
    draw();
    break;
   }
   if(key=='z' && state == 0) 			// zoom box
   {
    if(debug_var>=1) fprintf(errfp, "callback(): zoom_box call\n");
    zoom_box(BEGIN);break;
   }
   if(key=='Z' && state == ShiftMask) 			// zoom in
   {
    view_unzoom(0.0); break;
   }
   if(key=='p' && state == 0)		 		// pan
   {
    pan(BEGIN);break;
   }
   if(key=='w' && !ui_state && state==ControlMask)              // start polygon, 20171115
   {
     if(debug_var>=1) fprintf(errfp, "callback(): start polygon\n");
     mx_save = mx; my_save = my;       // 20070323
     mx_double_save=mousex_snap;
     my_double_save=mousey_snap;
     new_polygon(PLACE);
     break;
   }
   if(key=='p' && state == ControlMask)		 		// pan
   {
    pan(BEGIN);break;
   }
   if(key=='P' && state == ShiftMask) 			// pan, other way to.
   {
    xorigin=-mousex_snap+areaw*zoom/2.0;yorigin=-mousey_snap+areah*zoom/2.0;
    draw();
    break;
   }
   if(key=='5' && state == 0) { // 20110112 display only probes
    toggle_only_probes();
    break;
   }  // /20110112
   if(key<='9' && key >='0' && state==ControlMask) 		// choose layer
   {
    rectcolor = key - '0'+4;
    if(debug_var>=1) fprintf(errfp, "callback(): new color: %d\n",color_index[rectcolor]);
    break;
   }
   if(key==XK_Delete && (ui_state & SELECTION) )	// delete objects
   {
    delete();break;
   }
   if(key==XK_Right) 			// left
   {
    xorigin+=-CADMOVESTEP*zoom;
    draw();
    break;
   }
   if(key==XK_Left) 			// right
   {
    xorigin-=-CADMOVESTEP*zoom;
    draw();
    break;
   }
   if(key==XK_Down) 			// down
   {
    yorigin+=-CADMOVESTEP*zoom;
    draw();
    break;
   }
   if(key==XK_Up) 			// up
   {
    yorigin-=-CADMOVESTEP*zoom;
    draw();
    break;
   }
   if(key=='d' && state == ControlMask)	// exit
   {
     if(modified) {
       tkeval("tk_messageBox -type okcancel -message {UNSAVED data: want to exit?}");
       if(strcmp(Tcl_GetStringResult(interp),"ok")==0) {
         Tcl_Eval(interp, "exit");
       }
     } 
     else {
       Tcl_Eval(interp, "exit");
     }
     break;
   }
   if(key=='t' && state == 0)                        // place text
   {
    place_text(1, mousex_snap, mousey_snap); // 1 = draw text 24122002
    break;
   }
   if(key=='r' && !ui_state && state==0)              // start rect
   {
    if(debug_var>=1) fprintf(errfp, "callback(): start rect\n");
    mx_save = mx; my_save = my;	// 20070323
    mx_double_save=mousex_snap;
    my_double_save=mousey_snap;
    new_rect(PLACE); break;
   }
   if(key=='V' && state == ShiftMask)				// toggle spice/vhdl netlist 
   {
    netlist_type++; if(netlist_type==4) netlist_type=1;
    if(netlist_type == CAD_VHDL_NETLIST)
    {
     tkeval("alert_ { netlist type set to VHDL} {}");
     Tcl_SetVar(interp,"netlist_type","vhdl",TCL_GLOBAL_ONLY);
    }
    else if(netlist_type == CAD_SPICE_NETLIST)
    {
     tkeval("alert_ { netlist type set to SPICE} {}");
     Tcl_SetVar(interp,"netlist_type","spice",TCL_GLOBAL_ONLY);
    }
    else if(netlist_type == CAD_VERILOG_NETLIST)
    {
     tkeval("alert_ { netlist type set to VERILOG} {}");
     Tcl_SetVar(interp,"netlist_type","verilog",TCL_GLOBAL_ONLY);
    }
    break;
   }

   if(key=='S' && (state == ShiftMask) )	// save 20121201
   {
     if(semaphore==2) break;
     if(!strcmp(schematic[currentsch],"")) { // 20170622 check if unnamed schematic, use saveas in this case...
       saveas();
     } else {
       save(1);
     }
     break;
   }
   if(key=='S' && state == (ShiftMask | ControlMask)) // save as symbol 20121201
   {
     if(semaphore==2) break;
     current_type=SYMBOL;
     saveas();
     break;
   }
   if(key=='s' && state == 0)		// save as
   {
     if(semaphore==2) break;
     saveas();
     break;
   }
   if(key=='e' && state == 0)  		// descend to schematic
   {
    if(semaphore==2) break;
    descend();break;
   }
   if(key=='e' && state == Mod1Mask)  		// edit schematic in new window
   {
    Tcl_Eval(interp,"xschem edit_in_new_window" );
    break;
   }
   if(key=='i' && state == Mod1Mask)  		// edit symbol in new window
   {
    Tcl_Eval(interp,"xschem symbol_in_new_window" );
    break;
   }
   if( (key=='e' && state == ControlMask) || (key==XK_BackSpace))  // back
   {
    if(semaphore==2) break;
    go_back(1);break;
   }

   if(key=='a' && state == 0) 	// make symbol
   {
    tkeval("tk_messageBox -type okcancel -message {do you want to make symbol view ?}");
    if(strcmp(Tcl_GetStringResult(interp),"ok")==0) 
      if(current_type==SCHEMATIC)
      {
       save_schematic(NULL);
       make_symbol();
      }
    break;
   }
   if(key=='a' && state == ControlMask)         // select all
   {
    select_all();
    break;
   }
   if(key=='y' && state == 0)           		// toggle stretching
   {
    enable_stretch=!enable_stretch;
    
    if(enable_stretch) {
	tkeval("alert_ { enabling stretch mode } {}");
	Tcl_SetVar(interp,"enable_stretch","1",TCL_GLOBAL_ONLY);
    }
    else {
	tkeval("alert_ { disabling stretch mode } {}");
	Tcl_SetVar(interp,"enable_stretch","0",TCL_GLOBAL_ONLY);
    }
    break;
   }
   if(key=='x' && state == ControlMask) // cut into clipboard
   {
    if(semaphore==2) break;
    rebuild_selected_array();
    if(lastselected) {  // 20071203 check if something selected
      save_selection(2);
      delete();
    } 
    break;
   }
   if(key=='c' && state == ControlMask)   // save clipboard
   {
    rebuild_selected_array();
    if(lastselected) {  // 20071203 check if something selected
      save_selection(2);
    }
    break;
   }
   if(key=='C' && state == ShiftMask)   // Toggle light/dark colorscheme 20171113
   {
     dark_colorscheme=!dark_colorscheme;
     build_colors();
     draw();
     break;
   }
   if(key=='v' && state == ControlMask)   // load clipboard
   {
    if(semaphore==2) break;
    merge_file(2,".sch");
    break;
   }
   if(key=='q' && state == ControlMask) // view prop
   {
    edit_property(2);break;
   }
   if(key=='q' && state==0)           		// edit prop
   {
    if(semaphore==2) break;
    edit_property(0);

    break;
   }
   if(key=='q' && state==Mod1Mask)           		// edit .sch file (DANGER!!)
   {
    rebuild_selected_array();
    if(lastselected==0 ) {
      if(current_type==SCHEMATIC) {
        save_schematic(NULL); // sync data with disk file before editing file
        my_snprintf(str, S(str), "edit_file %s/%s.sch",
              Tcl_GetVar(interp, "XSCHEM_DESIGN_DIR", TCL_GLOBAL_ONLY),
              schematic[currentsch]);
      }
      else {
        save_symbol(NULL); // sync data with disk file before editing file
        my_snprintf(str, S(str), "edit_file %s/%s.sym",
              Tcl_GetVar(interp, "XSCHEM_DESIGN_DIR", TCL_GLOBAL_ONLY),
              schematic[currentsch]);
      }
      tkeval(str);
    }
    else if(selectedgroup[0].type==ELEMENT) {
      my_snprintf(str, S(str), "edit_file %s/%s.sch",
            Tcl_GetVar(interp, "XSCHEM_DESIGN_DIR", TCL_GLOBAL_ONLY),
            inst_ptr[selectedgroup[0].n].name);
      tkeval(str);
 
    }
    break;
   }
   if(key=='Q' && state == ShiftMask)           		// edit prop with vim
   {
    if(semaphore==2) break;
    edit_property(1);break;
   }
   if(key=='i' && state==0)           		// descend to  symbol
   {
    if(semaphore==2) break;
    edit_symbol();break;
   }
   if(key==XK_Insert)			// insert sym
   {
    place_symbol(-1,NULL,mousex_snap, mousey_snap, 0, 0, NULL,3);break;
   }
   if(key=='s' && state & Mod1Mask)			// reload
   {
    if(semaphore==2) break;
     tkeval("tk_messageBox -type okcancel -message {Are you sure you want to reload from disk?}");
     if(strcmp(Tcl_GetStringResult(interp),"ok")==0) {
        remove_symbols();
        load_schematic(1,NULL, 1);
        //zoom_full(1);
        draw();
     }
     break;
   }
   if(key=='L' && state == ShiftMask)   // load
   {
    if(semaphore==2) break;
    ask_new_file();
    break;
   }
   if(key=='s' && state == ControlMask)   // change element order
   {
    change_elem_order();
    break;
   }
   if(key=='k' && state==ControlMask)				// hilight net
   {
    //// 20160413
    unhilight_net();
    undraw_hilight_net();
    // draw();

    break;
   }
   if(key=='k' && state==0)				// hilight net
   {
    hilight_net();
    draw_hilight_net();
    break;
   }
   if(key=='K' && state == ShiftMask)				// delete hilighted nets
   {
    delete_hilight_net();
    // 20160413
    undraw_hilight_net();
    // draw();
    break;
   }
   if(key=='g' && state==0)                         // half snap factor
   {
    set_snap(cadsnap / 2.0);
    break;
   }
   if(key=='g' && state==ControlMask)              // set snap factor 20161212
   {
    my_snprintf(str, S(str),
     "input_number \"Enter snap value (default: %g current: %g)\" \"xschem set cadsnap_noalert\"", 
     cadsnap, CADSNAP);
    Tcl_EvalEx(interp, str, -1, TCL_EVAL_GLOBAL);
    break;
   }
   if(key=='G' && state==ShiftMask)                                    // double snap factor
   {
    set_snap(cadsnap * 2.0);
    break;
   }
   if(key=='*' && state==(Mod1Mask|ShiftMask) )		// svg print , 20121108
   {
    svg_draw();
    break;
   }
   if(key=='*' && state==ShiftMask )			// postscript print
   {
    ps_draw();
    break;
   }
   if(key=='*' && state==(ControlMask|ShiftMask) )	// xpm print
   {
    print_image();
    break;
   }
   if(key=='u' && state==Mod1Mask)			// align to grid
   {
    push_undo(); // 20150327
    round_schematic_to_grid(cadsnap);
    modified=1;
    draw();
    break;
   }
   if(key=='u' && state==ControlMask)			// testmode
   {
    break;
   }
   if(key=='u' && state==0)				// undo
   {
    pop_undo(0);
    draw();
    break;
   }
   if(key=='U' && state==ShiftMask)			// redo
   {
    pop_undo(1);
    draw();
    break;
   }
   if(key=='&')				// check wire connectivity
   {
    push_undo(); // 20150327
    collapse_wires();
    draw();
    break;
   }
   if(key=='l' && state == ControlMask) { // create schematic from selected symbol 20171004
     
     create_sch_from_sym();
     break;
   }
   if(key=='l' && state == 0) // start line
   {
    if(ui_state & STARTLINE) {
      if(!vertical_move) {
        mx_save = mx; 
        mx_double_save=mousex_snap;
      }
      if(!horizontal_move) {
        my_save = my;	// 20070323
        my_double_save=mousey_snap;
      }
      if(horizontal_move) mousey_snap = my_double_save; // 20171023
      if(vertical_move) mousex_snap = mx_double_save;
    } else {
      mx_save = mx; my_save = my;	// 20070323
      mx_double_save=mousex_snap;
      my_double_save=mousey_snap;
    }
    new_line(PLACE); break;
   }
   if(key=='F' && state==ShiftMask)				// Flip
   {
    if(ui_state & STARTMOVE) move_objects(FLIP,0,0,0);
    if(ui_state & STARTCOPY) copy_objects(FLIP);
    break;
   }
   if(key=='f'&& state==Mod1Mask)			// Fullscreen
   {
    if(debug_var>=1) fprintf(errfp, "callback(): toggle fullscreen\n");
    toggle_fullscreen();
    break;
   }
   if(key=='R' && state==ShiftMask)				// Rotate
   {
    if(ui_state & STARTMOVE) move_objects(ROTATE,0,0,0);
    if(ui_state & STARTCOPY) copy_objects(ROTATE);
    break;
   }
   if(key=='m' && state==0 && !(ui_state & (STARTMOVE | STARTCOPY)))// move selected obj.
   {
    mx_save = mx; my_save = my;	// 20070323
    mx_double_save=mousex_snap;
    my_double_save=mousey_snap;
    move_objects(BEGIN,0,0,0);
    break;
   }
   
   if(key=='c' && state==0 &&		// copy selected obj. 
     !(ui_state & (STARTMOVE | STARTCOPY)))
   {
    mx_save = mx; my_save = my;	// 20070323
    mx_double_save=mousex_snap;
    my_double_save=mousey_snap;
    copy_objects(BEGIN);
    break;
   }
   if(key=='N' && state==ShiftMask)              // hierarchical netlist
   {
    if(set_netlist_dir(0)) {
      if(debug_var>=1) fprintf(errfp, "callback(): -------------\n");
      if(netlist_type == CAD_SPICE_NETLIST)
        global_spice_netlist(1);
      else if(netlist_type == CAD_VHDL_NETLIST)
        global_vhdl_netlist(1);
      else if(netlist_type == CAD_VERILOG_NETLIST)
        global_verilog_netlist(1);
      if(debug_var>=1) fprintf(errfp, "callback(): -------------\n");
    }
    break;
   }
   if(key=='n' && state==0)              // netlist
   {
    if( set_netlist_dir(0) ) {
      if(debug_var>=1) fprintf(errfp, "callback(): -------------\n");
      if(netlist_type == CAD_SPICE_NETLIST)
        global_spice_netlist(0);
      else if(netlist_type == CAD_VHDL_NETLIST)
        global_vhdl_netlist(0);
      else if(netlist_type == CAD_VERILOG_NETLIST)
        global_verilog_netlist(0);
      if(debug_var>=1) fprintf(errfp, "callback(): -------------\n");
    }
    break;
   }
   if(key=='A' && state==ShiftMask)				// toggle show netlist
   {
    netlist_show = !netlist_show;
    if(netlist_show) {
	tkeval("alert_ { enabling show netlist window} {}");
	Tcl_SetVar(interp,"netlist_show","1",TCL_GLOBAL_ONLY);
    }
    else {
	tkeval("alert_ { disabling show netlist window } {}");
	Tcl_SetVar(interp,"netlist_show","0",TCL_GLOBAL_ONLY);
    }
    break;
   }
   if(key=='>') { // 20151117
     if(draw_single_layer< cadlayers-1) draw_single_layer++; 
     draw();
   }
   if(key=='<') { // 20151117
     if(draw_single_layer>=0 ) draw_single_layer--; 
     draw();
   }
   if(key==':')				// toggle flat netlist (only spice) 
   {
    flat_netlist = !flat_netlist;
    if(flat_netlist) {
	tkeval("alert_ { enabling flat netlist} {}");
	Tcl_SetVar(interp,"flat_netlist","1",TCL_GLOBAL_ONLY);
    }
    else {
	tkeval("alert_ { set hierarchical netlist } {}");
	Tcl_SetVar(interp,"flat_netlist","0",TCL_GLOBAL_ONLY);
    }
    break;
   }
   if(key=='b' && state==0) 	                // merge schematic
   {
    if(semaphore==2) break;
    merge_file(0, ""); // 2nd parameter not used any more for merge 25122002
    break;
   }
   if(key=='B' && state==ShiftMask) 	                // delete files
   {

    if(semaphore==2) break; // 20160423
    rebuild_selected_array();
    if(lastselected && selectedgroup[0].type==ELEMENT) {
      my_snprintf(str, S(str), "delete_files %s/%s",  
           Tcl_GetVar(interp, "XSCHEM_DESIGN_DIR", TCL_GLOBAL_ONLY), 
           inst_ptr[selectedgroup[0].n].name);
    } else {
      my_snprintf(str, S(str), "delete_files %s/%s",  
           Tcl_GetVar(interp, "XSCHEM_DESIGN_DIR", TCL_GLOBAL_ONLY), 
           schematic[currentsch]);
    }

    Tcl_EvalEx(interp, str, -1, TCL_EVAL_GLOBAL);
    break;
   }
   if(key=='x' && state == 0 ) 	                // new cad session
   {
    char * tmp;
    tmp = (char *) Tcl_GetVar(interp, "XSCHEM_START_WINDOW", TCL_GLOBAL_ONLY); // 20121110
    if(tmp && tmp[0]) new_window(tmp,0); // 20090708
    else {
      if(debug_var>=1) { // 20121110
        fprintf(errfp, "callback(): can not start new xschem session: XSCHEM_START_WINDOW unset");
      }
    }
    break;
   }
   if(key==';' && state & Mod1Mask ) 	// debug:  for performance testing
   {
    draw_stuff(); 
    draw();
    break;
   }
   if(key=='f' && state == ControlMask)         // search
   {
    Tcl_EvalEx(interp,"property_search", -1, TCL_EVAL_GLOBAL );
   }
   if(key=='f' && state == 0 ) 	                // full zoom
   {
    zoom_full(1);
    break;
   }
   if(key=='o' || (key=='z' && state==ControlMask))  	                // zoom out
   {
     view_zoom(0.0); 
     break;
   }
   if(key=='!') 	                // empty slot
   {
     if(has_x) XCopyArea(display, cad_icon_pixmap, window, gc[WIRELAYER], 0, 0, 60, 41,  100, 100);
      //XIconifyWindow(display, topwindow, DefaultScreen(display) );
     break;
   }
  break;
  case ButtonPress:			// end operation
   if(debug_var>=1) fprintf(errfp, "callback(): ButtonPress  ui_state=%ld state=%d\n",ui_state,state);
   if(button==Button4 && state == 0 ) view_zoom(CADZOOMSTEP);
   else if(button==Button5 && state == 0 ) view_unzoom(CADZOOMSTEP);
   // 20111114
   else if(button==Button5 && (state & ShiftMask) && !(state & Button2Mask)) {
    xorigin+=-CADMOVESTEP*zoom/2.;
    draw();
   }
   else if(button==Button4 && (state & ShiftMask) && !(state & Button2Mask)) {
    xorigin-=-CADMOVESTEP*zoom/2.;
    draw();
   }
   else if(button==Button5 && (state & ControlMask) && !(state & Button2Mask)) {
    yorigin+=-CADMOVESTEP*zoom/2.;
    draw();
   }
   else if(button==Button4 && (state & ControlMask) && !(state & Button2Mask)) {
    yorigin-=-CADMOVESTEP*zoom/2.;
    draw();
   }
   else if(button==Button3 && state==0)
   {
     if(semaphore<2) { // 20160425
       rebuild_selected_array();
       if(lastselected==0) ui_state &=~SELECTION;
     }
     pan2(BEGIN, mx, my);
     ui_state |= STARTPAN2;
   }
   else if(semaphore==2) {
     if(button==Button1 && state==0) {
       Tcl_EvalEx(interp, "set editprop_semaphore 2", -1, TCL_EVAL_GLOBAL); // 20160423
     }
     break;
   }
   else if(button==Button1)
   {
     if(!(ui_state & STARTPOLYGON)) {
       horizontal_move = vertical_move=0; // 20171023
       Tcl_EvalEx(interp,"set vertical_move 0; set horizontal_move 0" , -1, TCL_EVAL_GLOBAL);
     }
     if(ui_state & MENUSTARTTEXT) {
       place_text(1, mousex_snap, mousey_snap); // 20161201
       ui_state &=~MENUSTARTTEXT;
       break; // 20161201
     }
     if(ui_state & MENUSTARTWIRE) {
       mx_save = mx; my_save = my;	// 20070323
       mx_double_save=mousex_snap;
       my_double_save=mousey_snap;
       new_wire(PLACE, mousex_snap, mousey_snap); 
       ui_state &=~MENUSTARTWIRE;
       break;
     }
     if(ui_state & MENUSTARTSNAPWIRE) { // 20171022
       double x, y;
       int xx, yy;
       
       find_closest_net_or_symbol_pin(mousex, mousey, &x, &y);
       xx = (x+xorigin)*mooz;
       yy = (y+yorigin)*mooz;
       mx_save = xx; my_save = yy; // 20070323
       mx_double_save=rint(( xx*zoom - xorigin)/cadsnap)*cadsnap;
       my_double_save=rint(( yy*zoom - yorigin)/cadsnap)*cadsnap;
       new_wire(PLACE, x, y);
       ui_state &=~MENUSTARTSNAPWIRE;
       break;
     }
     if(ui_state & MENUSTARTLINE) {
       mx_save = mx; my_save = my;	// 20070323
       mx_double_save=mousex_snap;
       my_double_save=mousey_snap;
       new_line(PLACE);
       ui_state &=~MENUSTARTLINE;
       break;
     }
     if(ui_state & MENUSTARTRECT) {
       mx_save = mx; my_save = my;	// 20070323
       mx_double_save=mousex_snap;
       my_double_save=mousey_snap;
       new_rect(PLACE);
       ui_state &=~MENUSTARTRECT;
       break;
     }
     if(ui_state & MENUSTARTPOLYGON) {
       mx_save = mx; my_save = my;      // 20070323
       mx_double_save=mousex_snap;
       my_double_save=mousey_snap;
       new_polygon(PLACE);
       ui_state &=~MENUSTARTPOLYGON;
       break;
     }
     if(ui_state & MENUSTARTZOOM) {
       zoom_box(BEGIN);
       ui_state &=~MENUSTARTZOOM;
       break;
     }
     if(ui_state & STARTPAN) {
       pan(END);
       break;
     }
     if(ui_state & STARTZOOM) {
       zoom_box(END);
       break;
     }
     if(ui_state & STARTWIRE) {
       new_wire(PLACE|END, mousex_snap, mousey_snap);
       break;
     }
     if(ui_state & STARTLINE) {
       new_line(PLACE|END);
       break;
     }
     if(ui_state & STARTRECT) {
       new_rect(PLACE|END);
       break;
     }
     if(ui_state & STARTPOLYGON) { // 20171115
       if(horizontal_move) mousey_snap = my_double_save;
       if(vertical_move) mousex_snap = mx_double_save;
       if( mousex_snap == mx_save && mousey_snap == my_save) {
         new_polygon(PLACE);
       } else {
         new_polygon(ADD);
       }
       mx_double_save=mousex_snap;
       my_double_save=mousey_snap;
       horizontal_move = vertical_move=0; // 20171023
       Tcl_EvalEx(interp,"set vertical_move 0; set horizontal_move 0" , -1, TCL_EVAL_GLOBAL);
       break;
     }
     if(ui_state & STARTMOVE) {
       move_objects(END,0,0,0);
       break;
     }
     if(ui_state & STARTCOPY) {
       copy_objects(END);
       break;
     }
     if( !(ui_state & STARTSELECT) ) {
       mx_save = mx; my_save = my;
       mx_double_save=mousex_snap; // 20070322
       my_double_save=mousey_snap; // 20070322
       if( !(state & ShiftMask) ) {
         unselect_all();
       }
       sel = select_object(mousex,mousey,SELECTED);
 
       if(sel && state ==Mod1Mask) { // 20170416
         launcher();
       }
       if( !(state & ShiftMask) )  {
         if(auto_hilight && hilight_nets && sel == 0 ) { // 20160413 20160503
           delete_hilight_net();
           undraw_hilight_net();
         }
       }
       if(auto_hilight) { // 20160413
         hilight_net();
         if(lastselected) draw_hilight_net();
       }
       break;
     }
   } //button==Button1
   else if(button==Button2 && !(state & ShiftMask))
   {
    select_object(mousex, mousey, 0);
   }
   break;
  case ButtonRelease:
   if(debug_var>=1) fprintf(errfp, "callback(): ButtonRelease  ui_state=%ld state=%d\n",ui_state,state);
   if(ui_state & STARTPAN2) {  // 20121123
     ui_state &=~STARTPAN2;
   }
   if(semaphore==2) break; // 20160423
   if(ui_state & STARTSELECT) {
     if(state & ControlMask) {
       enable_stretch=1;
       select_rect(END,-1);
       enable_stretch=0;
       break;
     } else {
       // 20150927 filter out button4 and button5 events
       if(!(state&(Button5Mask|Button4Mask) ) ) select_rect(END,-1);
     }
   }
   break;
  case -3:  // double click  : edit prop
   if(semaphore==2) break;
   if(debug_var>=1) fprintf(errfp, "callback(): DoubleClick  ui_state=%ld state=%d\n",ui_state,state);
   if(button==Button1 && !(state & ShiftMask)) {
     unselect_all();
     select_object(mousex,mousey,SELECTED);
     edit_property(0);
   } else if(button==Button3 && state==0) {
     //unselect_all();
     //select_object(mousex,mousey,SELECTED);
      rebuild_selected_array();
      if(lastselected ==1 &&  selectedgroup[0].type==ELEMENT) descend();
      else  go_back(1);
   }
   break;
    
  default:
   if(debug_var>=1) fprintf(errfp, "callback(): Event:%d\n",event);
   break;
 } 
 
 semaphore--;
 return 0;
}

