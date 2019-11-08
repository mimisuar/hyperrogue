// Hyperbolic Rogue -- Arnold's cat map
// Copyright (C) 2011-2019 Zeno Rogue, see 'hyper.cpp' for details

/** \file asonov.cpp 
 *  \brief Arnold's cat map
 */


#include "hyper.h"

//#include <cstdio>
//#include <cmath>

namespace hr {

EX namespace asonov {

EX bool in() { return geometry == gArnoldCat; }

EX hyperpoint tx, ty, tz;
EX transmatrix straighten;

EX int period_xy = 8;
EX int period_z = 8;

struct coord: public array<int,3> {
  coord() {}
  coord(int x, int y, int z) : array<int,3>(make_array(zgmod(x, period_xy), zgmod(y, period_xy), zgmod(z, period_z))) {}
  coord shift(int x, int y, int z=0) { return coord(self[0]+x, self[1]+y, self[2]+z); }
  coord up() { return coord(self[0]*2-self[1], self[1]-self[0], self[2]+1); }
  coord down() { return coord(self[0]+self[1], self[0]+self[1]*2, self[2]-1); }
  
  coord addmove(int d) {
    switch(d) {
      case 0: return up().shift(0, 0);
      case 1: return up().shift(1, -1);
      case 2: return up().shift(-1, 0);
      case 3: return up().shift(0, -1);
      case 4: return shift(1, 0);
      case 5: return shift(0, 1);
      case 6: return down().shift(0, 0);
      case 7: return down().shift(0, 1);
      case 8: return down().shift(1, 1);
      case 9: return down().shift(1, 2);
      case 10: return shift(-1, 0);
      case 11: return shift(0, -1);
      default: throw "error";
      }
    }
  };

EX void prepare() {
  using namespace hr;
  transmatrix A = Id;
  A[0][0] = 1;
  A[0][1] = 1;
  A[1][0] = 1;
  A[1][1] = 2;
  static bool first = true;
  if(first) println(hlog, "amain");
  
  // if(true) {
  double det = hr::det(A);
  if(det != 1) { printf("wrong det\n"); return; }
  
  // (a00-x)(a11-x) - a01*a10 = 0
  
  // x^2 - (a00+a11) x + 1 = 0
  
  double b = (A[0][0] + A[1][1]) / 2;

  // x^2 - 2b x + b^2 = b^2-1
  
  // if(b*b <= 1) { printf("imaginary eigenvalues\n"); return 0; }
  
  // x = b + sqrt(b^2-1)
  
  hyperpoint lambda;
  lambda[0] = b + sqrt(b*b-1);
  lambda[1] = b - sqrt(b*b-1);
  
  if(first) println(hlog, "lambda = ", lambda);
  
  transmatrix eigen = Id;
  
  for(int i: {0,1}) {    
    eigen[0][i] = 1;
    eigen[1][i] = (lambda[i] - A[0][0]) / A[0][1];
    }
  
  if(first) println(hlog, "eigen = ", eigen);
  if(first) println(hlog, "A*eigen = ", A*eigen);
  
  transmatrix ieigen = inverse(eigen);
  if(first) println(hlog, "ieigen = ", ieigen);
  if(first) println(hlog, "ieigen*A = ", ieigen * A);
  
  tx = point3(ieigen[0][0], ieigen[1][0], 0);
  ty = point3(ieigen[0][1], ieigen[1][1], 0);
  tz = -point3(0, 0, log(lambda[0]));
  
  println(hlog, "tx = ", tx);
  println(hlog, "ty = ", ty);
  println(hlog, "tz = ", tz);
  
  for(int a=0; a<12; a++) {
    int b = (a+6) % 12;
    coord test(1, 10, 100);
    auto test1 = test.addmove(a).addmove(b);
    println(hlog, test == test1 ? "OK" : "BAD", " : ", test, " vs ", test1, " ## ", test.addmove(a));
    }
  
  straighten = inverse(build_matrix(asonov::tx/2, asonov::ty/2, asonov::tz/2, C0));
  }

EX transmatrix adjmatrix(int i) {
  dynamicval<int> pxy(period_xy, 64);
  dynamicval<int> pz(period_z, 64);
  coord c = coord(0,0,0).addmove(i);
  if(c[0] > period_xy/2) c[0] -= period_xy;
  if(c[1] > period_xy/2) c[1] -= period_xy;
  if(c[2] > period_z/2) c[2] -= period_z;
  transmatrix T = eupush(tz * c[2]) * eupush(tx * c[0] + ty * c[1]);;
  if(i < 4) return T * eupush(ty/2);
  if(i >= 6 && i < 10) return eupush(-ty/2) * T;
  return T;
  }
  
struct hrmap_asonov : hrmap {
  unordered_map<coord, heptagon*> at;
    unordered_map<heptagon*, coord> coords;
    
  heptagon *getOrigin() override { return get_at(coord(0,0,0)); }
    
  hrmap_asonov() { prepare(); }
  
  ~hrmap_asonov() {
    for(auto& p: at) clear_heptagon(p.second);
    }

  heptagon *get_at(coord c) {
    auto& h = at[c];
    if(h) return h;
    h = tailored_alloc<heptagon> (S7);
    h->c7 = newCell(S7, h);
    coords[h] = c;
    h->dm4 = 0;
    h->distance = c[2];
    h->zebraval = c[0];
    h->emeraldval = c[1];
    h->cdata = NULL;
    h->alt = NULL;
    return h;      
    }
  
  heptagon *create_step(heptagon *parent, int d) override {
    auto p = coords[parent];
    auto q = p.addmove(d);
    auto child = get_at(q);
    parent->c.connect(d, child, (d + 6) % 12, false);
    return child;
    }
  
  virtual transmatrix relative_matrix(heptagon *h2, heptagon *h1) override { 
    for(int a=0; a<S7; a++) if(h2 == h1->move(a)) return adjmatrix(a);
    return Id;
    }

  void draw() override {
    dq::visited_by_matrix.clear();

    dq::enqueue_by_matrix(viewctr.at, cview());
    
    while(!dq::drawqueue.empty()) {
      auto& p = dq::drawqueue.front();
      heptagon *h = get<0>(p);
      transmatrix V = get<1>(p);
      dq::drawqueue.pop();
      
      cell *c = h->c7;
      if(!do_draw(c, V)) continue;
      drawcell(c, V);
      if(wallopt && isWall3(c) && isize(dq::drawqueue) > 1000) continue;

      for(int i=0; i<S7; i++)
        dq::enqueue_by_matrix(h->cmove(i), V * adjmatrix(i));
      }
    }
  };

EX hrmap *new_map() { return new hrmap_asonov; }

EX int period_xy_edit, period_z_edit;

EX void set_flags() {
  auto& flag = ginf[gArnoldCat].flags;
  set_flag(flag, qANYQ, period_xy || period_z);
  set_flag(flag, qBOUNDED, period_xy && period_z);
  set_flag(flag, qSMALL, period_xy && period_z && (period_xy * period_xy * period_z <= 4096));
  }

EX void prepare_config() {
  period_xy_edit = period_xy;
  period_z_edit = period_z;
  }
  
EX void show_config() {
  cmode = sm::SIDE | sm::MAYDARK;
  gamescreen(1);  
  dialog::init(XLAT("Solv quotient spaces"));

  dialog::addSelItem(XLAT("%1 period", "X/Y"), its(period_xy_edit), 'x');
  dialog::add_action([=] { 
    dialog::editNumber(period_xy_edit, 0, 64, 1, 0, XLAT("%1 period", "X/Y"), 
      XLAT("Note: the value 0 functions effectively as the size of int (2^32).")
      ); 
    dialog::bound_low(0);
    });      

  dialog::addSelItem(XLAT("%1 period", "Z"), its(period_z_edit), 'z');
  dialog::add_action([=] { 
    dialog::editNumber(period_z_edit, 0, 64, 1, 0, XLAT("%1 period", "Z"), 
      XLAT("Set to 0 to make it non-periodic.")
      ); 
    dialog::bound_low(0);
    });      

  dialog::addBreak(50);

  dialog::addItem(XLAT("activate"), 'a');
  dialog::add_action([] {
    stop_game();
    period_xy = period_xy_edit;
    period_z = period_z_edit;
    set_flags();
    geometry = gArnoldCat;
    start_game();
    });
    
  dialog::addBreak(50);
  dialog::addBack();
  dialog::display();
  }

}
}
