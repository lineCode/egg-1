#pragma once

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

#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../ast.hpp"
#include "../dlf.hpp"

#include "../utils/flagvector.hpp"

#include "dlf-printer.hpp"

namespace dlf {

	/// Loads a set of derivatives from the grammar AST
	class loader : public ast::visitor {
		/// Gets unique nonterminal for each name
		ptr<nonterminal> get_nonterminal(const std::string& s) {
			auto it = nts.find(s);
			if ( it == nts.end() ) {
				ptr<nonterminal> nt = make_ptr<nonterminal>(s, fail_node::make());
				nts.emplace(s, nt);
				return nt;
			} else {
				return it->second;
			}
		}
		
		/// Sets unique nonterminal for each name
		void set_nonterminal(const std::string& s, ptr<node> n) { get_nonterminal(s)->reset(n); }
		
		/// Produces a new arc to the next node
		arc out(flags::vector&& blocking = flags::vector{}) {
			return arc{next, mgr, std::move(blocking), flags::vector{next_cuts}};
		}

		/// Sets the next and next_cut values
		inline void set_next(ptr<node> n) {
			next = n;
			next_cuts.clear();
		}
		
		inline void set_next(ptr<node> n, flags::vector&& cuts) {
			next = n;
			next_cuts = std::move(cuts);
		}
		
		void make_many(ptr<ast::matcher> mp) {
			// idea is to set up a new anonymous non-terminal R_i and set next to R_i
			// R_i = m.m [^ri] R_i end | [ri] end
			
			// set rule node for new anonymous non-terminal
			ptr<nonterminal> R_i = make_ptr<nonterminal>("*" + std::to_string(mi++));
			ptr<node> nt = rule_node::make(out(), R_i, mgr);
			
			// build anonymous rule
			flags::index i = ri++;                      // get a restriction index to use
			set_next(end_node::make());                 // make end node for rule
			arc skip = out(flags::vector::of(i));       // save arc that skips match
			set_next(rule_node::make(out(), R_i, mgr),  // build recursive invocation of rule
			         flags::vector::of(i));             // set up cut on out-edges of many-expression
			flags::index ri_bak = ri; ri = 0;           // save ri
			mp->accept(this);                           // build many-expression
			ri = ri_bak;                                // restore ri
			R_i->reset(alt_node::make({out(), skip}));  // reset rule
			
			// reset next to rule reference
			set_next(nt);
		}

	public:
		/// Builds a DLF parse DAG from the given PEG grammar
		loader(ast::grammar& g, bool dbg = false) 
		: mgr{}, nts{}, next{}, next_cuts{}, ri{0}, mi{0} {
			// Read in rules
			for (auto r : g.rs) {
				set_next(end_node::make());
				r->m->accept(this);
				set_nonterminal(r->name, next);
				ri = 0;
			}
			
			if ( dbg ) {
				dlf::printer p;
				for (auto ntp : nts) {
					p.print(ntp.second);
				}
				std::cout << "\n***** DONE LOADING RULES *****\n" << std::endl;
			}
		}
		
		std::map<std::string, ptr<nonterminal>>& get_nonterminals() { return nts; }
		
		virtual void visit(ast::char_matcher& m) { set_next(char_node::make(out(), m.c)); }
		
		virtual void visit(ast::str_matcher& m) { set_next(str_node::make(out(), m.s)); }
		
		virtual void visit(ast::range_matcher& m) {
			std::vector<arc> rs;
			for (const ast::char_range& r : m.rs) {
				rs.emplace_back(arc{range_node::make(out(), r.from, r.to), mgr});
			}
			set_next(alt_node::make(rs.begin(), rs.end()));
		}
		
		virtual void visit(ast::rule_matcher& m) {
			set_next(rule_node::make(out(), get_nonterminal(m.rule), mgr));
		}
		
		virtual void visit(ast::any_matcher& m) { set_next(any_node::make(out())); }
		
		virtual void visit(ast::empty_matcher& m) { /* do nothing; next remains next */ }
		
		virtual void visit(ast::action_matcher& m) { /* TODO implement; for now no-op */ }
		
		virtual void visit(ast::opt_matcher& m) {
			// Idea: m.m [^i] next | [i] next
			flags::index i = ri++;                    // get a restriction index to use
			arc skip = out(flags::vector::of(i));     // save arc that skips the optional
			next_cuts |= i;                           // add restriction to cut-set
			m.m->accept(this);                        // build opt-expression
			set_next(alt_node::make({out(), skip}));  // make alternation of two paths
		}
		
		virtual void visit(ast::many_matcher& m) {
			make_many(m.m);  // generate new many-rule nonterminal
		}
		
		virtual void visit(ast::some_matcher& m) {
			make_many(m.m);     // generate new many-rule nonterminal
			m.m->accept(this);  // sequence one copy of the rule before
		}
		
		virtual void visit(ast::seq_matcher& m) {
			// build out sequence in reverse order
			for (auto it = m.ms.rbegin(); it != m.ms.rend(); ++it) { (*it)->accept(this); }
		}
		
