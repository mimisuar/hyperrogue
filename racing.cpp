// Hyperbolic Rogue - Racing
// Copyright (C) 2011-2019 Zeno Rogue, see 'hyper.cpp' for details

/** \file racing.cpp
 *  \brief racing mode
 */

#include "hyper.h"
namespace hr {

/** \brief Racing mode */
EX namespace racing {

#if CAP_RACING
void set_race_configurer();

EX bool guiding = false;

EX bool on;
EX bool player_relative = false;
EX bool standard_centering = false;
EX bool track_ready;

bool official_race = false;

int TWIDTH;

EX ld race_advance = 0;

static const int LENGTH = 250;
static const int DROP = 1;

EX int ghosts_to_show = 5;
EX int ghosts_to_save = 10;

struct race_cellinfo {
  cell *c;
  int from_track;
  int completion;
  int from_start, from_goal;
  };  

vector<race_cellinfo> rti;
EX vector<cell*> track;
map<cell*, int> rti_id;

EX int trophy[MAXPLAYER];

EX string track_code = "OFFICIAL";

transmatrix straight;

int race_try;

EX void apply_seed() {
  int s = race_try;
  for(char c: track_code) s = 713 * s + c;
  shrand(s);
  }

EX int race_start_tick, race_finish_tick[MAXPLAYER];

typedef unsigned char uchar;

uchar frac_to_uchar(ld x) { return uchar(x * 256); }
uchar angle_to_uchar(ld x) { return frac_to_uchar(x / 2 / M_PI); }

ld uchar_to_frac(uchar x) { return x / 256.; }
transmatrix spin_uchar(uchar x) { return spin(uchar_to_frac(x) * 2 * M_PI); }

static const ld distance_multiplier = 4;

struct ghostmoment {
  int step, where_id;
  uchar alpha, distance, beta, footphase;
  };

struct ghost {
  charstyle cs;
  int result;
  int checksum;
  long long timestamp;
  vector<ghostmoment> history;
  };

typedef map<eLand, vector<ghost>> raceset;
map<pair<string, modecode_t>, raceset> race_ghosts;

map<pair<string, modecode_t>, raceset> official_race_ghosts;

raceset& ghostset() { return race_ghosts[make_pair(track_code, modecode())]; }
raceset& oghostset() { return official_race_ghosts[make_pair(track_code, modecode())]; }

int get_score_in_land(eLand l) {
  auto& gh = ghostset();
  if(!gh.count(l)) return 0;
  auto& v = gh[l];
  if(!isize(v)) return 0;
  return v[0].result;
  }

array<vector<ghostmoment>, MAXPLAYER> current_history;

string ghost_prefix = "default";

#if CAP_FILES
string ghost_filename(string seed, modecode_t mcode) {
  if(ghost_prefix == "default") {
    #ifdef FHS
    if(getenv("HOME")) {
      string s = getenv("HOME");
      mkdir((s + "/.hyperrogue").c_str(), 0755);
      mkdir((s + "/.hyperrogue/racing").c_str(), 0755);
      ghost_prefix = s + "/.hyperrogue/racing/";
      }
    #else
    #if WINDOWS
    mkdir("racing");
    #else
    mkdir("racing", 0755);
    #endif
    ghost_prefix = "racing/";
    #endif
    }
  return ghost_prefix + seed + "-" + itsh(mcode) + ".data";
  }

void hread(hstream& hs, ghostmoment& m) {
  hread(hs, m.step, m.where_id, m.alpha, m.distance, m.beta, m.footphase);
  }

void hwrite(hstream& hs, const ghostmoment& m) {
  hwrite(hs, m.step, m.where_id, m.alpha, m.distance, m.beta, m.footphase);
  }

void hread(hstream& hs, ghost& gh) {
  hread(hs, gh.cs, gh.result, gh.timestamp, gh.checksum, gh.history);
  }

void hwrite(hstream& hs, const ghost& gh) {
  hwrite(hs, gh.cs, gh.result, gh.timestamp, gh.checksum, gh.history);
  }

bool read_ghosts(string seed, modecode_t mcode) {

  if(seed == "OFFICIAL" && mcode == 2) {
    fhstream f("officials.data", "rb");
    if(f.f) {
      hread(f, f.vernum);
      hread(f, oghostset());
      }
    }
  
  string fname = ghost_filename(seed, mcode);
  println(hlog, "trying to read ghosts from: ", fname);
  fhstream f(fname, "rb");
  if(!f.f) return false;
  hread(f, f.vernum);
  if(f.vernum <= 0xA600) return true; // scores removed due to the possibility of cheating
  hread(f, ghostset());
  return true;
  }

void write_ghosts(string seed, int mcode) {
  fhstream f;
  f.f = fopen(ghost_filename(seed, mcode).c_str(), "wb");
  if(!f.f) throw hstream_exception(); // ("failed to write the ghost file");
  hwrite(f, f.vernum);
  hwrite(f, ghostset());
  }
#endif

shiftmatrix get_ghostmoment_matrix(ghostmoment& p) {
  cell *w = rti[p.where_id].c;
  transmatrix T = spin_uchar(p.alpha) * xpush(uchar_to_frac(p.distance) * distance_multiplier) * spin_uchar(p.beta);
  return gmatrix[w] * T;
  } 

void fix_cave(cell *c) {
  int v = 0;
  // if(c->wall == waCavewall) v++;
  // if(c->wall == waCavefloor) v--;
  forCellEx(c2, c) {
    if(c2->wall == waCavewall) v++;
    if(c2->wall == waCavefloor) v--;
    }
  else v--;
  if(v>0 && c->wall == waCavefloor) c->wall = waCavewall;
  if(v<0 && c->wall == waCavewall) c->wall = waCavefloor;
  }

bool keep_to_crossroads() {
  return specialland == laCrossroads && !(nonisotropic || hybri);
  }

bool bad(cell *c2, cell *c) {
  if(c2->land == laCaves) {
    forCellEx(c3, c2) fix_cave(c3);
    fix_cave(c2);
    }
  if(!passable(c2, c, P_ISPLAYER)) return true;
  if((c2->land == laCrossroads) ^ (c->land == laCrossroads) && keep_to_crossroads()) return true;
  return false;
  }

int rcelldist(cell *c) {
  #if CAP_CRYSTAL
  if(cryst) return crystal::space_distance(c, currentmap->gamestart());
  #endif
  return celldist(c);
  }

int pcelldist(cell *c) {
  #if CAP_CRYSTAL
  if(cryst) return crystal::precise_distance(c, currentmap->gamestart());
  #endif
  if(sl2) return PIU(celldist(c));
  return celldist(c);
  }

int trackval(cell *c) {
  int v = rcelldist(c);
  int bonus = 0;
  if(c->land != laCrossroads || !keep_to_crossroads())
  forCellEx(c2, c) {
    int d = rcelldist(c2) - v;
    if(d < 0 && bad(c2, c))
      bonus += 2;
    if(d == 0 && bad(c2, c))
      bonus ++;
    }
  return v + bonus;
  }

void tie_info(cell *c, int from_track, int comp) {
  rti_id[c] = isize(rti);
  rti.emplace_back(race_cellinfo{c, from_track, comp, -1, -1});
  }

race_cellinfo& get_info(cell *c) {
  return rti[rti_id.at(c)];
  }

int race_checksum;

ld start_line_width;

struct hr_track_failure : hr_exception {};

int length;

bool use_exhaustive_distance;

void find_track(cell *start, int sign, int len) {
  int dl = 7 - getDistLimit() - genrange_bonus;
  dl = (8 + dl) / 2;
  if(WDIM == 3 && dl < 6) dl = 6;
  cell *goal;
  map<cell*, cell*> parent;
  map<int, vector<cell*> > cellbydist;
  cellbydist[0].push_back(start);
    
  int traversed = 0;
  
  while(true) {
    traversed++;
    if(cellbydist.empty()) {
      println(hlog, "reset after traversing ", traversed, " width = ", TWIDTH, " length = ", length);
      throw hr_track_failure();
      }
    auto it = cellbydist.end();
    it--;
    // if(hrand(100) < 85 && it != cellbydist.begin()) it--;
    auto& v = it->second;
    if(v.empty()) { cellbydist.erase(it); continue; }
    int id = hrand(isize(v));
    cell *c = v[id];
    v[id] = v.back(); v.pop_back();    
    if(it->first >= length) {
      goal = c;
      break;
      }
    setdist(c, dl, parent[c]);
    forCellEx(c1, c) if(!bad(c1, c) && !parent.count(c1)) {
      parent[c1] = c;
      int id;
      /* if(nil && !nilv::nilperiod[0]) {
        switch(sign) {
          case 1: id = c1->master->zebraval - start->master->zebraval; break;
          case 2: id = start->master->emeraldval - c1->master->emeraldval; break;
          case 3: id = start->master->zebraval - c1->master->zebraval; break;
          case 4: id = start->master->emeraldval - c1->master->emeraldval; break;
          }
        } */
      if(use_exhaustive_distance)
        id = trackval(c1);
      else if(nil && !quotient) {
        switch(sign) { 
          case 1: id = c1->master->emeraldval - start->master->emeraldval; break;
          case 2: case 3: id = start->master->fieldval - c1->master->fieldval; break;
          case 4: id = start->master->emeraldval - c1->master->emeraldval; break;
          }
        }
      #if CAP_SOLV
      else if(asonov::in() && asonov::period_z) {
        auto co = asonov::get_coord(c->master);
        ld x = szgmod(co[0], asonov::period_xy);
        ld y = szgmod(co[1], asonov::period_xy);
        ld z = hypot(x, y);
        ld maxz = asonov::period_xy ? asonov::period_xy/3 : 1000000000;
        id = int((log(z) / log(maxz)) * length);
        }
      else if(sn::in())
        id = (start->master->distance - c1->master->distance) * sign;
      #endif
      else
        id = trackval(c1);
      cellbydist[id].push_back(c1);
      }
    }

  if(use_exhaustive_distance) permanent_long_distances(goal);
  
  if(nonisotropic) {
    vector<cell*> p;
    while(goal != start) p.push_back(goal), goal = parent[goal];
    while(!p.empty()) track.push_back(p.back()), p.pop_back();
    }
  else
    track = build_shortest_path(start, goal);  
  }

EX void block_cells(vector<cell*> to_block, function<bool(cell*)> blockbound) {
  hrandom_shuffle(to_block);
  
  for(cell *c: to_block) switch(specialland) {
    case laIce:
      c->wall = waIcewall;
      break;
    
    case laHell:
      c->wall = waSulphur;
      break;
    
    case laJungle: {
      vector<int> dirs;
      forCellIdEx(c2, i, c) if(among(c2->monst, moIvyRoot, moIvyWait)) dirs.push_back(i);
      if(dirs.empty()) c->monst = moIvyRoot;
      else c->monst = moIvyWait, c->mondir = dirs[hrand(isize(dirs))];
      break;
      }
    
    case laDeadCaves: 
      if(blockbound(c)) c->wall = waDeadwall;
      break;
    
    case laRedRock:
      if(blockbound(c)) c->wall = waRed3;
      break;
        
    case laDragon:
      c->wall = waChasm;
      break;

    case laDryForest:
      if(blockbound(c)) c->wall = waBigTree;
      break;
    
    case laDesert:
      if(blockbound(c)) c->wall = waDune;
      break;

    case laRuins:
      if(blockbound(c)) c->wall = waRuinWall;
      break;

    case laElementalWall:
      if(blockbound(c)) {
        if(c->land == laEFire) c->wall = waEternalFire;
        else if(c->land == laEWater) c->wall = waSea;
        else if(c->land == laEAir) c->wall = waChasm;
        else if(c->land == laEEarth) c->wall = waStone;
        }
      break;    
  
    default: break;
    }
  }

EX void make_bounded_track(cell *s) {

  celllister cl(s, 999, 1000000, NULL);
  for(cell *c: cl.lst)
    setdist(c, 0, NULL);
  println(hlog, "cls = ", isize(cl.lst));

  for(cell *c: cl.lst) {
    c->item = itNone; c->monst = moNone;    
    if(c->land != laAsteroids) c->wall = waNone;
    }

  map<cell*, int> mazetype;
  track.clear();
  track.push_back(s);
  while(true) {
    cell *last = track.back();
    mazetype[last] = 1;
    vector<cell*> choices;
    forCellCM(c1, last) {
      if(mazetype[c1] != 0) continue;
      choices.push_back(c1);
      }
    if(choices.empty()) break;
    cell *nxt = choices[hrand(isize(choices))];
    for(cell *cc: choices) mazetype[cc] = 2;
    track.push_back(nxt);
    }
  vector<cell*> to_block;
  for(auto p: mazetype) {
    if(p.second == 2) to_block.push_back(p.first);
    }
  block_cells(to_block, [] (cell *c) { return true; });
  for(cell *c: to_block) if(among(c->wall, waNone, waInvisibleFloor) && !c->monst) c->wall = waBarrier;
  }

EX bool bounded_track;

EX void generate_track() {

  TWIDTH = getDistLimit() - 1;  
  if(TWIDTH == 1) TWIDTH = 2;
  if(sl2) TWIDTH = 2;
  TWIDTH += race_try / 8;
  
  #if CAP_FILES
  if(ghostset().empty())
    read_ghosts(track_code, modecode());
  #endif

  track.clear();

  /*
  int t = -1;
  bignum full_id;
  bool onlychild = true;
  */
  
  cell *s = currentmap->gamestart();  

  if(specialland == laCrossroads) {
    celllister cl(s, TWIDTH, 1000000, NULL);
    for(cell *c: cl.lst) c->bardir = NOBARRIERS;
    }                              
  
  int dl = 7 - getDistLimit() - genrange_bonus;

  setdist(s, 6, NULL);
  makeEmpty(s);
  cview(); // needed for some virtualRebases
  
  use_exhaustive_distance = exhaustive_distance_appropriate();
    
  if(use_exhaustive_distance) 
    permanent_long_distances(s);

  bounded_track = false;
  
  length = LENGTH;
  if(WDIM == 3 || weirdhyperbolic) length = max(length - 10 * race_try, 10);

  if(use_exhaustive_distance) {
    int maxsd = max_saved_distance(s);
    println(hlog, "max length = ", maxsd);
    length = min(length, maxsd * 5/6);
    }
    
  try {
  if(bounded && !prod && !(cgflags & qHUGE_BOUNDED)) {
    bounded_track = true;
    make_bounded_track(s);
    }
  else if(use_exhaustive_distance) {
    find_track(s, 0, length);
    }
  else if(asonov::in()) {
    find_track(s, 0, length);
    }
  else if(sol && !asonov::in()) {
    track.push_back(s);
    find_track(s, 1, length/4);
    find_track(track.back(), -1, length-2*(length/4));
    find_track(track.back(), 1, length/4);
    }
  else if(nih) {
    track.push_back(s);
    find_track(s, 1, length/2);
    find_track(track.back(), -1, length/2);
    }
  else if(nil) {
    track.push_back(s);
    find_track(s, 1, length/4);
    find_track(track.back(), 2, length/4);
    find_track(track.back(), 3, length/4);
    find_track(track.back(), 4, length/4);
    }
  else find_track(s, 0, length);    
    }
  catch(const hr_track_failure&) {
    race_try++;
    gamegen_failure = true;
    return;
    }
  catch(const hr_shortest_path_exception&) {
    addMessage("error: could not build path");
    gamegen_failure = true;
    racing::on = false;
    return;
    }  
  
  if(WDIM == 3) dl = 7 - TWIDTH;
  for(cell *c:track) setdist(c, dl, NULL);
  
  if(true) {
    manual_celllister cl;
  
    for(int i=0; i<isize(track); i++) {
      tie_info(track[i], 0, i);
      cl.add(track[i]);
      }
    
    int win = isize(track) - DROP;
    
    for(int i=0; i<isize(cl.lst); i++) {
      cell *c = cl.lst[i];
      auto p = get_info(c);
      forCellEx(c2, c) if(!rti_id.count(c2)) {
        tie_info(c2, p.from_track+1, p.completion);
        cl.add(c2);
        }
      if(bounded_track) continue;
      c->item = itNone;
      if(c->wall == waMirror || c->wall == waCloud) c->wall = waNone;
      if(!isIvy(c))
        c->monst = moNone;
      if(c->monst == moIvyHead) c->monst = moIvyWait;
      if(inmirror(c->land))
        ;      
      else if(p.from_track == TWIDTH) {
        killMonster(c, moNone, 0);
        c->wall = waBarrier;
        c->land = laBarrier;
        }
      else if(p.from_track > TWIDTH) {
        killMonster(c, moNone, 0);
        c->land = laMemory;
        c->wall = waChasm;
        }
      if(p.completion >= win && p.from_track < TWIDTH) {
        c->wall = hrand(2) ? waMirror : waCloud;
        killMonster(c, moNone, 0);
        }
      }
    }
  
  int byat[65536];
  for(int a=0; a<16; a++) byat[a] = 0;
  for(const auto s: rti) byat[s.from_track]++;
  for(int a=0; a<16; a++) printf("%d: %d\n", a, byat[a]);
  
  if(s->land == laCaves || (s->land == laCrossroads && !keep_to_crossroads())) {
    set<unsigned> hash;
    while(true) {
      unsigned hashval = 7;
      int id = 0;
      for(auto s: rti) {
        fix_cave(s.c);
        if(s.c->wall == waCavewall) 
          hashval = (3+2*(id++)) * hashval + 1;
        if(s.c->wall == waCavefloor) 
          hashval = (3+2*(id++)) * hashval + 2;
        }
      printf("hashval = %x id = %d\n", hashval, id);
      if(hash.count(hashval)) break;
      hash.insert(hashval);
      }
    }
  
  if(!bounded_track) for(cell *sc: track) {
    straight = calc_relative_matrix(sc, track[0], C0);
    if(straight[GDIM][GDIM] > 1e8) break;
    }
  straight = rspintox(straight * C0);
  
  ld& a = start_line_width;
  if(WDIM == 2 && !bounded_track) for(a=0; a<10; a += .1) {
    hyperpoint h = straight * parabolic1(a) * C0;
    cell *at = s;
    virtualRebase(at, h);
    if(!rti_id.count(at) || get_info(at).from_track >= TWIDTH) break;
    }
  
  if(WDIM == 2 && !bounded_track) for(ld cleaner=0; cleaner<a*.75; cleaner += .2) for(int dir=-1; dir<=1; dir+=2) {
    transmatrix T = straight * parabolic1(cleaner * dir);
    cell *at = s;
    virtualRebase(at, T);
    get_info(at).from_start = 0;
    for(ld u=0; u<50; u++) {
      if(at->wall != waBarrier)
        makeEmpty(at);
      killMonster(at, moNone, 0);
      T = T * xpush(.1);
      virtualRebase(at, T);
      }
    }

  for(auto s: rti) if(s.c->monst == moIvyDead) s.c->monst = moNone;
  
  for(int i=0; i<motypes; i++) kills[i] = 0;
  
  vector<shiftmatrix> forbidden;
  for(auto& ghost: ghostset()[specialland])
    forbidden.push_back(get_ghostmoment_matrix(ghost.history[0]));
  for(auto& ghost: oghostset()[specialland]) 
    forbidden.push_back(get_ghostmoment_matrix(ghost.history[0]));

  for(int i=0; i<multi::players; i++) trophy[i] = 0;

  for(int i=0; i<multi::players; i++) {
    auto who = shmup::pc[i];
    // this is intentionally not hrand
    
    for(int j=0; j<100; j++) {
      if(WDIM == 3 || bounded_track)
        who->at = Id; // straight * cspin(0, 2, rand() % 360) * cspin(1, 2, rand() % 360);
      else
        who->at = straight * parabolic1(start_line_width * (rand() % 20000 - 10000) / 40000) * spin(rand() % 360);
      who->base = s;
      bool ok = true;
      for(const shiftmatrix& t: forbidden) if(hdist(t*C0, shiftless(who->at) * C0) < 10. / (j+10)) ok = false;
      if(ok) break;
      }
    virtualRebase(who);
    }

  if(bounded_track) track.back()->wall = waCloud;
  
  if(1) {
    manual_celllister cl;
    cl.add(s);
    bool goal = false;
    for(int i=0; i<isize(cl.lst); i++) {
      cell *c = cl.lst[i];
      forCellEx(c2, c) {
        if(among(c2->wall, waCloud, waMirror)) goal = true;
        if(passable(c2, c, P_ISPLAYER))
          cl.add(c2);
        }
      }
    if(!goal) {
      printf("error: goal unreachable\n");
      gamegen_failure = true;
      race_try++;
      return;    
      }
    }
  
  if(1) {
    map<cell*, pair<int, int> > cdists;
    manual_celllister cl;
    cl.add(s);
    for(auto cc: rti) if(cc.from_start == 0) cl.add(cc.c);

    for(int i=0; i<isize(cl.lst); i++) {
      cell *c = cl.lst[i];
      forCellEx(c2, c) {
        if(passable(c2, c, P_ISPLAYER) && !cl.listed(c2))
          cl.add(c2), get_info(c2).from_start = get_info(c).from_start + 1;
        }
      }
    
    }

  if(1) {
    map<cell*, pair<int, int> > cdists;
    manual_celllister cl;
    for(auto cc: rti) if(among(cc.c->wall, waCloud, waMirror))
      cc.from_goal = 0, cl.add(cc.c);

    for(int i=0; i<isize(cl.lst); i++) {
      cell *c = cl.lst[i];
      forCellEx(c2, c) {
        if(passable(c2, c, P_ISPLAYER) && !cl.listed(c2))
          cl.add(c2), get_info(c2).from_goal = get_info(c).from_goal + 1;
        }
      }
    
    }
  
  int total_track = get_info(s).from_goal;
  
  auto blockoff = [&total_track] (race_cellinfo& cc) {
    if(cc.from_start < 1 || cc.from_goal < 1) return false;
    int dif = cc.from_start + cc.from_goal - 2 * cc.from_track - total_track;
    return dif > 3;
    };

  vector<cell*> to_block;
  
  for(auto cc: rti) if(blockoff(cc)) to_block.push_back(cc.c);
  
  block_cells(to_block, [&blockoff] (cell *c) {
    forCellEx(c2, c) if(passable(c2, c, P_ISPLAYER) && !blockoff(get_info(c2))) return true;
    return false;
    });
  

  // for(cell *c: to_block) if(blockbound(c)) c->land = laOvergrown;

  /*
  for(cell *c: track) {
    int i = trackval(c) - celldist(c);
    if(i == 0) c->item = itDiamond;
    if(i == 1) c->item = itGold;
    if(i == 2) c->item = itEmerald;
    if(i == 3) c->item = itSapphire;
    if(i == 4) c->item = itRuby;
    if(i >= 5) c->item = itBone;
    }
  */

  track_ready = true;
  race_checksum = hrand(1000000);
  
  race_start_tick = 0;
  for(int i=0; i<MAXPLAYER; i++) race_finish_tick[i] = 0;

  official_race = (track_code == "OFFICIAL" && modecode() == 2);
  if(official_race && isize(oghostset() [specialland]) && race_checksum != oghostset() [specialland] [0].checksum) {
    official_race = false;
    addMessage(XLAT("Race did not generate correctly for some reason -- not ranked"));
    }
  }

bool inrec = false;

EX ld race_angle = 90;

EX bool force_standard_centering() {
  return nonisotropic || hybri || quotient || bounded;
  }

EX bool use_standard_centering() {
  return standard_centering || force_standard_centering();
  }

EX transmatrix track_matrix(int at, int dir) {
  transmatrix res = unshift(ggmatrix(racing::track[at]));
  while(true) {
    if(at+dir < 0 || at+dir >= isize(racing::track)) return res;
    for(int x=0; x<MXDIM; x++) for(int y=0; y<MXDIM; y++)
      if(abs(res[y][x]) > 10000) return res;
    cell *cur = racing::track[at];
    at += dir;
    cell *next = racing::track[at];
    int nei = neighborId(cur, next);
    if(nei == -1) return res;
    res = res * currentmap->adj(cur, nei);
    }
  }

EX bool set_view() {

  multi::cpid = subscreens::in ? subscreens::current_player : 0;

  if(race_start_tick == 0) race_start_tick = ticks + 5000;

  shmup::monster *who = shmup::pc[multi::cpid];
  
  if(!inrec) {
    const transmatrix T = who->at;
    ld alpha = -atan2(T * C0);
    ld distance = hdist0(T * C0);
    ld beta = -atan2(xpush(-distance) * spin(-alpha) * T * Cx1);
    current_history[multi::cpid].emplace_back(ghostmoment{ticks - race_start_tick, rti_id[who->base], 
      angle_to_uchar(alpha),
      frac_to_uchar(distance / distance_multiplier),
      angle_to_uchar(beta),
      frac_to_uchar(who->footphase)
      });
    }

  if(use_standard_centering()) return false;
  if(player_relative && specialland == laAsteroids) return false;

  transmatrix at = ypush(-vid.yshift) * unshift(ggmatrix(who->base)) * who->at;
  
  if(racing::player_relative || quotient || (kite::in() && GDIM == 3)) {
    View = iso_inverse(at) * View;
    }
  else {
    /* this works only in isotropic geometries, but we don't really care about this in 3D */
    int z = get_info(who->base).completion;
    
    transmatrix T1 = track_matrix(z, -1);
    transmatrix T2 = track_matrix(z, +1);
    
    transmatrix iT1 = iso_inverse(T1);

    transmatrix T = spintox(iT1 * T2 * C0);
    
    hyperpoint h = T * iT1 * ypush(vid.yshift) * at * C0;
    ld y = GDIM == 2 ? asin_auto(h[1]) : asin_auto(hypot(h[1], h[2]));
    ld x = asin_auto(h[0] / cos_auto(y));
    x += race_advance;
    if(GDIM == 3 && race_advance == 0 && pmodel == mdPerspective) race_advance = -1;

    View = xpush(-x) * T * iT1 * ypush(vid.yshift) * View;

    if(GDIM == 3) View = cspin(2, 0, M_PI/2) * View;
    fixmatrix(View);
    }
  if(GDIM == 3 && WDIM == 2)
    View = cspin(0, 1, M_PI) * cspin(2, 1, M_PI/2 + shmup::playerturny[multi::cpid]) * spin(-M_PI/2) * View;
  else if(GDIM == 2) View = spin(race_angle * degree) * View;
  return true;
  }

#if CAP_COMMANDLINE  
void show();

int readArgs() {
  using namespace arg;
           
  if(0) ;
  else if(argis("-racing")) {
    PHASEFROM(2);
    stop_game();
    switch_game_mode(rg::racing);
    }
  else if(argis("-rsc")) {
    standard_centering = true;
    }
  else if(argis("-rfast")) {
    PHASEFROM(3);
    start_game();
    race_start_tick = 1;
    }
  else return 1;
  return 0;
  }
#endif

int tstart, tstop;
heptspin sview;

auto hook = 
#if CAP_COMMANDLINE
  addHook(hooks_args, 100, readArgs)
#endif
+ addHook(hooks_clearmemory, 0, []() {
    track_ready = false;
    track.clear();
    rti.clear();
    rti_id.clear();
    for(auto &ch: current_history) ch.clear();
    })
+ addHook(hooks_configfile, 100, [] {
    addsaver(racing::race_advance, "race_advance");
    addsaver(racing::race_angle, "race_angle");
    addsaver(racing::ghosts_to_show, "race_ghosts_to_show");
    addsaver(racing::ghosts_to_save, "race_ghosts_to_save");
    addsaver(racing::guiding, "race_guiding");
    addsaver(racing::player_relative, "race_player_relative");
    addsaver(racing::standard_centering, "race_standard_centering");
    })
// + addHook(hooks_handleKey, 120, akh);
  ;

EX vector<eLand> race_lands = {
  laHunting,
  laCrossroads,
  laJungle,
  laDesert, 
  laRedRock,
  laDragon,
  laMirror,
  laRuins,
  laCaves,
  laWildWest,
  laIce, 
  laHell,  
  laTerracotta,
  laElementalWall,
  laDryForest,
  laDeadCaves,
  laAsteroids
  };

vector<string> playercmds_race = {
  "forward", "backward", "turn left", "turn right",
  "forward", "backward", "turn left", "turn right",
  "", "", "",
  "", "change camera", "", ""
  };

EX string racetimeformat(int t) {
  string times = "";
  int digits = 0;
  bool minus = (t < 0);
  if(t < 0) t = -t;
  while(t || digits < 6) {
    int mby = (digits == 4 ? 6 : 10);
    times = char('0'+(t%mby)) + times;
    t /= mby; digits++;
    if(digits == 3) times = "." + times;
    if(digits == 5) times = ":" + times;
    }
  if(minus) times = "-" + times;
  return times;
  }

extern int playercfg;

void track_chooser(string new_track) {
  cmode = 0;
  gamescreen(2);
  dialog::init(XLAT("Racing"));
  
  map<char, eLand> landmap;
  
  dynamicval<bool> so(shmup::on, true);
  dynamicval<bool> ro(racing::on, true);

  char let = 'a';
  for(eLand l: race_lands) {
    auto& ghs = race_ghosts[make_pair(new_track, modecode())];
    const int LOST = 3600000;
    int best = LOST;
    if(ghs.count(l)) {
      auto& gh = ghs[l];
      for(auto& gc: gh) best = min(best, gc.result);
      }
    string s = (best == LOST) ? "" : racetimeformat(best);
    landmap[let] = l;
    dialog::addSelItem(XLAT1(linf[l].name), s, let++);
    dialog::add_action([l, new_track] () {
      stop_game();
      multi::players = playercfg;
      if(!racing::on) switch_game_mode(rg::racing);
      track_code = new_track;
      specialland = l;
      // because of an earlier issue, some races in the Official track start with race_try = 1
      race_try = among(l, laCaves, laWildWest, laHell, laTerracotta, laElementalWall, laDryForest, laDeadCaves) ? 1 : 0;
      start_game();
      popScreenAll();
      });
    }
  
  dialog::addBack();
  dialog::display();
  
  if(landmap.count(getcstat) && new_track == "OFFICIAL" && modecode() == 2)
    displayScore(landmap[getcstat]);  
  }

void race_projection() {
  cmode = sm::SIDE | sm::MAYDARK;
  gamescreen(1);
  
  dialog::init(XLAT("racing projections"));

  dialog::addBoolItem(XLAT("Poincaré disk model"), pmodel == mdDisk && !pconf.camera_angle, '1');
  dialog::add_action([] () {
    pmodel = mdDisk;
    race_advance = 0;
    vid.yshift = 0;
    pconf.camera_angle = 0;
    pconf.xposition = 0;
    pconf.yposition = 0;
    pconf.scale = 1;
    vid.use_smart_range = 0;
    vid.smart_range_detail = 3;
    });
  
  dialog::addBoolItem(XLAT("band"), pmodel == mdBand, '2');
  dialog::add_action([] () {
    pmodel = mdBand;
    pconf.model_orientation = race_angle;
    race_advance = 1;
    vid.yshift = 0;
    pconf.camera_angle = 0;
    pconf.xposition = 0;
    pconf.yposition = 0;
    pconf.scale = 1;
    vid.use_smart_range = 1;
    vid.smart_range_detail = 3;
    });
  
  dialog::addBoolItem(XLAT("half-plane"), pmodel == mdHalfplane, '3');
  dialog::add_action([] () {
    pmodel = mdHalfplane;
    pconf.model_orientation = race_angle + 90;
    race_advance = 0.5;
    vid.yshift = 0;
    pconf.camera_angle = 0;
    pconf.xposition = 0;
    pconf.yposition = 0;
    pconf.scale = 1;
    vid.use_smart_range = 1;
    vid.smart_range_detail = 3;
    });

  dialog::addBoolItem(XLAT("third-person perspective"), pmodel == mdDisk && pconf.camera_angle, '4');
  dialog::add_action([] () {
    pmodel = mdDisk;
    race_advance = 0;
    vid.yshift = -0.3;
    pconf.camera_angle = -45;
    pconf.scale = 18/16. * vid.xres / vid.yres / multi::players;
    pconf.xposition = 0;
    pconf.yposition = -0.9;
    vid.use_smart_range = 1;
    vid.smart_range_detail = 3;
    });
  
  if(true) {    
    dialog::addSelItem(XLAT("point of view"), player_relative ? XLAT("player") : XLAT("track"), 'p');
    if(racing::use_standard_centering())
      dialog::lastItem().value = XLAT("N/A");
    dialog::add_action([] () { 
      player_relative = !player_relative; 
      if(pmodel == mdBand || pmodel == mdHalfplane)
        pmodel = mdDisk;
      if(racing::on)
        set_view();
      });
    }
  else dialog::addBreak(100);
        
  if(GDIM == 2) {
    dialog::addSelItem(XLAT("race angle"), fts(race_angle), 'a');
    dialog::add_action([] () { 
      dialog::editNumber(race_angle, 0, 360, 15, 90, XLAT("race angle"), "");
      int q = pconf.model_orientation - race_angle;
      dialog::reaction = [q] () { pconf.model_orientation = race_angle + q; };
      });
    }

  dialog::addSelItem(XLAT("show more in front"), fts(race_advance), 'A');
  dialog::add_action([] () {
    dialog::editNumber(race_advance, 0, 360, 0.1, 1, XLAT("show more in front"), "");
    });

  dialog::addBoolItem(XLAT("do not use special centering for racing"), standard_centering, 'C');
  if(force_standard_centering()) dialog::lastItem().value = XLAT("N/A");
  dialog::add_action([] () {
    standard_centering = !standard_centering;
    });

  dialog::addBack();
  dialog::display();
  }

  int playercfg;
  bool editing_track;
  string new_track;
  
/* struct race_configurer { */

  void set_race_configurer() { editing_track = false; new_track = track_code; playercfg = multi::players; }
  
  static string random_track_name() {
    string s = "";
    for(int a = 0; a < 4; a++)  {
      int u = rand() % 2;
      if(u == 0)
        s += "AEIOUY" [ rand() % 6];
      s += "BCDFGHJKLMNPRSTVWZ" [ rand() % 18];
      if(u == 1)
        s += "AEIOUY" [ rand() % 6];
      }
    return s;
    }
  
  static string racecheck(int sym, int uni) {
    if(uni >= 'A' && uni <= 'Z') return string("") + char(uni);
    if(uni >= 'a' && uni <= 'z') return string("") + char(uni - 32);
    return "";
    }

  bool alternate = false;
  
  #if MAXMDIM >= 4
  EX void thurston_racing() {
    gamescreen(1);
    dialog::init(XLAT("racing in Thurston geometries"));
    
    dialog::addBreak(100);
    
    char ch = '1';
    
    auto add_thurston_race = [&ch] (string caption, reaction_t launcher) {
      dialog::addBigItem(caption, ch++);
      dialog::add_action([=] { stop_game(); 
        if(!racing::on) switch_game_mode(rg::racing);
        racing::standard_centering = true;
        launcher();
        track_code = "OFFICIAL";
        start_game();
        popScreenAll();
        });
      dialog::addBreak(50);
      };

    if(!alternate) {
      add_thurston_race(XLAT("Euclidean"), [] { stop_game(); euc::clear_torus3(); set_geometry(gBitrunc3); });
      add_thurston_race(XLAT("hyperbolic"), [] { set_geometry(gBinary3); vid.texture_step = 4; });
      add_thurston_race(XLAT("spherical"), [] { set_geometry(gCell120); });
      #if CAP_SOLV
      add_thurston_race(XLAT("Solv geometry"), [] { sn::solrange_xy = 10; sn::solrange_z = 3; set_geometry(gSol); });
      #endif
      add_thurston_race(XLAT("S2xE"), [] { set_geometry(gSphere); set_variation(eVariation::bitruncated); set_geometry(gProduct); });
      add_thurston_race(XLAT("H2xE"), [] { set_geometry(gNormal); set_variation(eVariation::bitruncated); set_geometry(gProduct); });
      add_thurston_race(XLAT("Nil"), [] { stop_game(); nilv::nilperiod[0] = 0; set_geometry(gNil); });
      add_thurston_race(XLAT("PSL(2,R)"), [] { set_geometry(gNormal); set_variation(eVariation::pure); set_geometry(gRotSpace); });
      }
    else {
      #if CAP_SOLV
      add_thurston_race(XLAT("stretched hyperbolic"), [] { set_geometry(gNIH); vid.texture_step = 4; });
      add_thurston_race(XLAT("stretched Solv"), [] { set_geometry(gSolN); sn::solrange_xy = 10; sn::solrange_z = 3; vid.texture_step = 4; });
      add_thurston_race(XLAT("periodic Solv"), [] { stop_game(); sn::solrange_xy = 5; sn::solrange_z = 2; asonov::period_xy = 8; asonov::period_z = 0; asonov::set_flags(); set_geometry(gArnoldCat); });
      #endif
      add_thurston_race(XLAT("hyperbolic crystal"), [] { set_geometry(gCrystal344); vid.texture_step = 4; });
      add_thurston_race(XLAT("torus x E"), [] { stop_game(); euc::eu_input = euc::torus3(4, 4, 0); set_geometry(gCubeTiling); });
      add_thurston_race(XLAT("hyperbolic regular"), [] { set_geometry(gSpace534); });
      add_thurston_race(XLAT("S2xE regular"), [] { set_geometry(gSphere); set_variation(eVariation::pure); set_geometry(gProduct); });
      add_thurston_race(XLAT("H2xE regular"), [] { set_geometry(gNormal); set_variation(eVariation::pure); set_geometry(gProduct); });
      add_thurston_race(XLAT("periodic Nil"), [] { stop_game(); nilv::nilperiod[0] = 3; set_geometry(gNil); });
      }
    dialog::addBoolItem_action(XLAT("alternate versions"), alternate, 'x');

    dialog::addBreak(100);
    dialog::addBack();
    dialog::display();
    }
  #endif

  void raceconfigurer() {
  
    gamescreen(1);
  
    dialog::init(XLAT("Racing"));
  
    if(false)
      dialog::addInfo(XLAT("Racing available only in unbounded worlds."), 0xFF0000);
    else {
      dialog::addItem(XLAT("select the track and start!"), 's');
      dialog::add_action([/*this*/] () { 
        dynamicval<bool> so(shmup::on, true);
        dynamicval<bool> ro(racing::on, true);
        #if CAP_FILES
        if(race_ghosts[make_pair(new_track, modecode())].empty())
          read_ghosts(new_track, modecode());
        else
          println(hlog, "known ghosts: ", isize(race_ghosts[make_pair(new_track, modecode())]));
        #endif
        pushScreen([/*this*/] () { track_chooser(new_track); }); 
        });
      }
    
    dialog::addBreak(100);

    if(WDIM == 3) {
      dialog::addItem(XLAT("3D configuration"), '9');
      dialog::add_action_push(show3D);
      }
    else {
      dialog::addItem(XLAT("configure the projection"), 'p');
      dialog::add_action_push(race_projection);
      }
  
    dialog::addBoolItem_action(XLAT("guiding line"), guiding, 'g');
  
    dialog::addItem(multi::player_count_name(playercfg), 'n');
    dialog::add_action([/*this*/] () { 
      playercfg = playercfg == 1 ? 2 : 1;
      });
  
    dialog::addItem(XLAT("configure player 1"), '1');
    dialog::add_action([] () {
      pushScreen(multi::get_key_configurer(1, playercmds_race));
      });
  
    if(playercfg >= 2) {
      dialog::addItem(XLAT("configure player 2"), '2');
      dialog::add_action([] () {
        pushScreen(multi::get_key_configurer(2, playercmds_race));
        });
      }
    else dialog::addBreak(100);
    
    dialog::addBreak(100);

    dialog::addSelItem(XLAT("track seed"), editing_track ? dialog::view_edited_string() : new_track, '/');
    dialog::add_action([/*this*/] () { 
      editing_track = !editing_track;
      if(editing_track) dialog::start_editing(new_track);
      });
    dialog::addItem(XLAT("play the official seed"), 'o');
    dialog::add_action([/*this*/] () { new_track = "OFFICIAL"; });
    dialog::addItem(XLAT("play a random seed"), 'r');
    dialog::add_action([/*this*/] () { new_track = random_track_name(); });    

    dialog::addBreak(100);
    
    dialog::addSelItem(XLAT("best scores to show as ghosts"), its(ghosts_to_show), 'c');
    dialog::add_action([]() { dialog::editNumber(ghosts_to_show, 0, 100, 1, 5, "best scores to show as ghosts", ""); });

    dialog::addSelItem(XLAT("best scores to save"), its(ghosts_to_save), 'b');
    dialog::add_action([]() { dialog::editNumber(ghosts_to_save, 0, 100, 1, 10, "best scores to save", ""); });
    
    
    if(racing::on) {
      dialog::addItem(XLAT("disable the racing mode"), 'x');
      dialog::add_action([] () { 
        stop_game();
        switch_game_mode(rg::racing);
        race_try = 0;
        if(geometry == gNormal) specialland = firstland = laIce;
        start_game();
        });
      }
    
    #if MAXMDIM >= 4
    dialog::addItem(XLAT("racing in Thurston geometries"), 'T');
    dialog::add_action_push(thurston_racing);
    #endif

    dialog::addBack();
    dialog::display();

    keyhandler = [/*this*/] (int sym, int uni) {
      if(editing_track) {
        if(sym == SDLK_RETURN) sym = uni = '/';
        if(dialog::handle_edit_string(sym, uni, racecheck)) return;
        }
      dialog::handleNavigation(sym, uni);
      if(doexiton(sym, uni)) { if(editing_track) editing_track = false; else popScreen(); }
      };
    
    }
/*  }; */

EX void configure_race() { 
  set_race_configurer();

  pushScreen(raceconfigurer);
  }

auto hooks1 = 
  addHook(hooks_o_key, 90, [] (o_funcs& v) { 
    if(racing::on) { set_race_configurer();  v.push_back(named_dialog(XLAT("racing menu"), raceconfigurer)); }
    });

map<string, map<eLand, int> > scoreboard;

void uploadScore() {
  int tscore = 0;
  for(eLand l: race_lands) {
    int i = get_score_in_land(l);
    if(!i) continue;
    scoreboard[myname()][l] = i;
    int score = 60000000 / i; // 1000 points for minute, 2000 points for 30 sec
    tscore += score;
    }
    
  achievement_score(LB_RACING, tscore);
  }

EX void displayScore(eLand l) {
  int vf = min((vid.yres-64) / 70, vid.xres/80);
  int x = vid.xres / 4;
  
  set_priority_board(LB_RACING);
  
  if(get_sync_status() == 1) {
    displayfr(x, 56, 1, vf, "(syncing)", 0xC0C0C0, 0);
    }
  else {
    vector<pair<int, string> > scores;
    for(auto p: scoreboard) if(p.second.count(l)) scores.emplace_back(p.second[l], p.first);
    sort(scores.begin(), scores.end());
    int i = 0;
    for(auto& sc: scores) {
      int i0 = 56 + (i++) * vf;
      displayfr(x, i0, 1, vf, racetimeformat(sc.first), 0xC0C0C0, 16);
      displayfr(x+8, i0, 1, vf, sc.second, 0xC0C0C0, 0);
      }
    }
  }

EX void race_won() {
  if(!race_finish_tick[multi::cpid]) {
    int result = ticks - race_start_tick;
    int losers = 0;
    int place = 1;
    for(int i=0; i<multi::players; i++) if(race_finish_tick[i]) place++; else losers = 0;
    for(auto& ghost: ghostset()[specialland]) if(ghost.result < result) place++; else losers++;
    for(auto& ghost: oghostset()[specialland]) if(ghost.result < result) place++; else losers++;

    if(place == 1 && losers) trophy[multi::cpid] = 0xFFD500FF;
    if(place == 2) trophy[multi::cpid] = 0xFFFFC0FF;
    if(place == 3) trophy[multi::cpid] = 0x967444FF;
  
    if(place + losers > 1)
      addMessage(XLAT("Finished the race! Time: %1, place: %2 out of %3", racetimeformat(result), its(place), its(place+losers)));
    else
      addMessage(XLAT("Finished the race in time %1!", racetimeformat(result)));
    
    if(place == 1 && losers && official_race && isize(oghostset()[specialland]))
      achievement_gain("RACEWON", rg::racing);
    
    race_finish_tick[multi::cpid] = ticks;
    charstyle gcs = getcs();
    for(color_t *x: {&gcs.skincolor, &gcs.haircolor, &gcs.dresscolor, &gcs.swordcolor, &gcs.dresscolor2}) {
      for(int a=1; a<4; a++)
        part(*x, a) = (part(*x, a) + part(gcs.uicolor, a)) / 2;
      part(*x, 0) >>= 2;
      }
      
    auto &subtrack = ghostset() [specialland];

    int ngh = 0;
    for(int i=0; i<isize(subtrack); i++) {
      if(subtrack[i].checksum != race_checksum) {
        println(hlog, format("wrong checksum: %x, should be %x", subtrack[i].checksum, race_checksum));
        }
      else {
        if(i != ngh)
          subtrack[ngh] = move(subtrack[i]);
        ngh++;
        }
      }
    subtrack.resize(ngh);    

    subtrack.emplace_back(ghost{gcs, result, race_checksum, time(NULL), current_history[multi::cpid]});
    sort(subtrack.begin(), subtrack.end(), [] (const ghost &g1, const ghost &g2) { return g1.result < g2.result; });
    if(isize(subtrack) > ghosts_to_save && ghosts_to_save > 0) 
      subtrack.resize(ghosts_to_save);
    #if CAP_FILES
    if(ghosts_to_save > 0)
      write_ghosts(track_code, modecode());
    #endif
      
    if(official_race) uploadScore();
    }
  }

void draw_ghost_at(ghost& ghost, cell *w, const shiftmatrix& V, ghostmoment& p) {
  dynamicval<charstyle> x(getcs(), ghost.cs);
  if(ghost.cs.charid == -1) {
    dynamicval<bool> pc(peace::on, true);
    drawMonsterType(eMonster(ghost.cs.uicolor), w, V, ghost.cs.dresscolor, uchar_to_frac(p.footphase), NOCOLOR);
    return;
    }

  drawMonsterType(moPlayer, w, V, 0, uchar_to_frac(p.footphase), NOCOLOR);
  }

bool ghost_finished(ghost& ghost) {
  auto p = std::find_if(ghost.history.begin(), ghost.history.end(), [] (const ghostmoment gm) { return gm.step > ticks - race_start_tick;} );
  return p == ghost.history.end();
  }

ghostmoment& get_ghostmoment(ghost& ghost) {
  auto p = std::find_if(ghost.history.begin(), ghost.history.end(), [] (const ghostmoment gm) { return gm.step > ticks - race_start_tick;} );
  if(p == ghost.history.end()) p--, p->footphase = 0;
  return *p;
  }

void draw_ghost(ghost& ghost) {
  auto& p = get_ghostmoment(ghost);
  cell *w = rti[p.where_id].c;
  if(!gmatrix.count(w)) return;
  draw_ghost_at(ghost, w, get_ghostmoment_matrix(p), p);
  }

shiftmatrix racerel(ld rel) {
  int bsize = vid.fsize * 2;
  return shiftless(atscreenpos(bsize, vid.yres - bsize - rel * (vid.yres - bsize*2) / 100, bsize) * spin(M_PI/2));
  }

EX int get_percentage(cell *c) {
  return min(get_info(c).completion * 100 / (isize(track) - DROP), 100);
  }
  
EX int get_percentage(int i) {
  return get_percentage(shmup::pc[i]->base);
  }
  
void draw_ghost_state(ghost& ghost) {
  auto& p = get_ghostmoment(ghost);
  if(p.where_id >= isize(rti)) return;
  cell *w = rti[p.where_id].c;
  ld result = ghost_finished(ghost) ? 100 : get_percentage(w);
  draw_ghost_at(ghost, w, racerel(result), p);
  }

EX void drawStats() {

  if(!racing::on) return;
  
  flat_model_enabler fme;
  initquickqueue();
  
  int bsize = vid.fsize * 2;
  for(int y: {bsize, vid.yres - bsize}) {
    curvepoint(atscreenpos(bsize, y, bsize) * C0);
    curvepoint(atscreenpos(bsize/2, y, bsize) * C0);
    curvepoint(atscreenpos(bsize*3/2, y, bsize) * C0);
    curvepoint(atscreenpos(bsize, y, bsize) * C0);
    }
  queuecurve(shiftless(Id), 0xFFFFFFFF, 0, PPR::ZERO);
  
  for(auto& ghost: ghostset()[specialland]) draw_ghost_state(ghost);
  for(auto& ghost: oghostset()[specialland]) draw_ghost_state(ghost);
  
  for(int i=0; i<multi::players; i++) {
    dynamicval<int> d(multi::cpid, i);
    drawMonsterType(moPlayer, shmup::pc[i]->base, racerel(race_finish_tick[i] ? 100 : get_percentage(i)), 0xFFFFFFC0, shmup::pc[i]->footphase, NOCOLOR);
    }

  quickqueue();
  }

EX void markers() {
  if(!racing::on) return;
  if(guiding && WDIM == 2) for(int i=0; i<multi::players; i++) {
    shmup::monster *m = shmup::pc[i];
    if(!m) continue;
    for(int j=0; j<5; j++)
      gridline(m->pat, xpush0(j), xpush0(j+1), multi::scs[i].uicolor, 2);
    }
  if(racing::player_relative || use_standard_centering() || WDIM == 3) {
    using namespace racing;
    cell *goal = NULL;
    for(cell *c: track) if(inscreenrange(c)) goal = c;
    
    if(!goal) return;

    shiftpoint H = tC0(ggmatrix(goal));
    if(invalid_point(H.h)) return;

    queuestr(H, 2*vid.fsize, "X", 0x10100 * int(128 + 100 * sintick(150)));
    int cd = celldistance(track.back(), cwt.at);
    if(cd != DISTANCE_UNKNOWN)
    queuestr(H, vid.fsize, 
      #if CAP_CRYSTAL
      (cryst && !crystal::pure()) ? fts(crystal::space_distance(cwt.at, track.back())) :
      #endif
      its(cd), 0x10101 * int(128 - 100 * sintick(150)));
    addauraspecial(H, 0xFFD500, 0);
    }
  int ghosts_left = ghosts_to_show;
  for(auto& ghost: ghostset()[specialland]) {
    if(!ghosts_left) break;
    ghosts_left--;
    draw_ghost(ghost);
    }
  
  for(auto& ghost: oghostset()[specialland])
    draw_ghost(ghost);
  
  if(gmatrix.count(track[0])) {
    hyperpoint h = WDIM == 2 && GDIM == 3 ? zpush(cgi.FLOOR - cgi.human_height/80) * C0 : C0;
    for(ld z=-start_line_width; z<=start_line_width; z+=0.1) 
      curvepoint(parabolic1(z) * h);
    queuecurve(ggmatrix(track[0]) * straight, 0xFFFFFFFF, 0, PPR::BFLOOR);
    }
  }

EX void add_debug(cell *c) { 
  if(racing::on && racing::rti_id[c]) {
    auto& r = racing::get_info(c);
    dialog::addSelItem("from_track", its(r.from_track), 0);
    dialog::addSelItem("from_start", its(r.from_start), 0);
    dialog::addSelItem("from_goal", its(r.from_goal), 0);
    dialog::addSelItem("completion", its(r.completion), 0);
    }
  }

#if MAXMDIM >= 4
EX void start_thurston() {
  stop_game();
  resetModes();
  start_game();
  pushScreen(showStartMenu);
  pushScreen(racing::thurston_racing);
  }
#endif
#endif

#if !CAP_RACING
EX always_false on;
#endif
EX }

}
