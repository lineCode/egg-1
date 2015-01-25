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
#include <list>
#include <unordered_map>
#include <unordered_set>

#include "../dlf.hpp"
#include "../utils/strings.hpp"

namespace dlf {

	// Pretty-printer for DLF parse trees
	class printer : public visitor {
		// np passed by reference to not throw off ref-count
		void print_deduped(const ptr<node>& np) {
			// look up node
			const node* key = np.get();
			auto it = dups.find(key);

			if ( it == dups.end() ) {
				// not in duplicated set

				// just print non-duplicated nodes (and singleton terminals)
				auto ty = np->type();
				if ( np.unique()
					 || ty == match_type || ty == fail_type || ty == inf_type || ty == end_type ) {
					np->accept(this);
					return;
				}

				// otherwise add to set and print
				unsigned long ni = dups.size();
				dups.emplace(key, ni);
				out << ":" << ni << " ";
				np->accept(this);
			} else {
				// already seen
				out << "@" << it->second;
			}
		}

		/// Prints an arc, with its restrictions and successor
		void print_arc(const arc& a, bool follow_override = false) {
			if ( ! a.blocking.empty() ) {
				out << "[";
				auto ii = a.blocking.begin();
				out << (*ii)->cut;
//				out << *ii;
				while ( ++ii != a.blocking.end() ) {
					out << " " << (*ii)->cut;
//					out << " " << *ii;
				}
				out << "]";
				if ( do_follow || follow_override ) out << " ";
			}
			if ( do_follow ) print_deduped(a.succ);
			else if ( follow_override ) a.succ->accept(this);
		}

		/// Prints the set of un-printed nonterminals
		void print_nts() {
			for (auto it = pl.begin(); it != pl.end(); ++it) {
				out << (*it)->name << " := ";
				print_deduped((*it)->sub);
				out << std::endl;
			}
			pl.clear();
		}
	public:
		printer(std::ostream& out = std::cout,
		        const std::unordered_set<ptr<nonterminal>>& rp
		              = std::unordered_set<ptr<nonterminal>>{})
			: out{out}, rp{rp}, pl{}, dups{}, do_follow{true} {}

		/// Prints the definition of a rule
		void print(ptr<nonterminal> nt) {
			// Add this nonterminal to the list of ones to print, then print all of them
			rp.emplace(nt);
			pl.emplace_back(nt);
			print_nts();
			dups.clear();  // shouldn't persist duplicates over top-level calls
		}

		/// Prints an expression
		void print(ptr<node> n) {
			// Print the node, followed by any unprinted rules
			print_deduped(n);
			out << std::endl;
			print_nts();
			dups.clear();  // shouldn't persist duplicates over top-level calls
		}

		/// Prints an arc
		void print(const arc& a) {
			// Print the arc, followed by any unprinted rules
			print_arc(a);
			out << std::endl;
			print_nts();
			dups.clear();  // shouldn't persist duplicates over top-level calls
		}

		/// Prints an arcs restrictions and immediate followers, but nothing else
		void print_next(const arc& a) {
			do_follow = false;
			print_arc(a, true);
			out << std::endl;
			do_follow = true;
		}

		/// Prints the nonterminal; optionally provides output stream and pre-printed rules
		static void print(ptr<nonterminal> nt, std::ostream& out,
		                  const std::unordered_set<ptr<nonterminal>>& rp
		                        = std::unordered_set<ptr<nonterminal>>{}) {
			printer p(out, rp);
			p.print(nt);
		}

		/// Prints the expression; optionally provides output stream and pre-printed rules
		static void print(ptr<node> n, std::ostream& out,
		                  const std::unordered_set<ptr<nonterminal>>& rp
		                        = std::unordered_set<ptr<nonterminal>>{}) {
			printer p(out, rp);
			p.print(n);
		}

		/// Prints the arc; optionally provides output stream and pre-printed rules
		static void print(const arc& a, std::ostream& out,
		                  const std::unordered_set<ptr<nonterminal>>& rp
		                        = std::unordered_set<ptr<nonterminal>>{}) {
			printer p(out, rp);
			p.print(a);
		}

		/// Prints an arc's restrictions and immediate followers
		static void next(const arc& a, std::ostream& out = std::cout) {
			printer p(out);
			p.print_next(a);
		}

		virtual void visit(const match_node&)  { out << "{MATCH}"; }
		virtual void visit(const fail_node&)   { out << "{FAIL}"; }
		virtual void visit(const inf_node&)    { out << "{INF}"; }
		virtual void visit(const end_node&)    { out << "{END}"; }

		virtual void visit(const char_node& n) {
			out << "\'" << strings::escape(n.c) << "\' ";
			print_arc(n.out);
		}

		virtual void visit(const range_node& n) {
			out << "\'" << strings::escape(n.b) << "-" << strings::escape(n.e) << "\' ";
			print_arc(n.out);
		}

		virtual void visit(const any_node& n) {
			out << ". ";
			print_arc(n.out);
		}

		virtual void visit(const str_node& n) {
			out << "\"" << strings::escape(n.str()) << "\" ";
			print_arc(n.out);
		}

		virtual void visit(const rule_node& n) {
			// Mark rule for printing if not yet printed
			if ( rp.count(n.r) == 0 ) {
				rp.emplace(n.r);
				pl.emplace_back(n.r);
			}
			out << n.r->name << " ";
			print_arc(n.out);
		}

		virtual void visit(const cut_node& n) {
			out << "<" << n.cut << "> ";
			print_arc(n.out);
		}

		virtual void visit(const alt_node& n) {
			out << "(";
			auto it = n.out.begin();
			if ( it != n.out.end() ) do {
				print_arc(*it, true);
				if ( ++it == n.out.end() ) break;
				out << " | ";
			} while (true);
			out << ")";
		}

	private:
		std::ostream& out;                        ///< output stream
		std::unordered_set<ptr<nonterminal>> rp;  ///< Rules that have already been printed
		std::list<ptr<nonterminal>> pl;           ///< List of rules to print
		std::unordered_map<const node*,
		                   unsigned long> dups;   ///< IDs for duplicated nodes
		bool do_follow;                           ///< Flag to follow successor arcs
	}; // printer

} // namespace dlf