		virtual void visit(ast::alt_matcher& m) {
			// Idea: m0 [^0] next | [0] m1 [^1] next | ... | [0...n-1] mn next
			ptr<node> alt_next = next;           // save next value
			flags::vector alt_cuts = next_cuts;  // ... and cuts
			
			flags::vector blocking;              // cuts for greedy longest match
			
			std::vector<arc> rs;
			for (auto& mi : m.ms) {
				flags::index i = ri++;                          // get a restriction index to use
				next_cuts |= i;                                 // flag cut for greedy longest match
				mi->accept(this);                               // build subexpression
				rs.emplace_back(out(flags::vector{blocking}));  // add to list of arcs
				set_next(alt_next, flags::vector{alt_cuts});    // restore next values for next iteration
				blocking |= i;                                  // add index to greedy longest match blocker
			}
			set_next(alt_node::make(rs.begin(), rs.end()));
		}
		
		virtual void visit(ast::look_matcher& m) {
			// Idea - !!m.m: m.m [^j] fail | [j ^i] fail | [i] next
			// If m.m matches, we cut out the [j ^i] branch, freeing next to proceed safely
			
			// save restriction indices
			flags::index j = ri++;
			flags::index i = ri++;
			// build continuing branch
			arc cont = out(flags::vector::of(i));
			// build cut branch
			set_next(fail_node::make(), flags::vector::of(i));
			arc cut = out(flags::vector::of(j));
			// build matching branch
			set_next(fail_node::make(), flags::vector::of(j));
			m.m->accept(this);
			// set alternate paths
			next = alt_node::make({cont, cut, out()});
		}
		
		virtual void visit(ast::not_matcher& m) {
			// Idea - match both paths, failing if the not path matches: m.m [^i] fail | [i] next
			flags::index i = ri++;                    // get a restriction index to use
			arc cont = out(flags::vector::of(i));     // build continuing branch
			set_next(fail_node::make(),               // terminate blocking branch
                                 flags::vector::of(i));              // with a cut on the match index
			m.m->accept(this);                        // build blocking branch
			set_next(alt_node::make({cont, out()}));  // alternate continuing and blocking branches
		}
		
		virtual void visit(ast::capt_matcher& m) {
			// TODO implement; for now ignore the capture
			m.m->accept(this);
		}
		
		virtual void visit(ast::named_matcher& m) {
			// TODO implement; for now ignore the error message
			m.m->accept(this);
		}
		
		virtual void visit(ast::fail_matcher& m) {
			// TODO complete implementation; for now ignore the error message
			set_next(fail_node::make());
		}
		
	private:
		state_mgr mgr;                                ///< State manager
		// NOTE it is *very* important that mgr be declared before nts; we also want to 
		// deallocate nts first so that the nonterminals it's managing still have a 
		// manager when the references in them expire
		std::map<std::string, ptr<nonterminal>> nts;  ///< List of non-terminals
		ptr<node> next;                               ///< Next node
		flags::vector next_cuts;                      ///< Cuts to apply before next node
		flags::index ri;                              ///< Current restriction index
		unsigned long mi;                             ///< Index to uniquely name many-nodes
	}; // loader
	
	/// Recognizes the input
	/// @param l		Loaded DLF DAG
	/// @param in		Input stream
	/// @param rule		Start rule
	/// @param dbg		Print debug output? (default false)
	/// @return true for match, false for failure
	bool match(loader& l, std::istream& in, std::string rule, bool dbg = false) {
		// find rule
		auto& nts = l.get_nonterminals();
		auto nt = nts.find(rule);
		
		// fail on no such rule
		if ( nt == nts.end() ) return false;
		
		// Check for initial success
		if ( nt->second->nullable() ) return true;
		
		// set up printer
		std::unordered_set<ptr<nonterminal>> names;
		for (auto& nit : nts) { names.emplace(nit.second); }
		dlf::printer p(std::cout, names);
		
		// establish initial expression
		state_mgr mgr;
		arc e = matchable(nt->second, mgr);
		
		// teke derivatives until failure, match, or end of input
		char x = '\0';
		do {
			if ( dbg ) { p.print(e.succ); }
			
			if ( ! in.get(x) ) { x = '\0'; }  // read character, \0 for EOF
			
			if ( dbg ) {
				std::cout << "d(\'" << (x == '\0' ? "\\0" : strings::escape(x)) << "\') =====>" 
				          << std::endl;
			}
			
			// take derivative; return true if unrestricted match
			if ( e.d(x) ) {
				if ( dbg ) { p.print(e.succ); }
				return true;
			}
		} while ( mgr.match_reachable && x != '\0' );
		
		if ( dbg ) { p.print(e.succ); }
		
		return false;
	}
	
	/// Recognizes the input
	/// @param g		Source grammar
	/// @param in		Input stream
	/// @param rule		Start rule
	/// @param dbg		Print debug output? (default false)
	/// @return true for match, false for failure
	bool match(ast::grammar& g, std::istream& in, std::string rule, bool dbg = false) {
		loader l(g, dbg);
		return match(l, in, rule, dbg);
	}

}  // namespace dlf

