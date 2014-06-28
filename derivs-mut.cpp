/*
 * Copyright (c) 2014 Aaron Moss
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "derivs-mut.hpp"

namespace derivs {
	
	// UTILITY /////////////////////////////////////////////////////////////////////
	
	gen_map new_back_map(const expr& e, gen_type gm, bool& did_inc, ind i = 0) {
		if ( e.back(i).max() > 0 ) {
			assert(e.back(i).max() == 1 && "static lookahead gen <= 1");
			did_inc = true;
			return gen_map{0, gm+1};
		} else {
			return gen_map{0};
		}
	}
	
	gen_map new_back_map(const expr& e, gen_type& gm, ind i = 0) {
		if ( e.back(i).max() > 0 ) {
			assert(e.back(i).max() == 1 && "static lookahead gen <= 1");
			return gen_map{0, ++gm};
		} else {
			return gen_map{0};
		}
	}
	
	inline gen_map default_back_map(const expr& e, bool& did_inc, ind i = 0) {
		return new_back_map(e, 0, did_inc, i);
	}
	
	void update_back_map(gen_map& eg, gen_type ebm, const expr& de, gen_type& gm, ind i) {
		gen_type debm = de.back(i).max();
		if ( debm > ebm ) { eg.add_back(debm, ++gm); }
	}
	
	void update_back_map(gen_map& eg, gen_type ebm, const expr& de, 
	                     gen_type gm, bool& did_inc, ind i) {
		gen_type debm = de.back(i).max();
		if ( debm > ebm ) {
			did_inc = true;
			eg.add_back(debm, gm+1);
		}
	}
	
	// fail_node ///////////////////////////////////////////////////////////////////
	
	expr fail_node::make() { return expr::make<fail_node>(); }
	
	expr fail_node::clone() { return expr::make<fail_node>(); }
	
	void fail_node::d(expr& e, char, ind) { /* invariant */ }
		
	gen_set fail_node::match(ind) const { return gen_set{}; }
	
	gen_set fail_node::back(ind)  const { return gen_set{0}; }
	
	// inf_node ////////////////////////////////////////////////////////////////////
	
	expr inf_node::make() { return expr::make<inf_node>(); }
	
	expr inf_node::clone() { return expr::make<inf_node>(); }
	
	void inf_node::d(expr&, char, ind) { /* invariant */ }
	
	gen_set inf_node::match(ind) const { return gen_set{}; }
	
	gen_set inf_node::back(ind)  const { return gen_set{0}; }
	
	// eps_node ////////////////////////////////////////////////////////////////////
	
	expr eps_node::make() { return expr::make<eps_node>(); }
	
	expr eps_node::clone() { return expr::make<eps_node>(); }
	
	void eps_node::d(expr& e, char x, ind) {
		if ( x != '\0' ) { e.remake<fail_node>(); } // Only match on empty string
	}
	
	gen_set eps_node::match(ind) const { return gen_set{0}; }
	
	gen_set eps_node::back(ind)  const { return gen_set{0}; }
	
	// look_node ///////////////////////////////////////////////////////////////////
	
	expr look_node::make(gen_type g) {
		return (g == 0) ? expr::make<eps_node>() : expr::make<look_node>(g);
	}
	
	expr look_node::clone() { return expr::make<look_node>(b); }
	
	void look_node::d(expr&, char, ind) { /* invariant (unparsed suffixes okay) */ }

	gen_set look_node::match(ind) const { return gen_set{b}; }
	
	gen_set look_node::back(ind)  const { return gen_set{b}; }
	
	// char_node ///////////////////////////////////////////////////////////////////
	
	expr char_node::make(char c) { return expr::make<char_node>(c); }
	
	expr char_node::clone() { return expr::make<char_node>(c); }
	
	void char_node::d(expr& e, char x, ind) {
		if ( c == x ) { e.remake<eps_node>(); }  // Match on character
		else { e.remake<fail_node>(); }          // Fail otherwise
	}
	
	gen_set char_node::match(ind) const { return gen_set{}; }
	
	gen_set char_node::back(ind)  const { return gen_set{0}; }
	
	// range_node //////////////////////////////////////////////////////////////////
	
	expr range_node::make(char b, char e) { return expr::make<range_node>(b, e); }
	
	expr range_node::clone() { return expr::make<range_node>(b, e); }
	
	void range_node::d(expr& e, char x, ind) {
		if ( b <= x && x <= e ) { e.remake<eps_node>(); }  // Match if in range
		else { e.remake<fail_node>(); }                    // Fail otherwise
	}
	
	gen_set range_node::match(ind) const { return gen_set{}; }
	
	gen_set range_node::back(ind)  const { return gen_set{0}; }
	
	// any_node ////////////////////////////////////////////////////////////////////
	
	expr any_node::make() { return expr::make<any_node>(); }
	
	expr any_node::clone() { return expr::make<any_node>(); }
	
	void any_node::d(expr& e, char x, ind) {
		if ( x == '\0' ) { e.remake<fail_node>(); }  // Fail on no character
		else { e.remake<eps_node>(); }               // Match otherwise
	}
	
	gen_set any_node::match(ind) const { return gen_set{}; }
	
	gen_set any_node::back(ind)  const { return gen_set{0}; }
	
	// str_node ////////////////////////////////////////////////////////////////////
	
	expr str_expr::make(const std::string& s) {
		switch ( s.size() ) {
		case 0:  return eps_expr::make();
		case 1:  return char_expr::make(s[0]);
		default: return expr::make<str_node>(s);
		}
	}
	
	expr str_expr::clone() { return expr::make<str_node>(*this); }
	
	void str_expr::d(expr& e, char x, ind) {
		// REMEMBER CHARS IN s ARE IN REVERSE ORDER
		
		// Check that the first character matches
		if ( s.last() != x ) {
			e.remake<fail_node>();
			return;
		}
				
		// Switch to character if this derivative consumes the penultimate
		if ( s.size() == 2 ) {
			e.remake<char_node>(s[0]);
			return;
		}
		
		// Mutate string node otherwise
		s.pop_back();
	}
	
	gen_set str_expr::match(ind) const { return gen_set{}; }
	
	gen_set str_expr::back(ind)  const { return gen_set{0}; }
	
	// shared_node ///////////////////////////////////////////////////////////////////
	
	expr shared_node::make(expr&& e, ind crnt) {
		return expr::make<shared_node>(e, crnt);
	}
	
	expr shared_node::clone() {
		return expr::make<shared_node>(shared->e.clone(), 
		                               shared->crnt,
		                               shared->prev_cache);
	}
	
	expr shared_node::clone(ind i) {
		return expr::make<shared_node>(shared->e.clone(), i);
	}
	
	void shared_node::d(expr& e, char x, ind i) {
		if ( i == shared->crnt; ) {  // Computing current derivative
			// Cache previous values
			shared->prev_cache.set_back(shared->e.back(i));
			shared->prev_cache.set_match(shared->e.match(i));
			
			// Compute derivative and increment
			shared->e.d(x, i);
			++(shared->crnt);
		} else assert(i == shared->crnt - 1 && "shared node only keeps two generations");
		
		// if we reach here than we know that the previously-computed derivative 
		// was requested, and it's already stored
	}
		
	gen_set shared_node::match(ind i) const {
		if ( i == shared->crnt ) {
			// Current generation, pass through
			return shared->e.match(i);
		} else if ( i == shared->crnt - 1 ) {
			// Previous generation, read from cache
			assert(shared->prev_cache.flags.match && "match cached for previous generation");
			return shared->pref_cache.match;
		} else {
			assert(false && "shared node only keeps two generations");
			return gen_set{};
		}
	}
	
	gen_set shared_node::back(ind i)  const {
		if ( i == shared->crnt ) {
			// Current generation, pass through
			return shared->e.back(i);
		} else if ( i == shared->crnt - 1 ) {
			// Previous generation, read from cache
			assert(shared->prev_cache.flags.back && "back cached for previous generation");
			return shared->pref_cache.back;
		} else {
			assert(false && "shared node only keeps two generations");
			return gen_set{};
		}
	}
	
	// rule_node ////////////////////////////////////////////////////////////////////
	
	// Unlike the usual semantics, we want to reuse the shared rule node and cached functions
	expr rule_node::clone() { return expr::make<rule_node>(*this); }
	
	void rule_node::d(expr& e, char x, ind i) {
		// Break left recursion by returning an inf node
		if ( r.shared->dirty ) {
			e.remake<inf_node>();
			return;
		}
		
		r.shared->dirty = true;  // flag derivative calculations
		e = r.clone(i);          // clone rule into current expression with current index
		e.d(x, i);               // calculate derivative
		r.shared->dirty = false; // lower calculation flag
	}
	
	gen_set rule_node::match(ind) const {
		assert(cache.flags.match && "Rule match() pre-computed");
		return cache.match;
	}
	
	gen_set rule_node::back(ind)  const {
		assert(cache.flags.back && "Rule back() pre-computed");
		return cache.back;
	}
	
	// not_node ////////////////////////////////////////////////////////////////////
	
	expr not_node::make(const expr& s, ind i) {
		switch ( s.type() ) {
		case fail_type: return look_node::make(1);  // Match on subexpression failure
		case inf_type:  return s;                   // Propegate infinite loop
		default:        break;
		}
		
		// return failure on subexpression success
		if ( ! s.match(i).empty() ) return fail_node::make();
		
		return expr::make<not_expr>(s);
	}
	
	expr not_node::clone() { return expr::make<not_node>(s.clone()); }
	
	// Take negative lookahead of subexpression derivative
	void not_node::d(expr& e, char x, ind i) {
		s.d(x, i);  // TAKE DERIVATIVE OF s
		
		// normalize
		switch ( s.type() ) {
		case fail_type: e.remake<look_node>(1); return;
		case inf_type:  e.remake<inf_node>();   return;
		default:                                break;
		}
		
		if ( ! s.match(i+1).empty() ) { e.remake<fail_node>(); }
	}
	
	gen_set not_node::match(ind) const { return gen_set{}; }
	
	gen_set not_node::back(ind) const { return gen_set{1}; }
	
	// map_node ////////////////////////////////////////////////////////////////////
	
	expr map_node::make(const expr& s, const gen_map& sg, gen_type gm, ind i) {
		// account for unmapped generations
		assert(!sg.empty() && "non-empty generation map");
		assert(s.back(i).max() <= sg.max_key() && "no unmapped generations");
		assert(sg.max() <= gm && "max is actually max");
		
		switch ( s.type() ) {
		// Map expression match generation into exit generation
		case eps_type:  return look_expr::make(sg(0));
		case look_type: return look_expr::make(sg(s.match().max()));
		// Propegate fail and infinity errors
		case fail_type: return s; // a fail node
		case inf_type:  return s; // an inf node
		default:        break; // do nothing
		}
		
		// check if map isn't needed (identity map)
		if ( gm == sg.max_key() ) return s;
		
		return expr::make<map_node>(s, sg, gm);
	}
	
	expr map_node::clone() { return expr::make<map_node>(s.clone(), sg, gm); }
	
	void map_node::d(expr& e, char x, ind i) {
		gen_type sbm = s.back(i).max();
		s.d(x, i);  // TAKE DERIV OF s
		cache.invalidate();
		
		// Normalize
		switch ( s.type() ) {
		// Map subexpression match into exit generation
		case eps_type:  e = look_node::make(sg(0));               return;
		case look_type: e = look_node::make(sg(s.match().max())); return;
		// Propegate fail and infinity errors
		case fail_type: e.remake<fail_node>();                    return;
		case inf_type:  e.remake<inf_node>();                     return;
		default:                                                  break;
		}
		
		// Add new mapping if needed
		update_back_map(eg, sbm, s, gm, i+1);
	}
	
	gen_set map_node::match(ind i) const {
		if ( ! cache.flags.match ) { cache.set_match( sg(s.match(i)) ); }
		return cache.match;
	}
	
	gen_set map_node::back(ind i) const {
		if ( ! cache.flags.back ) { cache.set_back( sg(s.back(i)) ); }
		return cache.back;
	}
	
	// alt_node ////////////////////////////////////////////////////////////////////
	
	expr alt_node::make(const expr& a, const expr& b) {
		switch ( a.type() ) {
		// if first alternative fails, use second
		case fail_type: return b;
		// if first alternative is infinite loop, propegate
		case inf_type:  return a;
		default:        break;
		}
		
		// if first alternative matches or second alternative fails, use first
		if ( b.type() == fail_type || ! a.match().empty() ) return a;
		
		bool did_inc = false;
		gen_map ag = default_back_map(a, did_inc);
		gen_map bg = default_back_map(b, did_inc);
		return expr::make<alt_node>(a, b, ag, bg, did_inc ? 1 : 0);
	}
	
	expr alt_node::make(const expr& a, const expr& b, 
	                    const gen_map& ag, const gen_map& bg, 
	                    gen_type gm, ind i) {
		assert(gm >= ag.max() && gm >= bg.max() && "gm is actual maximum");
	    
		switch ( a.type() ) {
		// if first alternative fails, use second
		case fail_type: return map_expr::make(b, gm, bg, i);
		// if first alternative is infinite loop, propegate
		case inf_type:  return a;
		default:        break;
		}
		
		// if first alternative matches or second alternative fails, use first
		if ( b.type() == fail_type || ! a.match().empty() ) {
			return map_expr::make(a, gm, ag, i);
		}
		
		return expr::make<alt_node>(a, b, ag, bg, gm);
	}
	
	expr alt_node::clone() {
		return expr::make<alt_node>(a.clone(), b.clone(), ag, bg, gm);
	}
	
	void alt_node::d(expr& e, char x, ind i) {
		gen_type abm = a.back(i).max();
		a.d(x, i);  // TAKE DERIV OF a
		
		// Check conditions on a before we calculate dx(b) [same as make()]
		switch ( a.type() ) {
		case fail_type: {
			// Return map of b
			gen_type bbm = b.back(i).max(); 
			b.d(x, i);  // TAKE DERIV OF b
			update_back_map(bg, bbm, b, gm, i+1);
			e = map_node::make(b, bg, gm, i+1);
			return;
		}
		case inf_type:  e.remake<inf_node>(); return;
		default:                              break;
		}
		
		// Map in new lookahead generations for derivative
		bool did_inc = false;
		update_back_map(ag, abm, a, gm, did_inc, i+1);
		
		// Eliminate second alternative if first matches
		if ( ! a.match().empty() ) {
			e = map_node::make(a, ag, did_inc ? gm+1 : gm, i+1);
			return;
		}
		
		// Calculate other derivative and map in new lookahead generations
		gen_type bbm = b.back().max();
		b.d(x, i);  // TAKE DERIV OF b
		
		// Eliminate second alternative if it fails
		if ( b.type() == fail_type ) {
			e = map_node::make(a, ag, did_inc ? gm+1 : gm, i+1);
			return;
		}
		update_back_map(bg, bbm, b, gm, did_inc, i+1);
		
		if ( did_inc ) { ++gm; }
	}
	
	gen_set alt_node::match(ind i) const {
		if ( ! cache.flags.match ) {
			cache.set_match( ag(a.match(i)) | bg(b.match(i)) );
		}
		return cache.match;
	}
	
	gen_set alt_node::back(ind i) const {
		if ( ! cache.flags.back ) {
			cache.set_back( ag(a.back(i)) | bg(b.match(i)) );
		}
		return cache.back;
	}
	
	// seq_node ////////////////////////////////////////////////////////////////////
	
	expr seq_node::make(const expr& a, const expr& b) {
		switch ( b.type() ) {
		// empty second element just leaves first
		case eps_type:  return a;
		// failing second element propegates
		case fail_type: return b;
		default: break;  // do nothing
		}
		
		switch ( a.type() ) {
		// empty first element or lookahead success just leaves follower
		case eps_type:
		case look_type:
			return b;
		// failure or infinite loop propegates
		case fail_type:
		case inf_type:
			return a;
		default: break;  // do nothing
		}
		
		bool did_inc = false;
		
		// Set up match-fail follower
		expr c = fail_expr::make();
		gen_map cg{0};
		
		gen_set am = a.match();
		if ( ! am.empty() && am.min() == 0 ) {
			c = b.clone();
			update_back_map(cg, 0, b, gm, did_inc, 0);
		}
		
		// set up lookahead follower
		look_list bs;
		if ( a.back().max() > 0 ) {
			assert(a->back().max() == 1 && "static backtrack gen <= 1");
			
			gen_type gl = 0;
			gen_set bm = b.match();
			if ( ! bm.empty() && bm.min() == 0 ) {
				gl = 1;
				did_inc = true;
			}
			
			bs.emplace_front(1, b.clone(), default_back_map(b, did_inc), gl);
		}
		
		// return constructed expression
		return expr::make<seq_node>(a, b, bs, c, cg, did_inc ? 1 : 0);
	}
		
	expr seq_node::clone() {
		// clone lookahead list
		look_list cbs;
		auto cbt = cbs.before_begin();
		for (auto bi : bs) {
			cbt = cbs.emplace_after(cbt, bi.g, bi.s.clone(), bi.sg, bi.gl);
		}
		
		return expr::make<seq_node>(a.clone(), b.clone(), cbs, c.clone(), cg, gm);
	}
	
	void seq_node::d(expr& e, char x, ind i) {
		gen_type abm = a.back(i).max();
		a.d(x, i);  // TAKE DERIV OF a
		
		// Handle empty or failure results from predecessor derivative
		switch ( a.type() ) {
		case eps_type: {
			// Take follower (or follower's end-of-string derivative on end-of-string)
			if ( x == '\0' ) b.d('\0', i);  // TAKE DERIV OF b
			gen_map bg = new_back_map(b, gm, i+1);
			e = map_expr::make(b, bg, gm, i+1);
			return;
		} case look_type: {
			// Take lookahead follower (or lookahead follower match-fail)
			gen_type g = a.match(i+1).max();
			for (const look_node& bi : bs) {
				// find node in (sorted) generation list
				if ( bi.g < g ) continue;
				if ( bi.g > g ) {
					e.remake<fail_node>();
					return;
				}
				
				gen_type bibm = bi.s.back(i).max();
				bi.s.d(x, i);  // TAKE DERIV OF bi
				
				if ( bi.s.type() == fail_type ) {  // straight path fails ...
					if ( bi.gl > 0 ) {                // ... but matched in the past
						e.remake<look_node>(bi.gl);      // return appropriate lookahead
					} else {                          // ... and no previous match
						e.remake<fail_node>();           // return a failure expression
					}
					return;
				}
				
				update_back_map(bi.sg, bibm, bi.s, gm, i+1);
				
				// if there is no failure backtrack (or this generation is it) 
				// we don't have to track it
				gen_set dbim = bi.s.match(i+1);
				if ( bi.gl == 0 || (! dbim.empty() && dbim.min() == 0) ) {
					e = map_expr::make(bi.s, bi.sg, gm);
					return;
				}
				
				// Otherwise return alt-expr of this lookahead and its failure backtrack
				e = alt_node::make(bi.s, look_node::make(), bi.sg, gen_map{0,bi.gl}, gm);
				return;
			}
			// end-of-string is only case where we can get a lookahead success for an unseen gen
			if ( x == '\0' ) {
				b.d('\0', i);  // TAKE DERIV OF b
				gen_map bg = new_back_map(bn, gm, i+1);
				e = map_node::make(bn, bg, gm, i+1);
				return;
			}
			e.remake<fail_node>(); // if lookahead follower not found, fail
			return;
		} case fail_type: {
			// Return derivative of match-fail follower
			gen_type cbm = c.back(i).max();
			c.d(x, i);  // TAKE DERIV OF c
			update_back_map(cg, cbm, c, gm, i+1);
			e = map_node::make(c, cg, gm);
			return;
		} case inf_type: {
			// Propegate infinite loop
			e.remake<inf_node>();
			return;
		} default: {}  // do nothing
		}
		
		bool did_inc = false;
		
		// Update match-fail follower
		gen_set dam = da->match();
		if ( ! dam.empty() && dam.min() == 0 ) {
			// new failure backtrack
			c = b.clone();
			cg = new_back_map(b, gm, did_inc, i);
		} else {
			// continue previous failure backtrack
			gen_type cbm = c.back(i).max();
			c.d(x, i);  // TAKE DERIV OF c
			update_back_map(cg, cbm, c, gm, did_inc, i+1);
		}
		
		// Build derivatives of lookahead backtracks
		gen_set dab = a.back(i+1);
		auto dabt = dab.begin();
		assert(dabt != dab.end() && "backtrack set non-empty");
		if ( *dabt == 0 ) { ++dabt; }  // skip backtrack generation 0
		auto bit = bs.begin();
		auto bilt = bs.before_begin();

		while ( dabt != dab.end() && bit != bs.end() ) {
			const look_node& bi = *bit;
			
			// erase generations not in the backtrack list
			if ( bi.g < *dabt ) {
				bit = bs.erase_after(bilt);
				continue;
			}
			assert(bi.g == *dabt && "no generations missing from backtrack list");
			
			// take derivative of lookahead
			gen_type bibm = bi.s.back(i).max();
			bi.s.d(x, i);  // TAKE DERIV OF bi
			update_back_map(bi.sg, bibm, bi.s, gm, did_inc, i+1);
			
			gen_set dbim = dbi->match();
			if ( ! dbim.empty() && dbim.min() == 0 ) {  // set new match-fail backtrack if needed
				++bi.gl;
				did_inc = true;
			}
			
			++dabt; bilt = bit; ++bit;
		}
		
		// Add new lookahead backtrack if needed
		if ( dabt != dab.end() ) {
			gen_type dabm = *dabt;
			assert(dabm > abm && "leftover generation greater than previous");
			assert(++dabt == dab.end() && "only one new lookahead backtrack");
			
			gen_type gl = 0;
			gen_set bm = b.match();
			if ( ! bm.empty() && bm.min() == 0 ) {  // set new match-fail backtrack if needed
				gl = gm+1;
				did_inc = true;
			}
			dbs.emplace_after(bilt, dabm, b.clone(), new_back_map(b, gm, did_inc, i), gl);
		}
		
		if ( did_inc ) ++gm;
	}
	
	gen_set seq_node::match(ind i) const {
		if ( cache.flags.match ) return cache.match;
		
		// Include matches from match-fail follower
		gen_set x = cg(c.match(i));
		
		// Include matches from matching lookahead successors
		gen_set am = a.match(i);
		
		auto at = am.begin();
		auto bit = bs.begin();
		while ( at != am.end() && bit != bs.end() ) {
			auto& bi = *bit;
			auto  ai = *at;
			
			// Find matching generations
			if ( bi.g < ai ) { ++bit; continue; }
			else if ( bi.g > ai ) { ++at; continue; }
			
			// Add followers to match set as well as follower match-fail
			x |= bi.sg(bi.s.match(i));
			if ( bi.gl > 0 ) x |= bi.gl;
			
			++at; ++bit;
		}
		
		cache.set_match(x);
		return cache.match;
	}
	
	gen_set seq_node::back(ind i) const {
		if ( cache.flags.back ) return cache.back;
		
		// Check for gen-zero backtrack from predecessor
		gen_set x = ( a.back(i).min() == 0 ) ? gen_set{0} : gen_set{};
		
		// Include backtrack from match-fail follower
		x |= cg(c.back(i));
		
		// Include lookahead follower backtracks
		for (const look_node& bi : bs) {
			x |= bi.sg(bi.s.back(i));
			if ( bi.gl > 0 ) x |= bi.gl;
		}
		
		cache.set_back(x);
		return cache.match;
	}
	
} // namespace derivs
